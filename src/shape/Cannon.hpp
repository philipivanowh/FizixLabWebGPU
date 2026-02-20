#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape {

class Cannon : public Shape {
public:
    Cannon(const math::Vec2& pos,
           float angle,
           const std::array<float, 4>& color);
    ~Cannon() override = default;

    // ── Geometry generation ──────────────────────────────────────
    void GenerateVertices() const;

    float ComputeInertia() const override;

    // Combined (for AABB / selection hit-testing)
    std::vector<float>       GetVertexLocalPos()  const override;
    std::vector<math::Vec2>  GetVertexWorldPos()  const override;

    // ── Wheel sub-parts (local-space, centered at origin) ────────
    std::vector<float> GetWheelVertexLocalPos()       const; // legacy → rim
    std::vector<float> GetWheelRimVertexLocalPos()    const; // outer annulus ring
    std::vector<float> GetWheelSpokesVertexLocalPos() const; // 8 thin spokes
    std::vector<float> GetWheelHubVertexLocalPos()    const; // small center disc

    // ── Carriage (static, does NOT rotate with barrel) ───────────
    std::vector<float> GetCarriageVertexLocalPos() const;

    // ── Barrel sub-parts (all rotated by barrelAngleDegrees) ─────
    std::vector<float> GetBarrelVertexLocalPos()       const; // legacy → body
    std::vector<float> GetBarrelBodyVertexLocalPos()   const; // tapered main tube
    std::vector<float> GetBreechVertexLocalPos()       const; // wide rear block + cascabel
    std::vector<float> GetBarrelBandVertexLocalPos()   const; // reinforcing band ~45% length
    std::vector<float> GetMuzzleRingVertexLocalPos()   const; // raised lip at muzzle
    std::vector<float> GetBoreVertexLocalPos()         const; // dark bore at muzzle face

    // ── Tunable parameters ───────────────────────────────────────
    float wheelRadius        = 50.0f;
    int   steps              = 40;
    float barrelAngleDegrees = 0.0f;
    float barrelLength       = 80.0f;
    float barrelWidth        = 30.0f;
    int   verticesSize       = 0;

    // ── Colors ───────────────────────────────────────────────────
    std::array<float, 4> color           {0.0f, 0.0f, 0.0f, 1.0f};
    // Carriage is always wood-brown, independent of user color
    std::array<float, 4> carriageColor   {0.40f, 0.26f, 0.11f, 1.0f};
    // Barrel parts (derived from base color)
    std::array<float, 4> barrelColor     {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> breechColor     {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> bandColor       {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> muzzleRingColor {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> boreColor       {0.04f, 0.04f, 0.06f, 1.0f};
    // Wheel parts (derived from base color)
    std::array<float, 4> wheelColor      {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> spokesColor     {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 4> hubColor        {0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace shape