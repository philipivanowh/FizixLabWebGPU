#pragma once

#include "physics/Body.hpp"
#include "math/Vec2.hpp"

namespace physics {

struct Manifold {
	const Body* bodyA = nullptr;
	const Body* bodyB = nullptr;
	float depth = 0.0f;
	math::Vec2 normal = math::Vec2();
	math::Vec2 contact1 = math::Vec2();
	math::Vec2 contact2 = math::Vec2();
	int contactCount = 0;

	Manifold() = default;
	Manifold(const Body& a,
		 const Body& b,
		 const math::Vec2& normalValue,
		 float depthValue,
		 const math::Vec2& contactPoint1,
		 const math::Vec2& contactPoint2,
		 int contactCountValue)
		: bodyA(&a)
		, bodyB(&b)
		, depth(depthValue)
		, normal(normalValue)
		, contact1(contactPoint1)
		, contact2(contactPoint2)
		, contactCount(contactCountValue) {}
};

}
