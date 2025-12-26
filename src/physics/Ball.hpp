#pragma once

#include "physics/Body.hpp"

#include <array>
#include <vector>

namespace physics {

class Ball : public Body {
public:
	Ball(const math::Vec2& pos,
	     const math::Vec2& vel,
	     const math::Vec2& acc,
	     float radius,
	     const std::array<float, 4>& color,
	     float mass,
		 float restitution,
	     BodyType bodyType);

	std::vector<float> GetBall() const;
	std::vector<math::Vec2> GetVertexWorldPos() const;
	AABB GetAABB() const override;
	float ComputeInertia() const override;

	float radius = 0.0f;
	std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
	int steps = 40;
	float angle = 0.0f;
	int verticesSize = 0;

private:
	std::vector<math::Vec2> GenerateVertices() const;

	mutable std::vector<math::Vec2> vertices;
	mutable AABB aabb;
};

} // namespace physics
