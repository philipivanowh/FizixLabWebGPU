#pragma once

#include "physics/Body.hpp"

#include <array>
#include <vector>

namespace physics {

class Box : public Body {
public:
	Box(const math::Vec2& pos,
	    const math::Vec2& vel,
	    const math::Vec2& acc,
	    float width,
	    float height,
	    const std::array<float, 4>& color,
	    float mass,
	    float restitution,
	    BodyType bodyType);

	std::vector<float> GetRect() const;
	std::vector<math::Vec2> GetVertexWorldPos() const;
	AABB GetAABB() const override;
	float ComputeInertia() const override;

	float width = 0.0f;
	float height = 0.0f;
	std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
	int verticesSize = 0;

private:
	std::vector<math::Vec2> GetRotatedVertices() const;

	mutable std::vector<math::Vec2> vertices;
	mutable AABB aabb;
};

} // namespace physics
