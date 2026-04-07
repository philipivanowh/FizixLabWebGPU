#include "shape/Spring.hpp"
#include "collision/Collisions.hpp"
#include "common/settings.hpp"

#include <cmath>
#include <algorithm>

namespace
{
    inline math::Vec2 Rot(float x, float y, float ca, float sa)
    {
        return {x * ca - y * sa, x * sa + y * ca};
    }

    std::vector<math::Vec2> BuildRotRect(float x0, float x1, float hH,
                                          float ca, float sa)
    {
        math::Vec2 p0 = Rot(x0, -hH, ca, sa);
        math::Vec2 p1 = Rot(x1, -hH, ca, sa);
        math::Vec2 p2 = Rot(x1,  hH, ca, sa);
        math::Vec2 p3 = Rot(x0,  hH, ca, sa);
        return {p0, p1, p2,  p0, p2, p3};
    }

    void AppendWireSegment(std::vector<math::Vec2>& v,
                            math::Vec2 localA, math::Vec2 localB,
                            float hw, float ca, float sa)
    {
        math::Vec2 rA = Rot(localA.x, localA.y, ca, sa);
        math::Vec2 rB = Rot(localB.x, localB.y, ca, sa);
        const float dx  = rB.x - rA.x;
        const float dy  = rB.y - rA.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001f) return;
        const float nx = -dy / len * hw;
        const float ny =  dx / len * hw;
        v.push_back({rA.x + nx, rA.y + ny});
        v.push_back({rB.x + nx, rB.y + ny});
        v.push_back({rB.x - nx, rB.y - ny});
        v.push_back({rA.x + nx, rA.y + ny});
        v.push_back({rB.x - nx, rB.y - ny});
        v.push_back({rA.x - nx, rA.y - ny});
    }

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

} 



namespace shape
{

Spring::Spring(const math::Vec2&          p,
               float                      angleDeg,
               float                      restLen,
               float                      k,
               const std::array<float,4>& colorValue)
    : Shape(p, math::Vec2(), math::Vec2(), 100.0f, 0.0f,
            physics::RigidbodyType::Dynamic),
      angleDegrees(angleDeg),
      restLength(restLen),
      stiffness(k),
      color(colorValue)
{

    shapeType = ShapeType::Spring;
    plateColor = {
        colorValue[0] * 0.68f,
        colorValue[1] * 0.68f,
        colorValue[2] * 0.68f,
        colorValue[3]
    };
    GenerateVertices();
    verticesSize = static_cast<int>(vertices.size());
    UpdateMassProperties();
}

// ── Internal ─────────────────────────────────────────────────────

float Spring::GetCurrentLength() const
{
    return restLength * (1.0f - compressionFraction);
}

math::Vec2 Spring::GetTipDirection() const
{
    const float rad = (angleDegrees * math::PI / 180.0f) + rotation;
    return { std::cos(rad), std::sin(rad) };
}

math::Vec2 Spring::GetTipWorldPos() const
{
    const math::Vec2 dir = GetTipDirection();
    const float len      = GetCurrentLength();
    return { pos.x + dir.x * len, pos.y + dir.y * len };
}

// ================================================================
//  PhysicsUpdate
//
//  Per-frame spring physics.  Steps:
//
//  1. SCAN — find the dynamic body with the deepest penetration
//     into the plunger contact zone.  Contact zone is a rectangle
//     along the spring axis:
//       • Along axis  : from (restLength - plateWidth*2) to restLength
//       • Perpendicular: ±(plateHalfHeight + bodyRadius)
//
//  2. COMPRESS — if not locked, set compressionFraction from
//     penetration depth.  If no body is in contact, decay the
//     compression exponentially back to zero (spring returns).
//
//  3. PUSH — apply Hooke's force + damping as a velocity impulse:
//       F_spring  =  k * x                (pushes body away)
//       F_damping = -b * min(v_approach, 0) (resists compression only)
//       Δv = (F_spring + F_damping) * dt / mass
//
//  Units: stiffness in N/m, restLength in pixels, dt in seconds.
//  Pixel↔metre conversion uses SimulationConstants::PIXELS_PER_METER.
// ================================================================
void Spring::PhysicsUpdate(float dt,
                            const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies)
{
    if (dt <= 0.0f) return;

    const float PPM      = SimulationConstants::PIXELS_PER_METER;
    const math::Vec2 dir = GetTipDirection();

    // ── 1. Find the best (deepest) contact body ───────────────────
    physics::Rigidbody* best    = nullptr;
    float               bestPen = 0.0f; // pixels of extra compression along spring axis

    for (const auto& rb : bodies)
    {
        if (rb->bodyType != physics::RigidbodyType::Dynamic) continue;
        if (rb.get() == static_cast<physics::Rigidbody*>(this)) continue;

        const collision::HitInfo hit = collision::Collide(
            *static_cast<const physics::Rigidbody*>(this), *rb);
        if (!hit.result) continue;

        // hit.normal points from spring -> body.
        const float normalAlongAxis = math::Vec2::Dot(hit.normal, dir);
        if (normalAlongAxis < 0.5f) continue;

        // Convert MTV depth (along normal) into an approximate depth along spring axis.
        const float pen = hit.depth * normalAlongAxis;
        if (pen <= 0.0f) continue;

        if (pen > bestPen)
        {
            bestPen = pen;
            best    = rb.get();
        }
    }


    contactBody = best;

    // ── 2. Update compression fraction ───────────────────────────
    if (!isLocked)
    {
        if (best)
        {
            // Drive compression directly from penetration
            compressionFraction = std::min(
                compressionFraction + (bestPen / restLength), maxCompression);
        }
        else
        {
            // No contact — spring rebounds toward rest length.
            // Exponential decay at 12 s⁻¹ feels crisp without overshooting.
            const float k = 12.0f;
            compressionFraction = std::max(
                0.0f, compressionFraction - k * compressionFraction * dt);
        }
        isLoaded = false; // isLoaded is a locked-mode concept
    }

    // ── 3. Apply spring + damping force to contact body ──────────
    if (best)
    {
        // Compression in metres
        const float xMeters = (restLength * compressionFraction) / PPM;

        // Relative velocity along spring axis (m/s, negative = approaching)
        const float vAlongAxis =
            ((best->linearVel.x - linearVel.x) * dir.x +
             (best->linearVel.y - linearVel.y) * dir.y) / PPM;
        contactVelOnAxis = vAlongAxis;

        // Hooke: push body away from base along axis
        const float Fspring = stiffness * xMeters;

        // Damping: only resist approach, never pull the body in
        const float Fdamp = -damping * std::min(vAlongAxis, 0.0f);

        const float Ftotal = Fspring + Fdamp; // Newtons

        // Impulse → velocity change (semi-implicit Euler)
        if (best->invMass > 0.0f)
        {
            const float impulseMag = (Ftotal * dt * PPM);
            const math::Vec2 impulse(dir.x * impulseMag, dir.y * impulseMag);

            best->linearVel += impulse * best->invMass;

            auto [contact1, contact2, contactCount] =
                collision::FindContactPoints(*this, *best);
            math::Vec2 contactPoint = contact1;
            if (contactCount >= 2)
                contactPoint = (contact1 + contact2) * 0.5f;

            const math::Vec2 r = contactPoint - best->pos;
            best->angularVel += best->invInertia * math::Vec2::Cross(r, impulse);
        }
    }
    else
    {
        contactVelOnAxis = 0.0f;
    }

    // Rebuild bounding geometry with new compression state
    GenerateVertices();
}

// ── Manual interface ──────────────────────────────────────────────

void Spring::SetCompression(float fraction)
{
    compressionFraction = std::max(0.0f, std::min(fraction, maxCompression));
    isLocked = true;
    isLoaded = (compressionFraction > 0.001f);
    GenerateVertices();
}

float Spring::Release()
{
    if (!isLocked && compressionFraction < 0.001f) return 0.0f;

    const float PPM               = SimulationConstants::PIXELS_PER_METER;
    const float compressionMeters = (restLength * compressionFraction) / PPM;
    const float impulsePixels     = stiffness * compressionMeters * PPM;

    // Apply directly to the body currently in contact (if any)
    if (contactBody && contactBody->invMass > 0.0f)
    {
        const math::Vec2 dir = GetTipDirection();
        const math::Vec2 impulse(dir.x * impulsePixels, dir.y * impulsePixels);

        contactBody->linearVel += impulse * contactBody->invMass;

        auto [contact1, contact2, contactCount] =
            collision::FindContactPoints(*this, *contactBody);
        math::Vec2 contactPoint = contact1;
        if (contactCount >= 2)
            contactPoint = (contact1 + contact2) * 0.5f;

        const math::Vec2 r = contactPoint - contactBody->pos;
        contactBody->angularVel += contactBody->invInertia * math::Vec2::Cross(r, impulse);
    }

    isLocked            = false;
    isLoaded            = false;
    compressionFraction = 0.0f;
    GenerateVertices();

    return impulsePixels; // caller may use this for bodies NOT currently in contact
}

//  GEOMETRY

void Spring::GenerateVertices() const
{
    vertices.clear();
    const float angleRad = (angleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);
    const float len = GetCurrentLength();
    Append(vertices, BuildRotRect(0.0f, len, plateHalfHeight, ca, sa));
    aabbUpdateRequired = true;
}

float Spring::ComputeInertia() const { return 0.0f; }

std::vector<float> Spring::GetBasePlateVertexLocalPos() const
{
    const float rad = (angleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(rad), sa = std::sin(rad);
    return ToFloats(BuildRotRect(0.0f, plateWidth, plateHalfHeight, ca, sa));
}

std::vector<float> Spring::GetTopPlateVertexLocalPos() const
{
    const float rad = (angleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(rad), sa = std::sin(rad);
    const float len = GetCurrentLength();
    return ToFloats(BuildRotRect(len - plateWidth, len, plateHalfHeight, ca, sa));
}

std::vector<float> Spring::GetCoilBodyVertexLocalPos() const
{
    const float rad = (angleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(rad), sa = std::sin(rad);
    const float len         = GetCurrentLength();
    const float activeStart = plateWidth;
    const float activeEnd   = len - plateWidth;
    const float activeLen   = activeEnd - activeStart;
    if (activeLen < 0.001f) return {};

    const int   halfSteps = coilCount * 4;
    const float step      = activeLen / halfSteps;

    std::vector<math::Vec2> pts;
    pts.reserve(halfSteps + 1);
    for (int i = 0; i <= halfSteps; i++)
    {
        const float x = activeStart + i * step;
        float y = 0.0f;
        switch (i % 4)
        {
            case 1: y = +coilRadius; break;
            case 3: y = -coilRadius; break;
            default: y = 0.0f;      break;
        }
        pts.push_back({x, y});
    }

    std::vector<math::Vec2> result;
    result.reserve(halfSteps * 6);
    for (int i = 0; i < halfSteps; i++)
        AppendWireSegment(result, pts[i], pts[i + 1], wireRadius, ca, sa);

    AppendWireSegment(result,
        {activeStart - plateWidth * 0.45f, 0.0f}, {activeStart, 0.0f},
        wireRadius, ca, sa);
    AppendWireSegment(result,
        {activeEnd, 0.0f}, {activeEnd + plateWidth * 0.45f, 0.0f},
        wireRadius, ca, sa);

    return ToFloats(result);
}

std::vector<float> Spring::GetGuideBodyVertexLocalPos() const
{
    if (compressionFraction < 0.005f) return {};
    const float rad = (angleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(rad), sa = std::sin(rad);
    const float curLen  = GetCurrentLength();
    const float restLen = restLength;
    std::vector<math::Vec2> guide;
    AppendWireSegment(guide,
        {curLen, 0.0f}, {restLen, 0.0f}, 0.8f, ca, sa);
    Append(guide, BuildRotRect(restLen - plateWidth, restLen,
                               plateHalfHeight * 0.25f, ca, sa));
    return ToFloats(guide);
}

std::vector<float> Spring::GetVertexLocalPos() const
{
    GenerateVertices();
    return ToFloats(vertices);
}

std::vector<math::Vec2> Spring::GetVertexWorldPos() const
{
    const float angleRad = (angleDegrees * math::PI / 180.0f) + rotation;
    const float ca = std::cos(angleRad), sa = std::sin(angleRad);
    const float len = GetCurrentLength();
    const float hH = plateHalfHeight;

    // Return a proper convex polygon (4 corners) for collision/SAT.
    // Rendering uses the specialized plate/coil triangle lists.
    const math::Vec2 p0 = Rot(0.0f, -hH, ca, sa);
    const math::Vec2 p1 = Rot(len,  -hH, ca, sa);
    const math::Vec2 p2 = Rot(len,   hH, ca, sa);
    const math::Vec2 p3 = Rot(0.0f,  hH, ca, sa);

    std::vector<math::Vec2> out;
    out.reserve(4);
    out.emplace_back(p0.x + pos.x, p0.y + pos.y);
    out.emplace_back(p1.x + pos.x, p1.y + pos.y);
    out.emplace_back(p2.x + pos.x, p2.y + pos.y);
    out.emplace_back(p3.x + pos.x, p3.y + pos.y);
    return out;
}

} // namespace shape
