#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape
{

	class Incline : public Shape
	{
	public:
		Incline(const math::Vec2 &pos,
				const math::Vec2 &vel,
				const math::Vec2 &acc,
				float base,
				float angle,
				bool flip,
				const std::array<float, 4> &color,
				float staticFrictionValue,
				float kineticFrictionValue);
		~Incline() override = default;

		float ComputeInertia() const override;
		std::vector<math::Vec2> GetRotatedVertices() const;
		std::vector<float> GetVertexLocalPos() const override;
		std::vector<math::Vec2> GetVertexWorldPos() const override;

		void SetStaticFriction(float mu_s);
		void SetKineticFriction(float mu_k);
		float GetStaticFriction() const { return staticFriction; }
		float GetKineticFriction() const { return kineticFriction; }

		void SetAngle(float angleDegrees);
		void SetBase(float newBase);
		void SetFlip(bool shouldFlip);

		float GetAngle() const { return angle * 180.0f / math::PI; }
		float GetAngleRadians() const { return angle; }
		float GetBase() const { return base; }
		float GetHeight() const { return height; }
		bool IsFlipped() const { return flip; }
		void UpdateVertices();
		void AdjustPositionForBaseAnchor(float oldHeight, float newHeight);

		float base = 0.0f;
		float height = 0.0f;
		float angle = 0.0f;
		bool flip = true;
		std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
		int verticesSize = 0;
	};

} // namespace physics
