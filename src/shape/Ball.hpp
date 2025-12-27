#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape {

class Ball : public Shape {
public:
	Ball(const math::Vec2& pos,
	     const math::Vec2& vel,
	     const math::Vec2& acc,
	     float radius,
	     const std::array<float, 4>& color,
	     float mass,
		 float restitution,
	     physics::RigidbodyType RigidbodyType);
	~Ball() override = default;
		
	void GenerateVertices() const;
	std::vector<float> GetVertexLocalPos() const override;
	float ComputeInertia() const override;
	collision::AABB GetAABB() const override;

	float radius = 0.0f;
	std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
	int steps = 40;
	float angle = 0.0f;
	int verticesSize = 0;
};

} // namespace physics
