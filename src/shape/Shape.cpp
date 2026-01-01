#include "shape/Shape.hpp"

#include "collision/AABB.hpp"

#include <algorithm>
#include <limits>

namespace shape
{
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

	collision::AABB Shape::GetAABB() const
	{
		if (aabbUpdateRequired)
		{
			float minX = std::numeric_limits<float>::infinity();
			float minY = std::numeric_limits<float>::infinity();
			float maxX = -std::numeric_limits<float>::infinity();
			float maxY = -std::numeric_limits<float>::infinity();

			std::vector<math::Vec2> verts = GetVertexWorldPos();

			for (const auto &v : verts)
			{
				minX = std::min(minX, v.x);
				minY = std::min(minY, v.y);
				maxX = std::max(maxX, v.x);
				maxY = std::max(maxY, v.y);
			}

			aabb = collision::AABB(minX, minY, maxX, maxY);
			aabbUpdateRequired = false;
		}
		return aabb;
	}
}
