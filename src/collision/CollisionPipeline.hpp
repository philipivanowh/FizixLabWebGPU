#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "collision/AABB.hpp"

namespace physics {
class Rigidbody;
}

class CollisionPipeline {
public:
	using Pair = std::pair<size_t, size_t>;

	// Builds the candidate pair list from AABBs fattened by each body's
	// expected travel over dtSeconds plus a size-relative slop (Box2D-style
	// predictive margin). With dtSeconds covering the whole frame, the pair
	// list stays valid for every sub-step, so it only needs to be built once
	// per frame instead of once per sub-step.
	void BuildPairs(const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies,
	                float dtSeconds = 0.0f);
	const std::vector<Pair>& GetPairs() const;

private:
	std::vector<Pair> pairs;

	// Scratch buffers reused across calls — zero steady-state allocations.
	std::vector<collision::AABB> aabbs;
	std::vector<std::uint8_t> collidable;
	std::vector<std::pair<std::uint64_t, std::uint32_t>> cellEntries;
	std::vector<std::uint64_t> pairKeys;
};
