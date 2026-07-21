#include "shape/Cannon.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"

#include <algorithm>
#include <cmath>

// ================================================================
//  ANONYMOUS-NAMESPACE GEOMETRY HELPERS
//  All shapes are built as flat triangle lists (each 3 Vec2s = one
//  filled triangle).  No index buffers — straightforward to upload.
// ================================================================
namespace
{
    // ── Color utilities ──────────────────────────────────────────────

    std::array<float, 4> DarkenColor(const std::array<float, 4>& c, float s)
    {
        return {c[0] * s, c[1] * s, c[2] * s, c[3]};
    }

    std::array<float, 4> LightenColor(const std::array<float, 4>& c, float s)
    {
        return {std::min(c[0] * s, 1.0f),
                std::min(c[1] * s, 1.0f),
                std::min(c[2] * s, 1.0f),
                c[3]};
    }

    // ── Basic rotation helper ────────────────────────────────────────

    inline math::Vec2 Rot(float x, float y, float cosA, float sinA)
    {
        return {x * cosA - y * sinA, x * sinA + y * cosA};
    }

    // ── Wheel geometry ───────────────────────────────────────────────

    // Annulus (hollow ring) as a triangle strip between innerR and outerR.
    // Each angular step produces one quad (2 triangles).
    std::vector<math::Vec2> BuildAnnulusVertices(float innerR, float outerR, int steps)
    {
        std::vector<math::Vec2> v;
        v.reserve(steps * 6);
        const float dA = (math::PI * 2.0f) / steps;
        for (int i = 0; i < steps; ++i)
        {
            const float a0 = dA * i, a1 = dA * (i + 1);
            math::Vec2 o0(outerR * std::cos(a0), outerR * std::sin(a0));
            math::Vec2 o1(outerR * std::cos(a1), outerR * std::sin(a1));
            math::Vec2 i0(innerR * std::cos(a0), innerR * std::sin(a0));
            math::Vec2 i1(innerR * std::cos(a1), innerR * std::sin(a1));
            v.push_back(i0); v.push_back(o0); v.push_back(o1);
            v.push_back(i0); v.push_back(o1); v.push_back(i1);
        }
        return v;
    }

    // N thin rectangular spokes radiating outward from innerR to outerR.
    // Each spoke is a rectangle of half-width hw perpendicular to its axis.
    std::vector<math::Vec2> BuildSpokeVertices(int n, float innerR, float outerR, float hw)
    {
        std::vector<math::Vec2> v;
        v.reserve(n * 6);
        const float dA = (math::PI * 2.0f) / n;
        for (int i = 0; i < n; ++i)
        {
            const float a  = dA * i;
            const float ca = std::cos(a), sa = std::sin(a);
            const float cp = -sa, sp = ca; // unit perpendicular
            math::Vec2 p0(innerR * ca + hw * cp, innerR * sa + hw * sp);
            math::Vec2 p1(innerR * ca - hw * cp, innerR * sa - hw * sp);
            math::Vec2 p2(outerR * ca + hw * cp, outerR * sa + hw * sp);
            math::Vec2 p3(outerR * ca - hw * cp, outerR * sa - hw * sp);
            // 2 triangles forming the spoke rectangle
            v.push_back(p0); v.push_back(p2); v.push_back(p3);
            v.push_back(p0); v.push_back(p3); v.push_back(p1);
        }
        return v;
    }

    // Solid filled circle fan centered at origin (hub, bore face, etc.)
    std::vector<math::Vec2> BuildCircleVertices(float r, int steps)
    {
        std::vector<math::Vec2> v;
        v.reserve(steps * 3);
        const float dA = (math::PI * 2.0f) / steps;
        float px = r, py = 0.0f;
        for (int i = 1; i <= steps; ++i)
        {
            const float a  = dA * i;
            const float nx = r * std::cos(a), ny = r * std::sin(a);
            v.push_back({0.0f, 0.0f});
            v.push_back({px, py});
            v.push_back({nx, ny});
            px = nx; py = ny;
        }
        return v;
    }

    // ── Barrel geometry ──────────────────────────────────────────────

    // Axis-aligned rectangle [x0..x1] × [±hH], rotated by (cosA, sinA).
    std::vector<math::Vec2> BuildRotRect(float x0, float x1, float hH,
                                          float cosA, float sinA)
    {
        math::Vec2 p0 = Rot(x0, -hH, cosA, sinA);
        math::Vec2 p1 = Rot(x1, -hH, cosA, sinA);
        math::Vec2 p2 = Rot(x1,  hH, cosA, sinA);
        math::Vec2 p3 = Rot(x0,  hH, cosA, sinA);
        return {p0, p1, p2, p0, p2, p3};
    }

    // Tapered trapezoid: half-height hH0 at x0, hH1 at x1, rotated.
    // Produces the taper that makes the barrel look like a real cannon tube.
    std::vector<math::Vec2> BuildRotTrap(float x0, float hH0,
                                          float x1, float hH1,
                                          float cosA, float sinA)
    {
        math::Vec2 p0 = Rot(x0, -hH0, cosA, sinA);
        math::Vec2 p1 = Rot(x1, -hH1, cosA, sinA);
        math::Vec2 p2 = Rot(x1,  hH1, cosA, sinA);
        math::Vec2 p3 = Rot(x0,  hH0, cosA, sinA);
        return {p0, p1, p2, p0, p2, p3};
    }

    // Solid circle centered at (cx, cy) in local (pre-rotation) space,
    // every vertex rotated by (cosA, sinA).
    // Used for: bore opening at muzzle tip, cascabel knob at breech rear.
    std::vector<math::Vec2> BuildOffsetCircle(float r, int steps,
                                               float cx, float cy,
                                               float cosA, float sinA)
    {
        std::vector<math::Vec2> v;
        v.reserve(steps * 3);
        const float dA = (math::PI * 2.0f) / steps;
        float px = cx + r, py = cy;
        for (int i = 1; i <= steps; ++i)
        {
            const float a  = dA * i;
            const float nx = cx + r * std::cos(a), ny = cy + r * std::sin(a);
            v.push_back(Rot(cx, cy, cosA, sinA));
            v.push_back(Rot(px, py, cosA, sinA));
            v.push_back(Rot(nx, ny, cosA, sinA));
            px = nx; py = ny;
        }
        return v;
    }

    // ── Conversion / append utilities ────────────────────────────────

    std::vector<float> ToFloats(const std::vector<math::Vec2>& v)
    {
        std::vector<float> out;
        out.reserve(v.size() * 2);
        for (const auto& p : v) { out.push_back(p.x); out.push_back(p.y); }
        return out;
    }

    void Append(std::vector<math::Vec2>& dst, const std::vector<math::Vec2>& src)
    {
        dst.insert(dst.end(), src.begin(), src.end());
    }

} // anonymous namespace


// ================================================================
//  CANNON IMPLEMENTATION
// ================================================================
namespace shape
{

Cannon::Cannon(const math::Vec2& pos,
               float angle,
               const std::array<float, 4>& colorValue)
    : Shape(pos, math::Vec2(), math::Vec2(), 1.0f, 0.0f,
            physics::RigidbodyType::Static),
      barrelAngleDegrees(angle),
      color(colorValue)
{
    // ── Barrel color family ──────────────────────────────────────
    barrelColor      = colorValue;                     // base metal tone
    breechColor      = DarkenColor(colorValue, 0.78f); // heavier, darker rear block
    bandColor        = LightenColor(colorValue, 1.18f);// raised ring catches more light
    muzzleRingColor  = LightenColor(colorValue, 1.22f);// muzzle lip brightest highlight
    // boreColor stays near-black (set in header)

    // ── Wheel color family ───────────────────────────────────────
    wheelColor       = DarkenColor(colorValue, 0.60f); // dark outer rim
    spokesColor      = DarkenColor(colorValue, 0.52f); // slightly darker spokes
    hubColor         = DarkenColor(colorValue, 0.70f); // medium-dark hub

    // carriageColor is always wood-brown (set in header)

    GenerateVertices();
    verticesSize = static_cast<int>(vertices.size());
    UpdateMassProperties();
}

// ================================================================
//  GenerateVertices — builds the combined shape used for AABB /
//  hit-testing (not for rendering).  Wheel outer circle + barrel
//  trapezoid is sufficient for a tight bounding approximation.
// ================================================================
void Cannon::GenerateVertices() const
{
    vertices.clear();

    const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);

    // Wheel outer rim (annulus) for bounding
    Append(vertices, BuildAnnulusVertices(wheelRadius * 0.85f, wheelRadius, steps));

    // Barrel body trapezoid for bounding
    Append(vertices, BuildRotTrap(
        0.0f,         barrelWidth * 0.45f,
        barrelLength, barrelWidth * 0.29f,
        ca, sa));
}

float Cannon::ComputeInertia() const { return 0.0f; }

// ================================================================
//  WHEEL SUB-PARTS
// ================================================================

std::vector<float> Cannon::GetWheelVertexLocalPos() const
{
    return GetWheelRimVertexLocalPos();
}

// Outer rim: annulus from 85 % to 100 % of wheelRadius.
// The rim thickness (15 % of radius) gives a solid, weighty look.
std::vector<float> Cannon::GetWheelRimVertexLocalPos() const
{
    return ToFloats(BuildAnnulusVertices(wheelRadius * 0.85f, wheelRadius, steps));
}

// Eight evenly-spaced spokes.
// They run from the hub edge (18 %) to just inside the rim (85 %).
// Spoke half-width = 6.5 % of radius → visually readable at all zoom levels.
std::vector<float> Cannon::GetWheelSpokesVertexLocalPos() const
{
    return ToFloats(BuildSpokeVertices(
        8,
        wheelRadius * 0.18f,   // inner (hub edge)
        wheelRadius * 0.85f,   // outer (rim inner edge)
        wheelRadius * 0.065f   // half-width per spoke
    ));
}

// Small solid hub disc at the axle center.
// Radius = 18 % of wheelRadius so it neatly caps the spoke roots.
std::vector<float> Cannon::GetWheelHubVertexLocalPos() const
{
    return ToFloats(BuildCircleVertices(wheelRadius * 0.18f, steps / 2));
}

// ================================================================
//  CARRIAGE (STATIC — does NOT rotate with barrel)
// ================================================================
//
//  A simplified side-view wooden gun carriage trail.  It is drawn
//  as a horizontal trapezoid that extends forward (positive X) a
//  short way and backward (negative X) further as the "trail".
//
//  Because it is always horizontal it stays readable at all barrel
//  angles, trading a little realism for visual clarity.
// ================================================================
std::vector<float> Cannon::GetCarriageVertexLocalPos() const
{
    const float frontX  =  barrelLength * 0.14f;  //  short forward stub
    const float backX   = -wheelRadius  * 1.28f;  //  long trail backward
    const float hFront  =  wheelRadius  * 0.26f;  //  half-height at front
    const float hBack   =  wheelRadius  * 0.14f;  //  narrows toward trail tip

    // Trapezoid: wider at the wheel end, tapering toward the trail
    math::Vec2 p0(frontX, -hFront);
    math::Vec2 p1(frontX,  hFront);
    math::Vec2 p2(backX,   hBack);
    math::Vec2 p3(backX,  -hBack);

    return {
        p0.x, p0.y, p1.x, p1.y, p2.x, p2.y,
        p0.x, p0.y, p2.x, p2.y, p3.x, p3.y
    };
}

// ================================================================
//  BARREL SUB-PARTS  (all rotated by barrelAngleDegrees)
// ================================================================

std::vector<float> Cannon::GetBarrelVertexLocalPos() const
{
    return GetBarrelBodyVertexLocalPos();
}

// Main tube: tapered trapezoid, wider at the breech end, narrowing
// toward the muzzle — the defining silhouette of a cannon barrel.
std::vector<float> Cannon::GetBarrelBodyVertexLocalPos() const
{
    const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);

    return ToFloats(BuildRotTrap(
        0.0f,          barrelWidth * 0.44f,  // breech end: widest
        barrelLength,  barrelWidth * 0.29f,  // muzzle end: narrowest
        ca, sa));
}

// Breech block: the wider, heavier rear section of the cannon.
// Extends slightly behind the wheel axle (negative local X) so
// the cascabel knob peeks out from behind the wheel.
// The cascabel (small round knob at the very back) is appended as
// an offset circle — it completes the classic cannon silhouette.
std::vector<float> Cannon::GetBreechVertexLocalPos() const
{
    const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);

    const float backX  = -barrelWidth * 0.78f;  // rear face of breech block
    const float frontX =  barrelWidth * 0.62f;  // front face (overlaps main tube start)
    const float hH     =  barrelWidth * 0.57f;  // half-height: visibly wider than tube

    // Rectangular breech block
    auto block = BuildRotRect(backX, frontX, hH, ca, sa);

    // Cascabel knob: small circle at the very back of the breech.
    // In historical cannons this was used for attaching ropes / absorbing recoil.
    const float knobR = barrelWidth * 0.22f;
    auto knob  = BuildOffsetCircle(knobR, steps / 2, backX, 0.0f, ca, sa);

    std::vector<math::Vec2> combined;
    combined.reserve(block.size() + knob.size());
    Append(combined, block);
    Append(combined, knob);
    return ToFloats(combined);
}

// Reinforcing band: a slightly raised ring at ~45 % along the barrel.
// In real cannons these were cast strengthening rings; here they add
// visible layering that breaks up the plain tube silhouette.
std::vector<float> Cannon::GetBarrelBandVertexLocalPos() const
{
    const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);

    const float centre = barrelLength * 0.44f;
    const float half   = barrelWidth  * 0.17f; // band axial half-width
    const float hH     = barrelWidth  * 0.47f; // slightly taller than tube at same point

    return ToFloats(BuildRotRect(centre - half, centre + half, hH, ca, sa));
}

// Muzzle ring: a raised lip at the very front of the barrel.
// It is slightly taller than the tube end and creates a strong
// visual "cap" that tells the viewer where the projectile exits.
std::vector<float> Cannon::GetMuzzleRingVertexLocalPos() const
{
    const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);

    const float ringLen = barrelWidth * 0.45f;
    const float hH      = barrelWidth * 0.38f; // taller than tube end (0.29)

    return ToFloats(BuildRotRect(barrelLength - ringLen, barrelLength, hH, ca, sa));
}

// Bore: the dark circular opening at the muzzle face.
// This is the single most important "read" for the player —
// it marks exactly where projectiles will spawn from and gives
// the barrel convincing depth without any 3-D rendering.
std::vector<float> Cannon::GetBoreVertexLocalPos() const
{
    const float angleRad = (barrelAngleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);

    const float boreR = barrelWidth * 0.24f; // inner bore radius

    return ToFloats(BuildOffsetCircle(boreR, steps / 2,
                                      barrelLength, 0.0f,
                                      ca, sa));
}

// ================================================================
//  COMBINED (Shape interface)
// ================================================================

std::vector<float> Cannon::GetVertexLocalPos() const
{
    GenerateVertices();
    return ToFloats(vertices);
}

std::vector<math::Vec2> Cannon::GetVertexWorldPos() const
{
    GenerateVertices();
    std::vector<math::Vec2> out;
    out.reserve(vertices.size());
    for (const auto& v : vertices)
        out.emplace_back(v.x + pos.x, v.y + pos.y);
    return out;
}

} // namespace shape