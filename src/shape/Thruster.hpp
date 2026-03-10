#pragma once

#include "shape/Shape.hpp"
#include "physics/Rigidbody.hpp"

#include <array>
#include <vector>
#include <GLFW/glfw3.h>

namespace shape
{
	/*
	================================================================
	 Thruster

	 A placeable nozzle that applies a directional force to a target
	 Rigidbody every tick.  It lives as its own object in the World
	 and "snaps" to its attached body each frame via
	 SyncToAttachedBody().

	 Geometry (local space, thrust direction = +Y):

		   ┌──────────┐    <- mounting bracket  (top,  attachment side)
		   └────┬┬────┘
			   /  \
			  /    \       <- nozzle bell (flares toward exhaust end)
			 /      \
			└────────┘     <- exhaust exit       (bottom, -Y side)

	 The shape is built pointing up (+Y = thrust direction).
	 rotation is set so the exhaust faces opposite to angleDegrees.

	 Attachment:
	   - attachedBody       : the Rigidbody this thruster is mounted on
	   - mountLocalOffset   : mount point in attached body's LOCAL space (pixels)
	   - mountLocalAngle    : nozzle angle relative to attached body's rotation (radians)

	 Call SyncToAttachedBody() once per tick (Engine::Update) BEFORE
	 calling ApplyThrust() so pos and rotation are up to date.
	================================================================
	*/
	class Thruster : public Shape
	{
	public:
		// ── Dimensions (pixels) ──────────────────────────────────────
		static constexpr float W = 24.0f; // total width
		static constexpr float H = 48.0f; // total height

		// ── Constructor ──────────────────────────────────────────────
		// pos            : initial world position (pixels)
		// angleDegs      : thrust direction in degrees (0=right, 90=up)
		// magnitudeN     : force magnitude in Newtons
		// bodyRelative   : true  → angle is relative to attached body's rotation
		//                  false → angle is fixed in world space
		// keyHoldMode    : true  → fire only while `fireKey` is held
		//                  false → always active while enabled
		// fireKey        : GLFW key code, default SPACE
		Thruster(const math::Vec2 &pos,
				 float angleDegs,
				 float magnitudeN,
				 bool bodyRelative,
				 bool keyHoldMode,
				 int fireKey = GLFW_KEY_SPACE);

		~Thruster() override
	{
    // Always unregister the generator from the attached body before
    // this object's memory is freed. Without this, the body's
    // forceGenerators list holds a ThrustForceGenerator with a
    // dangling const Thruster& reference → heap corruption.
    Detach();
	}

		// ── Shape interface ──────────────────────────────────────────
		float ComputeInertia() const override;
		std::vector<float> GetVertexLocalPos() const override;
		std::vector<math::Vec2> GetVertexWorldPos() const override;
		std::array<float, 4> GetColor() const override { return bodyColor; }

		// ── Attachment API ───────────────────────────────────────────
		// Attach to a body.
		//   localOffset : mount point in attached body's local space (pixels)
		//   localAngle  : nozzle orientation relative to the body (radians)
		void AttachTo(physics::Rigidbody *body,
					  math::Vec2 localOffset = {0.0f, 0.0f},
					  float localAngle = 0.0f);

		void Detach();

		// Called every tick by Engine::Update BEFORE physics step.
		// Moves this thruster's pos/rotation to track the attached body.
		void SyncToAttachedBody();

		bool IsAttached() const { return attachedBody != nullptr; }
		physics::Rigidbody *GetAttachedBody() const { return attachedBody; }
		

		// ── Force application ────────────────────────────────────────
		// Applies force (and torque from off-centre mount) to attachedBody.
		// Call AFTER SyncToAttachedBody(), AFTER isActiveThisFrame is set.

		float GetAngleDegrees() const;
		void SetAngleDegrees(float angleDegs);

		float GetForceMagnitude() const { return magnitude; }
		void SetForceMagnitude(float magnitudeN) { magnitude = magnitudeN; }

		// ── Configuration ────────────────────────────────────────────
		
		float angleDegrees;			// user-facing angle in degrees (0=right, 90=up)
		float angleOffsetDegrees = 90.0f; // visual-only angle offset for flame rendering (degrees)
		float magnitude = 500.0f;	// Newtons
		bool enabled = false;		// master on/off
		bool bodyRelative = true;	// angle relative to attached body?
		bool keyHold = false;		// true = only fire while key is held
		int fireKey = GLFW_KEY_SPACE;
		bool accelerationMode = false;

		// Set by Engine each tick based on keyHold + key state
		bool isActiveThisFrame = false;

		// Visual colours
		std::array<float, 4> bodyColor{0.45f, 0.55f, 0.70f, 1.00f};	 // steel blue
		std::array<float, 4> flameColor{0.95f, 0.55f, 0.10f, 1.00f}; // amber
		math::Vec2 mountLocalOffset{0.0f, 0.0f};
		float mountLocalAngle = 0.0f; // radians
									  // ── Attachment state ────────────────────────────────────────
		physics::Rigidbody *attachedBody = nullptr;

		physics::ForceGenerator* thrustGenerator = nullptr;

		// ── Geometry helpers ─────────────────────────────────────────
		// Returns the 8 base vertices rotated by this->rotation.
		// Order: [0-3] mounting bracket, [4-7] nozzle bell.
		std::vector<math::Vec2> GetRotatedVertices() const;

	};

} // namespace shape