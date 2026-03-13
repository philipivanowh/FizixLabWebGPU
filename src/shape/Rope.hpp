#pragma once

#include "physics/Rigidbody.hpp"

#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace shape
{

struct RopeNode
{
    math::Vec2 pos;
    math::Vec2 prevPos;
    math::Vec2 vel;

    float invMass = 2.0f;
    bool pinned = false;

    physics::Rigidbody *body = nullptr;

    bool IsEndpoint() const { return body != nullptr; }
};

struct RopeConstraint
{
    RopeNode *a = nullptr;
    RopeNode *b = nullptr;

    float restLength = 0.0f;
    float stiffness = 1.0f;

    float tensionMagnitude = 0.0f;
    math::Vec2 tensionDir;
};

class Rope
{
public:

    struct SpawnParams
    {
        math::Vec2 startWorld;
        math::Vec2 endWorld;

        int segmentCount = 12;
        float segmentMass = 0.5f;

        float stiffness = 1.0f;
        float damping = 0.98f;

        int stickIterations = 10;

        bool pinStart = true;
        bool pinEnd = false;
    };

    std::vector<std::unique_ptr<physics::Rigidbody>>
    Build(const SpawnParams &p);

    void RegisterEndpoints(
        const std::vector<physics::Rigidbody *> &endpointBodies,
        const SpawnParams &p);

    void Solve(float deltaMs,int iterations);

    void SolveNodeCollisions(
        const std::vector<physics::Rigidbody *> &staticBodies);

    void UpdateTensionDisplay();

    RopeNode *PickNode(const math::Vec2 &worldPos, float radiusPx) const;
    void DragNode(RopeNode *node, const math::Vec2 &targetWorld);
    void ReleaseNode();

    bool ContainsBody(const physics::Rigidbody *b) const;

    bool AreAdjacent(
        const physics::Rigidbody *a,
        const physics::Rigidbody *b) const;

    const std::vector<RopeConstraint> &GetConstraints() const;
    const std::vector<RopeNode> &GetNodes() const;

private:

    std::vector<RopeNode> nodes_;
    std::vector<RopeConstraint> constraints_;

    RopeNode *draggedNode_ = nullptr;

    int stickIterations_ = 10;
    float damping_ = 0.98f;
    float nodeMass_ = 0.5f;

    std::unique_ptr<physics::Rigidbody> nodeProbe_;

    using BodyPair =
        std::pair<physics::Rigidbody *, physics::Rigidbody *>;

    std::set<BodyPair> adjacentEndpointPairs_;

    static BodyPair MakePair(
        physics::Rigidbody *a,
        physics::Rigidbody *b)
    {
        return (a < b)
                   ? BodyPair{a, b}
                   : BodyPair{b, a};
    }
};

}