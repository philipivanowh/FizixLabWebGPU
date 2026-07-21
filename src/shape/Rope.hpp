#pragma once

#include "physics/Rigidbody.hpp"

#include <memory>
#include <set>
#include <utility>
#include <vector>

// No World include — zero circular dependency.

namespace shape
{

// ─────────────────────────────────────────────────────────────────────────────
// RopeNode
//
// Hybrid design — two distinct kinds of node share this struct:
//
//   ENDPOINT nodes  (index 0 and n-1)
//     body != nullptr  →  a real Ball Rigidbody owned by World::objects.
//     World's broadphase and CollisionSolver handle them exactly like any
//     other body.  pos mirrors body->pos and is kept in sync every frame.
//
//   MIDDLE nodes    (all others)
//     body == nullptr  →  a plain verlet position, NOT in World::objects.
//     Rope::Solve() integrates them manually (gravity + damping).
//     Rope::SolveNodeCollisions() tests them against static geometry only,
//     using a lightweight point-pushout rather than the full impulse solver.
//     This removes O(n²) broadphase pairs and eliminates CollisionSolver cost
//     for every interior node.
// ─────────────────────────────────────────────────────────────────────────────
struct RopeNode
{
    math::Vec2 pos;
    math::Vec2 prevPos;           // used for velocity correction after sticks
    math::Vec2 vel;               // explicit velocity (gravity, damping, drag)
    float      invMass = 2.0f;    // 1 / segmentMass
    bool       pinned  = false;   // static anchor — never moved by solver

    // Non-null for endpoint nodes only.
    physics::Rigidbody *body = nullptr;

    bool IsEndpoint() const { return body != nullptr; }
};

// ─────────────────────────────────────────────────────────────────────────────
// RopeConstraint
// ─────────────────────────────────────────────────────────────────────────────
struct RopeConstraint
{
    RopeNode  *a            = nullptr;
    RopeNode  *b            = nullptr;
    float      restLength   = 0.0f;
    float      stiffness    = 1.0f;
    float      tensionMagnitude = 0.0f;
    math::Vec2 tensionDir;         // unit vector a → b
};

// ─────────────────────────────────────────────────────────────────────────────
// Rope
// ─────────────────────────────────────────────────────────────────────────────
class Rope
{
public:
    struct SpawnParams
    {
        math::Vec2 startWorld;
        math::Vec2 endWorld;
        int        segmentCount    = 12;
        float      segmentMass     = 0.5f;
        float      stiffness       = 1.0f;
        float      damping         = 0.98f;
        int        stickIterations = 10;
        bool       pinStart        = true;
        bool       pinEnd          = false;
    };

    // ── Build ─────────────────────────────────────────────────────────────────
    // Creates ONLY the two endpoint Ball Rigidbodies and returns them.
    // World::AddRope calls world.Add() on each, then calls RegisterEndpoints().
    std::vector<std::unique_ptr<physics::Rigidbody>> Build(const SpawnParams &p);

    // Called by World::AddRope after it has Add()-ed both endpoint bodies.
    // Fills all RopeNodes (including middle nodes) and all RopeConstraints.
    // endpointBodies[0] = start body ptr, endpointBodies[1] = end body ptr.
    void RegisterEndpoints(const std::vector<physics::Rigidbody *> &endpointBodies,
                           const SpawnParams &p);

    // ── Per-frame ─────────────────────────────────────────────────────────────

    // Call AFTER Rigidbody::Update() for all objects (endpoints already integrated).
    // Integrates middle nodes, then runs Gauss-Seidel stick projection.
    void Solve(float deltaMs,int iterations);

    // Thin collision loop — tests every MIDDLE node against each static body.
    // Uses a reusable probe Ball so existing collision::Collide() is called
    // with no new code paths.  Endpoints are handled by the main CollisionSolver.
    // Call AFTER Solve(), before UpdateTensionDisplay().
    void SolveNodeCollisions(const std::vector<physics::Rigidbody *> &staticBodies);

    // Writes tension ForceInfo onto endpoint bodies so FBD display picks them up.
    void UpdateTensionDisplay();

    // ── Drag ──────────────────────────────────────────────────────────────────
    // Searches all nodes (endpoints + middle) by position.
    RopeNode *PickNode(const math::Vec2 &worldPos, float radiusPx) const;

    // Moves the node to targetWorld each frame during drag.
    // Endpoint: calls body->TranslateTo and zeroes vel.
    // Middle:   sets pos/prevPos/vel directly.
    void DragNode(RopeNode *node, const math::Vec2 &targetWorld);

    void ReleaseNode();

    // ── Collision helpers used by World::SameRopeAndAdjacent ─────────────────
    // Only meaningful for endpoint bodies (middle nodes are not in objects).
    bool ContainsBody(const physics::Rigidbody *b) const;
    bool AreAdjacent(const physics::Rigidbody *a,
                     const physics::Rigidbody *b) const;

    // ── Accessors ─────────────────────────────────────────────────────────────
    const std::vector<RopeConstraint> &GetConstraints() const;
    const std::vector<RopeNode>       &GetNodes()       const;

private:
    std::vector<RopeNode>       nodes_;
    std::vector<RopeConstraint> constraints_;
    RopeNode                   *draggedNode_     = nullptr;
    int                         stickIterations_ = 10;
    float                       damping_         = 0.98f;
    float                       nodeMass_        = 0.5f;

    // Reusable probe Ball for middle-node collision detection.
    // Avoids allocating a new object per node per frame.
    std::unique_ptr<physics::Rigidbody> nodeProbe_;

    // Set of adjacent endpoint-body pointer pairs for AreAdjacent().
    // Only populated when segmentCount == 2 (endpoints are directly adjacent).
    using BodyPair = std::pair<physics::Rigidbody *, physics::Rigidbody *>;
    std::set<BodyPair> adjacentEndpointPairs_;

    static BodyPair MakePair(physics::Rigidbody *a, physics::Rigidbody *b)
    {
        return (a < b) ? BodyPair{a, b} : BodyPair{b, a};
    }
};

} // namespace shape