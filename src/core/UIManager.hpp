#pragma once

#include "core/Renderer.hpp"
#include "math/Vec2.hpp"
#include "shape/Shape.hpp"
#include "physics/Rigidbody.hpp"
#include "common/settings.hpp"
#include "core/Engine.hpp"

#include <array>
#include <cstddef>

struct SpawnSettings
{
    shape::ShapeType shapeType = shape::ShapeType::Box;
    math::Vec2 position{500.0f, 500.0f};
    math::Vec2 velocity{0.0f, 0.0f};
    float boxWidth = 60.0f;
    float boxHeight = 60.0f;
    float base = 300.0f;
    float angle = 30.0f;
    float radius = 30.0f;
    float mass = 10.0f;
    float staticFriction = 1.0f;
    float kineticFriction = 0.7f;
    bool flip = true;
    std::array<float, 4> color = {255.0f, 255.0f, 255.0f, 1.0f};
    float restitution = 0.4f;
    physics::RigidbodyType bodyType = physics::RigidbodyType::Dynamic;
};

struct CannonFireSettings
{
    // ── Projectile kind ──────────────────────────────────────────
    enum class ProjectileType { Ball, Box } projectileType = ProjectileType::Ball;

    // ── Aim ──────────────────────────────────────────────────────
    math::Vec2 cannonPos{0.0f, 0.0f}; // world-space barrel-tip origin
    float      angleDegrees = 45.0f;  // barrel angle (degrees, 0 = right)
    float      speed        = 10.0f; // launch speed magnitude (vel/s)

    // Derived — recomputed live whenever angle or speed changes
    float vx = 0.0f;
    float vy = 0.0f;

    // ── Projectile properties ────────────────────────────────────
    float mass        = 10.0f;
    float restitution = 0.4f;                                 // 0–1
    std::array<float, 4> color = {255.f, 255.f, 255.f, 1.f}; // RGBA 0–255

    // Ball-specific
    float radius   = 20.0f;

    // Box-specific
    float boxWidth  = 40.0f;
    float boxHeight = 40.0f;

    void Recompute()
    {
        const float rad = angleDegrees * math::PI / 180.0f;
        vx = speed * std::cos(rad);
        vy = speed * std::sin(rad);
    }
};

class UIManager
{
public:
    void InitializeImGui(Renderer &renderer, Settings* settings);
    void TerminateImGui();
    void BeginImGuiFrame();
    void EndImGuiFrame(Renderer &renderer);

    void RenderMainControls(std::size_t bodyCount, physics::Rigidbody *selectedBody);
    void RenderSpawner();

    // ── Measurement overlay ──────────────────────────────────────
    // startScreen/endScreen = raw GLFW coords (Y-down)
    // wStart/wEnd           = world coords (Y-up) for label values
    void RenderMeasurementOverlay(const math::Vec2 &startScreen,
                                  const math::Vec2 &endScreen,
                                  const math::Vec2 &wStart,
                                  const math::Vec2 &wEnd,
                                  bool active);

    // ── Mouse position overlay ───────────────────────────────────
    // worldPos = world-space coords (Y-up) to display in the tooltip
    void RenderMousePositionOverlay(const math::Vec2 &worldPos);

    bool ConsumeSpawnRequest(SpawnSettings &out);

    // Returns true (and fills `out`) when the user clicked FIRE in the
    // cannon inspector.  Call once per frame; resets the pending flag.
    bool ConsumeCannonFireRequest(CannonFireSettings &out);

private:
    // ── Theme ────────────────────────────────────────────────────
    void ApplyNeonTheme();

    // ── Panels ───────────────────────────────────────────────────
    void RenderTopTimelineBar();
    void RenderSpawnerPanel();
    void RenderSimPanel(std::size_t bodyCount);
    void RenderInspectorPanel(physics::Rigidbody* body);

     // ── Cannon inspector (shown when selectedBody is a Cannon) ────
    void RenderCannonInspector(shape::Cannon *cannon);

    // ── Spawner sub-sections ─────────────────────────────────────
    void RenderSpawnBasics();
    void RenderSpawnPhysicsControls();
    void RenderSpawnSizeControls();
    void RenderSpawnActions();

    // ── Neon helpers ─────────────────────────────────────────────
    bool NeonButton(const char *label, bool active = false);
    void PulsingRecordDot();

    // ── Measurement internals ────────────────────────────────────
    void DrawMeasurementOverlay(const math::Vec2 &screenA,
                                const math::Vec2 &screenB,
                                const math::Vec2 &wStart,
                                const math::Vec2 &wEnd);

    SpawnSettings spawnSettings;
    bool spawnRequestPending = false;

    
    CannonFireSettings cannonFireSettings;
    bool cannonFirePending = false;

    Settings* settings = nullptr;

    // Window dimensions — set once in RenderMainControls
    float screenW = 1800;
    float screenH = 1000;
};