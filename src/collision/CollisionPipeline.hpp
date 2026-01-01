#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace physics {
class Rigidbody;
}

class CollisionPipeline {
public:
	using Pair = std::pair<size_t, size_t>;

	void BuildPairs(const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies);
	const std::vector<Pair>& GetPairs() const;

private:
	std::vector<Pair> pairs;
	float cellSize = 200.0f;
};
