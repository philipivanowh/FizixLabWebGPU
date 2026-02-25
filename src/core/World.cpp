#include "core/World.hpp"

#include "core/Renderer.hpp"

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
							   obj->angularVel});
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
	}
}

int World::ClampIterations(int value)
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

void World::Add(std::unique_ptr<physics::Rigidbody> body)
{
	objects.push_back(std::move(body));
}
physics::Rigidbody *World::PickBody(const math::Vec2 &p)
{
	for (auto &obj : objects)
	{
		// Transform the pick point into the body's local space by
		// inverse-rotating it around the body's origin. This lets us
		// test against the local (unrotated) AABB accurately even when
		// the body is at an angle.
		const float angle = -obj->rotation; // inverse rotation
		const float cosA = std::cos(angle);
		const float sinA = std::sin(angle);

		const float dx = p.x - obj->pos.x;
		const float dy = p.y - obj->pos.y;

		const math::Vec2 localP{
			cosA * dx - sinA * dy + obj->pos.x,
			sinA * dx + cosA * dy + obj->pos.y};

		collision::AABB aabb;
		if (const auto *shape = dynamic_cast<const shape::Shape *>(obj.get()))
		{
			aabb = shape->GetLocalAABB();
		}
		else
		{
			aabb = obj->GetAABB();
		}

		if (localP.x >= aabb.min.x && localP.x <= aabb.max.x &&
			localP.y >= aabb.min.y && localP.y <= aabb.max.y)
		{
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
        if (obj->bodyType != physics::RigidbodyType::Dynamic)
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

	if(nearestBody)
		std::cout << nearestBody->pos.x << " " << nearestBody->pos.y << std::endl;

    // Return snapped position or original position if no snap
    return nearestBody ? nearestBody->pos : position;
}

void World::Update(float deltaMs, int iterations, Settings &settings, physics::Rigidbody *selectedBody, physics::Rigidbody *draggedBody)
{
	iterations = ClampIterations(iterations);

	for (const auto &object : objects)
	{
		object->BeginFrameForces();
	}

	for (int currIteration = 0; currIteration < iterations; currIteration++)
	{
		for (const auto &object : objects)
		{
			object->Update(deltaMs, iterations);
		}
		collisionPipeline.BuildPairs(objects);
		for (const auto &pair : collisionPipeline.GetPairs())
		{
			physics::Rigidbody &bodyA = *objects[pair.first];
			physics::Rigidbody &bodyB = *objects[pair.second];
			collisionSolver.ResolveIfColliding(bodyA, bodyB);
		}
	}

	for (const auto &object : objects)
	{
		object->FinalizeForces(deltaMs);
	}

	if (!settings.recording)
		RemoveObjects(selectedBody,draggedBody);
}

void World::Draw(Renderer &renderer) const
{
	for (const auto &object : objects)
	{
		renderer.DrawShape(*object, object->isHighlighted);
	}
}

size_t World::RigidbodyCount() const
{
	return objects.size();
}

void World::RemoveObjects(physics::Rigidbody *selectedBody, physics::Rigidbody *draggedBody)
{

	for (size_t i = 0; i < objects.size();)
	{
		collision::AABB box = objects[i]->GetAABB();
		if (box.max.y < viewBottom)
		{
			if(selectedBody == objects[i].get())
			{
				selectedBody = nullptr;
			}
			if(draggedBody == objects[i].get())
			{
				draggedBody = nullptr;
			}
			objects.erase(objects.begin() + static_cast<long>(i));
		}
		else
		{
			++i;
		}
	}
}

void World::ClearObjects()
{
	objects.clear();
}
