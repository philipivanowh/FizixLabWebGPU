#include "collision/CollisionSolver.hpp"

#include "collision/Collisions.hpp"
#include "collision/ContactManifold.hpp"
#include "common/settings.hpp"
#include "math/Math.hpp"
#include "physics/Rigidbody.hpp"

#include <algorithm>
#include <cmath>

bool CollisionSolver::ResolveIfColliding(physics::Rigidbody& bodyA, physics::Rigidbody& bodyB)
{
	const collision::HitInfo hit = collision::Collide(bodyA, bodyB);
	if (!hit.result)
	{
		return false;
	}

	SeparateBodies(bodyA, bodyB, hit.normal * hit.depth);
	auto [contact1, contact2, contactCount] = collision::FindContactPoints(bodyA, bodyB);
	collision::ContactManifold contactManifold(
		bodyA,
		bodyB,
		hit.normal,
		hit.depth,
		contact1,
		contact2,
		contactCount);

	ResolveWithRotationAndFriction(contactManifold);
	return true;
}

void CollisionSolver::SeparateBodies(physics::Rigidbody& bodyA, physics::Rigidbody& bodyB, const math::Vec2& mtv)
{
	const float maxCorrectionDepth = PhysicsConstants::MAX_PENETRATION_CORRECTION;
	const float mtvLength = mtv.Length();
	math::Vec2 correction = mtv;
	if (mtvLength > maxCorrectionDepth)
	{
		correction = mtv * (maxCorrectionDepth / mtvLength);
	}

	if (bodyA.bodyType == physics::RigidbodyType::Static)
	{
		bodyB.Translate(correction);
	}
	else if (bodyB.bodyType == physics::RigidbodyType::Static)
	{
		bodyA.Translate(correction.Negate());
	}
	else
	{
		bodyA.Translate(correction.Negate() / 2);
		bodyB.Translate(correction / 2);
	}
}

void CollisionSolver::ResolveWithRotationAndFriction(const collision::ContactManifold& contact)
{
	physics::Rigidbody& bodyA = *const_cast<physics::Rigidbody*>(contact.bodyA);
	physics::Rigidbody& bodyB = *const_cast<physics::Rigidbody*>(contact.bodyB);
	const math::Vec2& normal = contact.normal;
	const math::Vec2 contact1 = contact.contact1;
	const math::Vec2 contact2 = contact.contact2;
	const int contactCount = contact.contactCount;

	const float restitution = std::min(bodyA.restitution, bodyB.restitution);
	const float staticFriction = (bodyA.staticFriction + bodyB.staticFriction) / 2.0f;
	const float kineticFriction = (bodyA.kineticFriction + bodyB.kineticFriction) / 2.0f;

	contactList[0] = contact1;
	contactList[1] = contact2;

	for (int i = 0; i < contactCount; i++)
	{
		impulseList[i] = math::Vec2();
		raList[i] = math::Vec2();
		rbList[i] = math::Vec2();
		impulseFrictionList[i] = math::Vec2();
		jList[i] = 0.0f;
	}

	for (int i = 0; i < contactCount; i++)
	{
		const math::Vec2 ra = contactList[i] - bodyA.pos;
		const math::Vec2 rb = contactList[i] - bodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		const math::Vec2 raPerp(-ra.y, ra.x);
		const math::Vec2 rbPerp(-rb.y, rb.x);

		const math::Vec2 angularTangentialVelA = raPerp * bodyA.angularVel;
		const math::Vec2 angularTangentialVelB = rbPerp * bodyB.angularVel;

		const math::Vec2 relativeVelocity =
			(bodyB.linearVel + angularTangentialVelB) - (bodyA.linearVel + angularTangentialVelA);

		const float contactVelMag = math::Vec2::Dot(relativeVelocity, normal);
		if (contactVelMag > 0.0f)
		{
			continue;
		}

		const float raPerpDotN = math::Vec2::Dot(raPerp, normal);
		const float rbPerpDotN = math::Vec2::Dot(rbPerp, normal);

		const float denom = bodyA.invMass + bodyB.invMass +
					  (raPerpDotN * raPerpDotN) * bodyA.invInertia +
					  (rbPerpDotN * rbPerpDotN) * bodyB.invInertia;
		float j = -(1 + restitution) * contactVelMag;
		j /= denom;
		j /= static_cast<float>(contactCount);

		jList[i] = j;
		impulseList[i] = normal * j;
	}

	for (int i = 0; i < contactCount; i++)
	{
		const math::Vec2 impulse = impulseList[i];
		const math::Vec2 ra = raList[i];
		const math::Vec2 rb = rbList[i];

		bodyA.linearVel += impulse * -bodyA.invMass;
		bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulse);
		bodyB.linearVel += impulse * bodyB.invMass;
		bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulse);

		if (bodyA.bodyType != physics::RigidbodyType::Static)
		{
			bodyA.AccumulateNormalImpulse(impulse.Negate());
		}
		if (bodyB.bodyType != physics::RigidbodyType::Static)
		{
			bodyB.AccumulateNormalImpulse(impulse);
		}
	}

	for (int i = 0; i < contactCount; i++)
	{
		const math::Vec2 ra = contactList[i] - bodyA.pos;
		const math::Vec2 rb = contactList[i] - bodyB.pos;

		raList[i] = ra;
		rbList[i] = rb;

		const math::Vec2 raPerp(-ra.y, ra.x);
		const math::Vec2 rbPerp(-rb.y, rb.x);

		const math::Vec2 angularTangentialVelA = raPerp * bodyA.angularVel;
		const math::Vec2 angularTangentialVelB = rbPerp * bodyB.angularVel;

		const math::Vec2 relativeVelocity =
			(bodyB.linearVel + angularTangentialVelB) - (bodyA.linearVel + angularTangentialVelA);

		math::Vec2 tangent = relativeVelocity - (normal * math::Vec2::Dot(relativeVelocity, normal));

		if (math::NearlyEqualVec(tangent, math::Vec2(0.0f, 0.0f)))
		{
			continue;
		}
		tangent = tangent.Normalize();

		const float raPerpDotT = math::Vec2::Dot(raPerp, tangent);
		const float rbPerpDotT = math::Vec2::Dot(rbPerp, tangent);

		const float denom = bodyA.invMass + bodyB.invMass +
					  (raPerpDotT * raPerpDotT) * bodyA.invInertia +
					  (rbPerpDotT * rbPerpDotT) * bodyB.invInertia;
		float jt = -math::Vec2::Dot(relativeVelocity, tangent);
		jt /= denom;
		jt /= static_cast<float>(contactCount);

		const float j = jList[i];
		if (std::abs(jt) <= j * staticFriction)
		{
			impulseFrictionList[i] = tangent * jt;
		}
		else
		{
			impulseFrictionList[i] = tangent * -(kineticFriction * j);
		}
	}

	for (int i = 0; i < contactCount; i++)
	{
		const math::Vec2 impulseFriction = impulseFrictionList[i];
		const math::Vec2 ra = raList[i];
		const math::Vec2 rb = rbList[i];

		bodyA.linearVel += impulseFriction * -bodyA.invMass;
		bodyA.angularVel += -bodyA.invInertia * math::Vec2::Cross(ra, impulseFriction);
		bodyB.linearVel += impulseFriction * bodyB.invMass;
		bodyB.angularVel += bodyB.invInertia * math::Vec2::Cross(rb, impulseFriction);

			if (bodyA.bodyType != physics::RigidbodyType::Static)
		{
			bodyA.AccumulateFrictionImpulse(impulseFriction.Negate());
		}
		if (bodyB.bodyType != physics::RigidbodyType::Static)
		{
			bodyB.AccumulateFrictionImpulse(impulseFriction);
		}
	}
}
