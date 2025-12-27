#include "shape/Ball.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"
#include <algorithm>
#include <cmath>

namespace shape
{

	Ball::Ball(const math::Vec2 &pos,
			   const math::Vec2 &vel,
			   const math::Vec2 &acc,
			   float radiusValue,
			   const std::array<float, 4> &colorValue,
			   float massValue,
			   float restitution,
			   physics::RigidbodyType RigidbodyType)
		: Shape(pos, vel, acc, massValue, restitution, RigidbodyType), radius(radiusValue), color(colorValue)
	{
		steps = 40;
		constexpr float kPi = 3.14159265358979323846f;
		angle = (kPi * 2.0f) / static_cast<float>(steps);
		GenerateVertices();
		verticesSize = static_cast<int>(vertices.size());

		UpdateMassProperties();
	}

	void Ball::GenerateVertices() const
	{
		vertices.clear();
		vertices.reserve(static_cast<size_t>(steps) * 3);

		float prevX = radius * std::cos(0.0f);
		float prevY = radius * std::sin(0.0f);
		for (int i = 1; i <= steps; i++)
		{
			float theta = angle * static_cast<float>(i);
			float newX = radius * std::cos(theta);
			float newY = radius * std::sin(theta);

			vertices.emplace_back(0.0f, 0.0f);
			vertices.emplace_back(prevX, prevY);
			vertices.emplace_back(newX, newY);

			prevX = newX;
			prevY = newY;
		}

	}

	std::vector<float> Ball::GetVertexLocalPos() const
	{
		// Regenerate vertices each frame to update the rotation indicator
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

	std::vector<float> Shape::GetVertexLocalPos() const
	{
		std::vector<float> out;
		out.reserve(vertices.size() * 2);
		for (const auto &v : vertices)
		{
			out.push_back(v.x);
			out.push_back(v.y);
		}
		return out;
	}

	std::vector<math::Vec2> Shape::GetVertexWorldPos() const
	{
		std::vector<math::Vec2> out;
		out.reserve(vertices.size());
		for (const auto &v : vertices)
		{
			out.emplace_back(v.x + pos.x, v.y + pos.y);
		}
		return out;
	}

	collision::AABB Ball::GetAABB() const
	{
		if (aabbUpdateRequired)
		{
			float minX = pos.x - radius;
			float minY = pos.y - radius;
			float maxX = pos.x + radius;
			float maxY = pos.y + radius;
			aabb = collision::AABB(minX, minY, maxX, maxY);
			aabbUpdateRequired = false;
		}
		return aabb;
	}

	float Ball::ComputeInertia() const
	{
		if (mass <= 0.0f)
		{
			return 0.0f;
		}
		return 0.5f * mass * radius * radius;
	}

} // namespace physics
