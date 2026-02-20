#pragma once
#include "physics/Rigidbody.hpp"
#include <vector>

namespace shape
{
    enum class ShapeType
    {
        Ball,
        Incline,
        Box,
        Cannon
    };

    class Shape : public physics::Rigidbody
    {
    public:
        ShapeType ShapeType;
        Shape(const math::Vec2 &pos,
              const math::Vec2 &vel,
              const math::Vec2 &acc,
              float massValue,
              float restitution,
              physics::RigidbodyType RigidbodyType) : physics::Rigidbody(pos, vel, acc, massValue, restitution, RigidbodyType) {}
        virtual std::vector<math::Vec2> GetVertexWorldPos() const;
        virtual collision::AABB GetAABB() const;
        virtual collision::AABB GetLocalAABB() const;
        virtual std::vector<float> GetVertexLocalPos() const;

        mutable std::vector<math::Vec2> vertices;
        mutable collision::AABB aabb;
    };
}
