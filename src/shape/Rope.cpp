#include "shape/Rope.hpp"
#include "shape/Ball.hpp"
#include "collision/Collisions.hpp"
#include "math/Math.hpp"
#include "common/settings.hpp"
#include "physics/Rigidbody.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace shape
{

std::vector<std::unique_ptr<physics::Rigidbody>>
Rope::Build(const SpawnParams &p)
{
    stickIterations_ = p.stickIterations;
    damping_ = p.damping;
    nodeMass_ = p.segmentMass;

    constexpr float NODE_RADIUS_METRES = 0.04f;

    const std::array<float, 4> endpointColor =
        {0.55f, 0.42f, 0.25f, 1.0f};

    std::vector<std::unique_ptr<physics::Rigidbody>> out;
    out.reserve(2);

    {
        const physics::RigidbodyType t =
            p.pinStart
                ? physics::RigidbodyType::Static
                : physics::RigidbodyType::Dynamic;

        out.push_back(std::make_unique<Ball>(
            p.startWorld,
            math::Vec2(0,0),
            math::Vec2(0,0),
            NODE_RADIUS_METRES,
            endpointColor,
            p.segmentMass,
            0.2f,
            t));

        out.back()->isRopeNode = true;
    }

    {
        const physics::RigidbodyType t =
            p.pinEnd
                ? physics::RigidbodyType::Static
                : physics::RigidbodyType::Dynamic;

        out.push_back(std::make_unique<Ball>(
            p.endWorld,
            math::Vec2(0,0),
            math::Vec2(0,0),
            NODE_RADIUS_METRES,
            endpointColor,
            p.segmentMass,
            0.2f,
            t));

        out.back()->isRopeNode = true;
    }

    return out;
}

void Rope::RegisterEndpoints(
    const std::vector<physics::Rigidbody *> &endpointBodies,
    const SpawnParams &p)
{
    nodes_.clear();
    constraints_.clear();

    int n = std::max(2, p.segmentCount);

    float invMass =
        (p.segmentMass > 0)
            ? 1.0f / p.segmentMass
            : 0.0f;

    nodes_.reserve(n);

    for (int i = 0; i < n; i++)
    {
        float t = (float)i / (n - 1);

        math::Vec2 pos =
            p.startWorld +
            (p.endWorld - p.startWorld) * t;

        pos *= SimulationConstants::PIXELS_PER_METER;

        RopeNode node;

        node.pos = pos;
        node.prevPos = pos;
        node.vel = math::Vec2(0,0);
        node.invMass = invMass;

        if (i == 0)
        {
            node.body = endpointBodies[0];
            node.pos = node.body->pos;
            node.prevPos = node.pos;
            node.pinned = p.pinStart;
        }

        if (i == n - 1)
        {
            node.body = endpointBodies[1];
            node.pos = node.body->pos;
            node.prevPos = node.pos;
            node.pinned = p.pinEnd;
        }

        nodes_.push_back(node);
    }

    for (int i = 0; i < n - 1; i++)
    {
        RopeConstraint c;

        c.a = &nodes_[i];
        c.b = &nodes_[i + 1];

        c.restLength =
            (c.b->pos - c.a->pos).Length();

        c.stiffness = p.stiffness;

        constraints_.push_back(c);
    }
}

void Rope::Solve(float deltaMs,int iterations)
{
    if (nodes_.empty()) return;

    float dt = deltaMs / 1000.0f / iterations;

    float gPPM =
        PhysicsConstants::GRAVITY *
        SimulationConstants::PIXELS_PER_METER;

    math::Vec2 gravity(0, gPPM);

    for (auto &n : nodes_)
        if (n.IsEndpoint())
            n.pos = n.body->pos;

    for (auto &n : nodes_)
    {
        if (n.IsEndpoint() || n.pinned) continue;

        math::Vec2 vel = n.pos - n.prevPos;

        n.prevPos = n.pos;

        n.pos += vel * damping_;
        n.pos += gravity * dt * dt;
    }

    for (int k = 0;
         k < stickIterations_ * iterations;
         k++)
    {
        for (auto &c : constraints_)
        {
            math::Vec2 delta =
                c.b->pos - c.a->pos;

            float dist = delta.Length();

            if (dist < 1e-6f) continue;

            math::Vec2 dir = delta / dist;

            float error = dist - c.restLength;

            float wA =
                c.a->pinned ? 0 : c.a->invMass;

            float wB =
                c.b->pinned ? 0 : c.b->invMass;

            float total = wA + wB;

            if (total == 0) continue;

            float corr = error * c.stiffness;

            float corrA = corr * (wA / total);
            float corrB = corr * (wB / total);

            if (!c.a->pinned)
                c.a->pos += dir * corrA;

            if (!c.b->pinned)
                c.b->pos -= dir * corrB;

            c.tensionDir = dir;
            c.tensionMagnitude =
                fabs(error) * 100;
        }
    }

    for (auto &n : nodes_)
    {
        if (n.IsEndpoint())
            n.body->TranslateTo(n.pos);
    }
}

void Rope::SolveNodeCollisions(
    const std::vector<physics::Rigidbody *> &)
{
}

void Rope::UpdateTensionDisplay()
{
    if (constraints_.empty()) return;

    auto &first = constraints_.front();
    auto &last = constraints_.back();

    if (first.a->IsEndpoint())
        first.a->body->AddDisplayForce(
            first.tensionDir *
            first.tensionMagnitude,
            physics::ForceType::Tension);

    if (last.b->IsEndpoint())
        last.b->body->AddDisplayForce(
            last.tensionDir.Negate() *
            last.tensionMagnitude,
            physics::ForceType::Tension);
}

RopeNode *
Rope::PickNode(
    const math::Vec2 &worldPos,
    float radiusPx) const
{
    RopeNode *best = nullptr;

    float d = radiusPx;

    for (auto &n : nodes_)
    {
        float dist =
            (n.pos - worldPos).Length();

        if (dist < d)
        {
            d = dist;
            best = const_cast<RopeNode *>(&n);
        }
    }

    return best;
}

void Rope::DragNode(
    RopeNode *node,
    const math::Vec2 &target)
{
    draggedNode_ = node;

    if (!node) return;

    node->pos = target;
    node->prevPos = target;

    if (node->IsEndpoint())
        node->body->TranslateTo(target);
}

void Rope::ReleaseNode()
{
    draggedNode_ = nullptr;
}

bool Rope::ContainsBody(
    const physics::Rigidbody *b) const
{
    for (auto &n : nodes_)
        if (n.body == b) return true;

    return false;
}

bool Rope::AreAdjacent(
    const physics::Rigidbody *,
    const physics::Rigidbody *) const
{
    return false;
}

const std::vector<RopeConstraint> &
Rope::GetConstraints() const
{
    return constraints_;
}

const std::vector<RopeNode> &
Rope::GetNodes() const
{
    return nodes_;
}

}