#include "shape/Box.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shape
{

	Box::Box(const math::Vec2 &pos,
			 const math::Vec2 &vel,
			 const math::Vec2 &acc,
			 float widthValue,
			 float heightValue,
			 const std::array<float, 4> &colorValue,
			 float massValue,
			 float restitution,
			 physics::RigidbodyType RigidbodyTypeValue)
		: Shape(pos, vel, acc, massValue, restitution, RigidbodyTypeValue), width(widthValue), height(heightValue), color(colorValue)
	{
		verticesSize = 6;
		UpdateMassProperties();

		float halfWidth = width / 2.0f;
		float halfHeight = height / 2.0f;
		vertices = {
			math::Vec2(-halfWidth, -halfHeight),
			math::Vec2(halfWidth, -halfHeight),
			math::Vec2(halfWidth, halfHeight),
			math::Vec2(-halfWidth, halfHeight)};
	}

	std::vector<float> Box::GetVertexLocalPos() const
	{
		std::vector<math::Vec2> rotated = GetRotatedVertices();
		const math::Vec2 &v0 = rotated[0];
		const math::Vec2 &v1 = rotated[1];
		const math::Vec2 &v2 = rotated[2];
		const math::Vec2 &v3 = rotated[3];

		return {
			v0.x, v0.y,
			v1.x, v1.y,
			v2.x, v2.y,
			v0.x, v0.y,
			v2.x, v2.y,
			v3.x, v3.y};
	}

	std::vector<math::Vec2> Box::GetVertexWorldPos() const
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

	float Box::ComputeInertia() const
	{
		if (mass <= 0.0f)
		{
			return 0.0f;
		}
		return (mass * (width * width + height * height)) / 12.0f;
	}

	std::vector<math::Vec2> Box::GetRotatedVertices() const
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
