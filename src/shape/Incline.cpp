#include "shape/Incline.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"
#include "math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shape
{

	Incline::Incline(const math::Vec2 &pos,
					 const math::Vec2 &vel,
					 const math::Vec2 &acc,
					 float base,
					 float angle,
					 bool flip,
					 const std::array<float, 4> &colorValue,
					 float staticFrictionValue,
					 float kineticFrictionValue)
		: Shape(pos, vel, acc, 1, 0, physics::RigidbodyType::Static), base(base), height(std::tan(angle * math::PI / 180.0f) * base), angle(angle * math::PI / 180.0f), flip(flip), color(colorValue)
	{

		staticFriction = staticFrictionValue;
		kineticFriction = kineticFrictionValue;
		verticesSize = 3;
		UpdateMassProperties();

		float halfWidth = base / 2.0f;
		float halfHeight = height / 2.0f;

		if (flip)
		{
			vertices = {
				math::Vec2(-halfWidth, -halfHeight),
				math::Vec2(halfWidth, -halfHeight),
				math::Vec2(halfWidth, halfHeight),
			};
		}
		else
		{
			vertices = {
				math::Vec2(-halfWidth, -halfHeight),
				math::Vec2(halfWidth, -halfHeight),
				math::Vec2(-halfWidth, halfHeight),
			};
		}
	}

	std::vector<float> Incline::GetVertexLocalPos() const
	{
		std::vector<math::Vec2> rotated = GetRotatedVertices();
		const math::Vec2 &v0 = rotated[0];
		const math::Vec2 &v1 = rotated[1];
		const math::Vec2 &v2 = rotated[2];

		return {
			v0.x,
			v0.y,
			v1.x,
			v1.y,
			v2.x,
			v2.y,
		};
	}

	std::vector<math::Vec2> Incline::GetVertexWorldPos() const
	{
		std::vector<math::Vec2> out;
		std::vector<math::Vec2> rotated = GetRotatedVertices();
		out.reserve(rotated.size());
		for (const auto &v : rotated)
		{
			out.emplace_back(v.x + pos.x, v.y + pos.y);
		}
		transformUpdateRequired = false;
		return out;
	}

	float Incline::ComputeInertia() const
	{
		return 0.0f;
	}

	std::vector<math::Vec2> Incline::GetRotatedVertices() const
	{
		float cosValue = std::cos(rotation);
		float sinValue = std::sin(rotation);
		std::vector<math::Vec2> rotated;
		rotated.reserve(vertices.size());
		for (const auto &v : vertices)
		{
			rotated.emplace_back(
				v.x * cosValue - v.y * sinValue,
				v.x * sinValue + v.y * cosValue);
		}
		return rotated;
	}

} // namespace physics
