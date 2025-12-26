#include "physics/Body.hpp"// Required for std::clamp
#include <algorithm>

namespace physics {

namespace {
constexpr float kPixelsPerMeter = 100.0f;
}

Body::Body(const math::Vec2& position,
		 const math::Vec2& initialLinearVel,
		 const math::Vec2& initialLinearAcc,
		 float bodyMass,
		 float restitution,
		 BodyType bodyTypeValue)
	: pos(position)
	, linearVel(initialLinearVel * kPixelsPerMeter)
	, linearAcc(initialLinearAcc * kPixelsPerMeter)
	, bodyType(bodyTypeValue)
	, restitution(std::clamp(restitution, 0.0f, 1.0f)) {
	mass = (bodyMass > 0.0f) ? bodyMass : 1.0f;
	area = 1.0f;
	if (bodyType != BodyType::Static) {
		invMass = 1.0f / mass;
	} else {
		invMass = 0.0f;
	}
}

void Body::UpdateMassProperties() {
	inertia = ComputeInertia();
	if (bodyType == BodyType::Static || inertia == 0.0f) {
		invInertia = 0.0f;
	} else {
		invInertia = 1.0f / inertia;
	}
}

float Body::ComputeInertia() const {
	return 0.0f;
}

void Body::Update(float deltaMs, int iterations) {
	if (bodyType == BodyType::Static) {
		return;
	}

	if (bodyType == BodyType::Dynamic) {
		if (iterations <= 0) {
			iterations = 1;
		}
		const float dtSeconds = (deltaMs / 1000.0f) / static_cast<float>(iterations);

		UpdateForce();
		linearAcc = force / mass;
		linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
		pos = pos + (linearVel * dtSeconds);
		linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
		rotation = rotation + angularVel * dtSeconds;

		force = math::Vec2();
		transformUpdateRequired = true;
		aabbUpdateRequired = true;
	}
}

void Body::Translate(const math::Vec2& amount) {
	pos = pos + amount;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Body::TranslateTo(const math::Vec2& position) {
	pos = position;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Body::RotateTo(float angleRadians) {
	rotation = angleRadians;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Body::Rotate(float amountRadians) {
	rotation += amountRadians;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Body::ApplyForce(const math::Vec2& forceAmount) {
	force = force + forceAmount;
}

void Body::ApplyGravity() {
	const float strength = GRAVITATIONAL_STRENGTH * kPixelsPerMeter;
	const math::Vec2 gravityForce(0.0f, -mass * strength);
	ApplyForce(gravityForce);
}

void Body::UpdateForce() {
	ApplyGravity();

}

void Body::DrawShape() const {
}

} // namespace physics
