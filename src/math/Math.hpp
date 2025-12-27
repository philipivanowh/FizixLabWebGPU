#pragma once

#include <cmath>
#include "Vec2.hpp"

namespace math
{

    static constexpr float kEpsilon = 1e-5f;
    inline float Clamp(float value, float min, float max)
    {
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    };

    inline bool NearlyEqual(float a, float b, float epsilon = kEpsilon)
    {
        return std::fabs(a - b) <= epsilon;
    }

    inline bool NearlyEqualVec(const math::Vec2 &a, const math::Vec2 &b, float epsilon = kEpsilon)
    {
        return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon);
    }

}