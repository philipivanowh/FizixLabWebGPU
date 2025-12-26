#pragma once

#include "physics/AABB.hpp"
#include "physics/PhysicsConstants.hpp"
#include "physics/Rigidbody.hpp"
#include "math/Vec2.hpp"
#include "math/Math.hpp"


namespace physics {

class Body {
public:
	Body(const math::Vec2& pos,
		 const math::Vec2& linearVel,
		 const math::Vec2& linearAcc,
		 float mass,
		 float restitution,
		 BodyType bodyType);

	virtual ~Body() = default;

	void Update(float deltaMs, int iterations);
	void Translate(const math::Vec2& amount);
	void TranslateTo(const math::Vec2& position);
	void RotateTo(float angleRadians);
	void Rotate(float amountRadians);
	void ApplyForce(const math::Vec2& forceAmount);
	void UpdateForce();

	void UpdateMassProperties();
	virtual float ComputeInertia() const;
	virtual AABB GetAABB() const = 0;
	virtual void DrawShape() const;

	math::Vec2 pos;
	math::Vec2 linearVel;
	math::Vec2 linearAcc;
	BodyType bodyType;
	float rotation = 0.0f;
	float angularVel = 0.0f;
	float angularAcc = 0.0f;
	float restitution = 0.5f;
	math::Vec2 force = math::Vec2();
	float area = 1.0f;
	float inertia = 0.0f;
	float invInertia = 0.0f; 
	float density = 1.0f;
	float mass = 1.0f;
	float invMass = 0.0f;
	float staticFriction = 0.6f;
	float kineticFriction = 0.3f;
	mutable bool transformUpdateRequired = true;
	mutable bool aabbUpdateRequired = true;

protected:
	void ApplyGravity();
};

} // namespace physics
