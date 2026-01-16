#pragma once

#include "collision/AABB.hpp"
#include "math/Vec2.hpp"
#include "math/Math.hpp"
#include <memory>
#include <vector>

namespace physics
{
	enum RigidbodyType
	{
		Static,
		Dynamic
	};

	enum ForceType
	{
		Normal,
		Frictional,
		Gravitational,
		Tension,
		Apply
	};

	struct ForceInfo
	{
		math::Vec2 force;
		ForceType type;
	};

	class ForceGenerator
	{
	public:
		virtual ~ForceGenerator() = default;
		virtual void Apply(class Rigidbody &body, float deltaMs) = 0;
	};

	class Rigidbody
	{
	public:
		Rigidbody(const math::Vec2 &pos,
				  const math::Vec2 &linearVel,
				  const math::Vec2 &linearAcc,
				  float mass,
				  float restitution,
				  RigidbodyType RigidbodyType);
		virtual ~Rigidbody() = default;

		void Update(float deltaMs, int iterations);
		void Translate(const math::Vec2 &amount);
		void TranslateTo(const math::Vec2 &position);
		void RotateTo(float angleRadians);
		void Rotate(float amountRadians);
		void AddDisplayForce(const math::Vec2 &forceAmount, const ForceType type);
		void AddForceGenerator(std::unique_ptr<ForceGenerator> generator);
		void UpdateForces(float deltaMs);
		void BeginFrameForces();
		void AccumulateNormalImpulse(const math::Vec2 &normalImpulse);
		void FinalizeForces(float deltaMs);
		math::Vec2 GetNormalForce() const;
		const std::vector<ForceInfo> &GetForcesForDisplay() const;
		void ClearForces();


		void UpdateMassProperties();
		virtual float ComputeInertia() const;
		virtual collision::AABB GetAABB() const = 0;

		math::Vec2 pos;
		math::Vec2 linearVel;
		math::Vec2 linearAcc;
		RigidbodyType bodyType;
		float rotation = 0.0f;
		float angularVel = 0.0f;
		float angularAcc = 0.0f;
		float restitution = 0.5f;
		math::Vec2 netForce = math::Vec2();
		std::vector<ForceInfo> forces;
		float area = 1.0f;
		float inertia = 0.0f;
		float invInertia = 0.0f;
		float density = 1.0f;
		float mass = 1.0f;
		float invMass = 0.0f;
		float staticFriction = 0.0f;
		float kineticFriction = 0.1f;
		math::Vec2 normalImpulseAccum = math::Vec2();
		math::Vec2 normalForce = math::Vec2();
		math::Vec2 dragForce = math::Vec2();
		std::vector<std::unique_ptr<ForceGenerator>> forceGenerators;
		mutable bool transformUpdateRequired = true;
		mutable bool aabbUpdateRequired = true;

	protected:
		void UpdateGravity();
	};

} // namespace physics
