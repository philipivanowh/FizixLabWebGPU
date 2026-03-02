#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape {

class Box : public Shape {
public:
	Box(const math::Vec2& pos,
	    const math::Vec2& vel,
	    const math::Vec2& acc,
	    float width,
	    float height,
	    const std::array<float, 4>& color,
	    float mass,
	    float restitution,
	    physics::RigidbodyType RigidbodyType);
	~Box() override = default;
	
	float ComputeInertia() const override;
	std::vector<math::Vec2> GetRotatedVertices() const;
	std::vector<float> GetVertexLocalPos() const override;
	std::vector<math::Vec2> GetVertexWorldPos() const override;
	std::array<float, 4> GetColor() const override { return color; }

	float width = 0.0f;
	float height = 0.0f;
	std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
	int verticesSize = 0;
};

} // namespace physics
