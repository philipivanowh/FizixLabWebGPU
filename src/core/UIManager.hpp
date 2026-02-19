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

class UIManager
{
public:
    void InitializeImGui(Renderer &renderer);
    void TerminateImGui();
    void BeginImGuiFrame();
    void EndImGuiFrame(Renderer &renderer);

    void RenderMainControls(std::size_t bodyCount, Settings &settings, physics::Rigidbody *selectedBody);
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

private:
    // ── Theme ────────────────────────────────────────────────────
    void ApplyNeonTheme();

    // ── Panels ───────────────────────────────────────────────────
    void RenderTopTimelineBar(Settings &settings);
    void RenderSpawnerPanel();
    void RenderSimPanel(std::size_t bodyCount, Settings &settings);
    void RenderInspectorPanel(physics::Rigidbody* body);

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

    // Window dimensions — set once in RenderMainControls
    float screenW = 1470.0f;
    float screenH = 956.0f;
};