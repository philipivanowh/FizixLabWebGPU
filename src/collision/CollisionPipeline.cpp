#include "collision/CollisionPipeline.hpp"

#include "collision/Collisions.hpp"
#include "physics/Rigidbody.hpp"
#include "shape/Canon.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

void CollisionPipeline::BuildPairs(const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies)
{
	pairs.clear();

	if (bodies.size() < 2)
	{
		return;
	}

	std::vector<collision::AABB> aabbs;
	aabbs.reserve(bodies.size());
	std::vector<bool> collidable;
	collidable.reserve(bodies.size());

	for (const auto& body : bodies)
	{
		aabbs.push_back(body->GetAABB());
		collidable.push_back(dynamic_cast<const shape::Canon*>(body.get()) == nullptr);
	}

	const float inverseCellSize = 1.0f / cellSize;
	std::unordered_map<std::uint64_t, std::vector<size_t>> grid;
	grid.reserve(bodies.size());

	auto cellKey = [](int x, int y) -> std::uint64_t
	{
		const std::uint64_t keyX = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
		const std::uint64_t keyY = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y));
		return (keyX << 32) | keyY;
	};

	for (size_t i = 0; i < bodies.size(); ++i)
	{
		if (!collidable[i])
		{
			continue;
		}
		const collision::AABB& box = aabbs[i];
		const int minX = static_cast<int>(std::floor(box.min.x * inverseCellSize));
		const int maxX = static_cast<int>(std::floor(box.max.x * inverseCellSize));
		const int minY = static_cast<int>(std::floor(box.min.y * inverseCellSize));
		const int maxY = static_cast<int>(std::floor(box.max.y * inverseCellSize));

		for (int x = minX; x <= maxX; ++x)
		{
			for (int y = minY; y <= maxY; ++y)
			{
				grid[cellKey(x, y)].push_back(i);
			}
		}
	}

	std::unordered_set<std::uint64_t> pairSet;
	pairSet.reserve(bodies.size() * 4);

	for (const auto& cell : grid)
	{
		const auto& indices = cell.second;
		if (indices.size() < 2)
		{
			continue;
		}

		for (size_t a = 0; a + 1 < indices.size(); ++a)
		{
			const size_t i = indices[a];
			physics::Rigidbody& bodyA = *bodies[i];

			for (size_t b = a + 1; b < indices.size(); ++b)
			{
				const size_t j = indices[b];
				physics::Rigidbody& bodyB = *bodies[j];

				if (bodyA.bodyType == physics::RigidbodyType::Static &&
					bodyB.bodyType == physics::RigidbodyType::Static)
				{
					continue;
				}

				const size_t minIndex = i < j ? i : j;
				const size_t maxIndex = i < j ? j : i;
				const std::uint64_t pairKey =
					(static_cast<std::uint64_t>(minIndex) << 32) |
					static_cast<std::uint64_t>(maxIndex);

				if (!pairSet.insert(pairKey).second)
				{
					continue;
				}

				if (!IntersectAABBs(aabbs[i], aabbs[j]))
				{
					continue;
				}

				pairs.emplace_back(minIndex, maxIndex);
			}
		}
	}
}

const std::vector<CollisionPipeline::Pair>& CollisionPipeline::GetPairs() const
{
	return pairs;
}
