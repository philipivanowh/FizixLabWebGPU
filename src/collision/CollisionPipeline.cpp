#include "collision/CollisionPipeline.hpp"

#include "collision/Collisions.hpp"
#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

void CollisionPipeline::BuildPairs(const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies,
                                   float dtSeconds)
{
	pairs.clear();

	if (bodies.size() < 2)
	{
		return;
	}

	aabbs.clear();
	collidable.clear();
	aabbs.reserve(bodies.size());
	collidable.reserve(bodies.size());

	// Fat AABBs: the margin covers the body's travel over the whole frame
	// plus a size-relative slop, so the pair list built from them remains a
	// valid superset for every sub-step of the frame.
	float extentSum = 0.0f;
	int extentCount = 0;
	for (const auto& body : bodies)
	{
		collision::AABB box = body->GetAABB();
		const float extentX = box.max.x - box.min.x;
		const float extentY = box.max.y - box.min.y;
		const float speed = body->linearVel.Length();
		const float margin = 1.5f * speed * dtSeconds +
		                     0.05f * std::max(extentX, extentY);
		box.min.x -= margin;
		box.min.y -= margin;
		box.max.x += margin;
		box.max.y += margin;
		aabbs.push_back(box);
		collidable.push_back(shape::KindOf(*body) != shape::ShapeType::Cannon ? 1 : 0);

		if (body->bodyType == physics::RigidbodyType::Dynamic)
		{
			extentSum += std::max(extentX, extentY);
			++extentCount;
		}
	}

	// Cell size adapts to the average dynamic-body size so the grid works
	// for meter-scale and pixel-scale scenes alike (a fixed cell size
	// degenerates one of the two into a single giant cell = O(n²)).
	const float averageExtent = extentCount > 0 ? extentSum / static_cast<float>(extentCount) : 1.0f;
	const float cellSize = std::max(2.0f * averageExtent, 1e-3f);
	const float inverseCellSize = 1.0f / cellSize;

	auto cellKey = [](int x, int y) -> std::uint64_t
	{
		const std::uint64_t keyX = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
		const std::uint64_t keyY = static_cast<std::uint64_t>(static_cast<std::uint32_t>(y));
		return (keyX << 32) | keyY;
	};

	// Flat sort-based grid: (cell, body) entries sorted by cell. No per-cell
	// heap containers, no hash maps — just two sorts over reused buffers.
	cellEntries.clear();
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
				cellEntries.emplace_back(cellKey(x, y), static_cast<std::uint32_t>(i));
			}
		}
	}

	std::sort(cellEntries.begin(), cellEntries.end());

	// Candidate pairs = bodies sharing a cell. Duplicates (bodies sharing
	// several cells) are removed with a sort+unique over packed index pairs.
	pairKeys.clear();
	for (size_t start = 0; start < cellEntries.size();)
	{
		size_t end = start + 1;
		while (end < cellEntries.size() && cellEntries[end].first == cellEntries[start].first)
		{
			++end;
		}

		for (size_t a = start; a + 1 < end; ++a)
		{
			const std::uint32_t i = cellEntries[a].second;
			for (size_t b = a + 1; b < end; ++b)
			{
				const std::uint32_t j = cellEntries[b].second;

				if (bodies[i]->bodyType == physics::RigidbodyType::Static &&
					bodies[j]->bodyType == physics::RigidbodyType::Static)
				{
					continue;
				}

				const std::uint32_t minIndex = i < j ? i : j;
				const std::uint32_t maxIndex = i < j ? j : i;
				pairKeys.push_back((static_cast<std::uint64_t>(minIndex) << 32) | maxIndex);
			}
		}
		start = end;
	}

	std::sort(pairKeys.begin(), pairKeys.end());
	pairKeys.erase(std::unique(pairKeys.begin(), pairKeys.end()), pairKeys.end());

	for (const std::uint64_t key : pairKeys)
	{
		const size_t i = static_cast<size_t>(key >> 32);
		const size_t j = static_cast<size_t>(key & 0xffffffffu);
		if (IntersectAABBs(aabbs[i], aabbs[j]))
		{
			pairs.emplace_back(i, j);
		}
	}
}

const std::vector<CollisionPipeline::Pair>& CollisionPipeline::GetPairs() const
{
	return pairs;
}
