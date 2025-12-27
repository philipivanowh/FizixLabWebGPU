#pragma once

#include "physics/Rigidbody.hpp"
#include "math/Vec2.hpp"

namespace collision {

struct ContactManifold {
	const physics::Rigidbody* RigidbodyA = nullptr;
	const physics::Rigidbody* RigidbodyB = nullptr;
	float depth = 0.0f;
	math::Vec2 normal = math::Vec2();
	math::Vec2 contact1 = math::Vec2();
	math::Vec2 contact2 = math::Vec2();
	int contactCount = 0;

	ContactManifold() = default;
	ContactManifold(const physics::Rigidbody& a,
		 const physics::Rigidbody& b,
		 const math::Vec2& normalValue,
		 float depthValue,
		 const math::Vec2& contactPoint1,
		 const math::Vec2& contactPoint2,
		 int contactCountValue)
		: RigidbodyA(&a)
		, RigidbodyB(&b)
		, depth(depthValue)
		, normal(normalValue)
		, contact1(contactPoint1)
		, contact2(contactPoint2)
		, contactCount(contactCountValue) {}
};

}
