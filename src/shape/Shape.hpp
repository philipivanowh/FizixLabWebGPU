#pragma once
#include "physics/Rigidbody.hpp"
#include <vector>
#include <array>

namespace shape
{
    enum class ShapeType
    {
        Ball,
        Incline,
        Box,
        Cannon,
        Thruster,
        Trigger,
        Rope, //Uncomment it if you want the rope
        Spring
    };

    class Shape : public physics::Rigidbody
    {
    public:
        ShapeType shapeType;
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
        virtual std::array<float, 4> GetColor() const { return {1.0f, 1.0f, 1.0f, 1.0f}; }  // Default white

        mutable std::vector<math::Vec2> vertices;
        mutable collision::AABB aabb;

        ShapeType GetShapeType() const { return shapeType; }
    };

    // Every Rigidbody in World::objects is a Shape (all spawn paths construct
    // shape subclasses, including rope endpoint Balls), so this static_cast is
    // safe. Replaces dynamic_cast in the per-substep hot paths, where RTTI
    // lookups dominated the profile.
    inline ShapeType KindOf(const physics::Rigidbody &body)
    {
        return static_cast<const Shape &>(body).shapeType;
    }
}
