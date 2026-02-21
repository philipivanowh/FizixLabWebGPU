#include "physics/Rigidbody.hpp" // Required for std::clamp
#include "common/settings.hpp"

#include <cmath>
#include <cstdint>

namespace physics
{

	class GravityForceGenerator final : public ForceGenerator
	{
	public:
		void Apply(Rigidbody &body, float) override
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
		void Apply(Rigidbody &body, float) override
		{

			body.netForce += body.dragForce;
		}
	};

	Rigidbody::Rigidbody(const math::Vec2 &position,
						 const math::Vec2 &initialLinearVel,
						 const math::Vec2 &initialLinearAcc,
						 float RigidbodyMass,
						 float restitution,
						 RigidbodyType RigidbodyTypeValue)
		: pos(position), linearVel(initialLinearVel * SimulationConstants::PIXELS_PER_METER), linearAcc(initialLinearAcc * SimulationConstants::PIXELS_PER_METER), bodyType(RigidbodyTypeValue), restitution(math::Clamp(restitution, 0.0f, 1.0f))
	{
		mass = (RigidbodyMass > 0.0f) ? RigidbodyMass : 1.0f;
		area = 1.0f;
		if (bodyType != RigidbodyType::Static)
		{
			invMass = 1.0f / mass;
		}
		else
		{
			invMass = 0.0f;
		}
		AddForceGenerator(std::make_unique<GravityForceGenerator>());
		AddForceGenerator(std::make_unique<DragForceGenerator>());
	}

	void Rigidbody::UpdateMassProperties()
	{
		inertia = ComputeInertia();
		if (bodyType == RigidbodyType::Static || inertia == 0.0f)
		{
			invInertia = 0.0f;
		}
		else
		{
			invInertia = 1.0f / inertia;
		}
	}

	float Rigidbody::ComputeInertia() const
	{
		return 0.0f;
	}

	void Rigidbody::Update(float deltaMs, int iterations)
	{
		if (bodyType == RigidbodyType::Static)
		{
			return;
		}

		if (bodyType == RigidbodyType::Dynamic)
		{
			if (iterations <= 0)
			{
				iterations = 1;
			}
			const float dtSeconds = (deltaMs / 1000.0f) / static_cast<float>(iterations);

			ClearForces();
			UpdateForces(deltaMs);
			linearAcc = netForce / mass;
			linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
			pos = pos + (linearVel * dtSeconds);
			linearVel = linearVel + (linearAcc * dtSeconds) * 0.5;
			rotation = std::fmod((rotation + angularVel * dtSeconds), 360.0f);

			netForce = math::Vec2();
			dragForce = math::Vec2();
			transformUpdateRequired = true;
			aabbUpdateRequired = true;
		}
	}

	void Rigidbody::Translate(const math::Vec2 &amount)
	{
		pos = pos + amount;
		transformUpdateRequired = true;
		aabbUpdateRequired = true;
	}

	void Rigidbody::TranslateTo(const math::Vec2 &position)
	{
		pos = position;
		transformUpdateRequired = true;
		aabbUpdateRequired = true;
	}

	void Rigidbody::RotateTo(float angleRadians)
	{
		rotation = angleRadians;
		transformUpdateRequired = true;
		aabbUpdateRequired = true;
	}

	void Rigidbody::Rotate(float amountRadians)
	{
		rotation += amountRadians;
		transformUpdateRequired = true;
		aabbUpdateRequired = true;
	}

	void Rigidbody::AddDisplayForce(const math::Vec2 &forceAmount, const ForceType type)
	{
		forces.push_back(ForceInfo{forceAmount, type});
	}

	void Rigidbody::AddForceGenerator(std::unique_ptr<ForceGenerator> generator)
	{
		forceGenerators.push_back(std::move(generator));
	}

	void Rigidbody::BeginFrameForces()
	{
		ClearForces();
		// Normal
		normalImpulseAccum = math::Vec2();
		normalForce = math::Vec2();
		// Friction
		frictionImpulseAccum = math::Vec2();
		frictionForce = math::Vec2();
		netForce = math::Vec2();
	}

	void Rigidbody::AccumulateNormalImpulse(const math::Vec2 &normalImpulse)
	{
		normalImpulseAccum += normalImpulse;
	}

	void Rigidbody::AccumulateFrictionImpulse(const math::Vec2 &frictionImpulse)
	{
		frictionImpulseAccum += frictionImpulse;
	}

	void Rigidbody::FinalizeForces(float deltaMs)
	{
		if (deltaMs > 0.0f)
		{
			const float dtSeconds = deltaMs / 1000.0f;
			normalForce = normalImpulseAccum / dtSeconds;
			frictionForce = frictionImpulseAccum / dtSeconds;
		}
		else
		{
			normalForce = math::Vec2();
			frictionForce = math::Vec2();
		}

		const float alpha = 0.5f;
		smoothedNormalForce = smoothedNormalForce * (1.0f - alpha) + normalForce * alpha;
		smoothedFrictionForce = smoothedFrictionForce * (1.0f - alpha) + frictionForce * alpha;

		// ── Direction-flip suppression ────────────────────────────────────────────
		// When the solver oscillates on a stationary body, friction flips ~180°
		// every frame. A real friction force never does that — if the direction
		// dot product is negative the body is at rest and we're seeing solver noise.
		const float frictionLen = smoothedFrictionForce.Length();
		if (frictionLen > 1e-1f)
		{
			const math::Vec2 currentDir = smoothedFrictionForce * (1.0f / frictionLen);

			const bool directionFlipped =
				math::Vec2::Dot(currentDir, prevSmoothedFrictionDir) < 0.0f &&
				prevSmoothedFrictionDir.Length() > 0.5f; // guard for first frame

			if (directionFlipped)
			{
				// Oscillating — body is still, zero out display and reset smoother
				smoothedFrictionForce = math::Vec2();
				prevSmoothedFrictionDir = math::Vec2();
			}
			else
			{
				prevSmoothedFrictionDir = currentDir;
			}
		}
		else
		{
			// Force too small — reset so next frame starts fresh
			prevSmoothedFrictionDir = math::Vec2();
		}
		// ─────────────────────────────────────────────────────────────────────────

		if (!math::NearlyEqualVec(smoothedNormalForce, math::Vec2(0.0f, 0.0f)))
			AddDisplayForce(smoothedNormalForce, ForceType::Normal);

		const float frictionMag = smoothedFrictionForce.Length() * invMass;
		std::cout << frictionMag <<
		if (frictionMag >= PhysicsConstants::FRICTION_DISPLAY_THRESHOLD)
			AddDisplayForce(smoothedFrictionForce, ForceType::Frictional);
	}

	math::Vec2 Rigidbody::GetNormalForce() const
	{
		return normalForce;
	}

	math::Vec2 Rigidbody::GetFrictionForce() const
	{
		return frictionForce;
	}

	const std::vector<ForceInfo> &Rigidbody::GetForcesForDisplay() const
	{
		return forces;
	}

	void Rigidbody::ClearForces()
	{
		forces.clear();
	}

	void Rigidbody::UpdateGravity()
	{
		// const float strength = PhysicsConstants::GRAVITY * SimulationConstants::PIXELS_PER_METER;
		// const math::Vec2 gravityForce(0.0f, -mass * strength);
		// ApplyForce(gravityForce, ForceType::Gravitational);
	}

	void Rigidbody::UpdateForces(float deltaMs)
	{
		for (const auto &generator : forceGenerators)
		{
			generator->Apply(*this, deltaMs);
		}
	}

} // namespace physics
