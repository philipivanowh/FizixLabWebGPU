#pragma once

#include "math/Vec2.hpp"

namespace collision {

struct AABB {
	math::Vec2 min;
	math::Vec2 max;

	constexpr AABB() = default;
	constexpr AABB(float minX, float minY, float maxX, float maxY)
		: min(minX, minY), max(maxX, maxY) {}
};

} // namespace physics
