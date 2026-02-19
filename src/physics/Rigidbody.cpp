#include "physics/Rigidbody.hpp"// Required for std::clamp
#include "common/settings.hpp"

#include <cmath>
#include <cstdint>

namespace physics {


	class GravityForceGenerator final : public ForceGenerator
	{
	public:
		void Apply(Rigidbody& body, float) override
		{
			const float strength = PhysicsConstants::GRAVITY * SimulationConstants::PIXELS_PER_METER;
			const math::Vec2 gravityForce(0.0f, -body.mass * strength);
			body.netForce += gravityForce;
			body.AddDisplayForce(gravityForce, ForceType::Gravitational);
		}
	};

	class DragForceGenerator final : public ForceGenerator
	{
		public:
			void Apply(Rigidbody& body,float ) override
			{
				//std::cout<<body.dragForce.x<<std::endl;
				body.netForce += body.dragForce;
				//body.AddDisplayForce(gravityForce, ForceType::Apply);
			}
	};



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
	AddForceGenerator(std::make_unique<GravityForceGenerator>());
	AddForceGenerator(std::make_unique<DragForceGenerator>());
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
		UpdateForces(deltaMs);
		linearAcc = netForce / mass;
		linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
		pos = pos + (linearVel * dtSeconds);
		linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
		rotation = rotation + angularVel * dtSeconds;

		netForce = math::Vec2();
		dragForce = math::Vec2();
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
	std::cout<<position.x<<std::endl;
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

void Rigidbody::AddDisplayForce(const math::Vec2& forceAmount, const ForceType type) {
	forces.push_back(ForceInfo{forceAmount, type});
}

void Rigidbody::AddForceGenerator(std::unique_ptr<ForceGenerator> generator) {
	forceGenerators.push_back(std::move(generator));
}

void Rigidbody::BeginFrameForces() {
	ClearForces();
	//Normal
	normalImpulseAccum = math::Vec2();
	normalForce = math::Vec2();
	//Friction
	frictionImpulseAccum = math::Vec2();
	frictionForce = math::Vec2();
	netForce = math::Vec2();
}

void Rigidbody::AccumulateNormalImpulse(const math::Vec2& normalImpulse) {
	normalImpulseAccum = normalImpulse;
}

void Rigidbody::AccumulateFrictionImpulse(const math::Vec2& frictionImpulse){
	frictionImpulseAccum = frictionImpulse;
}


void Rigidbody::FinalizeForces(float deltaMs)
{
    if (deltaMs > 0.0f)
    {
        normalForce  = normalImpulseAccum  / deltaMs;
        frictionForce = frictionImpulseAccum / deltaMs;
    }
    else
    {
        normalForce   = math::Vec2();
        frictionForce = math::Vec2();
    }

    // Always show normal force if it exists
    if (!math::NearlyEqualVec(normalForce, math::Vec2(0.0f, 0.0f)))
        AddDisplayForce(normalForce, ForceType::Normal);

    // Only show friction once it crosses a meaningful magnitude threshold.
    // This filters out sub-pixel solver noise while still showing real friction.
    // Newtons — tune to taste
    const float frictionMag = frictionForce.Length();

    if (frictionMag > PhysicsConstants::FRICTION_DISPLAY_THRESHOLD && linearVel.Length() > 1.10f)
        AddDisplayForce(frictionForce, ForceType::Frictional);
}

math::Vec2 Rigidbody::GetNormalForce() const {
	return normalForce;
}

math::Vec2 Rigidbody::GetFrictionForce() const {
	return normalForce;
}

const std::vector<ForceInfo> &Rigidbody::GetForcesForDisplay() const {
	return forces;
}

void Rigidbody::ClearForces() {
	forces.clear();
}

void Rigidbody::UpdateGravity() {
	//const float strength = PhysicsConstants::GRAVITY * SimulationConstants::PIXELS_PER_METER;
	//const math::Vec2 gravityForce(0.0f, -mass * strength);
	//ApplyForce(gravityForce, ForceType::Gravitational);
}

void Rigidbody::UpdateForces(float deltaMs) {
	for (const auto &generator : forceGenerators) {
		generator->Apply(*this, deltaMs);
	}

}

} // namespace physics
