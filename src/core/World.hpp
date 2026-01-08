#pragma once

#include "collision/CollisionPipeline.hpp"
#include "collision/CollisionSolver.hpp"
#include "physics/Rigidbody.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class Renderer;


class World {
public:
	void Add(std::unique_ptr<physics::Rigidbody> body);
	void Update(float deltaMs, int iterations);
	void Draw(Renderer& renderer) const;

	size_t RigidbodyCount() const;
	void ClearObjects();
	
	physics::Rigidbody* PickBody(const math::Vec2& p);

private:
	int ClampIterations(int value);
	void RemoveObjects();

	std::vector<std::unique_ptr<physics::Rigidbody>> objects;
	CollisionPipeline collisionPipeline;
	CollisionSolver collisionSolver;

	float viewBottom = -200.0f;

};
