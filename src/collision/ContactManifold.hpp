#pragma once

#include "physics/Rigidbody.hpp"
#include "math/Vec2.hpp"

namespace collision {

struct ContactManifold {
	const physics::Rigidbody* bodyA = nullptr;
	const physics::Rigidbody* bodyB = nullptr;
	float depth = 0.0f;
	math::Vec2 normal = math::Vec2();
	math::Vec2 contact1 = math::Vec2();
	math::Vec2 contact2 = math::Vec2();
	int contactCount = 0;

	ContactManifold() = default;
	ContactManifold(const physics::Rigidbody& bodyA,
		 const physics::Rigidbody& bodyB,
		 const math::Vec2& normalValue,
		 float depthValue,
		 const math::Vec2& contactPoint1,
		 const math::Vec2& contactPoint2,
		 int contactCountValue)
		: bodyA(&bodyA)
		, bodyB(&bodyB)
		, depth(depthValue)
		, normal(normalValue)
		, contact1(contactPoint1)
		, contact2(contactPoint2)
		, contactCount(contactCountValue) {}
};

}
