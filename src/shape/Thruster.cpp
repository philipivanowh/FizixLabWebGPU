#include "shape/Thruster.hpp"
#include "collision/AABB.hpp"
#include "common/settings.hpp"
#include "math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shape
{
    // ================================================================
    //  Geometry layout (local space, BEFORE rotation)
    //
    //  All coordinates are in pixels, centered at (0, 0).
    //  Thrust direction = +Y (up).  Exhaust exits from -Y (down).
    //
    //  Y
    //  ^
    //  |   v0────────v1          <- bracket top    y = +H/2
    //  |   │  bracket │
    //  |   v3────────v2          <- bracket bot    y = +H/2 - BRACKET_H
    //  |      v4──v5             <- throat         y = +H/2 - BRACKET_H
    //  |       \    /
    //  |        \  /             <- nozzle bell (flares outward)
    //  |         \/
    //  |   v7────────v6          <- bell exit      y = -H/2
    //  └──────────────────> X
    //
    //  Vertices stored as: [v0 v1 v2 v3] bracket, [v4 v5 v6 v7] nozzle bell
    // ================================================================

    static constexpr float BRACKET_H = Thruster::H * 0.20f;
    static constexpr float BRACKET_HW = Thruster::W * 0.50f;
    static constexpr float THROAT_HW = Thruster::W * 0.16f;
    static constexpr float BELL_HW = Thruster::W * 0.50f;
    static constexpr float BRACKET_TOP = +Thruster::H * 0.50f;
    static constexpr float BRACKET_BOT = BRACKET_TOP - BRACKET_H;
    static constexpr float BELL_BOT = -Thruster::H * 0.50f;

    // ================================================================
    //  ThrustForceGenerator
    //  Lives entirely inside Thruster.cpp — Rigidbody never sees it.
    //  Registered on the attached body via AddForceGenerator(), so it
    //  fires automatically inside Rigidbody::UpdateForces() each tick.
    //  No include of Thruster.hpp is needed by Rigidbody — the only
    //  type that crosses the boundary is ForceGenerator (base class),
    //  which Rigidbody already owns.
    // ================================================================
    class ThrustForceGenerator final : public physics::ForceGenerator
    {
    public:
        explicit ThrustForceGenerator(const Thruster &thruster)
            : thruster(thruster) {}

        void Apply(physics::Rigidbody &body, float /*dt*/) override
        {
            if (!thruster.isActiveThisFrame || !thruster.enabled)
                return;

            // Thrust direction in world space
            float dirRad = thruster.angleDegrees * math::PI / 180.0f;
            if (thruster.bodyRelative)
                dirRad += body.rotation;

            const math::Vec2 thrustDir(std::cos(dirRad), std::sin(dirRad));
            const math::Vec2 force = thrustDir * thruster.magnitude;

            // Accumulate into netForce directly — display force recorded once
            body.netForce += force;
            body.AddDisplayForce(force, physics::ForceType::Applied);

            // Torque from off-centre mount: τ = r × F (2-D cross product)
            const float c = std::cos(body.rotation);
            const float s = std::sin(body.rotation);
            const math::Vec2 r(
                thruster.mountLocalOffset.x * c - thruster.mountLocalOffset.y * s,
                thruster.mountLocalOffset.x * s + thruster.mountLocalOffset.y * c);

            body.netTorque += r.x * force.y - r.y * force.x;
        }

    private:
        const Thruster &thruster; // non-owning ref — Thruster outlives generator
    };

    // ================================================================
    //  Constructor
    // ================================================================
    Thruster::Thruster(const math::Vec2 &pos,
                       float angleDegs,
                       float magnitudeN,
                       bool bodyRelativeMode,
                       bool keyHoldMode,
                       int fireKey_)
        : Shape(pos,
                math::Vec2(0.0f, 0.0f),
                math::Vec2(0.0f, 0.0f),
                1.0f,
                0.0f,
                physics::RigidbodyType::Static),
          magnitude(magnitudeN * SimulationConstants::PIXELS_PER_METER),
          bodyRelative(bodyRelativeMode),
          keyHold(keyHoldMode),
          fireKey(fireKey_)
    {
        angleDegrees = angleDegs;
        rotation = math::DegToRad(angleDegrees-90.0f);

        vertices = {
            // Mounting bracket
            math::Vec2(-BRACKET_HW, BRACKET_TOP), // v0 top-left
            math::Vec2(+BRACKET_HW, BRACKET_TOP), // v1 top-right
            math::Vec2(+BRACKET_HW, BRACKET_BOT), // v2 bottom-right
            math::Vec2(-BRACKET_HW, BRACKET_BOT), // v3 bottom-left
            // Nozzle bell
            math::Vec2(-THROAT_HW, BRACKET_BOT), // v4 throat left
            math::Vec2(+THROAT_HW, BRACKET_BOT), // v5 throat right
            math::Vec2(+BELL_HW, BELL_BOT),      // v6 bell exit right
            math::Vec2(-BELL_HW, BELL_BOT),      // v7 bell exit left
        };
    }

    // ================================================================
    //  ATTACHMENT
    // ================================================================
    void Thruster::AttachTo(physics::Rigidbody *body, math::Vec2 localOffset, float localAngle)
    {
        Detach(); // Clean up old ones

        (void)localOffset; // silence unused warning if not bodyRelative
        attachedBody = body;
        mountLocalOffset = math::Vec2(0.0f,0.0f);
        mountLocalAngle = localAngle;

        // 1. Create the generator as a local unique_ptr (temporary owner)
        auto newGenerator = std::make_unique<ThrustForceGenerator>(*this);

        // 2. Save the memory address as our "ID" (This works now because thrustGenerator is a raw ptr)
        this->thrustGenerator = newGenerator.get();

        // 3. Hand over ownership to the Rigidbody.
        // After this line, newGenerator becomes null, and 'body' owns the memory.
        attachedBody->AddForceGenerator(std::move(newGenerator));

        SyncToAttachedBody();
    }

    void Thruster::Detach()
    {
        if (attachedBody && thrustGenerator)
        {
            attachedBody->RemoveForceGenerator(thrustGenerator);
        }
        attachedBody = nullptr;
        thrustGenerator = nullptr;
    }

    // ── Angle helpers ─────────────────────────────────────────────────
    // SetAngleDegrees is the single write-point for the user-facing angle.
    // It updates both angleDegrees AND the visual rotation so the inspector
    // dial and rendered shape are always in sync.
    void Thruster::SetAngleDegrees(float degs)
    {
        angleDegrees = std::fmod(degs + 360.0f, 360.0f);

        if (attachedBody)
            SyncToAttachedBody(); // recomputes rotation from body + angleDegrees
        else
            rotation = math::DegToRad(angleDegrees-angleOffsetDegrees);
    }

    float Thruster::GetAngleDegrees() const
    {
        return angleDegrees;
    }

    void Thruster::SyncToAttachedBody()
    {
        if (!attachedBody)
            return;

        const float c = std::cos(attachedBody->rotation);
        const float s = std::sin(attachedBody->rotation);

        pos.x = attachedBody->pos.x + mountLocalOffset.x * c - mountLocalOffset.y * s;
        pos.y = attachedBody->pos.y + mountLocalOffset.x * s + mountLocalOffset.y * c;

        if (bodyRelative)
            rotation = attachedBody->rotation + math::DegToRad(angleDegrees) - math::DegToRad(angleOffsetDegrees);
        else
            rotation = math::DegToRad(angleDegrees) - math::DegToRad(angleOffsetDegrees);
    }
    

    // ================================================================
    //  GEOMETRY
    // ================================================================
    std::vector<math::Vec2> Thruster::GetRotatedVertices() const
    {
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);
        std::vector<math::Vec2> out;
        out.reserve(vertices.size());
        for (const auto &v : vertices)
            out.emplace_back(v.x * c - v.y * s,
                             v.x * s + v.y * c);
        return out;
    }

    std::vector<float> Thruster::GetVertexLocalPos() const
    {
        const std::vector<math::Vec2> r = GetRotatedVertices();

        const math::Vec2 &v0 = r[0];
        const math::Vec2 &v1 = r[1];
        const math::Vec2 &v2 = r[2];
        const math::Vec2 &v3 = r[3];
        const math::Vec2 &v4 = r[4];
        const math::Vec2 &v5 = r[5];
        const math::Vec2 &v6 = r[6];
        const math::Vec2 &v7 = r[7];

        return {
            // Bracket triangle 1
            v0.x,
            v0.y,
            v1.x,
            v1.y,
            v2.x,
            v2.y,
            // Bracket triangle 2
            v0.x,
            v0.y,
            v2.x,
            v2.y,
            v3.x,
            v3.y,
            // Bell triangle 1
            v4.x,
            v4.y,
            v5.x,
            v5.y,
            v6.x,
            v6.y,
            // Bell triangle 2
            v4.x,
            v4.y,
            v6.x,
            v6.y,
            v7.x,
            v7.y,
        };
    }

    std::vector<math::Vec2> Thruster::GetVertexWorldPos() const
    {
        const std::vector<math::Vec2> r = GetRotatedVertices();
        std::vector<math::Vec2> out;
        out.reserve(r.size());
        for (const auto &v : r)
            out.emplace_back(v.x + pos.x, v.y + pos.y);
        return out;
    }

    float Thruster::ComputeInertia() const
    {
        return 0.0f;
    }

} // namespace shape