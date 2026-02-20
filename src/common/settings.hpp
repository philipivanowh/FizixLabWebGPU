#pragma once

namespace WindowConstants
{
    constexpr int windowWidth = 1800;
    constexpr int windowHeight = 1000;
}

namespace SimulationConstants
{
    // Simulation settings
    constexpr float TIME_STEP_MS = 16.67f;      // Time step per frame in milliseconds
    constexpr int MAX_PHYSICS_ITERATIONS = 128; // Maximum physics iterations per frame
    constexpr int MIN_PHYSICS_ITERATIONS = 1;   // Minimum physics iterations per frame
    constexpr float PIXELS_PER_METER = 100.0f;  // Conversion factor from meters to pixels
    constexpr float FBD_SCALER_ADJUSTMENT = 0.005;

} // namespace constants

namespace PhysicsConstants
{
    constexpr float GRAVITY = 9.81f; // Gravity acceleration in m/s^2
    inline constexpr float MIN_Rigidbody_SIZE = 0.01f * 0.01f;
    inline constexpr float MAX_Rigidbody_SIZE = 1'000'000.0f;
    inline constexpr float MIN_DENSITY = 0.5f;
    inline constexpr float MAX_DENSITY = 21.4f;
    inline constexpr float DEFAULT_RESTITUTION = 0.4f;
    inline constexpr float MAX_PENETRATION_CORRECTION = 5.0f;
    constexpr float FRICTION_DISPLAY_THRESHOLD = 0.002f;
}

namespace DragConstants
{
    constexpr float DRAG_STIFNESS = 2000.0f;
}

namespace VisualizationConstants
{
    constexpr float FBD_ARROW_MIN = 100.0f;
    constexpr float FBD_ARROW_MAX = 250.0f;
    constexpr float FBD_FORCE_MIN = 50.0f;
    constexpr float FBD_FORCE_MAX = 10000.0f;
    constexpr float FBD_CURVE_EXPONENT = 4.0f;
    constexpr float FBD_ARROW_THICKNESS = 8.0f;
    constexpr float FBD_ARROW_HEAD_THICKNESS = 16.0f;
    constexpr float FBD_ARROW_HEAD_SCALE = 1.1f;
}

enum class DragMode
{
    percisionDrag,
    physicsDrag
};

class Settings
{
public:
    DragMode dragMode = DragMode::physicsDrag;

    // --- Time Control ---
    float timeScale = 1.0f; // 0 = paused, 1 = normal, 0.1 = slow-mo
    bool paused = false;
    bool     stepOneFrame = false;
     bool     rewinding    = false; 
     bool     recording    = false;
     int      recordInterval = 1;
     int scrubIndex = -1;
};
