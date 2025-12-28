#pragma once

#include <cmath>
#include <algorithm>
#include <iostream>
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

    inline float MapForceToLength(const Vec2 &force,
                                  float inMin,
                                  float inMax,
                                  float outMin,
                                  float outMax,
                                  float exponent)
    {
        const float inRange = inMax - inMin;
        if (inRange <= 0.0f)
            return outMin;

        const float magnitude = force.Length();
        float t = math::Clamp((magnitude - inMin) / (inMax - inMin), 0.0f, 1.0f);
        float shaped = 1.0f - std::pow(1.0f - t, exponent); // exponent > 1 spreads high end
        float out = outMin + (outMax - outMin) * shaped;

        return out;
    }

    inline bool NearlyEqual(float a, float b, float epsilon = kEpsilon)
    {
        return std::fabs(a - b) <= epsilon;
    }

    inline bool NearlyEqualVec(const Vec2 &a, const Vec2 &b, float epsilon = kEpsilon)
    {
        return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon);
    }

}
