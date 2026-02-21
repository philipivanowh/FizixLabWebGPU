#include "core/Engine.hpp"
#include <imgui.h>

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Cannon.hpp"
#include "shape/Incline.hpp"
#include "shape/Shape.hpp"
#include "math/Math.hpp"

#include <array>
#include <memory>
#include <cmath>
#include <string>

using math::PI;
using math::Vec2;
using physics::ForceType;
using physics::RigidbodyType;

// ── Static members ────────────────────────────────────────────────
Recorder Engine::recorder;
int Engine::recordFrameCounter = 0;
int Engine::spawnNudgeFrames = 0;
Renderer Engine::renderer;
World Engine::world;
Settings Engine::settings;
UIManager Engine::uiManager;
physics::Rigidbody *Engine::draggedBody = nullptr;
physics::Rigidbody *Engine::selectedBody = nullptr;
math::Vec2 Engine::mouseWorld{};
math::Vec2 Engine::mouseScreen{};
bool Engine::mouseDownLeft = false;
bool Engine::mouseDownRight = false;
math::Vec2 Engine::mouseInitialPos;
math::Vec2 Engine::mouseInitialScreen;
math::Vec2 Engine::mouseDeltaScale;
math::Vec2 Engine::staticDragOffset;

math::Vec2 Engine::cameraOffset{0.0f, 0.0f};
math::Vec2 Engine::panStartMouse;
math::Vec2 Engine::panStartCamera;
bool Engine::isPanning = false;

// ================================================================
// INIT / SHUTDOWN
// ================================================================
bool Engine::Initialize()
{
    if (!renderer.Initialize(settings, Engine::Scroll_Feedback))
        return false;

    uiManager.InitializeImGui(renderer, &settings);
    AddDefaultObjects();
    return true;
}

void Engine::Shutdown()
{
    renderer.Terminate();
    uiManager.TerminateImGui();
}

// ================================================================
// SPAWNING
// ================================================================
void Engine::CheckSpawning()
{
    SpawnSettings req;
    if (!uiManager.ConsumeSpawnRequest(req))
        return;

    if (req.shapeType == shape::ShapeType::Box)
    {
        world.Add(std::make_unique<shape::Box>(
            req.position, req.velocity, Vec2(0, 0),
            req.boxWidth, req.boxHeight,
            req.color, req.mass, req.restitution, req.bodyType));
    }
    else if (req.shapeType == shape::ShapeType::Ball)
    {
        world.Add(std::make_unique<shape::Ball>(
            req.position, req.velocity, Vec2(0, 0),
            req.radius, req.color, req.mass, req.restitution, req.bodyType));
    }
    else if (req.shapeType == shape::ShapeType::Incline)
    {
        world.Add(std::make_unique<shape::Incline>(
            req.position, req.velocity, Vec2(0, 0),
            req.base, req.angle, req.flip,
            req.color, req.staticFriction, req.kineticFriction));
    }
    else if (req.shapeType == shape::ShapeType::Cannon)
    {
        world.Add(std::make_unique<shape::Cannon>(
            req.position, req.angle, req.color));
    }

    // Always nudge physics forward after a spawn so the object
    // visibly appears even when paused / not recording
    spawnNudgeFrames = 1;

    // Rewind history is invalid now that body count changed
    recorder.Clear();
    settings.scrubIndex = -1;
}

void Engine::Scroll_Feedback(GLFWwindow *window, double xoffset, double yoffset)
{
    settings.zoom += static_cast<float>(yoffset) * 0.1f;
    (void)xoffset;
    (void)(window);
}

// ================================================================
// UPDATE
// ================================================================
void Engine::Update(float deltaMs, int iterations)
{
    glfwPollEvents();

    GLFWwindow *window = renderer.GetWindow();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        renderer.Terminate();
    }

    // ── Mouse position ────────────────────────────────────────────
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    // Window size in OS screen-points (what glfwGetCursorPos uses).
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    // Screen space: Y-down, origin at top-left
    const float scaledMx = static_cast<float>(mx);
    const float scaledMy = static_cast<float>(my);

    // Screen space: Y-down, origin at top-left — used by measurement overlay
    mouseScreen = Vec2(scaledMx, scaledMy);

    // ── Mouse buttons ─────────────────────────────────────────────
    bool pressedLeft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool pressedRight = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // Block mouse interaction when cursor is inside the top bar
    ImGuiIO &io = ImGui::GetIO();
    const bool overUI = io.WantCaptureMouse;

    // ── Zoom controls (global) ────────────────────────────────────
    settings.zoom = math::Clamp(settings.zoom, 0.1f, 4.0f);

    renderer.SetZoom(settings.zoom);

    // World space: apply inverse zoom around window center, then Y-up flip.
    const float zoom = settings.zoom;
    const float cx = static_cast<float>(winW) * 0.5f;
    const float cy = static_cast<float>(winH) * 0.5f;

    // Apply zoom around center, then add camera offset
    // NEW (CORRECT)
    const float zoomedX = (scaledMx - cx) / zoom + cx;
    const float zoomedY = (scaledMy - cy) / zoom + cy;
    const float worldX = zoomedX + cameraOffset.x;
    const float worldY = static_cast<float>(winH) - zoomedY + cameraOffset.y;
    mouseWorld = Vec2(worldX, worldY);

    if (pressedLeft && !mouseDownLeft && !overUI)
    {
        mouseDownLeft = true;

        // Try to pick a body first
        draggedBody = world.PickBody(mouseWorld);
        selectedBody = draggedBody;

        if (draggedBody)
        {
            // Dragging an object
            staticDragOffset = draggedBody->pos - mouseWorld;
            isPanning = false;
        }
        else
        {
            // No object picked - start camera panning
            isPanning = true;
            panStartMouse = mouseScreen;
            panStartCamera = cameraOffset;
        }
    }

    if (!pressedLeft)
    {
        mouseDownLeft = false;
        draggedBody = nullptr;
        staticDragOffset = Vec2(0.0f, 0.0f);
        isPanning = false;
    }

    // ── Camera panning update ─────────────────────────────────────
    if (isPanning && mouseDownLeft)
    {
        // Calculate mouse delta in screen space
        Vec2 mouseDelta = mouseScreen - panStartMouse;

        // Apply delta to camera offset (invert X and Y for natural panning)
        // Divide by zoom so panning speed stays consistent at different zoom levels
        cameraOffset.x = panStartCamera.x - mouseDelta.x / zoom;
        cameraOffset.y = panStartCamera.y + mouseDelta.y / zoom;

        // Update renderer camera offset
        renderer.SetCameraOffset(cameraOffset);
    }

    // ── Right mouse button (measurement) ──────────────────────────
    if (pressedRight && !mouseDownRight && !overUI)
    {
        mouseDownRight = true;
        mouseInitialPos = mouseWorld.Clone();
        mouseInitialScreen = mouseScreen.Clone();
    }

    if (!pressedRight)
    {
        mouseDownRight = false;
        mouseInitialPos = math::Vec2();
        mouseInitialScreen = math::Vec2();
        mouseDeltaScale = math::Vec2();
    }

    if (mouseDownRight)
        mouseDeltaScale = mouseWorld - mouseInitialPos;

    // ── Drag force ────────────────────────────────────────────────
    if (draggedBody && !isPanning)
    {

        // If the body is static then it can only move in percisionDragMode
        if (draggedBody->bodyType == RigidbodyType::Static)
        {
            if (!(dynamic_cast<shape::Cannon *>(draggedBody)))
                draggedBody->TranslateTo(mouseWorld + staticDragOffset);
            else
                draggedBody->TranslateTo(mouseWorld);
        }

        else if (draggedBody->bodyType == RigidbodyType::Dynamic)
        {
            switch (settings.dragMode)
            {
            case DragMode::percisionDrag:

                draggedBody->TranslateTo(mouseWorld);
                break;
            case DragMode::physicsDrag:
            {
                const float stiffness = DragConstants::DRAG_STIFNESS;
                const float damping = 5.0f * std::sqrt(
                                                 stiffness * math::Clamp(draggedBody->mass, 20.f, 100.f) / 20.f);

                Vec2 delta = mouseWorld - draggedBody->pos;
                draggedBody->dragForce =
                    (delta * stiffness - draggedBody->linearVel * damping) * draggedBody->mass;
                break;
            }
            }
        }
    }

    // ── Physics / time control ────────────────────────────────────
    if (settings.rewinding)
    {
        // Hold-button rewind (pop from back)
        const int steps = std::max(1, static_cast<int>(settings.timeScale));
        WorldSnapshot snap;
        for (int i = 0; i < steps; i++)
        {
            if (!recorder.Rewind(snap))
            {
                settings.rewinding = false;
                break;
            }
        }
        world.RestoreSnapshot(snap);
        // Physics does NOT run during rewind
    }
    else if (settings.scrubIndex >= 0)
    {
        // Timeline scrubber — restore the chosen frame, freeze physics
        const WorldSnapshot *frame =
            recorder.GetFrame(static_cast<size_t>(settings.scrubIndex));
        if (frame)
            world.RestoreSnapshot(*frame);
    }
    else
    {
        // ── LIVE MODE ─────────────────────────────────────────────

        // Record a snapshot if recording is enabled
        if (settings.recording)
        {
            if (++recordFrameCounter % settings.recordInterval == 0)
                recorder.Record(world.CaptureSnapshot());
        }

        // Decide how much simulated time passes this frame
        float scaledDelta = 0.0f;

        if (spawnNudgeFrames > 0)
        {
            // Force physics forward regardless of pause state
            scaledDelta = 16.67f;
            spawnNudgeFrames--;
        }
        else if (settings.stepOneFrame)
        {
            scaledDelta = 16.67f * settings.timeScale;
            settings.stepOneFrame = false;
        }
        else if (!settings.paused)
        {
            scaledDelta = deltaMs * settings.timeScale;
        }

        world.Update(scaledDelta, iterations, settings);
        CheckSpawning();
        CheckCannon();
    }
}

void Engine::CheckCannon()
{
    CannonFireSettings fire;
    if (uiManager.ConsumeCannonFireRequest(fire))
    {
        math::Vec2 vel{fire.vx, fire.vy};

        if (fire.projectileType == CannonFireSettings::ProjectileType::Ball)
        {
            world.Add(std::make_unique<shape::Ball>(
                fire.cannonPos, vel, Vec2(0, 0),
                fire.radius, fire.color, fire.mass, fire.restitution, RigidbodyType::Dynamic));
        }
        else if (fire.projectileType == CannonFireSettings::ProjectileType::Box)
        {
            world.Add(std::make_unique<shape::Box>(
                fire.cannonPos, vel, Vec2(0, 0),
                fire.boxWidth, fire.boxHeight, fire.color, fire.mass, fire.restitution, RigidbodyType::Dynamic));
        }
    }
}

// ================================================================
// RENDER
// ================================================================
void Engine::Render()
{
    renderer.BeginFrame();
    world.Draw(renderer);

    // ── ImGui ─────────────────────────────────────────────────────
    uiManager.BeginImGuiFrame();

    uiManager.RenderMainControls(world.RigidbodyCount(), selectedBody);

    uiManager.RenderSpawner();

    // Measurement overlay — screen coords for drawing, world coords for label values
    uiManager.RenderMeasurementOverlay(
        mouseInitialScreen, mouseScreen,
        mouseInitialPos, mouseWorld,
        mouseDownRight);

    // Mouse position badge
    uiManager.RenderMousePositionOverlay(mouseWorld);

    uiManager.EndImGuiFrame(renderer);
    renderer.EndFrame();
}

// ================================================================
// FRAME / CONTROL
// ================================================================
void Engine::RunFrame(float deltaMs, int iterations)
{
    Update(deltaMs, iterations);
    Render();
}

bool Engine::IsRunning()
{
    return renderer.IsRunning();
}

Renderer &Engine::GetRenderer()
{
    return renderer;
}

// ================================================================
// SCENES
// ================================================================
void Engine::AddDefaultObjects()
{
    using physics::RigidbodyType;
    using shape::Ball;
    using shape::Box;

    const std::array<float, 4> warmRed{0.070588f, 0.180392f, 0.219608f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> skyBlue{0.313726f, 0.627451f, 1.0f, 1.0f};
    const std::array<float, 4> yellow{1.0f, 0.784314f, 0.078431f, 1.0f};

    world.Add(std::make_unique<Box>(
        Vec2(700, 200), Vec2(0, 0), Vec2(0, 0),
        1600, 50, skyBlue, 100, 0, RigidbodyType::Static));

    world.Add(std::make_unique<Box>(
        Vec2(700, 300), Vec2(0, 1), Vec2(0, 0),
        80, 80, white, 1, 0.5f, RigidbodyType::Dynamic));

    auto slope = std::make_unique<Box>(
        Vec2(1000, 400), Vec2(0, 0), Vec2(0, 0),
        500, 40, skyBlue, 100, 0, RigidbodyType::Static);
    slope->RotateTo(-PI / 6.0f);
    world.Add(std::move(slope));

    world.Add(std::make_unique<Box>(
        Vec2(500, 700), Vec2(5, 1), Vec2(0, 0),
        200, 200, white, 2000, 1, RigidbodyType::Dynamic));

    world.Add(std::make_unique<Box>(
        Vec2(896, 670), Vec2(0, 0), Vec2(0, 0),
        70, 70, yellow, 15, 0.5f, RigidbodyType::Dynamic));

    world.Add(std::make_unique<Box>(
        Vec2(906, 570), Vec2(0, 0), Vec2(0, 0),
        70, 70, warmRed, 50, 0.5f, RigidbodyType::Dynamic));

    world.Add(std::make_unique<Ball>(
        Vec2(906, 770), Vec2(0, 0), Vec2(0, 0),
        50, warmRed, 50, 0.5f, RigidbodyType::Dynamic));

    world.Add(std::make_unique<Ball>(
        Vec2(500, 700), Vec2(-1, 0), Vec2(0, 0),
        20, warmRed, 5, 0.2f, RigidbodyType::Dynamic));
}

void Engine::ComparisonScene()
{
    const std::array<float, 4> warmRed{0.070588f, 0.180392f, 0.219608f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> skyBlue{0.313726f, 0.627451f, 1.0f, 1.0f};
    const std::array<float, 4> yellow{1.0f, 0.784314f, 0.078431f, 1.0f};

    world.Add(std::make_unique<shape::Box>(
        Vec2(700, 400), Vec2(0, 0), Vec2(0, 0),
        1600, 50, skyBlue, 2000, 0, RigidbodyType::Static));

    world.Add(std::make_unique<shape::Box>(
        Vec2(100, 500), Vec2(0, 0), Vec2(0, 0),
        25, 25, warmRed, 10, 0, RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(150, 500), Vec2(0, 0), Vec2(0, 0),
        25, 25, white, 1000, 0, RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(200, 500), Vec2(0, 0), Vec2(0, 0),
        25, 25, skyBlue, 100, 0, RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(250, 500), Vec2(1, 0), Vec2(0, 0),
        25, 25, yellow, 100, 0, RigidbodyType::Dynamic));
}

void Engine::InclineProblemScene()
{
    const std::array<float, 4> warmRed{0.705882f, 0.164706f, 0.400000f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> cannonBlue{0.352941f, 0.549020f, 0.862745f, 1.0f};

    world.Add(std::make_unique<shape::Box>(
        Vec2(700, 200), Vec2(0, 0), Vec2(0, 0),
        1600, 50, warmRed, 100, 0, RigidbodyType::Static));

    world.Add(std::make_unique<shape::Incline>(
        Vec2(800, 400), Vec2(0, 0), Vec2(0, 0),
        600, 20, true, warmRed, 0.5f, 0.1f));

    world.Add(std::make_unique<shape::Cannon>(
        Vec2(520, 280), 30.0f, cannonBlue));

    world.Add(std::make_unique<shape::Box>(
        Vec2(970, 590), Vec2(0, 0), Vec2(0, 0),
        100, 100, white, 100, 1, RigidbodyType::Dynamic));
}

void Engine::ClearBodies()
{
    world.ClearObjects();
    recorder.Clear();
    settings.scrubIndex = -1;
}
