#include "shape/Trigger.hpp"
#include "collision/AABB.hpp"
#include "physics/Rigidbody.hpp"
#include "common/settings.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace shape
{

    Trigger::Trigger(const math::Vec2 &pos,
                     const math::Vec2 &vel,
                     const math::Vec2 &acc,
                     float widthValue,
                     float heightValue,
                     const std::array<float, 4> &colorValue,
                     float massValue,
                     float restitution,
                     physics::RigidbodyType RigidbodyTypeValue,
                     TriggerAction actionValue)
        : Shape(pos, vel, acc, massValue, restitution, RigidbodyTypeValue), width(widthValue * SimulationConstants::PIXELS_PER_METER), height(heightValue * SimulationConstants::PIXELS_PER_METER), color(colorValue), originalColor(colorValue), action(actionValue)
    {
        verticesSize = 12; // 4 for outer box + 4 for inner box (mini-scaled) = 8 total vertices, but since we are using two triangles per face, we need to multiply by 3/2
        UpdateMassProperties();

        float halfWidth = width / 2.0f;
        float halfHeight = height / 2.0f;
        vertices = {
            math::Vec2(-halfWidth, -halfHeight),
            math::Vec2(halfWidth, -halfHeight),
            math::Vec2(halfWidth, halfHeight),
            math::Vec2(-halfWidth, halfHeight),

            math::Vec2(-halfWidth * miniScaled, -halfHeight * miniScaled),
            math::Vec2(halfWidth * miniScaled, -halfHeight * miniScaled),
            math::Vec2(halfWidth * miniScaled, halfHeight * miniScaled),
            math::Vec2(-halfWidth * miniScaled, halfHeight * miniScaled),

        };
    }

    std::vector<float> ToFloats(const std::vector<math::Vec2> &v)
    {
        std::vector<float> out;
        out.reserve(v.size() * 2);
        for (const auto &p : v)
        {
            out.push_back(p.x);
            out.push_back(p.y);
        }
        return out;
    }


    std::vector<float> Trigger::GetOuterBoxVertexLocalPos() const{
        // Return vertices as two triangles to form a complete box
        // Triangle 1: v0, v1, v2
        // Triangle 2: v0, v2, v3
        std::vector<math::Vec2> rotated = GetRotatedVertices();
        return ToFloats({
            rotated[0], rotated[1], rotated[2],  // Triangle 1
            rotated[0], rotated[2], rotated[3]   // Triangle 2
        });
    }

    std::vector<float> Trigger::GetInnerBoxVertexLocalPos() const{
        // Return vertices as two triangles to form a complete inner box
        // Triangle 1: v4, v5, v6
        // Triangle 2: v4, v6, v7
        std::vector<math::Vec2> rotated = GetRotatedVertices();
        return ToFloats({
            rotated[4], rotated[5], rotated[6],  // Triangle 1
            rotated[4], rotated[6], rotated[7]   // Triangle 2
        });
    }
    
 
    std::vector<float> Trigger::GetVertexLocalPos() const
    {
        std::vector<math::Vec2> rotated = GetRotatedVertices();
        // outer vertices for trigger visualization
        const math::Vec2 &v0 = rotated[0];
        const math::Vec2 &v1 = rotated[1];
        const math::Vec2 &v2 = rotated[2];
        const math::Vec2 &v3 = rotated[3];

        // inner vertices for trigger visualization
        const math::Vec2 &v4 = rotated[4];
        const math::Vec2 &v5 = rotated[5];
        const math::Vec2 &v6 = rotated[6];
        const math::Vec2 &v7 = rotated[7];

        return {
            v0.x, v0.y,
            v1.x, v1.y,
            v2.x, v2.y,
            v0.x, v0.y,
            v2.x, v2.y,
            v3.x, v3.y,
            v4.x, v4.y,
            v5.x, v5.y,
            v6.x, v6.y,
            v4.x, v4.y,
            v6.x, v6.y,
            v7.x, v7.y};
    }

    std::vector<math::Vec2> Trigger::GetVertexWorldPos() const
    {
        std::vector<math::Vec2> out;
        std::vector<math::Vec2> rotated = GetRotatedVertices();
        out.reserve(rotated.size());
        for (const auto &v : rotated)
        {
            out.emplace_back(v.x + pos.x, v.y + pos.y);
        }
        transformUpdateRequired = false;
        return out;
    }

    float Trigger::ComputeInertia() const
    {
        if (mass <= 0.0f)
        {
            return 0.0f;
        }
        return (mass * (width * width + height * height)) / 12.0f;
    }

    std::vector<math::Vec2> Trigger::GetRotatedVertices() const
    {
        float cosValue = std::cos(rotation);
        float sinValue = std::sin(rotation);
        std::vector<math::Vec2> rotated;
        rotated.reserve(vertices.size());
        for (const auto &v : vertices)
        {
            rotated.emplace_back(
                v.x * cosValue - v.y * sinValue,
                v.x * sinValue + v.y * cosValue);
        }
        return rotated;
    }

} // namespace physics
