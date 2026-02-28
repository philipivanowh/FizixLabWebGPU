#include "core/Engine.hpp"
#include <imgui.h>

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Cannon.hpp"
#include "shape/Incline.hpp"
#include "shape/Trigger.hpp"
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

// Static members Initialization
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

bool Engine::prevKeyP = false;
bool Engine::prevKeyO = false;
bool Engine::prevKeyR = false;

int Engine::selectedBodyHoldFrames = 0;
int Engine::dragThresholdFrames = 15; // Hold for 5 frames before allowing drag
bool Engine::wasSelectedBodyJustClicked = false;

// INIT / SHUTDOWN
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

// SPAWNING
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
    else if (req.shapeType == shape::ShapeType::Trigger)
    {
        world.Add(std::make_unique<shape::Trigger>(
            req.position, req.velocity, Vec2(0, 0),
            req.boxWidth, req.boxHeight,
            req.color, req.mass, req.restitution, RigidbodyType::Static));
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
    ImGuiIO &io = ImGui::GetIO();
    const bool overUI = io.WantCaptureMouse;
    if (overUI)
        return;

    settings.zoom += static_cast<float>(yoffset) * 0.1f;
    (void)xoffset;
    (void)(window);
}

// UPDATE
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

    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    const float scaledMx = static_cast<float>(mx);
    const float scaledMy = static_cast<float>(my);

    // Screen space: Y-down, origin at top-left
    mouseScreen = Vec2(scaledMx, scaledMy);

    bool pressedControl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    bool mouseButtonLeft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool mouseButtonRight = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    // ── Mouse buttons ─────────────────────────────────────────────
    bool pressedLeft = false;

    bool pressedRight = false;
#ifdef __APPLE__
    pressedRight = pressedControl && mouseButtonLeft;
    pressedLeft = mouseButtonLeft && !pressedControl;
    (void)mouseButtonRight; // Silence unused variable warning when Control is not used for right-click
#else
    pressedRight = mouseButtonRight;
    pressedLeft = mouseButtonLeft;
    (void)pressedControl;
#endif

    ImGuiIO &io = ImGui::GetIO();
    const bool overUI = io.WantCaptureMouse;
    const bool overUIKeyboard = io.WantCaptureKeyboard;

    bool keyP = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (keyP && !prevKeyP)
    {
        settings.paused = !settings.paused;
    }
    prevKeyP = keyP;

    bool keyO = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (keyO && !prevKeyO)
    {
        settings.stepOneFrame = true;
    }
    prevKeyO = keyO;

    bool keyR = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (keyR && !prevKeyR)
    {
        settings.recording = true;
    }
    prevKeyR = keyR;

    // ── Zoom controls ─────────────────────────────────────────────
    if (!overUIKeyboard)
    {
        const float zoomStep = 0.05f;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            settings.zoom *= (1.0f + zoomStep);
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            settings.zoom *= (1.0f - zoomStep);
        }
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_0) == GLFW_PRESS)
        {
            settings.zoom = 1.0f;
            cameraOffset = Vec2(0.0f, 0.0f); // Reset camera position too
        }
        settings.zoom = math::Clamp(settings.zoom, 0.1f, 4.0f);
    }

    renderer.SetZoom(settings.zoom);

    // ── World space calculation with camera offset ────────────────
    const float zoom = settings.zoom;
    const float cx = static_cast<float>(winW) * 0.5f;
    const float cy = static_cast<float>(winH) * 0.5f;

    // Apply zoom around center, then add camera offset
    const float zoomedX = (scaledMx - cx) / zoom + cx + cameraOffset.x;
    const float zoomedY = (scaledMy - cy) / zoom + cy - cameraOffset.y;
    mouseWorld = Vec2(zoomedX, static_cast<float>(winH) - zoomedY);

    // Pattern: single-click to select, hold/double-click to drag
    if (pressedLeft && !mouseDownLeft && !overUI)
    {
        mouseDownLeft = true;
        physics::Rigidbody *clickedBody = world.PickBody(mouseWorld);

        if (clickedBody == selectedBody && selectedBody != nullptr)
        {
            // Second click on same body — enable dragging
            draggedBody = selectedBody;
            staticDragOffset = draggedBody->pos - mouseWorld;
            wasSelectedBodyJustClicked = false;
            selectedBodyHoldFrames = 0;
            isPanning = false;
        }
        else if (clickedBody)
        {
            // First click on a new body — only select and highlight
            if (selectedBody)
            {
                selectedBody->isHighlighted = false;
            }
            selectedBody = clickedBody;
            selectedBody->isHighlighted = true;
            draggedBody = nullptr; // Don't drag yet
            wasSelectedBodyJustClicked = true;
            selectedBodyHoldFrames = 0;
            isPanning = false;
        }
        else
        {
            // Click on empty space — start panning
            if (selectedBody)
            {
                selectedBody->isHighlighted = false;
                selectedBody = nullptr;
            }
            draggedBody = nullptr;
            wasSelectedBodyJustClicked = false;
            selectedBodyHoldFrames = 0;
            isPanning = true;
            panStartMouse = mouseScreen;
            panStartCamera = cameraOffset;
        }
    }

    // ── Update hold counter and enable drag if threshold reached ───
    if (mouseDownLeft && selectedBody && !draggedBody && !isPanning)
    {
        selectedBodyHoldFrames++;
        if (selectedBodyHoldFrames >= dragThresholdFrames)
        {
            // Hold threshold reached — enable dragging
            draggedBody = selectedBody;
            staticDragOffset = draggedBody->pos - mouseWorld;
        }
    }

    if (!pressedLeft)
    {
        mouseDownLeft = false;
        draggedBody = nullptr;
        staticDragOffset = Vec2(0.0f, 0.0f);
        isPanning = false;
        selectedBodyHoldFrames = 0;
        wasSelectedBodyJustClicked = false;
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

    // ── Object dragging ───────────────────────────────────────────
    if (draggedBody && !isPanning)
    {
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
    }
    else if (settings.scrubIndex >= 0)
    {
        const WorldSnapshot *frame =
            recorder.GetFrame(static_cast<size_t>(settings.scrubIndex));
        if (frame)
            world.RestoreSnapshot(*frame);
    }
    else
    {
        // ── LIVE MODE ─────────────────────────────────────────────
        if (settings.recording)
        {
            if (++recordFrameCounter % settings.recordInterval == 0)
                recorder.Record(world.CaptureSnapshot());
        }

        float scaledDelta = 0.0f;

        if (spawnNudgeFrames > 0)
        {
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

        world.Update(scaledDelta, iterations, settings, selectedBody, draggedBody);
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

    ImGuiIO &io = ImGui::GetIO();
    const bool overUI = io.WantCaptureMouse;
    if (!overUI)
    {
        // Snap measurement endpoints to nearby dynamic bodies so it's easy to
        // measure between objects.  World::SnapToNearestDynamicObject returns
        // the input position when nothing is in range.
        const float snapRadius = 25.0f; // world units; tweak as needed
        math::Vec2 snappedStartWorld = world.SnapToNearestDynamicObject(mouseInitialPos, snapRadius);
        math::Vec2 snappedEndWorld = world.SnapToNearestDynamicObject(mouseWorld, snapRadius);

        // Convert snapped world coordinates back to screen space so the overlay
        // lines up with the clicked shapes.  This mirrors the transformation
        // done in Engine::Update() when converting screen->world.
        GLFWwindow *window = renderer.GetWindow();
        int winW, winH;
        glfwGetWindowSize(window, &winW, &winH);
        const float zoom = settings.zoom;
        const float cx = static_cast<float>(winW) * 0.5f;
        const float cy = static_cast<float>(winH) * 0.5f;
        auto worldToScreen = [&](const math::Vec2 &w)
        {
            float sx = (w.x - cameraOffset.x - cx) * zoom + cx;
            float sy = ((static_cast<float>(winH) - w.y) - cy + cameraOffset.y) * zoom + cy;
            return math::Vec2(sx, sy);
        };
        math::Vec2 snappedStartScreen = worldToScreen(snappedStartWorld);
        math::Vec2 snappedEndScreen = worldToScreen(snappedEndWorld);

        // Measurement overlay — screen coords for drawing, world coords for label values
        uiManager.RenderMeasurementOverlay(
            snappedStartScreen, snappedEndScreen,
            snappedStartWorld, snappedEndWorld,
            mouseDownRight);
    }
    // Mouse position badge

    if (!overUI)
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

    //  const std::array<float, 4> warmRed{0.070588f, 0.180392f, 0.219608f, 1.0f};
    // const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> skyBlue{0.313726f, 0.627451f, 1.0f, 1.0f};
    // const std::array<float, 4> yellow{1.0f, 0.784314f, 0.078431f, 1.0f};

    world.Add(std::make_unique<Box>(
        Vec2(0, 0), Vec2(0, 0), Vec2(0, 0),
        20, 1, skyBlue, 100, 0, RigidbodyType::Static));

    // world.Add(std::make_unique<Box>(
    //     Vec2(700/50, 300/50), Vec2(0, 1), Vec2(0, 0),
    //     80, 80, white, 1, 0.5f, RigidbodyType::Dynamic));

    // auto slope = std::make_unique<Box>(
    //     Vec2(200, 80), Vec2(0, 0), Vec2(0, 0),
    //     500, 40, skyBlue, 100, 0, RigidbodyType::Static);
    // slope->RotateTo(-PI / 6.0f);
    // world.Add(std::move(slope));

    // world.Add(std::make_unique<Box>(
    //     Vec2(50, 120), Vec2(5, 1), Vec2(0, 0),
    //     40, 40, white, 2000, 1, RigidbodyType::Dynamic));

    // world.Add(std::make_unique<Ball>(
    //     Vec2(150, 120), Vec2(0, 0), Vec2(0, 0),
    //     50, warmRed, 50, 0.5f, RigidbodyType::Dynamic));

    // world.Add(std::make_unique<Ball>(
    //     Vec2(250, 120), Vec2(-1, 0), Vec2(0, 0),
    //     20, yellow, 5, 0.2f, RigidbodyType::Dynamic));
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
