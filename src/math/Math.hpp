#pragma once

#include <cmath>
#include "Vec2.hpp"

namespace math
{
    class Math
    {
    private:
        static constexpr float kEpsilon = 1e-5f;
    public:
        static float Clamp(float value, float min, float max)
        {
            if (value < min)
                return min;
            if (value > max)
                return max;
            return value;
        };

        static bool NearlyEqual(float a, float b)
        {
            return std::fabs(a - b) <= kEpsilon;
        }

        static bool NearlyEqualVec(const Vec2 &a, const Vec2 &b)
        {
            return NearlyEqual(a.x, b.x) && NearlyEqual(a.y, b.y);
        }
    };
}