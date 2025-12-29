#pragma once

#include "math/Vec2.hpp"

namespace physics {
class Rigidbody;
}

namespace collision {
struct ContactManifold;
struct HitInfo;
}

class CollisionSolver {
public:
	bool ResolveIfColliding(physics::Rigidbody& bodyA, physics::Rigidbody& bodyB);

private:
	void SeparateBodies(physics::Rigidbody& bodyA, physics::Rigidbody& bodyB, const math::Vec2& mtv);
	void ResolveWithRotationAndFriction(const collision::ContactManifold& contact);

	math::Vec2 contactList[2];
	math::Vec2 impulseList[2];
	math::Vec2 impulseFrictionList[2];
	math::Vec2 raList[2];
	math::Vec2 rbList[2];
	float jList[2];
};
