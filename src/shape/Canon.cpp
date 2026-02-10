#include "shape/Canon.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"

#include <cmath>

namespace
{
	std::array<float, 4> DarkenColor(const std::array<float, 4> &color, float scale)
	{
		std::array<float, 4> out = color;
		out[0] *= scale;
		out[1] *= scale;
		out[2] *= scale;
		return out;
	}

	std::vector<math::Vec2> BuildWheelVertices(float radius, int steps)
	{
		std::vector<math::Vec2> wheel;
		wheel.reserve(static_cast<size_t>(steps) * 3);

		const float stepAngle = (math::PI * 2.0f) / static_cast<float>(steps);
		float prevX = radius * std::cos(0.0f);
		float prevY = radius * std::sin(0.0f);
		for (int i = 1; i <= steps; i++)
		{
			float theta = stepAngle * static_cast<float>(i);
			float newX = radius * std::cos(theta);
			float newY = radius * std::sin(theta);

			wheel.emplace_back(0.0f, 0.0f);
			wheel.emplace_back(prevX, prevY);
			wheel.emplace_back(newX, newY);

			prevX = newX;
			prevY = newY;
		}

		return wheel;
	}

	std::vector<math::Vec2> BuildBarrelVertices(float length,
												float width,
												float angleRadians)
	{
		std::vector<math::Vec2> barrel;
		barrel.reserve(6);

		const float halfThickness = width / 2.0f;
		const math::Vec2 b0(0.0f, -halfThickness);
		const math::Vec2 b1(length, -halfThickness);
		const math::Vec2 b2(length, halfThickness);
		const math::Vec2 b3(0.0f, halfThickness);

		const float cosValue = std::cos(angleRadians);
		const float sinValue = std::sin(angleRadians);
		auto rotate = [&](const math::Vec2 &v) -> math::Vec2
		{
			return math::Vec2(
				v.x * cosValue - v.y * sinValue,
				v.x * sinValue + v.y * cosValue);
		};

		const math::Vec2 r0 = rotate(b0);
		const math::Vec2 r1 = rotate(b1);
		const math::Vec2 r2 = rotate(b2);
		const math::Vec2 r3 = rotate(b3);

		barrel.push_back(r0);
		barrel.push_back(r1);
		barrel.push_back(r2);
		barrel.push_back(r0);
		barrel.push_back(r2);
		barrel.push_back(r3);

		return barrel;
	}
}

namespace shape
{

	Canon::Canon(const math::Vec2 &pos,
				 float angle,
				 const std::array<float, 4> &colorValue)
		: Shape(pos, math::Vec2(), math::Vec2(), 1.0f, 0.0f, physics::RigidbodyType::Static),
		  barrelAngleDegrees(angle),
		  color(colorValue)
	{
		barrelColor = colorValue;
		wheelColor = DarkenColor(colorValue, 0.6f);
		GenerateVertices();

		verticesSize = static_cast<int>(vertices.size());
		UpdateMassProperties();
	}

	void Canon::GenerateVertices() const
	{
		vertices.clear();
		
		const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
		const std::vector<math::Vec2> barrel = BuildBarrelVertices(barrelLength, barrelWidth, angleRad);
		const std::vector<math::Vec2> wheel = BuildWheelVertices(wheelRadius, steps);

		vertices.reserve(wheel.size() + barrel.size());
		vertices.insert(vertices.end(), wheel.begin(), wheel.end());
		vertices.insert(vertices.end(), barrel.begin(), barrel.end());
	}

	void Canon::GenerateWheelVertices() const
	{
		const std::vector<math::Vec2> wheel = BuildWheelVertices(wheelRadius, steps);
		vertices.insert(vertices.end(), wheel.begin(), wheel.end());
	}

	void Canon::GenerateBarrelVerticies() const
	{
		const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
		const std::vector<math::Vec2> barrel = BuildBarrelVertices(barrelLength, barrelWidth, angleRad);
		vertices.insert(vertices.end(), barrel.begin(), barrel.end());
	}

	float Canon::ComputeInertia() const
	{
		return 0.0f;
	}


	std::vector<float> Canon::GetVertexLocalPos() const
	{
		// Regenerate vertices each frame to update the barrel angle
		GenerateVertices();

		std::vector<float> out;
		out.reserve(vertices.size() * 2);
		for (const auto &v : vertices)
		{
			out.push_back(v.x);
			out.push_back(v.y);
		}
		return out;
	}

	std::vector<float> Canon::GetWheelVertexLocalPos() const
	{
		const std::vector<math::Vec2> wheel = BuildWheelVertices(wheelRadius, steps);
		std::vector<float> out;
		out.reserve(wheel.size() * 2);
		for (const auto &v : wheel)
		{
			out.push_back(v.x);
			out.push_back(v.y);
		}
		return out;
	}

	std::vector<float> Canon::GetBarrelVertexLocalPos() const
	{
		const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
		const std::vector<math::Vec2> barrel = BuildBarrelVertices(barrelLength, barrelWidth, angleRad);
		std::vector<float> out;
		out.reserve(barrel.size() * 2);
		for (const auto &v : barrel)
		{
			out.push_back(v.x);
			out.push_back(v.y);
		}
		return out;
	}

	std::vector<math::Vec2> Canon::GetVertexWorldPos() const
	{
		std::vector<math::Vec2> out;
		out.reserve(vertices.size());
		for (const auto &v : vertices)
		{
			out.emplace_back(v.x + pos.x, v.y + pos.y);
		}
		return out;
	}

} // namespace shape
