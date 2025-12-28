#include "physics/Rigidbody.hpp"// Required for std::clamp
#include "common/settings.hpp"

namespace physics {


Rigidbody::Rigidbody(const math::Vec2& position,
		 const math::Vec2& initialLinearVel,
		 const math::Vec2& initialLinearAcc,
		 float RigidbodyMass,
		 float restitution,
		 RigidbodyType RigidbodyTypeValue)
	: pos(position)
	, linearVel(initialLinearVel * SimulationConstants::PIXELS_PER_METER)
	, linearAcc(initialLinearAcc * SimulationConstants::PIXELS_PER_METER)
	, bodyType(RigidbodyTypeValue)
	, restitution(math::Clamp(restitution, 0.0f, 1.0f)) {
	mass = (RigidbodyMass > 0.0f) ? RigidbodyMass : 1.0f;
	area = 1.0f;
	if (bodyType != RigidbodyType::Static) {
		invMass = 1.0f / mass;
	} else {
		invMass = 0.0f;
	}
}

void Rigidbody::UpdateMassProperties() {
	inertia = ComputeInertia();
	if (bodyType == RigidbodyType::Static || inertia == 0.0f) {
		invInertia = 0.0f;
	} else {
		invInertia = 1.0f / inertia;
	}
}

float Rigidbody::ComputeInertia() const {
	return 0.0f;
}

void Rigidbody::Update(float deltaMs, int iterations) {
	if (bodyType == RigidbodyType::Static) {
		return;
	}

	if (bodyType == RigidbodyType::Dynamic) {
		if (iterations <= 0) {
			iterations = 1;
		}
		const float dtSeconds = (deltaMs / 1000.0f) / static_cast<float>(iterations);

		ClearForces();
		netForce = math::Vec2();
		UpdateForce();
		linearAcc = netForce / mass;
		linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
		pos = pos + (linearVel * dtSeconds);
		linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
		rotation = rotation + angularVel * dtSeconds;

		netForce = math::Vec2();
		transformUpdateRequired = true;
		aabbUpdateRequired = true;
	}
}

void Rigidbody::Translate(const math::Vec2& amount) {
	pos = pos + amount;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Rigidbody::TranslateTo(const math::Vec2& position) {
	pos = position;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Rigidbody::RotateTo(float angleRadians) {
	rotation = angleRadians;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Rigidbody::Rotate(float amountRadians) {
	rotation += amountRadians;
	transformUpdateRequired = true;
	aabbUpdateRequired = true;
}

void Rigidbody::ApplyForce(const math::Vec2& forceAmount, const ForceType type) {
	forces.push_back(ForceInfo{forceAmount, type});
	netForce += forceAmount;
}

void Rigidbody::ClearForces() {
	forces.clear();
}

void Rigidbody::ApplyGravity() {
	const float strength = PhysicsConstants::GRAVITY * SimulationConstants::PIXELS_PER_METER;
	const math::Vec2 gravityForce(0.0f, -mass * strength);
	ApplyForce(gravityForce, ForceType::Gravitational);
}

void Rigidbody::UpdateForce() {
	ApplyGravity();

}

} // namespace physics
