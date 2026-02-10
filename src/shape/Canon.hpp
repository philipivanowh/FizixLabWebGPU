#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape {

class Canon : public Shape {
public:
	Canon(const math::Vec2& pos,
	      float angle,
	      const std::array<float, 4>& color);
	~Canon() override = default;

	void GenerateVertices() const;
	void GenerateWheelVertices() const;
	void GenerateBarrelVerticies() const;
	float ComputeInertia() const override;
	std::vector<float> GetVertexLocalPos() const override;
	std::vector<float> GetWheelVertexLocalPos() const;
	std::vector<float> GetBarrelVertexLocalPos() const;
	std::vector<math::Vec2> GetVertexWorldPos() const override;

	float wheelRadius = 50.0f;
	int steps = 40;
	float barrelAngleDegrees = 0.0f;
	std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
	std::array<float, 4> wheelColor{0.0f, 0.0f, 0.0f, 1.0f};
	std::array<float, 4> barrelColor{0.0f, 0.0f, 0.0f, 1.0f};
	float barrelLength = 80.0f;
	float barrelWidth = 30.0f;
	int verticesSize = 0;
};

} // namespace shape
