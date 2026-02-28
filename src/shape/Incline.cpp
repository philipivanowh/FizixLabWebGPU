#include "shape/Incline.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"
#include "math/Math.hpp"
#include "common/settings.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shape
{

	Incline::Incline(const math::Vec2 &pos,
					 const math::Vec2 &vel,
					 const math::Vec2 &acc,
					 float base,
					 float angle,
					 bool flip,
					 const std::array<float, 4> &colorValue,
					 float staticFrictionValue,
					 float kineticFrictionValue)
		: Shape(pos, vel, acc, 1, 0, physics::RigidbodyType::Static), base(base* SimulationConstants::PIXELS_PER_METER), height(std::tan(angle * math::PI / 180.0f) * base), angle(angle * math::PI / 180.0f), flip(flip), color(colorValue)
	{

		staticFriction = staticFrictionValue;
		kineticFriction = kineticFrictionValue;
		verticesSize = 3;
		UpdateMassProperties();

		UpdateVertices();
	}

	// In Incline.cpp
	void Incline::SetStaticFriction(float mu_s)
	{
		staticFriction = math::Clamp(mu_s, 0.0f, 2.0f); // Clamp to reasonable range
	}

	void Incline::SetKineticFriction(float mu_k)
	{
		kineticFriction = math::Clamp(mu_k, 0.0f, 2.0f); // Clamp to reasonable range

		// Ensure kinetic friction doesn't exceed static friction
		if (kineticFriction > staticFriction)
		{
			kineticFriction = staticFriction;
		}
	}

	void Incline::SetFlip(bool shouldFlip)
	{
		if (flip != shouldFlip)
		{
			flip = shouldFlip;
			UpdateVertices();
			transformUpdateRequired = true;
		}
	}

	void Incline::SetBase(float newBase)
	{
		float oldHeight = height;

		base = std::max(newBase, 10.0f); // Minimum base width

		// Recalculate height (angle stays the same)
		height = std::tan(angle) * base;

		// Adjust position so base stays anchored
		AdjustPositionForBaseAnchor(oldHeight, height);

		// Update vertices with new dimensions
		UpdateVertices();

		transformUpdateRequired = true;
	}

	void Incline::SetAngle(float angleDegrees)
	{
		// Convert to radians and store
		float oldHeight = height;

		// Convert to radians and store
		angle = angleDegrees * math::PI / 180.0f;

		// Clamp angle to reasonable range (avoid vertical or negative)
		angle = math::Clamp(angle, 0.01f, math::PI / 2.0f - 0.01f);

		// Recalculate height based on new angle
		height = std::tan(angle) * base;

		AdjustPositionForBaseAnchor(oldHeight, height);

		// Update the vertices with new dimensions
		UpdateVertices();

		// Mark that transform needs update for collision detection
		transformUpdateRequired = true;
	}
	void Incline::UpdateVertices()
	{
		float halfWidth = base / 2.0f;

		// Anchor vertices at the base (y = 0 is the bottom)
		// The base stays fixed, and the apex moves up with height
		if (flip)
		{
			// Right triangle with hypotenuse on the right
			vertices = {
				math::Vec2(-halfWidth, 0.0f),  // Bottom left (base)
				math::Vec2(halfWidth, 0.0f),   // Bottom right (base)
				math::Vec2(halfWidth, height), // Top right (apex)
			};
		}
		else
		{
			// Right triangle with hypotenuse on the left
			vertices = {
				math::Vec2(-halfWidth, 0.0f),	// Bottom left (base)
				math::Vec2(halfWidth, 0.0f),	// Bottom right (base)
				math::Vec2(-halfWidth, height), // Top left (apex)
			};
		}
	}

	void Incline::AdjustPositionForBaseAnchor(float oldHeight, float newHeight)
	{
		// Since vertices are now anchored at base (y=0), but the object's
		// position represents its center, we need to adjust pos.y when height changes
		//
		// Old center Y was at oldHeight/2, new center Y should be at newHeight/2
		// The difference in center position is what we add to pos.y
		float centerShift = (newHeight - oldHeight) / 2.0f;
		pos.y += centerShift;
	}

	std::vector<float> Incline::GetVertexLocalPos() const
	{
		std::vector<math::Vec2> rotated = GetRotatedVertices();
		const math::Vec2 &v0 = rotated[0];
		const math::Vec2 &v1 = rotated[1];
		const math::Vec2 &v2 = rotated[2];

		return {
			v0.x,
			v0.y,
			v1.x,
			v1.y,
			v2.x,
			v2.y,
		};
	}

	std::vector<math::Vec2> Incline::GetVertexWorldPos() const
	{
		std::vector<math::Vec2> out;
		std::vector<math::Vec2> rotated = GetRotatedVertices();
		out.reserve(rotated.size());
		for (const auto &v : rotated)
		{
			out.emplace_back(v.x + pos.x, v.y + pos.y);
		}
		transformUpdateRequired = false;
		return out;
	}

	float Incline::ComputeInertia() const
	{
		return 0.0f;
	}

	std::vector<math::Vec2> Incline::GetRotatedVertices() const
	{
		float cosValue = std::cos(rotation);
		float sinValue = std::sin(rotation);
		std::vector<math::Vec2> rotated;
		rotated.reserve(vertices.size());
		for (const auto &v : vertices)
		{
			rotated.emplace_back(
				v.x * cosValue - v.y * sinValue,
				v.x * sinValue + v.y * cosValue);
		}
		return rotated;
	}

} // namespace physics
