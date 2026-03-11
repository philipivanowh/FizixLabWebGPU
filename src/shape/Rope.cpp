#include "shape/Rope.hpp"
#include "shape/Ball.hpp"
#include "collision/Collisions.hpp" // collision::Collide — for node probe tests
#include "math/Math.hpp"
#include "common/settings.hpp"
#include "physics/Rigidbody.hpp"

#include <algorithm>
#include <array>
#include <cmath>

// No World.hpp — zero circular dependency.

namespace shape
{

    // ─────────────────────────────────────────────────────────────────────────────
    // Build
    //
    // Creates only the TWO endpoint Ball Rigidbodies and returns them.
    // Middle nodes are NOT Rigidbodies — they are plain verlet positions created
    // in RegisterEndpoints() once World has Add()-ed the endpoint bodies.
    // ─────────────────────────────────────────────────────────────────────────────
    std::vector<std::unique_ptr<physics::Rigidbody>>
    Rope::Build(const SpawnParams &p)
    {
        // Store params we need later for the probe and damping
        stickIterations_ = p.stickIterations;
        damping_ = p.damping;
        nodeMass_ = p.segmentMass;

        constexpr float NODE_RADIUS_METRES = 0.04f;
        const std::array<float, 4> endpointColor = {0.55f, 0.42f, 0.25f, 1.0f};

        std::vector<std::unique_ptr<physics::Rigidbody>> out;
        out.reserve(2);

        //    const int n = std::max(2, p.segmentCount);

        // ── Start endpoint (index 0) ──────────────────────────────────────────────
        {
            const physics::RigidbodyType t = p.pinStart
                                                 ? physics::RigidbodyType::Static
                                                 : physics::RigidbodyType::Dynamic;
            out.push_back(std::make_unique<Ball>(
                p.startWorld,
                math::Vec2(0.0f, 0.0f),
                math::Vec2(0.0f, 0.0f),
                NODE_RADIUS_METRES,
                endpointColor,
                p.segmentMass,
                0.2f,
                t));
            out.back()->isRopeNode = true;
        }

        // ── End endpoint (index n-1) ──────────────────────────────────────────────
        {
            const physics::RigidbodyType t = p.pinEnd
                                                 ? physics::RigidbodyType::Static
                                                 : physics::RigidbodyType::Dynamic;
            out.push_back(std::make_unique<Ball>(
                p.endWorld,
                math::Vec2(0.0f, 0.0f),
                math::Vec2(0.0f, 0.0f),
                NODE_RADIUS_METRES,
                endpointColor,
                p.segmentMass,
                0.2f,
                t));
            out.back()->isRopeNode = true;
        }

        // ── Collision probe — a small reusable Ball for middle-node tests ─────────
        // Its pos is overwritten before every Collide() call; mass/type don't matter.
        nodeProbe_ = std::make_unique<Ball>(
            math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 0.0f),
            NODE_RADIUS_METRES,
            endpointColor,
            p.segmentMass,
            0.0f,
            physics::RigidbodyType::Dynamic);

        return out;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // RegisterEndpoints
    //
    // Called by World::AddRope after it has Add()-ed both endpoint bodies.
    // Fills nodes_ (all n nodes) and constraints_ (n-1 constraints).
    // ─────────────────────────────────────────────────────────────────────────────
    void Rope::RegisterEndpoints(const std::vector<physics::Rigidbody *> &endpointBodies,
                                 const SpawnParams &p)
    {
        nodes_.clear();
        constraints_.clear();
        adjacentEndpointPairs_.clear();

        const int n = std::max(2, p.segmentCount);
        const float invMass = (p.segmentMass > 0.0f) ? 1.0f / p.segmentMass : 0.0f;

        nodes_.reserve(static_cast<size_t>(n));

        // ── Create all n nodes ────────────────────────────────────────────────────
        for (int i = 0; i < n; ++i)
        {
            const float t = (n > 1)
                                ? static_cast<float>(i) / static_cast<float>(n - 1)
                                : 0.0f;

            const math::Vec2 posMetres =
                p.startWorld + (p.endWorld - p.startWorld) * t;

            // Convert to pixels the same way Ball's constructor does
            const math::Vec2 posPx = posMetres * SimulationConstants::PIXELS_PER_METER;

            const bool isStart = (i == 0);
            const bool isEnd = (i == n - 1);

            RopeNode node;
            node.pos = posPx;
            node.prevPos = posPx;
            node.vel = math::Vec2(0.0f, 0.0f);
            node.invMass = invMass;

            if (isStart)
            {
                node.pinned = p.pinStart;
                node.invMass = p.pinStart ? 0.0f : invMass;
                node.body = endpointBodies[0]; // real Rigidbody in World::objects
                node.pos = node.body->pos;     // use actual pixel pos from Ball ctor
                node.prevPos = node.pos;
            }
            else if (isEnd)
            {
                node.pinned = p.pinEnd;
                node.invMass = p.pinEnd ? 0.0f : invMass;
                node.body = endpointBodies[1];
                node.pos = node.body->pos;
                node.prevPos = node.pos;
            }

            nodes_.push_back(node);
        }

        // ── Build constraints ─────────────────────────────────────────────────────
        constraints_.reserve(static_cast<size_t>(n - 1));
        for (int i = 0; i < n - 1; ++i)
        {
            RopeConstraint c;
            c.a = &nodes_[static_cast<size_t>(i)];
            c.b = &nodes_[static_cast<size_t>(i + 1)];
            c.restLength = (c.b->pos - c.a->pos).Length();
            c.stiffness = p.stiffness;
            constraints_.push_back(c);
        }

        // Edge case: with only 2 segments the endpoints ARE directly adjacent —
        // World must skip CollisionSolver for that pair.
        if (n == 2 && endpointBodies.size() == 2)
            adjacentEndpointPairs_.insert(
                MakePair(endpointBodies[0], endpointBodies[1]));
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Solve — Hybrid Verlet + Sticks
    //
    //   Endpoints:    Rigidbody::Update() already ran their Euler integration.
    //                 We read their pos into node.pos, run stick projection, then
    //                 write corrections back via body->Translate().
    //
    //   Middle nodes: We integrate them manually here (explicit Euler with gravity
    //                 and damping) before stick projection, then derive velocity
    //                 from the post-projection displacement.
    // ─────────────────────────────────────────────────────────────────────────────
    void Rope::Solve(float deltaMs, int iterations)
    {
        if (nodes_.empty() || constraints_.empty())
            return;

        const float dt = (deltaMs / 1000.0f) / static_cast<float>(iterations);
        if (dt <= 0.0f)
            return;

        const float gPPM =
            PhysicsConstants::GRAVITY * SimulationConstants::PIXELS_PER_METER;

        // ── Step 0: Sync endpoint node.pos from body->pos ─────────────────────────
        // Rigidbody::Update() has already integrated the endpoint bodies by now.
        // Pulling their positions into node.pos keeps the constraint solver consistent.
        for (auto &node : nodes_)
        {
            if (node.IsEndpoint())
                node.pos = node.body->pos;
        }

        // ── Step 1: Integrate middle nodes ────────────────────────────────────────
        // Explicit Euler with gravity.  Damping is applied to velocity here so the
        // rope loses oscillation energy without the solver needing extra iterations.
        for (auto &node : nodes_)
        {
            if (node.IsEndpoint() || node.pinned)
                continue;

            const math::Vec2 gravity(0.0f, -gPPM);
            node.vel += gravity * dt;
            const float stepDamping = std::pow(damping_, 1.0f / static_cast<float>(iterations));
            node.vel = node.vel * stepDamping;

            node.prevPos = node.pos;
            node.pos += node.vel * dt;
        }

        // Snapshot pre-projection endpoint positions so we can compute the
        // constraint correction to apply to body->linearVel in Step 3.
        std::vector<math::Vec2> endpointPreProjection;
        endpointPreProjection.reserve(nodes_.size());
        for (const auto &node : nodes_)
            endpointPreProjection.push_back(node.pos);

        // ── Step 2: Gauss-Seidel stick projection ────────────────────────────────
        for (int iter = 0; iter < stickIterations_; ++iter)
        {
            for (auto &c : constraints_)
            {
                const math::Vec2 delta = c.b->pos - c.a->pos;
                const float dist = delta.Length();
                if (dist < 1e-6f)
                    continue;

                const float error = dist - c.restLength;
                if (error <= 0.0f)
                {
                    c.tensionMagnitude = 0.0f;
                    continue;
                }

                const math::Vec2 dir = delta * (1.0f / dist);
                c.tensionDir = dir;

                const float wA = c.a->pinned ? 0.0f : c.a->invMass;
                const float wB = c.b->pinned ? 0.0f : c.b->invMass;
                const float totalW = wA + wB;
                if (totalW < 1e-8f)
                    continue;

                const float correctionMag = (error / totalW) * c.stiffness;

                if (!c.a->pinned)
                    c.a->pos += dir * wA * correctionMag;
                if (!c.b->pinned)
                    c.b->pos += dir.Negate() * wB * correctionMag;

                const float reducedMass = 1.0f / totalW;
                c.tensionMagnitude = reducedMass * correctionMag / (dt * dt);
            }
        }

        // ── Step 3: Write corrections back and update velocities ─────────────────
        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            auto &node = nodes_[i];

            if (node.pinned)
                continue;

            if (node.IsEndpoint())
            {
                // Compute the net displacement the stick solver applied to this
                // endpoint's position and forward it to the Rigidbody.
                const math::Vec2 correction = node.pos - endpointPreProjection[i];
                if (correction.Length() > 1e-6f)
                {
                    node.body->Translate(correction);
                    // Apply the constraint impulse as a velocity nudge so the
                    // endpoint body reacts correctly next frame.
                    node.body->linearVel += correction / dt;
                }
                // Keep node.pos in sync after Translate moved body->pos.
                node.pos = node.body->pos;
            }
            else
            {
                // Derive middle-node velocity from actual post-projection movement.
                // This automatically includes both free-flight and constraint corrections.
                if (&node == draggedNode_)
                    node.vel = math::Vec2(0.0f, 0.0f); // mouse controls position; don't derive vel
                else
                {
                    const math::Vec2 displacement = node.pos - node.prevPos;
                    node.vel = displacement / dt;
                }
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // SolveNodeCollisions
    //
    // Thin static-geometry collision loop for middle nodes only.
    // Uses a reusable probe Ball so the existing collision::Collide() path is
    // taken — no new collision code.  Endpoints are handled by the main solver.
    // ─────────────────────────────────────────────────────────────────────────────
    void Rope::SolveNodeCollisions(
        const std::vector<physics::Rigidbody *> &staticBodies)
    {
        if (!nodeProbe_)
            return;

        for (auto &node : nodes_)
        {
            if (node.IsEndpoint() || node.pinned)
                continue;

            // Reposition the probe to match this node
            nodeProbe_->pos = node.pos;
            nodeProbe_->aabbUpdateRequired = true;
            nodeProbe_->transformUpdateRequired = true;

            for (auto *staticBody : staticBodies)
            {
                const collision::HitInfo hit =
                    collision::Collide(*nodeProbe_, *staticBody);

                if (!hit.result)
                    continue;

                // Positional pushout
                node.pos += hit.normal * hit.depth;
                nodeProbe_->pos = node.pos; // keep probe in sync
                nodeProbe_->aabbUpdateRequired = true;

                // Cancel the velocity component into the surface.
                // Velocity is explicit (node.vel), so we just remove the
                // normal component — equivalent to a perfectly inelastic
                // normal response with zero restitution.
                const float velIntoSurface =
                    math::Vec2::Dot(node.vel, hit.normal);
                if (velIntoSurface < 0.0f)
                    node.vel -= hit.normal * velIntoSurface;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // UpdateTensionDisplay
    //
    // Tension arrows are written only to endpoint bodies (they are the only nodes
    // that have a Rigidbody to receive ForceInfo).  The tension at the first
    // constraint acts on the start endpoint; the last constraint acts on the end.
    // ─────────────────────────────────────────────────────────────────────────────
    void Rope::UpdateTensionDisplay()
    {
        // Clear endpoint force lists
        for (auto &node : nodes_)
        {
            if (node.IsEndpoint())
                node.body->ClearForces();
        }

        if (constraints_.empty())
            return;

        // Start endpoint — tension from first constraint
        {
            const auto &c = constraints_.front();
            if (c.tensionMagnitude >= PhysicsConstants::TENSION_DISPLAY_THRESHOLD &&
                !c.a->pinned && c.a->IsEndpoint())
            {
                const math::Vec2 force = c.tensionDir * c.tensionMagnitude;
                c.a->body->AddDisplayForce(force, physics::ForceType::Tension);
            }
        }

        // End endpoint — tension from last constraint
        {
            const auto &c = constraints_.back();
            if (c.tensionMagnitude >= PhysicsConstants::TENSION_DISPLAY_THRESHOLD &&
                !c.b->pinned && c.b->IsEndpoint())
            {
                const math::Vec2 force = (c.tensionDir).Negate() * c.tensionMagnitude;
                c.b->body->AddDisplayForce(force, physics::ForceType::Tension);
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Drag
    // ─────────────────────────────────────────────────────────────────────────────
    RopeNode *Rope::PickNode(const math::Vec2 &worldPos, float radiusPx) const
    {
        RopeNode *closest = nullptr;
        float bestDist = radiusPx;

        for (const auto &node : nodes_)
        {
            const float d = (node.pos - worldPos).Length();
            if (d < bestDist)
            {
                bestDist = d;
                closest = const_cast<RopeNode *>(&node);
            }
        }
        return closest;
    }

    void Rope::DragNode(RopeNode *node, const math::Vec2 &targetWorld)
    {
        draggedNode_ = node;
        if (!node || node->pinned)
            return;

        if (node->IsEndpoint())
        {
            node->body->TranslateTo(targetWorld);
            node->body->linearVel = math::Vec2(0.0f, 0.0f);
            node->pos = targetWorld;
            node->prevPos = targetWorld;
            node->vel = math::Vec2(0.0f, 0.0f);
        }
        else
        {
            node->prevPos = node->pos; // don't let teleport cause velocity spike
            node->pos = targetWorld;
            node->vel = math::Vec2(0.0f, 0.0f);
        }
    }

    void Rope::ReleaseNode()
    {
        draggedNode_ = nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Collision helpers
    // ─────────────────────────────────────────────────────────────────────────────
    bool Rope::ContainsBody(const physics::Rigidbody *b) const
    {
        for (const auto &node : nodes_)
            if (node.body == b)
                return true;
        return false;
    }

    bool Rope::AreAdjacent(const physics::Rigidbody *a,
                           const physics::Rigidbody *b) const
    {
        auto *nca = const_cast<physics::Rigidbody *>(a);
        auto *ncb = const_cast<physics::Rigidbody *>(b);
        return adjacentEndpointPairs_.count(MakePair(nca, ncb)) > 0;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Accessors
    // ─────────────────────────────────────────────────────────────────────────────
    const std::vector<RopeConstraint> &Rope::GetConstraints() const { return constraints_; }
    const std::vector<RopeNode> &Rope::GetNodes() const { return nodes_; }

} // namespace shape