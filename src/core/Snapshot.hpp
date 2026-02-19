#pragma once
#include "math/Vec2.hpp"
#include <vector>
#include <cstddef>

struct BodySnapshot {
    math::Vec2 pos;
    math::Vec2 linearVel;
    math::Vec2 linearAcc;
    float      rotation;
    float      angularVel;
};

struct WorldSnapshot {
    std::vector<BodySnapshot> bodies;
};