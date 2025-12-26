#include "core/Scene.hpp"

#include "physics/Collisions.hpp"
#include "physics/PhysicsConstants.hpp"
#include "core/Renderer.hpp"
#include "physics/Rigidbody.hpp"

#include <algorithm>
#include <cmath>

namespace physics
{

	int Scene::ClampIterations(int value)
	{
		if (value < MIN_ITERATIONS)
		{
			return MIN_ITERATIONS;
		}
		if (value > MAX_ITERATIONS)
		{
			return MAX_ITERATIONS;
		}
		return value;
	}

	void Scene::Add(std::unique_ptr<Body> body)
	{
		objects.push_back(std::move(body));
	}

	void Scene::Update(float deltaMs, int iterations)
	{
		iterations = ClampIterations(iterations);
		contactPointsList.clear();

		for (int currIteration = 0; currIteration < iterations; currIteration++)
		{
			contactPairs.clear();
			for (const auto &body : objects)
			{
				body->Update(deltaMs, iterations);
			}
			BroadPhaseDetection();
			NarrowPhaseDetection();
		}

		RemoveObjects();
	}

	void Scene::BroadPhaseDetection()
	{
		for (size_t i = 0; i + 1 < objects.size(); i++)
		{
			Body &objectA = *objects[i];
			AABB objectA_aabb = objectA.GetAABB();

			for (size_t j = i + 1; j < objects.size(); j++)
			{
				Body &objectB = *objects[j];
				AABB objectB_aabb = objectB.GetAABB();

				if (objectA.bodyType == BodyType::Static &&
					objectB.bodyType == BodyType::Static)
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

	void Scene::NarrowPhaseDetection()
	{
		for (const auto &contact : contactPairs)
		{
			Body &objectA = *objects[std::get<0>(contact)];
			Body &objectB = *objects[std::get<1>(contact)];

			HitInfo hit = Collide(objectA, objectB);
			if (!hit.result)
			{
				continue;
			}

			SeperateBodies(objectA, objectB, hit.normal * hit.depth);
			auto [contact1, contact2, contactCount] = FindContactPoints(objectA, objectB);
			Manifold contactManifold(
				objectA,
				objectB,
				hit.normal,
				hit.depth,
				contact1,
				contact2,
				contactCount);

			ResolveCollisionWithRotationAndFriction(contactManifold);
		}
	}

	void Scene::Draw(Renderer &renderer) const
	{
		for (const auto &body : objects)
		{
			renderer.DrawShape(*body);
		}
	}

	size_t Scene::BodyCount() const
	{
		return objects.size();
	}

	void Scene::SeperateBodies(Body &bodyA, Body &bodyB, const math::Vec2 &mtv)
	{
		const float maxCorrectionDepth = 5.0f;
		float mtvLength = mtv.Length();
		math::Vec2 correction = mtv;
		if (mtvLength > maxCorrectionDepth)
		{
			correction = mtv * (maxCorrectionDepth / mtvLength);
		}

		if (bodyA.bodyType == BodyType::Static)
		{
			bodyB.Translate(correction);
		}
		else if (bodyB.bodyType == BodyType::Static)
		{
			bodyA.Translate(correction.Negate());
		}
		else
		{
			bodyA.Translate(correction.Negate() / 2);
			bodyB.Translate(correction / 2);
		}
	}

void Scene::ResolveCollisionBasic(const Manifold &contact)
{
	Body &bodyA = *const_cast<Body *>(contact.bodyA);
	Body &bodyB = *const_cast<Body *>(contact.bodyB);
	const math::Vec2 &normal = contact.normal;

	math::Vec2 relativeVelocity = bodyB.linearVel.Subtract(bodyA.linearVel);
	if (math::Vec2::Dot(relativeVelocity, normal) > 0.0f)
	{
		return;
	}

	float e = std::min(bodyA.restitution, bodyB.restitution);

	float j = -(1 + e) * math::Vec2::Dot(relativeVelocity, normal);
	j /= bodyA.invMass + bodyB.invMass;

	math::Vec2 impulse = normal * j;

	bodyA.linearVel -= impulse * bodyA.invMass;
	bodyB.linearVel += impulse * bodyB.invMass;
}

void Scene::ResolveCollisionWithRotation(const Manifold &contact)
{
	Body &bodyA = *const_cast<Body *>(contact.bodyA);
	Body &bodyB = *const_cast<Body *>(contact.bodyB);
	const math::Vec2 &normal = contact.normal;
	math::Vec2 contact1 = contact.contact1;
	math::Vec2 contact2 = contact.contact2;
	int contactCount = contact.contactCount;

	float e = std::min(bodyA.restitution, bodyB.restitution);

	contactList[0] = contact1;
	contactList[1] = contact2;

	for(int i = 0; i < contactCount; i++)
	{
		impulseList[i] = math::Vec2();
		raList[i] = math::Vec2();
		rbList[i] = math::Vec2();
	}

	for (int i = 0; i < contactCount; i++)
	{

		math::Vec2 ra = contactList[i] - bodyA.pos;
		math::Vec2 rb = contactList[i] - bodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		math::Vec2 raPerp(-ra.y, ra.x);

		math::Vec2 rbPerp(-rb.y, rb.x);

		math::Vec2 angularTangentalVelA = raPerp * bodyA.angularVel;
		math::Vec2 angularTangentalVelB = rbPerp * bodyB.angularVel;

		math::Vec2 relativeVelocity = (bodyB.linearVel + angularTangentalVelB) - (bodyA.linearVel + angularTangentalVelA);

		float contactVelMag = math::Vec2::Dot(relativeVelocity, normal);

		if (contactVelMag > 0.0f)
			continue;

		float raPerpDotN = math::Vec2::Dot(raPerp, normal);
		float rbPerpDotN = math::Vec2::Dot(rbPerp, normal);

		float denom = bodyA.invMass + bodyB.invMass +
					  (raPerpDotN * raPerpDotN) * bodyA.invInertia +
					  (rbPerpDotN * rbPerpDotN) * bodyB.invInertia;
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

		bodyA.linearVel += impulse * -bodyA.invMass;

		bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulse);

		bodyB.linearVel += impulse * bodyB.invMass;

		bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulse);
	}
}

void Scene::ResolveCollisionWithRotationAndFriction(const Manifold &contact)
{
	Body &bodyA = *const_cast<Body *>(contact.bodyA);
	Body &bodyB = *const_cast<Body *>(contact.bodyB);
	const math::Vec2 &normal = contact.normal;
	math::Vec2 contact1 = contact.contact1;
	math::Vec2 contact2 = contact.contact2;
	int contactCount = contact.contactCount;

	float e = std::min(bodyA.restitution, bodyB.restitution);

	float staticF = (bodyA.staticFriction + bodyB.staticFriction) / 2.0f;
	float kineticF = (bodyA.kineticFriction + bodyB.kineticFriction) / 2.0f;

	contactList[0] = contact1;
	contactList[1] = contact2;

	for(int i = 0; i < contactCount; i++)
	{
		impulseList[i] = math::Vec2();
		raList[i] = math::Vec2();
		rbList[i] = math::Vec2();
		impulseFrictionList[i] = math::Vec2();
		jList[i] = 0;
	}

	for (int i = 0; i < contactCount; i++)
	{

		math::Vec2 ra = contactList[i] - bodyA.pos;
		math::Vec2 rb = contactList[i] - bodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		math::Vec2 raPerp(-ra.y, ra.x);

		math::Vec2 rbPerp(-rb.y, rb.x);

		math::Vec2 angularTangentalVelA = raPerp * bodyA.angularVel;
		math::Vec2 angularTangentalVelB = rbPerp * bodyB.angularVel;

		math::Vec2 relativeVelocity = (bodyB.linearVel + angularTangentalVelB) - (bodyA.linearVel + angularTangentalVelA);

		float contactVelMag = math::Vec2::Dot(relativeVelocity, normal);

		if (contactVelMag > 0.0f)
			continue;

		float raPerpDotN = math::Vec2::Dot(raPerp, normal);
		float rbPerpDotN = math::Vec2::Dot(rbPerp, normal);

		float denom = bodyA.invMass + bodyB.invMass +
					  (raPerpDotN * raPerpDotN) * bodyA.invInertia +
					  (rbPerpDotN * rbPerpDotN) * bodyB.invInertia;
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

		bodyA.linearVel += impulse * -bodyA.invMass;

		bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulse);

		bodyB.linearVel += impulse * bodyB.invMass;

		bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulse);
	}

	for (int i = 0; i < contactCount; i++)
	{

		math::Vec2 ra = contactList[i] - bodyA.pos;
		math::Vec2 rb = contactList[i] - bodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		math::Vec2 raPerp(-ra.y, ra.x);

		math::Vec2 rbPerp(-rb.y, rb.x);

		math::Vec2 angularTangentalVelA = raPerp * bodyA.angularVel;
		math::Vec2 angularTangentalVelB = rbPerp * bodyB.angularVel;

		math::Vec2 relativeVelocity = (bodyB.linearVel + angularTangentalVelB) - (bodyA.linearVel + angularTangentalVelA);

		math::Vec2 tangent = relativeVelocity - (normal * math::Vec2::Dot(relativeVelocity, normal));

		if(math::Math::NearlyEqualVec(tangent, math::Vec2(0.0f,0.0f)))
		{
			continue;
		}else{
			tangent = tangent.Normalize();
		}

		float raPerpDotT = math::Vec2::Dot(raPerp, tangent);
		float rbPerpDotT = math::Vec2::Dot(rbPerp, tangent);

		float denom = bodyA.invMass + bodyB.invMass +
					  (raPerpDotT * raPerpDotT) * bodyA.invInertia +
					  (rbPerpDotT * rbPerpDotT) * bodyB.invInertia;
		float jt = -math::Vec2::Dot(relativeVelocity, tangent);
		jt /= denom;
		jt /= (float)contactCount;

		math::Vec2 impulseFriction = math::Vec2();
		float j = jList[i];
		if(std::abs(jt) <= j * staticF){
			impulseFriction = tangent * jt;
		}else{
			impulseFriction = tangent * -(kineticF * j);
		}
		impulseFrictionList[i] = impulseFriction;
	}

	for (int i = 0; i < contactCount; i++)
	{
		math::Vec2 impulseFriction = impulseFrictionList[i];

		math::Vec2 ra = raList[i];
		math::Vec2 rb = rbList[i];

		bodyA.linearVel += impulseFriction * -bodyA.invMass;

		bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulseFriction);

		bodyB.linearVel += impulseFriction * bodyB.invMass;

		bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulseFriction);
	} 
}



	void Scene::RemoveObjects()
	{
		for (size_t i = 0; i < objects.size();)
		{
			AABB box = objects[i]->GetAABB();
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

	bool Scene::ContainsContactPoint(const std::vector<math::Vec2> &list,
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
}
