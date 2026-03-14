#include "core/World.hpp"

#include "core/Renderer.hpp"
#include "shape/Spring.hpp"
#include "shape/Trigger.hpp"
#include "shape/Thruster.hpp"
#include "collision/Collisions.hpp"
#include "common/settings.hpp"

WorldSnapshot World::CaptureSnapshot() const
{
	WorldSnapshot snap;
	snap.bodies.reserve(objects.size());
	for (const auto &obj : objects)
	{
		snap.bodies.push_back({obj->pos,
							   obj->linearVel,
							   obj->linearAcc,
							   obj->rotation,
							   obj->angularVel,
							   obj->forces});
	}
	return snap;
}

void World::RestoreSnapshot(const WorldSnapshot &snap)
{
	// Body count must match — don't rewind across spawns/deletions
	const size_t n = std::min(snap.bodies.size(), objects.size());
	for (size_t i = 0; i < n; i++)
	{
		objects[i]->pos = snap.bodies[i].pos;
		objects[i]->linearVel = snap.bodies[i].linearVel;
		objects[i]->linearAcc = snap.bodies[i].linearAcc;
		objects[i]->rotation = snap.bodies[i].rotation;
		objects[i]->angularVel = snap.bodies[i].angularVel;
		objects[i]->forces = snap.bodies[i].forces;
	}
}

int World::ClampIterations(int value) const
{
	if (value < SimulationConstants::MIN_PHYSICS_ITERATIONS)
	{
		return SimulationConstants::MIN_PHYSICS_ITERATIONS;
	}
	if (value > SimulationConstants::MAX_PHYSICS_ITERATIONS)
	{
		return SimulationConstants::MAX_PHYSICS_ITERATIONS;
	}
	return value;
}

int World::ComputeAdaptiveIterations(float deltaMs, int requestedIterations) const
{
	constexpr float CFL_SAFETY_FACTOR = 0.5f; // body must cross at most 50% of its size per step

	int minNeeded = requestedIterations;
	const float dtSeconds = deltaMs / 1000.0f;

	for (const auto &obj : objects)
	{
		if (obj->bodyType != physics::RigidbodyType::Dynamic)
			continue;

		const float speed = obj->linearVel.Length();
		if (speed < 1.0f)
			continue; // Stationary — no boost needed

		// Estimate body size from its axis-aligned bounding box diagonal half-length
		const collision::AABB aabb = obj->GetAABB();
		const float width = aabb.max.x - aabb.min.x;
		const float height = aabb.max.y - aabb.min.y;
		const float bodySize = std::sqrt(width * width + height * height) * 0.5f;

		if (bodySize < 0.1f)
			continue; // Degenerate shape — skip

		// How far does this body travel in the full frame?
		const float distanceThisFrame = speed * dtSeconds;

		// How many steps are needed so each step ≤ CFL_SAFETY_FACTOR * bodySize?
		const int neededIter = static_cast<int>(
			std::ceil(distanceThisFrame / (CFL_SAFETY_FACTOR * bodySize)));

		minNeeded = std::max(minNeeded, neededIter);
	}

	return ClampIterations(minNeeded);
}

void World::SweptContactCorrection(physics::Rigidbody &body,
								   const math::Vec2 &prevPos,
								   const math::Vec2 &integratedPos)
{
	if (body.bodyType != physics::RigidbodyType::Dynamic)
		return;

	// How far did the solver push the body back?
	const math::Vec2 solverCorrection = body.pos - integratedPos;
	const float correctionMag = solverCorrection.Length();

	// If the solver barely moved the body, no significant collision was resolved
	if (correctionMag < 0.5f)
		return;

	// Full displacement vector for this sub-step (before solver)
	const math::Vec2 displacement = integratedPos - prevPos;
	const float dispLen = displacement.Length();

	// Body didn't actually travel — nothing to sweep
	if (dispLen < 0.001f)
		return;

	// ── Binary search for the exact first-contact time t ∈ [0, 1] ────────────
	// t = 0 → prevPos (definitely clear, body was there last sub-step)
	// t = 1 → integratedPos (definitely overlapping, solver already proved it)
	//
	// We want the largest t where the body is still NOT overlapping any static
	// body.  That gives us the surface-contact position.
	constexpr int BINARY_STEPS = 10;		  // 10 steps → position error < 0.1% of dispLen
	constexpr float CONVERGENCE_DELTA = 0.5f; // stop early if bracket width < 0.5 px

	float tLow = 0.0f;
	float tHigh = 1.0f;

	for (int step = 0; step < BINARY_STEPS; ++step)
	{
		if ((tHigh - tLow) * dispLen < CONVERGENCE_DELTA)
			break; // Sub-pixel accuracy reached — stop early

		const float tMid = (tLow + tHigh) * 0.5f;

		// Temporarily teleport the body to the midpoint test position
		body.pos = {
			prevPos.x + displacement.x * tMid,
			prevPos.y + displacement.y * tMid};
		body.aabbUpdateRequired = true;
		body.transformUpdateRequired = true;

		// Check against every STATIC body only (static surfaces are what cause
		// landing errors; dynamic-dynamic pairs are resolved by the solver itself)
		bool overlapsStatic = false;
		for (const auto &other : objects)
		{
			if (other.get() == &body)
				continue;
			if (other->bodyType != physics::RigidbodyType::Static)
				continue;
			if (dynamic_cast<shape::Trigger *>(other.get()))
				continue; // Triggers never block physically

			const collision::HitInfo hit = collision::Collide(body, *other);
			if (hit.result)
			{
				overlapsStatic = true;
				break;
			}
		}

		if (overlapsStatic)
			tHigh = tMid; // Contact point is earlier — search lower half
		else
			tLow = tMid; // Still clear — search upper half
	}

	// tLow is the last confirmed non-overlapping position → exact contact surface
	body.pos = {
		prevPos.x + displacement.x * tLow,
		prevPos.y + displacement.y * tLow};
	body.aabbUpdateRequired = true;
	body.transformUpdateRequired = true;

	// Zero out velocity component into the surface so the body doesn't jitter.
	// We infer the surface normal from the solver's correction direction.
	if (correctionMag > 1e-4f)
	{
		const math::Vec2 surfaceNormal = solverCorrection * (1.0f / correctionMag);
		const float velIntoSurface = math::Vec2::Dot(body.linearVel, surfaceNormal);

		// Only cancel the into-surface component; keep tangential velocity intact
		if (velIntoSurface < 0.0f)
		{
			body.linearVel.x -= surfaceNormal.x * velIntoSurface;
			body.linearVel.y -= surfaceNormal.y * velIntoSurface;
		}
	}
}

void World::Add(std::unique_ptr<physics::Rigidbody> body)
{
	objects.push_back(std::move(body));
}

void World::Initialize(Settings *settings)
{
	this->settings = settings;
}

std::vector<shape::Rope> &World::GetRopes()
{
	return ropes;
}

physics::Rigidbody *World::PickBody(const math::Vec2 &p)
{
	// Helper lambda to perform the actual intersection math
	auto isPointingAt = [&](physics::Rigidbody *obj) -> bool
	{
		const float angle = -obj->rotation;
		const float cosA = std::cos(angle);
		const float sinA = std::sin(angle);
		const float dx = p.x - obj->pos.x;
		const float dy = p.y - obj->pos.y;

		const math::Vec2 localP{
			cosA * dx - sinA * dy + obj->pos.x,
			sinA * dx + cosA * dy + obj->pos.y};

		collision::AABB aabb;
		if (const auto *shape = dynamic_cast<const shape::Shape *>(obj))
			aabb = shape->GetLocalAABB();
		else
			aabb = obj->GetAABB();

		return (localP.x >= aabb.min.x && localP.x <= aabb.max.x &&
				localP.y >= aabb.min.y && localP.y <= aabb.max.y);
	};

	// PASS 1: Check for Thrusters first (so they are always on top of the "stack")
	for (auto &obj : objects)
	{
		if (dynamic_cast<shape::Thruster *>(obj.get()))
		{
			if (isPointingAt(obj.get()))
				return obj.get();
		}
	}

	// PASS 2: Check all other objects
	for (auto &obj : objects)
	{
		if (!dynamic_cast<shape::Thruster *>(obj.get()))
		{
			if (isPointingAt(obj.get()))
				return obj.get();
		}
	}

	return nullptr;
}

math::Vec2 World::SnapToNearestDynamicObject(const math::Vec2 &position, float snapRadius)
{
	physics::Rigidbody *nearestBody = nullptr;
	float nearestDistSq = snapRadius * snapRadius;

	// Search through all objects
	for (const auto &obj : objects)
	{
		// Only snap to dynamic objects

		if (obj->bodyType != physics::RigidbodyType::Dynamic && (!(dynamic_cast<const shape::Incline *>(&*obj)) || !(dynamic_cast<const shape::Cannon *>(&*obj))))
			continue;

		// Calculate distance to object center
		math::Vec2 delta = obj->pos - position;
		float distSq = delta.x * delta.x + delta.y * delta.y;

		// Check if this is the nearest object within snap radius
		if (distSq < nearestDistSq)
		{
			nearestDistSq = distSq;
			nearestBody = obj.get();
		}
	}

	// Return snapped position or original position if no snap
	return nearestBody ? nearestBody->pos : position;
}

void World::Update(float deltaMs, int iterations,
				   physics::Rigidbody *&selectedBody,
				   physics::Rigidbody *&draggedBody)
{
	iterations = ComputeAdaptiveIterations(deltaMs, iterations);

	//shape::Spring *draggedSpring = dynamic_cast<shape::Spring *>(draggedBody);
	// float savedDraggedInvMass = 0.0f;
	// float savedDraggedInvInertia = 0.0f;

	for (const auto &object : objects)
		object->BeginFrameForces();

	// Build static-body list once — passed to rope node collision every sub-step.
	// Triggers are excluded: they are never physical barriers.
	std::vector<physics::Rigidbody *> staticBodies;
	for (const auto &obj : objects)
	{
		if (obj->bodyType == physics::RigidbodyType::Static &&
			!dynamic_cast<shape::Trigger *>(obj.get()) && !obj->isRopeNode)
			staticBodies.push_back(obj.get());
	}

	std::vector<math::Vec2> prevPositions(objects.size());

	for (int currIteration = 0; currIteration < iterations; currIteration++)
	{
		// While dragging a spring, treat it as kinematic/immovable for physics so
		// other objects can't shove it around. The mouse position still controls
		// it via Engine::Update's TranslateTo().
		// if (draggedSpring)
		// {
		// 	savedDraggedInvMass = draggedSpring->invMass;
		// 	savedDraggedInvInertia = draggedSpring->invInertia;

		// 	draggedSpring->invMass = 0.0f;
		// 	draggedSpring->invInertia = 0.0f;
		// 	draggedSpring->linearVel = math::Vec2();
		// 	draggedSpring->linearAcc = math::Vec2();
		// 	draggedSpring->angularVel = 0.0f;
		// 	draggedSpring->angularAcc = 0.0f;
		// 	draggedSpring->netForce = math::Vec2();
		// 	draggedSpring->netTorque = 0.0f;
		// 	draggedSpring->dragForce = math::Vec2();
		// }

		for (size_t i = 0; i < objects.size(); ++i)
			prevPositions[i] = objects[i]->pos;

		// Integrate all Rigidbodies (endpoints integrate here)
		for (const auto &object : objects)
			object->Update(deltaMs, iterations);

		
		// Rope: integrate middle nodes + run stick constraints
		for (auto &rope : ropes)
		{
			rope.Solve(deltaMs,iterations);
		}

		// Rope: thin static-only collision for middle nodes
		
		for (auto &rope : ropes)
			rope.SolveNodeCollisions(staticBodies);

		// Springs: update compression/forces before collision resolution so the
		// collider length tracks the moving top plate within the same tick.
		const float dtSeconds = (iterations > 0)
			? (deltaMs / (1000.0f * static_cast<float>(iterations)))
			: (deltaMs / 1000.0f);
		for (auto &rb : objects)
		{
			if (auto *spring = dynamic_cast<shape::Spring *>(rb.get()))
				spring->PhysicsUpdate(dtSeconds, objects);
		}

		std::vector<math::Vec2> integratedPositions(objects.size());
		for (size_t i = 0; i < objects.size(); ++i)
			integratedPositions[i] = objects[i]->pos;

		// Main collision pipeline — endpoints participate normally.
		// Middle nodes are not in objects so they never appear here.
		collisionPipeline.BuildPairs(objects);
		for (const auto &pair : collisionPipeline.GetPairs())
		{
			physics::Rigidbody &bodyA = *objects[pair.first];
			physics::Rigidbody &bodyB = *objects[pair.second];

			if (dynamic_cast<shape::Trigger *>(&bodyA) ||
				dynamic_cast<shape::Trigger *>(&bodyB))
				continue;
			if (dynamic_cast<shape::Thruster *>(&bodyA) ||
				dynamic_cast<shape::Thruster *>(&bodyB))
				continue;

			// Skip only for the degenerate 2-node rope (endpoints are adjacent)
			if (SameRopeAndAdjacent(bodyA, bodyB))
				continue;

			collisionSolver.ResolveIfColliding(bodyA, bodyB);

			SweptContactCorrection(bodyA,
								   prevPositions[pair.first],
								   integratedPositions[pair.first]);
			SweptContactCorrection(bodyB,
								   prevPositions[pair.second],
								   integratedPositions[pair.second]);
		}

		// if (draggedSpring)
		// {
		// 	draggedSpring->invMass = savedDraggedInvMass;
		// 	draggedSpring->invInertia = savedDraggedInvInertia;
		// }
	}

	for (auto &rope : ropes)
		rope.UpdateTensionDisplay();

	UpdateTriggerCollisions();

	for (const auto &object : objects)
		object->FinalizeForces(deltaMs);

	UpdateTrails(deltaMs);

	if (!settings->recording)
		RemoveObjects(deltaMs, selectedBody, draggedBody);
}

void World::Draw(Renderer &renderer) const
{
	// Trails sit behind everything
	DrawTrails(renderer);

	// Draw all regular bodies.
	// isRopeNode skips the two endpoint Ball bodies — the rope draws them
	// as part of its line segments so they don't render as circles.
	for (const auto &object : objects)
	{
		if (object->isRopeNode)
			continue;
		renderer.DrawShape(*object, object->isHighlighted);
	}

	// Draw each rope: segment lines + taut highlight + pin anchor discs.
	// Middle nodes are pure verlet positions and are never in objects, so
	// they have no DrawShape call at all — DrawRope is their only renderer.
	
    std::cout << ropes.size() << std::endl;
	for (const auto &rope : ropes)
		renderer.DrawRope(rope);
}

size_t World::RigidbodyCount() const
{
	return objects.size();
}

// At the top of World.cpp, add this helper (or make it a private method):
static void SafeErase(std::vector<std::unique_ptr<physics::Rigidbody>> &objects,
					  size_t index)
{
	physics::Rigidbody *dying = objects[index].get();

	// If the dying object is a thruster, Detach() is handled by ~Thruster().
	// If it is a body that thrusters are attached to, we must find those
	// thrusters and detach them NOW — before 'dying' is freed — so they
	// don't hold a dangling attachedBody pointer.
	for (auto &obj : objects)
	{
		if (auto *t = dynamic_cast<shape::Thruster *>(obj.get()))
		{
			if (t->GetAttachedBody() == dying)
				t->Detach();
		}
	}

	objects.erase(objects.begin() + static_cast<long>(index));
}

void World::RemoveObject(physics::Rigidbody *draggedBody)
{

	for (size_t i = 0; i < objects.size(); ++i)
	{
		if (draggedBody == objects[i].get())
		{
			SafeErase(objects, i);
			return;
		}
	}
}

void World::RemoveObjects(float deltaMs, physics::Rigidbody *&selectedBody, physics::Rigidbody *&draggedBody)
{

	float deltaSeconds = deltaMs / 1000.0f;

	for (size_t i = 0; i < objects.size();)
	{

		collision::AABB box = objects[i]->GetAABB();
		if (box.max.y < viewBottom)
		{

			objects[i]->removalTimer += deltaSeconds;
			if (objects[i]->removalTimer >= REMOVAL_TIME_THRESHOLD)
			{
				if (selectedBody == objects[i].get())
					selectedBody = nullptr;
				if (draggedBody == objects[i].get())
					draggedBody = nullptr;
				SafeErase(objects, i);
			}
			else
			{
				++i;
			}
		}

		else
		{
			// 4. RESET the timer if it pops back into view (e.g., via a bounce or explosion)
			objects[i]->removalTimer = 0.0f;
			++i;
		}
	}
}

shape::Rope &World::AddRope(const shape::Rope::SpawnParams &p)
{
	ropes.emplace_back();
	shape::Rope &rope = ropes.back();

	// Step 1: get the two endpoint Ball unique_ptrs — no World knowledge needed
	std::vector<std::unique_ptr<physics::Rigidbody>> endpoints = rope.Build(p);

	// Step 2: World takes ownership, collect raw back-pointers
	std::vector<physics::Rigidbody *> rawPtrs;
	rawPtrs.reserve(endpoints.size());
	for (auto &ep : endpoints)
	{
		rawPtrs.push_back(ep.get());
		Add(std::move(ep));
	}

	// Step 3: Rope builds all RopeNodes (including middle) + constraints
	rope.RegisterEndpoints(rawPtrs, p);

	return rope;
}

// Returns true when bodyA and bodyB belong to the SAME rope AND share a direct
// constraint.  Used to suppress CollisionSolver on adjacent node pairs.
bool World::SameRopeAndAdjacent(const physics::Rigidbody &bodyA,
								const physics::Rigidbody &bodyB) const
{
	for (const auto &rope : ropes)
	{
		if (rope.ContainsBody(&bodyA) && rope.ContainsBody(&bodyB))
			return rope.AreAdjacent(&bodyA, &bodyB);
	}
	return false;
}

// Returns a pointer to the Rope that owns `body`, or nullptr.
shape::Rope *World::FindRopeForBody(physics::Rigidbody *body)
{
	for (auto &rope : ropes)
	{
		if (rope.ContainsBody(body))
			return &rope;
	}
	return nullptr;
}

const std::vector<shape::Rope> &World::GetRopes() const
{
	return ropes;
}

void World::UpdateTriggerCollisions()
{

	// Reset all triggers' collision states
	for (auto &obj : objects)
	{
		if (auto trigger = dynamic_cast<shape::Trigger *>(obj.get()))
		{
			trigger->isColliding = false;
		}
	}

	// Check for collisions between triggers and all other objects
	for (size_t i = 0; i < objects.size(); ++i)
	{
		if (auto trigger = dynamic_cast<shape::Trigger *>(objects[i].get()))
		{
			bool currentlyColliding = false;

			for (size_t j = 0; j < objects.size(); ++j)
			{
				if (i != j) // Don't test trigger against itself
				{
					physics::Rigidbody &otherBody = *objects[j];
					const collision::HitInfo hit = collision::Collide(*trigger, otherBody);

					if (hit.result)
					{
						currentlyColliding = true;
						trigger->isColliding = true;

						// Only trigger if entering (wasn't colliding last frame but is now)
						if (!trigger->wasCollidingLastFrame)
						{
							// Execute trigger action if configured
							if (trigger->action == shape::TriggerAction::PauseSimulation)
							{
								// Pause simulation via settings reference
								settings->paused = true;
							}
						}

						break; // Found a collision, no need to check further
					}
				}
			}

			// Update the state for next frame
			trigger->wasCollidingLastFrame = currentlyColliding;
		}
	}
}

void World::ClearObjects()
{
	// Detach all thrusters before clearing so no generator outlives
	// the thruster that owns it
	for (auto &obj : objects)
	{
		if (auto *t = dynamic_cast<shape::Thruster *>(obj.get()))
			t->Detach();
	}
	objects.clear();
	trails.clear();
	ropes.clear();
}

void World::SetCameraInfo(const math::Vec2 &pos, float zoom)
{
	cameraPos = pos;
	cameraZoom = zoom;
}

void World::StartTrail(physics::Rigidbody *projectile, float lifetime)
{
	ProjectileTrail trail;
	trail.projectile = projectile;
	trail.lifetime = lifetime;
	trail.lastPointTime = 0.0f;
	trails.push_back(trail);
}

void World::UpdateTrails(float deltaMs)
{
	float deltaSeconds = deltaMs / 1000.0f;
	const size_t MAX_POINTS_PER_TRAIL = 300;	// Reduced from 500 for better performance
	const size_t MAX_TOTAL_TRAIL_POINTS = 5000; // Global cap on all trail points

	// Calculate total points to enforce global limit
	size_t totalPoints = 0;
	for (const auto &trail : trails)
	{
		totalPoints += trail.points.size();
	}

	// Update all active trails
	for (auto it = trails.begin(); it != trails.end();)
	{
		ProjectileTrail &trail = *it;

		// Validate that projectile pointer still points to a valid object
		bool projectileStillValid = false;
		if (trail.projectile && trail.isActive)
		{
			for (const auto &obj : objects)
			{
				if (obj.get() == trail.projectile)
				{
					projectileStillValid = true;
					break;
				}
			}
		}

		// Mark as inactive if projectile is gone
		if (!projectileStillValid)
		{
			trail.isActive = false;
		}

		trail.lastPointTime += deltaSeconds;

		if (trail.lastPointTime >= trail.pointSpacing &&
			projectileStillValid &&
			trail.points.size() < MAX_POINTS_PER_TRAIL &&
			totalPoints < MAX_TOTAL_TRAIL_POINTS)
		{
			TrailPoint point;
			point.position = trail.projectile->pos;
			point.color = dynamic_cast<shape::Shape *>(trail.projectile)->GetColor();
			point.radius = 3.0f;
			point.age = 0.0f;
			point.lifetime = trail.lifetime;

			trail.points.push_back(point);
			trail.lastPointTime = 0.0f;
			totalPoints++;
		}

		// Update all trail points (age them)
		for (auto &point : trail.points)
		{
			point.age += deltaSeconds;
		}

		// Remove expired trail points
		trail.points.erase(
			std::remove_if(trail.points.begin(), trail.points.end(),
						   [](const TrailPoint &p)
						   { return p.age >= p.lifetime; }),
			trail.points.end());

		// Remove trail entirely if:
		//   • It is inactive AND
		//   • It has no points left
		if (!trail.isActive && trail.points.empty())
		{
			it = trails.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void World::DrawTrails(Renderer &renderer) const
{
	// Draw each trail with LOD culling
	for (const auto &trail : trails)
	{
		for (const auto &point : trail.points)
		{
			// Calculate LOD multiplier based on distance from camera
			float lodMultiplier = CalculateLODMultiplier(point.position);

			// Skip points if LOD culls them (random sampling at distance)
			if (lodMultiplier < 1.0f && (rand() % 100) > (lodMultiplier * 100))
			{
				continue;
			}

			// Calculate fade factor (0 at age=0, 1 at age=lifetime)
			float fadeRatio = point.age / point.lifetime;

			// Make color fade to transparent
			std::array<float, 4> fadeColor = point.color;
			fadeColor[3] *= (1.0f - fadeRatio); // Fade alpha

			// Draw individual point with correct color and fade
			renderer.DrawTrailPoint(point.position, point.radius, fadeColor);
		}
	}
}

float World::CalculateLODMultiplier(const math::Vec2 &trailPosition) const
{
	// Calculate distance from camera to trail point
	math::Vec2 delta = trailPosition - cameraPos;
	float distanceSq = delta.x * delta.x + delta.y * delta.y;

	// LOD tiers based on distance and zoom level
	// Base distance scales with zoom to keep visibility consistent at all zoom levels

	// At 0.1x zoom (zoomed out): full detail up to 2000 units
	// At 1.0x zoom (normal):     full detail up to 4000 units
	// At 4.0x zoom (zoomed in):  full detail up to 16000 units

	// This ensures trails remain visible when zoomed out but still get culled when very far
	float baseLodDistance = 4000.0f * cameraZoom;
	float lodDistanceSq = baseLodDistance * baseLodDistance;

	if (distanceSq < lodDistanceSq)
	{
		return 1.0f; // Full detail (show all points)
	}
	else if (distanceSq < lodDistanceSq * 4.0f)
	{
		return 0.8f;
	}
	else if (distanceSq < lodDistanceSq * 16.0f)
	{
		return 0.65f;
	}
	else if (distanceSq < lodDistanceSq * 32.0f)
	{
		return 0.5f;
	}
	else
	{
		return 0.0f; // Cull entirely (very far away)
	}
}