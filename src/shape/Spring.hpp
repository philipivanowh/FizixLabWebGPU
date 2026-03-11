#pragma once

#include "shape/Shape.hpp"
#include "physics/Rigidbody.hpp"

#include <array>
#include <vector>
#include <memory>

namespace shape {

// ================================================================
//  Spring
//
//  A STATIC rigidbody (anchored base) with a DYNAMIC plunger.
//  The base plate is fixed at `pos`.  The top plate is a physics-
//  driven plunger: each frame PhysicsUpdate() projects nearby
//  bodies onto the spring axis, computes penetration depth, and
//  applies Hooke's + damping forces — exactly like a real spring.
//
//  Two operating modes coexist:
//
//    PHYSICS mode  (default)
//      PhysicsUpdate() drives compressionFraction from contact.
//      Any dynamic body touching the top plate is pushed back.
//      The spring returns to rest automatically when the body leaves.
//
//    LOCKED mode  (isLocked = true, set by UI ARM button)
//      compressionFraction is frozen at its current value.
//      PhysicsUpdate() still applies the stored force to any
//      body inside the contact zone but does not change compression.
//      Call Release() to fire: clears the lock, zero compression,
//      and emits one large impulse applied directly to contactBody.
//
//  State diagram:
//    IDLE <──contact──> COMPRESSING ──(ARM)──> LOCKED ──(RELEASE)──> IDLE
// ================================================================
class Spring : public Shape {
public:
    Spring(const math::Vec2&          pos,
           float                      angleDegrees,
           float                      restLength,
           float                      stiffness,
           const std::array<float,4>& color);
    ~Spring() override = default;

    // ── Dynamic physics ───────────────────────────────────────────
    // Call once per physics tick from Engine::Update().
    // dt is in seconds, already scaled by timeScale.
    void PhysicsUpdate(float dt,
                       const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies);

    // ── Manual interface (UI) ─────────────────────────────────────
    void  SetCompression(float fraction); // sets isLocked = true
    float GetCompression() const { return compressionFraction; }
    void  Lock()   { isLocked = true;  isLoaded = (compressionFraction > 0.001f); }
    void  Unlock() { isLocked = false; isLoaded = false; }

    // Release: fires stored energy, applies impulse to contactBody.
    // Returns impulse magnitude in pixel-space for Engine to use on
    // any OTHER nearby body (e.g. after a manual ARM+RELEASE).
    float Release();

    // ── Accessors ─────────────────────────────────────────────────
    math::Vec2          GetTipWorldPos()  const;
    math::Vec2          GetTipDirection() const;
    physics::Rigidbody* GetContactBody()  const { return contactBody; }

    // ── Geometry (Shape interface) ────────────────────────────────
    void GenerateVertices() const;
    float ComputeInertia() const override;
    std::vector<float>       GetVertexLocalPos()  const override;
    std::vector<math::Vec2>  GetVertexWorldPos()  const override;

    // ── Sub-part geometry for layered rendering ───────────────────
    std::vector<float> GetBasePlateVertexLocalPos()  const;
    std::vector<float> GetTopPlateVertexLocalPos()   const;
    std::vector<float> GetCoilBodyVertexLocalPos()   const;
    std::vector<float> GetGuideBodyVertexLocalPos()  const;

    // ── Tunable parameters ────────────────────────────────────────
    float angleDegrees        =  90.0f;  // 0=right, 90=up, 180=left, 270=down
    float restLength          = 120.0f;  // natural length (pixels)
    float stiffness           = 500.0f;  // k  (N/m)
    float damping             =   8.0f;  // b  (N·s/m) — resists approach velocity
    float compressionFraction =   0.0f;  // current state [0 .. maxCompression]
    float maxCompression      =   0.85f;
    float plateWidth          =  12.0f;
    float plateHalfHeight     =  22.0f;  // also the contact half-height
    float coilRadius          =  13.0f;
    float wireRadius          =   2.2f;
    int   coilCount           =   6;
    int   verticesSize        =   0;

    // ── State ─────────────────────────────────────────────────────
    bool isLoaded = false; // compressionFraction > 0 AND isLocked
    bool isLocked = false; // when true, PhysicsUpdate won't change compression

    // ── Colors ────────────────────────────────────────────────────
    std::array<float,4> color       {0.62f, 0.68f, 0.78f, 1.0f};
    std::array<float,4> plateColor  {0.42f, 0.48f, 0.58f, 1.0f};
    std::array<float,4> loadedColor {0.90f, 0.60f, 0.15f, 1.0f};
    std::array<float,4> guideColor  {0.25f, 0.28f, 0.35f, 0.40f};

private:
    float GetCurrentLength() const;

    // Raw pointer into World — never owns. Nulled when body leaves zone.
    physics::Rigidbody* contactBody      = nullptr;
    float               contactVelOnAxis = 0.0f;
};

} // namespace shape