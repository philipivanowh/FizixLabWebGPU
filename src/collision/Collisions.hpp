#pragma once

#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"
#include "math/Vec2.hpp"

#include <tuple>
#include <vector>

namespace collision {

struct HitInfo {
	bool result = false;
	math::Vec2 normal = math::Vec2();
	float depth = 0.0f;
};

struct ProjectionRange {
	float min = 0.0f;
	float max = 0.0f;
};

bool IntersectAABBs(const AABB& a, const AABB& b);

std::tuple<math::Vec2, math::Vec2, int> FindContactPoints(const physics::Rigidbody& objectA, const physics::Rigidbody& objectB);

HitInfo Collide(const physics::Rigidbody& objectA, const physics::Rigidbody& objectB);

HitInfo IntersectCirclePolygon(const math::Vec2& circleCenter,
					   float circleRadius,
					   const math::Vec2& polygonCenter,
					   const std::vector<math::Vec2>& vertices);

HitInfo IntersectCirclePolygonVerticesOnly(const math::Vec2& circleCenter,
							   float circleRadius,
							   const std::vector<math::Vec2>& vertices);

HitInfo IntersectPolygons(const math::Vec2& centerA,
				 const std::vector<math::Vec2>& verticesA,
				 const math::Vec2& centerB,
				 const std::vector<math::Vec2>& verticesB);

HitInfo IntersectPolygonsVerticesOnly(const std::vector<math::Vec2>& verticesA,
						     const std::vector<math::Vec2>& verticesB);

HitInfo IntersectCircles(const math::Vec2& centerA,
			     float radiusA,
			     const math::Vec2& centerB,
			     float radiusB);

} // namespace physics
