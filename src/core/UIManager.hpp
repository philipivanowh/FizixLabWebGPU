#pragma once

#include "core/Renderer.hpp"
#include "math/Vec2.hpp"
#include "shape/Shape.hpp"
#include "physics/Rigidbody.hpp"
#include "common/settings.hpp"
#include "core/Engine.hpp"
#include "core/Recorder.hpp"

#include <array>
#include <cstddef>

struct SpawnSettings {
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
    void RenderMainControls(std::size_t bodyCount,Settings& settings);
    void RenderSpawner();
    void InitializeImGui(Renderer& renderer);
	void TerminateImGui();
    void BeginImGuiFrame();
    void EndImGuiFrame(Renderer& renderer);
    bool ConsumeSpawnRequest(SpawnSettings& out);
private:
    void RenderSimulationControlsWindow(std::size_t bodyCount, Settings& settings);
    void RenderSpawnerWindow();
    void RenderSpawnBasics();
    void RenderSpawnPhysicsControls();
    void RenderSpawnSizeControls();
    void RenderSpawnActions();

    SpawnSettings spawnSettings;
    bool spawnRequestPending = false;
};
