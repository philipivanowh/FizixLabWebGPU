#pragma once

#include "physics/Rigidbody.hpp"
#include "shape/Shape.hpp"

#include <array>
#include <vector>

namespace shape
{

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
                physics::RigidbodyType RigidbodyType);
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
        int verticesSize = 0;

    private:
        //const float miniScaled = 0.5f; // Slightly smaller to ensure trigger is fully contained within its AABB
    };

} // namespace physics
