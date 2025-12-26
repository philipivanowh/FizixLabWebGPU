#pragma once

namespace physics {
inline constexpr float GRAVITATIONAL_STRENGTH = 9.8f;
inline constexpr float MIN_BODY_SIZE = 0.01f * 0.01f;
inline constexpr float MAX_BODY_SIZE = 1'000'000.0f;
inline constexpr float MIN_DENSITY = 0.5f;
inline constexpr float MAX_DENSITY = 21.4f;
inline constexpr int MIN_ITERATIONS = 1;
inline constexpr int MAX_ITERATIONS = 128;
inline constexpr float DEFAULT_RESTITUTION = 0.4f;
} // namespace physics
