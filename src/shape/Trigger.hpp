#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape
{
    enum class TriggerAction
    {
        DoNothing = 0,
        PauseSimulation = 1
    };

    class Trigger : public Shape
    {
    public:
        Trigger(const math::Vec2 &pos,
                const math::Vec2 &vel,
                const math::Vec2 &acc,
                float width,
                float height,
                const std::array<float, 4> &color,
                float mass,
                float restitution,
                physics::RigidbodyType RigidbodyType,
                TriggerAction action);
        ~Trigger() override = default;

        float ComputeInertia() const override;
        std::vector<math::Vec2> GetRotatedVertices() const;
        std::vector<float> GetVertexLocalPos() const override;
        std::vector<math::Vec2> GetVertexWorldPos() const override;

        std::vector<float> GetOuterBoxVertexLocalPos() const;
        std::vector<float> GetInnerBoxVertexLocalPos() const;

        float width = 0.0f;
        float height = 0.0f;
        std::array<float, 4> color{0.0f, 0.0f, 0.0f, 1.0f};
        std::array<float, 4> originalColor{0.0f, 0.0f, 0.0f, 1.0f};
        std::array<float, 4> collisionColor{1.0f, 0.0f, 0.0f, 1.0f};  // Red when colliding
        int verticesSize = 0;
        bool isColliding = false;
        bool wasCollidingLastFrame = false;  // Track if colliding in previous frame
        TriggerAction action = TriggerAction::DoNothing;
        bool hasTriggeredThisFrame = false; // To prevent multiple triggers in the same frame

    private:
        const float miniScaled = 0.5f; // Slightly smaller to ensure trigger is fully contained within its AABB
    };

} // namespace physics
