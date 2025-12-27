#include "core/World.hpp"

#include "collision/Collisions.hpp"
#include "core/Renderer.hpp"
#include "common/settings.hpp"

#include <algorithm>
#include <cmath>

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

void World::Update(float deltaMs, int iterations)
{
	iterations = ClampIterations(iterations);
	contactPointsList.clear();

	for (int currIteration = 0; currIteration < iterations; currIteration++)
	{
		contactPairs.clear();
		for (const auto &object: objects)
		{
			object->Update(deltaMs, iterations);
		}
		BroadPhaseDetection();
		NarrowPhaseDetection();
	}

	RemoveObjects();
}

void World::BroadPhaseDetection()
{
	for (size_t i = 0; i + 1 < objects.size(); i++)
	{
		physics::Rigidbody&objectA = *objects[i];
		collision::AABB objectA_aabb = objectA.GetAABB();

		for (size_t j = i + 1; j < objects.size(); j++)
		{
			physics::Rigidbody&objectB = *objects[j];
			collision::AABB objectB_aabb = objectB.GetAABB();

			if (objectA.bodyType ==physics::RigidbodyType::Static &&
				objectB.bodyType ==physics::RigidbodyType::Static)
			{
				continue;
			}

			if (!IntersectAABBs(objectA_aabb, objectB_aabb))
			{
				continue;
			}

			this->contactPairs.push_back(std::make_tuple(i, j));
		}
	}
}

void World::NarrowPhaseDetection()
{
	for (const auto &contact : contactPairs)
	{
		physics::Rigidbody&objectA = *objects[std::get<0>(contact)];
		physics::Rigidbody&objectB = *objects[std::get<1>(contact)];

		collision::HitInfo hit = collision::Collide(objectA, objectB);
		if (!hit.result)
		{
			continue;
		}

		SeperateBodies(objectA, objectB, hit.normal * hit.depth);
		auto [contact1, contact2, contactCount] = collision::FindContactPoints(objectA, objectB);
		collision::ContactManifold contactContactManifold(
			objectA,
			objectB,
			hit.normal,
			hit.depth,
			contact1,
			contact2,
			contactCount);

		ResolveCollisionWithRotationAndFriction(contactContactManifold);
	}
}

void World::Draw(Renderer &renderer) const
{
	for (const auto &object: objects)
	{
		renderer.DrawShape(*object);
	}
}

size_t World::RigidbodyCount() const
{
	return objects.size();
}

void World::SeperateBodies(physics::Rigidbody&RigidbodyA, physics::Rigidbody&RigidbodyB, const math::Vec2 &mtv)
{
	const float maxCorrectionDepth = 5.0f;
	float mtvLength = mtv.Length();
	math::Vec2 correction = mtv;
	if (mtvLength > maxCorrectionDepth)
	{
		correction = mtv * (maxCorrectionDepth / mtvLength);
	}

	if (RigidbodyA.bodyType ==physics::RigidbodyType::Static)
	{
		RigidbodyB.Translate(correction);
	}
	else if (RigidbodyB.bodyType ==physics::RigidbodyType::Static)
	{
		RigidbodyA.Translate(correction.Negate());
	}
	else
	{
		RigidbodyA.Translate(correction.Negate() / 2);
		RigidbodyB.Translate(correction / 2);
	}
}

void World::ResolveCollisionBasic(const collision::ContactManifold &contact)
{
	physics::Rigidbody&RigidbodyA = *const_cast<physics::Rigidbody*>(contact.RigidbodyA);
	physics::Rigidbody&RigidbodyB = *const_cast<physics::Rigidbody*>(contact.RigidbodyB);
	const math::Vec2 &normal = contact.normal;

	math::Vec2 relativeVelocity = RigidbodyB.linearVel.Subtract(RigidbodyA.linearVel);
	if (math::Vec2::Dot(relativeVelocity, normal) > 0.0f)
	{
		return;
	}

	float e = std::min(RigidbodyA.restitution, RigidbodyB.restitution);

	float j = -(1 + e) * math::Vec2::Dot(relativeVelocity, normal);
	j /= RigidbodyA.invMass + RigidbodyB.invMass;

	math::Vec2 impulse = normal * j;

	RigidbodyA.linearVel -= impulse * RigidbodyA.invMass;
	RigidbodyB.linearVel += impulse * RigidbodyB.invMass;
}

void World::ResolveCollisionWithRotation(const collision::ContactManifold &contact)
{
	physics::Rigidbody&RigidbodyA = *const_cast<physics::Rigidbody*>(contact.RigidbodyA);
	physics::Rigidbody&RigidbodyB = *const_cast<physics::Rigidbody*>(contact.RigidbodyB);
	const math::Vec2 &normal = contact.normal;
	math::Vec2 contact1 = contact.contact1;
	math::Vec2 contact2 = contact.contact2;
	int contactCount = contact.contactCount;

	float e = std::min(RigidbodyA.restitution, RigidbodyB.restitution);

	contactList[0] = contact1;
	contactList[1] = contact2;

	for (int i = 0; i < contactCount; i++)
	{
		impulseList[i] = math::Vec2();
		raList[i] = math::Vec2();
		rbList[i] = math::Vec2();
	}

	for (int i = 0; i < contactCount; i++)
	{

		math::Vec2 ra = contactList[i] - RigidbodyA.pos;
		math::Vec2 rb = contactList[i] - RigidbodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		math::Vec2 raPerp(-ra.y, ra.x);

		math::Vec2 rbPerp(-rb.y, rb.x);

		math::Vec2 angularTangentalVelA = raPerp * RigidbodyA.angularVel;
		math::Vec2 angularTangentalVelB = rbPerp * RigidbodyB.angularVel;

		math::Vec2 relativeVelocity = (RigidbodyB.linearVel + angularTangentalVelB) - (RigidbodyA.linearVel + angularTangentalVelA);

		float contactVelMag = math::Vec2::Dot(relativeVelocity, normal);

		if (contactVelMag > 0.0f)
			continue;

		float raPerpDotN = math::Vec2::Dot(raPerp, normal);
		float rbPerpDotN = math::Vec2::Dot(rbPerp, normal);

		float denom = RigidbodyA.invMass + RigidbodyB.invMass +
					  (raPerpDotN * raPerpDotN) * RigidbodyA.invInertia +
					  (rbPerpDotN * rbPerpDotN) * RigidbodyB.invInertia;
		float j = -(1 + e) * contactVelMag;
		j /= denom;
		j /= (float)contactCount;

		math::Vec2 impulse = normal * j;
		impulseList[i] = impulse;
	}

	for (int i = 0; i < contactCount; i++)
	{
		math::Vec2 impulse = impulseList[i];

		math::Vec2 ra = raList[i];
		math::Vec2 rb = rbList[i];

		RigidbodyA.linearVel += impulse * -RigidbodyA.invMass;

		RigidbodyA.angularVel += -RigidbodyA.invInertia * math::Vec2::Cross(ra, impulse);

		RigidbodyB.linearVel += impulse * RigidbodyB.invMass;

		RigidbodyB.angularVel += RigidbodyB.invInertia * math::Vec2::Cross(rb, impulse);
	}
}

void World::ResolveCollisionWithRotationAndFriction(const collision::ContactManifold &contact)
{
	physics::Rigidbody&RigidbodyA = *const_cast<physics::Rigidbody*>(contact.RigidbodyA);
	physics::Rigidbody&RigidbodyB = *const_cast<physics::Rigidbody*>(contact.RigidbodyB);
	const math::Vec2 &normal = contact.normal;
	math::Vec2 contact1 = contact.contact1;
	math::Vec2 contact2 = contact.contact2;
	int contactCount = contact.contactCount;

	float e = std::min(RigidbodyA.restitution, RigidbodyB.restitution);

	float staticF = (RigidbodyA.staticFriction + RigidbodyB.staticFriction) / 2.0f;
	float kineticF = (RigidbodyA.kineticFriction + RigidbodyB.kineticFriction) / 2.0f;

	contactList[0] = contact1;
	contactList[1] = contact2;

	for (int i = 0; i < contactCount; i++)
	{
		impulseList[i] = math::Vec2();
		raList[i] = math::Vec2();
		rbList[i] = math::Vec2();
		impulseFrictionList[i] = math::Vec2();
		jList[i] = 0;
	}

	for (int i = 0; i < contactCount; i++)
	{

		math::Vec2 ra = contactList[i] - RigidbodyA.pos;
		math::Vec2 rb = contactList[i] - RigidbodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		math::Vec2 raPerp(-ra.y, ra.x);

		math::Vec2 rbPerp(-rb.y, rb.x);

		math::Vec2 angularTangentalVelA = raPerp * RigidbodyA.angularVel;
		math::Vec2 angularTangentalVelB = rbPerp * RigidbodyB.angularVel;

		math::Vec2 relativeVelocity = (RigidbodyB.linearVel + angularTangentalVelB) - (RigidbodyA.linearVel + angularTangentalVelA);

		float contactVelMag = math::Vec2::Dot(relativeVelocity, normal);

		if (contactVelMag > 0.0f)
			continue;

		float raPerpDotN = math::Vec2::Dot(raPerp, normal);
		float rbPerpDotN = math::Vec2::Dot(rbPerp, normal);

		float denom = RigidbodyA.invMass + RigidbodyB.invMass +
					  (raPerpDotN * raPerpDotN) * RigidbodyA.invInertia +
					  (rbPerpDotN * rbPerpDotN) * RigidbodyB.invInertia;
		float j = -(1 + e) * contactVelMag;
		j /= denom;
		j /= (float)contactCount;

		jList[i] = j;

		math::Vec2 impulse = normal * j;
		impulseList[i] = impulse;
	}

	for (int i = 0; i < contactCount; i++)
	{
		math::Vec2 impulse = impulseList[i];

		math::Vec2 ra = raList[i];
		math::Vec2 rb = rbList[i];

		RigidbodyA.linearVel += impulse * -RigidbodyA.invMass;

		RigidbodyA.angularVel += -RigidbodyA.invInertia * math::Vec2::Cross(ra, impulse);

		RigidbodyB.linearVel += impulse * RigidbodyB.invMass;

		RigidbodyB.angularVel += RigidbodyB.invInertia * math::Vec2::Cross(rb, impulse);
	}

	for (int i = 0; i < contactCount; i++)
	{

		math::Vec2 ra = contactList[i] - RigidbodyA.pos;
		math::Vec2 rb = contactList[i] - RigidbodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		math::Vec2 raPerp(-ra.y, ra.x);

		math::Vec2 rbPerp(-rb.y, rb.x);

		math::Vec2 angularTangentalVelA = raPerp * RigidbodyA.angularVel;
		math::Vec2 angularTangentalVelB = rbPerp * RigidbodyB.angularVel;

		math::Vec2 relativeVelocity = (RigidbodyB.linearVel + angularTangentalVelB) - (RigidbodyA.linearVel + angularTangentalVelA);

		math::Vec2 tangent = relativeVelocity - (normal * math::Vec2::Dot(relativeVelocity, normal));

		if (math::NearlyEqualVec(tangent, math::Vec2(0.0f, 0.0f)))
		{
			continue;
		}
		else
		{
			tangent = tangent.Normalize();
		}

		float raPerpDotT = math::Vec2::Dot(raPerp, tangent);
		float rbPerpDotT = math::Vec2::Dot(rbPerp, tangent);

		float denom = RigidbodyA.invMass + RigidbodyB.invMass +
					  (raPerpDotT * raPerpDotT) * RigidbodyA.invInertia +
					  (rbPerpDotT * rbPerpDotT) * RigidbodyB.invInertia;
		float jt = -math::Vec2::Dot(relativeVelocity, tangent);
		jt /= denom;
		jt /= (float)contactCount;

		math::Vec2 impulseFriction = math::Vec2();
		float j = jList[i];
		if (std::abs(jt) <= j * staticF)
		{
			impulseFriction = tangent * jt;
		}
		else
		{
			impulseFriction = tangent * -(kineticF * j);
		}
		impulseFrictionList[i] = impulseFriction;
	}

	for (int i = 0; i < contactCount; i++)
	{
		math::Vec2 impulseFriction = impulseFrictionList[i];

		math::Vec2 ra = raList[i];
		math::Vec2 rb = rbList[i];

		RigidbodyA.linearVel += impulseFriction * -RigidbodyA.invMass;

		RigidbodyA.angularVel += -RigidbodyA.invInertia * math::Vec2::Cross(ra, impulseFriction);

		RigidbodyB.linearVel += impulseFriction * RigidbodyB.invMass;

		RigidbodyB.angularVel += RigidbodyB.invInertia * math::Vec2::Cross(rb, impulseFriction);
	}
}

void World::RemoveObjects()
{
	for (size_t i = 0; i < objects.size();)
	{
		collision::AABB box = objects[i]->GetAABB();
		if (box.max.y < viewBottom)
		{
			objects.erase(objects.begin() + static_cast<long>(i));
		}
		else
		{
			++i;
		}
	}
}

bool World::ContainsContactPoint(const std::vector<math::Vec2> &list,
								 const math::Vec2 &candidate,
								 float epsilon)
{
	for (const auto &point : list)
	{
		if (std::fabs(point.x - candidate.x) <= epsilon &&
			std::fabs(point.y - candidate.y) <= epsilon)
		{
			return true;
		}
	}
	return false;
}
