#include "collision/Collisions.hpp"

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Incline.hpp"
#include "shape/Trigger.hpp"
#include "math/Math.hpp"

#include <cmath>
#include <limits>

namespace collision
{

	std::pair<float, math::Vec2> PointSegmentDistance(const math::Vec2 &p,
													  const math::Vec2 &a,
													  const math::Vec2 &b)
	{
		math::Vec2 ab = b - a;
		math::Vec2 ap = p - a;

		float abLenSq = ab.LengthSquared();
		float proj = (abLenSq == 0.0f) ? 0.0f : math::Vec2::Dot(ap, ab);
		float d = (abLenSq == 0.0f) ? 0.0f : proj / abLenSq;
		math::Vec2 cp = math::Vec2();

		if (d <= 0.0f)
		{
			cp = a;
		}
		else if (d >= 1.0f)
		{
			cp = b;
		}
		else
		{
			cp = a + (ab * d);
		}

		float dx = p.x - cp.x;
		float dy = p.y - cp.y;
		float distanceSquared = dx * dx + dy * dy;

		return {distanceSquared, cp};
	}

	ProjectionRange ProjectVertices(const std::vector<math::Vec2> &vertices, const math::Vec2 &axis)
	{
		ProjectionRange range;
		range.min = std::numeric_limits<float>::infinity();
		range.max = -std::numeric_limits<float>::infinity();

		for (const auto &vertex : vertices)
		{
			float proj = math::Vec2::Dot(vertex, axis);
			if (proj < range.min)
				range.min = proj;
			if (proj > range.max)
				range.max = proj;
		}
		return range;
	}

	ProjectionRange ProjectCircle(const math::Vec2 &center, float radius, const math::Vec2 &axis)
	{
		math::Vec2 dirRad = axis * radius;

		math::Vec2 p1 = center + dirRad;
		math::Vec2 p2 = center - dirRad;

		float min = math::Vec2::Dot(p1, axis);
		float max = math::Vec2::Dot(p2, axis);
		if (min > max)
		{
			float t = min;
			min = max;
			max = t;
		}
		return {min, max};
	}

	int FindClosestPointOnPolygon(const math::Vec2 &circleCenter,
								  const std::vector<math::Vec2> &vertices)
	{
		int result = -1;
		float minDistance = std::numeric_limits<float>::infinity();

		for (int i = 0; i < static_cast<int>(vertices.size()); i++)
		{
			float d = math::Vec2::DistanceSquared(vertices[i], circleCenter);
			if (d < minDistance)
			{
				minDistance = d;
				result = i;
			}
		}
		return result;
	}

	math::Vec2 FindArithmeticMean(const std::vector<math::Vec2> &vertices)
	{
		float sumX = 0.0f;
		float sumY = 0.0f;
		for (const auto &vertex : vertices)
		{
			sumX += vertex.x;
			sumY += vertex.y;
		}
		return math::Vec2(sumX / static_cast<float>(vertices.size()),
						  sumY / static_cast<float>(vertices.size()));
	}

	std::tuple<math::Vec2, math::Vec2, int> FindContactPointsFromPolygons(
		const std::vector<math::Vec2> &verticesA,
		const std::vector<math::Vec2> &verticesB)
	{
		math::Vec2 contact1 = math::Vec2();
		math::Vec2 contact2 = math::Vec2();
		int contactCount = 0;
		float minDistSq = std::numeric_limits<float>::infinity();

		auto tryUpdateContacts = [&](const math::Vec2 &point,
									 const math::Vec2 &va,
									 const math::Vec2 &vb)
		{
			auto [distSq, cp] = PointSegmentDistance(point, va, vb);

			if (math::NearlyEqual(distSq, minDistSq))
			{
				if (!math::NearlyEqualVec(cp, contact1) && !math::NearlyEqualVec(cp, contact2))
				{
					contact2 = cp;
					contactCount = 2;
				}
			}
			else if (distSq < minDistSq)
			{
				minDistSq = distSq;
				contact1 = cp;
				contactCount = 1;
			}
		};

		for (size_t i = 0; i < verticesA.size(); i++)
		{
			const math::Vec2 &p = verticesA[i];
			for (size_t j = 0; j < verticesB.size(); j++)
			{
				const math::Vec2 &va = verticesB[j];
				const math::Vec2 &vb = verticesB[(j + 1) % verticesB.size()];
				tryUpdateContacts(p, va, vb);
			}
		}

		for (size_t i = 0; i < verticesB.size(); i++)
		{
			const math::Vec2 &p = verticesB[i];
			for (size_t j = 0; j < verticesA.size(); j++)
			{
				const math::Vec2 &va = verticesA[j];
				const math::Vec2 &vb = verticesA[(j + 1) % verticesA.size()];
				tryUpdateContacts(p, va, vb);
			}
		}

		return {contact1, contact2, contactCount};
	}

	math::Vec2 FindCirclePolygonContactPoint(const math::Vec2 &circleCenter,
											 float /* circleRadius */,
											 const std::vector<math::Vec2> &polygonVertices)
	{
		float minDistSq = std::numeric_limits<float>::infinity();
		math::Vec2 cp = math::Vec2();

		for (size_t i = 0; i < polygonVertices.size(); i++)
		{
			const math::Vec2 &va = polygonVertices[i];
			const math::Vec2 &vb = polygonVertices[(i + 1) % polygonVertices.size()];
			auto [distSq, contact] = PointSegmentDistance(circleCenter, va, vb);

			if (distSq < minDistSq)
			{
				minDistSq = distSq;
				cp = contact;
			}
		}
		return cp;
	}

	math::Vec2 FindCircleCircleContactPoint(const math::Vec2 &centerA,
											float radiusA,
											const math::Vec2 &centerB)
	{
		math::Vec2 ab = centerB - centerA;
		math::Vec2 dir = ab.Normalize();
		return centerA + (dir * radiusA);
	}

	bool IntersectAABBs(const collision::AABB &a, const collision::AABB &b)
	{
		if (a.max.x <= b.min.x || b.max.x <= a.min.x ||
			a.max.y <= b.min.y || b.max.y <= a.min.y)
		{
			return false;
		}

		return true;
	}

	std::tuple<math::Vec2, math::Vec2, int> FindContactPoints(const physics::Rigidbody &bodyA, const physics::Rigidbody &bodyB)
	{
		math::Vec2 contact1 = math::Vec2();
		math::Vec2 contact2 = math::Vec2();
		int contactCount = 0;

		const shape::Box *boxA = dynamic_cast<const shape::Box *>(&bodyA);
		const shape::Box *boxB = dynamic_cast<const shape::Box *>(&bodyB);
		const shape::Ball *ballA = dynamic_cast<const shape::Ball *>(&bodyA);
		const shape::Ball *ballB = dynamic_cast<const shape::Ball *>(&bodyB);
		const shape::Incline *inclineA = dynamic_cast<const shape::Incline *>(&bodyA);
		const shape::Incline *inclineB = dynamic_cast<const shape::Incline *>(&bodyB);

		if (boxA)
		{
			if (boxB)
			{
				return FindContactPointsFromPolygons(
					boxA->GetVertexWorldPos(),
					boxB->GetVertexWorldPos());
			}

			if (inclineB)
			{
				return FindContactPointsFromPolygons(
					boxA->GetVertexWorldPos(),
					inclineB->GetVertexWorldPos());
			}

			if (ballB)
			{
				contact1 = FindCirclePolygonContactPoint(
					ballB->pos,
					ballB->radius,
					boxA->GetVertexWorldPos());
				contactCount = 1;
			}
		}
		else if (ballA)
		{
			if (boxB)
			{
				contact1 = FindCirclePolygonContactPoint(
					ballA->pos,
					ballA->radius,
					boxB->GetVertexWorldPos());
				contactCount = 1;
			}

			if (inclineB)
			{
				contact1 = FindCirclePolygonContactPoint(
					ballA->pos,
					ballA->radius,
					inclineB->GetVertexWorldPos());
				contactCount = 1;
			}

			else if (ballB)
			{
				contact1 = FindCircleCircleContactPoint(
					ballA->pos,
					ballA->radius,
					ballB->pos);
				contactCount = 1;
			}
		}
		if (inclineA)
		{
			if (boxB)
			{
				return FindContactPointsFromPolygons(
					inclineA->GetVertexWorldPos(),
					boxB->GetVertexWorldPos());
			}

			if (inclineB)
			{
				return FindContactPointsFromPolygons(
					inclineA->GetVertexWorldPos(),
					inclineB->GetVertexWorldPos());
			}

			if (ballB)
			{
				contact1 = FindCirclePolygonContactPoint(
					ballB->pos,
					ballB->radius,
					inclineA->GetVertexWorldPos());
				contactCount = 1;
			}
		}
		

		return {contact1, contact2, contactCount};
	}

	HitInfo Collide(const physics::Rigidbody &bodyA, const physics::Rigidbody &bodyB)
	{
		HitInfo hit;

		const shape::Box *boxA = dynamic_cast<const shape::Box *>(&bodyA);
		const shape::Box *boxB = dynamic_cast<const shape::Box *>(&bodyB);
		const shape::Ball *ballA = dynamic_cast<const shape::Ball *>(&bodyA);
		const shape::Ball *ballB = dynamic_cast<const shape::Ball *>(&bodyB);
		const shape::Incline *inclineA = dynamic_cast<const shape::Incline *>(&bodyA);
		const shape::Incline *inclineB = dynamic_cast<const shape::Incline *>(&bodyB);
		const shape::Trigger *triggerA = dynamic_cast<const shape::Trigger *>(&bodyA);
		const shape::Trigger *triggerB = dynamic_cast<const shape::Trigger *>(&bodyB);

		if (boxA)
		{
			if (boxB)
			{
				const auto &vertsA = boxA->GetVertexWorldPos();
				const auto &vertsB = boxB->GetVertexWorldPos();

				hit = IntersectPolygons(boxA->pos, vertsA, boxB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}

			if (ballB)
			{
				const auto &vertsA = boxA->GetVertexWorldPos();
				hit = IntersectCirclePolygon(ballB->pos, ballB->radius, boxA->pos, vertsA);
				if (!hit.result)
				{
					return hit;
				}
				hit.normal = hit.normal.Negate();
				return hit;
			}

			if (inclineB)
			{
				const auto &vertsA = boxA->GetVertexWorldPos();
				const auto &vertsB = inclineB->GetVertexWorldPos();

				hit = IntersectPolygons(boxA->pos, vertsA, inclineB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}

			if (triggerB)
			{
				const auto &vertsA = boxA->GetVertexWorldPos();
				const auto &vertsB = triggerB->GetVertexWorldPos();

				hit = IntersectPolygons(boxA->pos, vertsA, triggerB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}
		}

		if (ballA)
		{
			if (boxB)
			{
				const auto &vertsB = boxB->GetVertexWorldPos();
				hit = IntersectCirclePolygon(ballA->pos, ballA->radius, boxB->pos, vertsB);
				return hit;
			}

			if (ballB)
			{
				hit = IntersectCircles(ballA->pos, ballA->radius, ballB->pos, ballB->radius);
				return hit;
			}

			if (inclineB)
			{
				const auto &vertsB = inclineB->GetVertexWorldPos();
				hit = IntersectCirclePolygon(ballA->pos, ballA->radius, inclineB->pos, vertsB);
				return hit;
			}

			if (triggerB)
			{
				const auto &vertsB = triggerB->GetVertexWorldPos();
				hit = IntersectCirclePolygon(ballA->pos, ballA->radius, triggerB->pos, vertsB);
				return hit;
			}
		}

		if (inclineA)
		{
			if (boxB)
			{
				const auto &vertsA = inclineA->GetVertexWorldPos();
				const auto &vertsB = boxB->GetVertexWorldPos();

				hit = IntersectPolygons(inclineA->pos, vertsA, boxB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}

			if (ballB)
			{
				const auto &vertsA = inclineA->GetVertexWorldPos();
				hit = IntersectCirclePolygon(ballB->pos, ballB->radius, inclineA->pos, vertsA);
				if (!hit.result)
				{
					return hit;
				}
				hit.normal = hit.normal.Negate();
				return hit;
			}

			if (inclineB)
			{
				const auto &vertsA = inclineA->GetVertexWorldPos();
				const auto &vertsB = inclineB->GetVertexWorldPos();

				hit = IntersectPolygons(inclineA->pos, vertsA, inclineB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}

			if (triggerB)
			{
				const auto &vertsA = inclineA->GetVertexWorldPos();
				const auto &vertsB = triggerB->GetVertexWorldPos();

				hit = IntersectPolygons(inclineA->pos, vertsA, triggerB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}
		}

		if (triggerA)
		{
			if (boxB)
			{
				const auto &vertsA = triggerA->GetVertexWorldPos();
				const auto &vertsB = boxB->GetVertexWorldPos();

				hit = IntersectPolygons(triggerA->pos, vertsA, boxB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}

			if (ballB)
			{
				const auto &vertsA = triggerA->GetVertexWorldPos();
				hit = IntersectCirclePolygon(ballB->pos, ballB->radius, triggerA->pos, vertsA);
				if (!hit.result)
				{
					return hit;
				}
				hit.normal = hit.normal.Negate();
				return hit;
			}

			if (inclineB)
			{
				const auto &vertsA = triggerA->GetVertexWorldPos();
				const auto &vertsB = inclineB->GetVertexWorldPos();

				hit = IntersectPolygons(triggerA->pos, vertsA, inclineB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}

			if (triggerB)
			{
				const auto &vertsA = triggerA->GetVertexWorldPos();
				const auto &vertsB = triggerB->GetVertexWorldPos();

				hit = IntersectPolygons(triggerA->pos, vertsA, triggerB->pos, vertsB);
				if (!hit.result)
				{
					return hit;
				}
				return hit;
			}
		}

		return hit;
	}

	HitInfo IntersectCirclePolygon(const math::Vec2 &circleCenter,
								   float circleRadius,
								   const math::Vec2 &polygonCenter,
								   const std::vector<math::Vec2> &vertices)
	{
		HitInfo hit;
		hit.result = true;
		hit.normal = math::Vec2();
		hit.depth = std::numeric_limits<float>::infinity();

		for (size_t i = 0; i < vertices.size(); i++)
		{
			const math::Vec2 &va = vertices[i];
			const math::Vec2 &vb = vertices[(i + 1) % vertices.size()];

			math::Vec2 edge = vb - va;
			math::Vec2 axis = math::Vec2(-edge.y, edge.x).Normalize();

			ProjectionRange projA = ProjectVertices(vertices, axis);
			ProjectionRange projB = ProjectCircle(circleCenter, circleRadius, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		int cpIndex = FindClosestPointOnPolygon(circleCenter, vertices);
		math::Vec2 cp = vertices[cpIndex];
		const math::Vec2 closestDir = cp - circleCenter;
		math::Vec2 axis = math::NearlyEqualVec(closestDir, math::Vec2(0.0f, 0.0f))
							  ? math::Vec2(1.0f, 0.0f)
							  : closestDir.Normalize();

		{
			ProjectionRange projA = ProjectVertices(vertices, axis);
			ProjectionRange projB = ProjectCircle(circleCenter, circleRadius, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		math::Vec2 direction = polygonCenter.Subtract(circleCenter);
		if (math::Vec2::Dot(direction, hit.normal) < 0.0f)
		{
			hit.normal = hit.normal.Negate();
		}

		return hit;
	}

	HitInfo IntersectCirclePolygonVerticesOnly(const math::Vec2 &circleCenter,
											   float circleRadius,
											   const std::vector<math::Vec2> &vertices)
	{
		HitInfo hit;
		hit.result = true;
		hit.normal = math::Vec2();
		hit.depth = std::numeric_limits<float>::infinity();

		for (size_t i = 0; i < vertices.size(); i++)
		{
			const math::Vec2 &va = vertices[i];
			const math::Vec2 &vb = vertices[(i + 1) % vertices.size()];

			math::Vec2 edge = vb - va;
			math::Vec2 axis = math::Vec2(-edge.y, edge.x).Normalize();

			ProjectionRange projA = ProjectVertices(vertices, axis);
			ProjectionRange projB = ProjectCircle(circleCenter, circleRadius, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		int cpIndex = FindClosestPointOnPolygon(circleCenter, vertices);
		math::Vec2 cp = vertices[cpIndex];
		const math::Vec2 closestDir = cp - circleCenter;
		math::Vec2 axis = math::NearlyEqualVec(closestDir, math::Vec2(0.0f, 0.0f))
							  ? math::Vec2(1.0f, 0.0f)
							  : closestDir.Normalize();

		{
			ProjectionRange projA = ProjectVertices(vertices, axis);
			ProjectionRange projB = ProjectCircle(circleCenter, circleRadius, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		math::Vec2 polygonCenter = FindArithmeticMean(vertices);
		math::Vec2 direction = polygonCenter - circleCenter;
		if (math::Vec2::Dot(direction, hit.normal) < 0.0f)
		{
			hit.normal = hit.normal.Negate();
		}

		return hit;
	}

	HitInfo IntersectPolygons(const math::Vec2 &centerA,
							  const std::vector<math::Vec2> &verticesA,
							  const math::Vec2 &centerB,
							  const std::vector<math::Vec2> &verticesB)
	{
		HitInfo hit;
		hit.result = true;
		hit.normal = math::Vec2();
		hit.depth = std::numeric_limits<float>::infinity();

		for (size_t i = 0; i < verticesA.size(); i++)
		{
			const math::Vec2 &va = verticesA[i];
			const math::Vec2 &vb = verticesA[(i + 1) % verticesA.size()];

			math::Vec2 edge = vb - va;
			math::Vec2 axis = math::Vec2(-edge.y, edge.x).Normalize();

			ProjectionRange projA = ProjectVertices(verticesA, axis);
			ProjectionRange projB = ProjectVertices(verticesB, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		for (size_t i = 0; i < verticesB.size(); i++)
		{
			const math::Vec2 &va = verticesB[i];
			const math::Vec2 &vb = verticesB[(i + 1) % verticesB.size()];

			math::Vec2 edge = vb - va;
			math::Vec2 axis = math::Vec2(-edge.y, edge.x).Normalize();

			ProjectionRange projA = ProjectVertices(verticesA, axis);
			ProjectionRange projB = ProjectVertices(verticesB, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		math::Vec2 direction = centerB - centerA;
		if (math::Vec2::Dot(direction, hit.normal) < 0.0f)
		{
			hit.normal = hit.normal.Negate();
		}

		return hit;
	}

	HitInfo IntersectPolygonsVerticesOnly(const std::vector<math::Vec2> &verticesA,
										  const std::vector<math::Vec2> &verticesB)
	{
		HitInfo hit;
		hit.result = true;
		hit.normal = math::Vec2();
		hit.depth = std::numeric_limits<float>::infinity();

		for (size_t i = 0; i < verticesA.size(); i++)
		{
			const math::Vec2 &va = verticesA[i];
			const math::Vec2 &vb = verticesA[(i + 1) % verticesA.size()];

			math::Vec2 edge = vb - va;
			math::Vec2 axis = math::Vec2(-edge.y, edge.x).Normalize();

			ProjectionRange projA = ProjectVertices(verticesA, axis);
			ProjectionRange projB = ProjectVertices(verticesB, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		for (size_t i = 0; i < verticesB.size(); i++)
		{
			const math::Vec2 &va = verticesB[i];
			const math::Vec2 &vb = verticesB[(i + 1) % verticesB.size()];

			math::Vec2 edge = vb - va;
			math::Vec2 axis = math::Vec2(-edge.y, edge.x).Normalize();

			ProjectionRange projA = ProjectVertices(verticesA, axis);
			ProjectionRange projB = ProjectVertices(verticesB, axis);

			if (projA.min >= projB.max || projB.min >= projA.max)
			{
				hit.result = false;
				return hit;
			}

			float axisDepth = std::min(projB.max - projA.min, projA.max - projB.min);
			if (axisDepth < hit.depth)
			{
				hit.depth = axisDepth;
				hit.normal = axis;
			}
		}

		math::Vec2 centerA = FindArithmeticMean(verticesA);
		math::Vec2 centerB = FindArithmeticMean(verticesB);
		math::Vec2 direction = centerB.Subtract(centerA);

		if (math::Vec2::Dot(direction, hit.normal) < 0.0f)
		{
			hit.normal = hit.normal.Negate();
		}

		return hit;
	}

	HitInfo IntersectCircles(const math::Vec2 &centerA,
							 float radiusA,
							 const math::Vec2 &centerB,
							 float radiusB)
	{
		const float radii = radiusA + radiusB;
		const float radiiSq = radii * radii;
		const float distanceSq = math::Vec2::DistanceSquared(centerA, centerB);

		if (distanceSq >= radiiSq)
		{
			return {};
		}

		HitInfo hit;
		hit.result = true;
		const math::Vec2 delta = centerB.Subtract(centerA);
		const float distance = std::sqrt(distanceSq);
		hit.normal = (distance > 0.0f) ? (delta / distance) : math::Vec2(1.0f, 0.0f);
		hit.depth = radii - distance;
		return hit;
	}

} // namespace physics
