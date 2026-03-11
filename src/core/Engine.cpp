#include "core/Engine.hpp"
#include <imgui.h>

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Cannon.hpp"
#include "shape/Incline.hpp"
#include "shape/Spring.hpp"
#include "shape/Trigger.hpp"
#include "shape/Thruster.hpp"
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

// ================================================================
// Static members
// ================================================================
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
int Engine::dragThresholdFrames = 15;
bool Engine::wasSelectedBodyJustClicked = false;
float Engine::simulationTimeMs = 0.0f;

// ── Thruster drag state ───────────────────────────────────────────
shape::Thruster *Engine::draggingThruster = nullptr;
physics::Rigidbody *Engine::thrusterSnapTarget = nullptr;

// -- Rope drag state -------------
shape::RopeNode *Engine::draggedRopeNode = nullptr;
shape::Rope *Engine::draggedRope = nullptr;

// ================================================================
// INIT / SHUTDOWN
// ================================================================
bool Engine::Initialize()
{
    if (!renderer.Initialize(&settings, Engine::Scroll_Feedback))
        return false;

    uiManager.InitializeImGui(renderer, &settings, &world);
    world.Initialize(&settings);
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
    else if (req.shapeType == shape::ShapeType::Trigger)
    {
        world.Add(std::make_unique<shape::Trigger>(
            req.position, req.velocity, Vec2(0, 0),
            req.boxWidth, req.boxHeight,
            req.color, req.mass, req.restitution, RigidbodyType::Static, req.triggerAction));
    }
    else if (req.shapeType == shape::ShapeType::Thruster)
    {
        // Spawn a free-floating thruster at cursor — user then drags it onto a body
        world.Add(std::make_unique<shape::Thruster>(
            req.position,
            req.angle,         // default angle: thrust upward
            req.thrusterForce, // default magnitude
            true,              // body-relative
            false,             // always-on
            req.thrustKey));
    }
    // Rope is very unstable at this stage so it will be avoided uncomment it if you want to see it
    //  else if (req.shapeType == shape::ShapeType::Rope)
    //  {
    //      shape::Rope::SpawnParams rp;
    //      rp.startWorld = req.position;
    //      rp.endWorld = req.ropeEndPosition;
    //      rp.segmentCount = req.ropeSegments;
    //      rp.segmentMass = req.mass;
    //      rp.stiffness = req.ropeStiffness;
    //      rp.stickIterations = req.ropeStickIterations;
    //      rp.damping = req.ropeDamping;
    //      rp.pinStart = req.ropePinStart;
    //      rp.pinEnd = req.ropePinEnd;
    //      world.AddRope(rp);
    //  }
    else if (req.shapeType == shape::ShapeType::Spring)
    {
        const float PPM = SimulationConstants::PIXELS_PER_METER;

        auto *sp = new shape::Spring(
            req.position,
            req.springAngle,
            req.springRestLength * PPM,
            req.springStiffness,
            req.color);
        sp->damping = req.springDamping;
        sp->coilCount = req.springCoilCount;
        if (req.springInitialCompression > 0.001f)
            sp->SetCompression(req.springInitialCompression);
        world.Add(std::unique_ptr<shape::Spring>(sp));
    }

    // Always nudge physics forward after a spawn so the object
    // visibly appears even when paused / not recording
    spawnNudgeFrames = 1;

    // Rewind history is invalid now that body count changed
    recorder.Clear();
    simulationTimeMs = 0.0f;
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

// ================================================================
// UPDATE
// ================================================================
void Engine::Update(float deltaMs, int iterations)
{
    glfwPollEvents();

    GLFWwindow *window = renderer.GetWindow();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        renderer.Terminate();

    // ── Mouse position ────────────────────────────────────────────
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    const float scaledMx = static_cast<float>(mx);
    const float scaledMy = static_cast<float>(my);

    mouseScreen = Vec2(scaledMx, scaledMy);

    bool pressedControl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    bool mouseButtonLeft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool mouseButtonRight = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    bool pressedLeft = false;
    bool pressedRight = false;
#ifdef __APPLE__
    pressedRight = pressedControl && mouseButtonLeft;
    pressedLeft = mouseButtonLeft && !pressedControl;
    (void)mouseButtonRight;
#else
    pressedRight = mouseButtonRight;
    pressedLeft = mouseButtonLeft;
    (void)pressedControl;
#endif

    ImGuiIO &io = ImGui::GetIO();
    const bool overUI = io.WantCaptureMouse;
    const bool overUIKeyboard = io.WantCaptureKeyboard;

    // ── Hotkeys ───────────────────────────────────────────────────
    bool keyP = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (keyP && !prevKeyP)
        settings.paused = !settings.paused;
    prevKeyP = keyP;

    bool keyO = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (keyO && !prevKeyO)
        settings.stepOneFrame = true;
    prevKeyO = keyO;

    bool keyR = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (keyR && !prevKeyR)
        settings.recording = true;
    prevKeyR = keyR;

    // ── Zoom controls ─────────────────────────────────────────────
    if (!overUIKeyboard)
    {
        const float zoomStep = 0.05f;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            settings.zoom *= (1.0f + zoomStep);

        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            settings.zoom *= (1.0f - zoomStep);

        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_0) == GLFW_PRESS)
        {
            settings.zoom = 1.0f;
            cameraOffset = Vec2(0.0f, 0.0f);
        }
        settings.zoom = math::Clamp(settings.zoom,
                                    SimulationConstants::MIN_ZOOM,
                                    SimulationConstants::MAX_ZOOM);
    }

    renderer.SetZoom(settings.zoom);

    // ── World space calculation with camera offset ────────────────
    const float zoom = settings.zoom;
    const float cx = static_cast<float>(winW) * 0.5f;
    const float cy = static_cast<float>(winH) * 0.5f;

    const float zoomedX = (scaledMx - cx) / zoom + cx + cameraOffset.x;
    const float zoomedY = (scaledMy - cy) / zoom + cy - cameraOffset.y;
    mouseWorld = Vec2(zoomedX, static_cast<float>(winH) - zoomedY);

    // ── Mouse down ────────────────────────────────────────────────
    if (pressedLeft && !mouseDownLeft && !overUI)
    {
        mouseDownLeft = true;

        // ── NEW: rope node pick — checked before regular PickBody ─────────
        // Rope nodes are tiny so we offer a generous 20 px grab radius.
        {
            constexpr float ROPE_PICK_RADIUS = 20.0f;
            for (auto &rope : world.GetRopes())
            {
                shape::RopeNode *node = rope.PickNode(mouseWorld, ROPE_PICK_RADIUS);
                if (node)
                {
                    draggedRopeNode = node;
                    draggedRope = &rope;
                    isPanning = false;
                    continue;
                }
            }
        }

        {
            physics::Rigidbody *clickedBody = world.PickBody(mouseWorld);

            if (clickedBody == selectedBody && selectedBody != nullptr)
            {
                if (auto *t = dynamic_cast<shape::Thruster *>(selectedBody))
                {
                    draggingThruster = t;
                    thrusterSnapTarget = nullptr;
                    t->Detach();
                    isPanning = false;
                }
                else
                {
                    draggedBody = selectedBody;
                    staticDragOffset = draggedBody->pos - mouseWorld;
                    isPanning = false;
                }
                wasSelectedBodyJustClicked = false;
                selectedBodyHoldFrames = 0;
            }
            else if (clickedBody)
            {
                if (selectedBody)
                    selectedBody->isHighlighted = false;
                selectedBody = clickedBody;
                selectedBody->isHighlighted = true;
                draggedBody = nullptr;
                draggingThruster = nullptr;
                wasSelectedBodyJustClicked = true;
                selectedBodyHoldFrames = 0;
                isPanning = false;
            }
            else
            {
                if (selectedBody)
                {
                    selectedBody->isHighlighted = false;
                    selectedBody = nullptr;
                }
                draggedBody = nullptr;
                draggingThruster = nullptr;
                wasSelectedBodyJustClicked = false;
                selectedBodyHoldFrames = 0;
                isPanning = true;
                panStartMouse = mouseScreen;
                panStartCamera = cameraOffset;
            }
        }
    }

    // ── Hold counter — promote select → drag ──────────────────────
    if (!draggingThruster && mouseDownLeft && selectedBody && !draggedBody &&
        !isPanning && !draggedRopeNode) // ← don't promote while rope-dragging
    {
        selectedBodyHoldFrames++;

        auto *thruster = dynamic_cast<shape::Thruster *>(selectedBody);
        float distToMouse = (mouseWorld - selectedBody->pos).Length();

        bool shouldStartDrag = (selectedBodyHoldFrames >= dragThresholdFrames);
        if (thruster && thruster->IsAttached() && distToMouse > 40.0f)
            shouldStartDrag = true;

        if (shouldStartDrag)
        {
            if (thruster)
            {
                draggingThruster = thruster;
                thrusterSnapTarget = nullptr;
                thruster->Detach();
            }
            else
            {
                draggedBody = selectedBody;
                staticDragOffset = draggedBody->pos - mouseWorld;
            }
        }
    }

    // ── Mouse release ─────────────────────────────────────────────
    if (!pressedLeft)
    {
        // ── NEW: rope drag release ────────────────────────────────
        if (draggedRope)
        {
            draggedRope->ReleaseNode();
            draggedRope = nullptr;
            draggedRopeNode = nullptr;
        }

        if (draggingThruster)
        {
            if (thrusterSnapTarget)
            {
                const float c = std::cos(-thrusterSnapTarget->rotation);
                const float s = std::sin(-thrusterSnapTarget->rotation);
                const Vec2 diff = draggingThruster->pos - thrusterSnapTarget->pos;
                const Vec2 localOffset(diff.x * c - diff.y * s,
                                       diff.x * s + diff.y * c);

                draggingThruster->AttachTo(thrusterSnapTarget, localOffset,
                                           draggingThruster->GetAngleDegrees());
                draggingThruster->enabled = true;
                thrusterSnapTarget->isHighlighted = false;
            }

            draggingThruster = nullptr;
            thrusterSnapTarget = nullptr;
        }

        mouseDownLeft = false;
        draggedBody = nullptr;
        staticDragOffset = Vec2(0.0f, 0.0f);
        isPanning = false;
        selectedBodyHoldFrames = 0;
        wasSelectedBodyJustClicked = false;
    }

    // ── Camera panning ────────────────────────────────────────────
    if (isPanning && mouseDownLeft)
    {
        Vec2 mouseDelta = mouseScreen - panStartMouse;
        cameraOffset.x = panStartCamera.x - mouseDelta.x / zoom;
        cameraOffset.y = panStartCamera.y + mouseDelta.y / zoom;
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

    // ── NEW: rope node drag — move node to cursor each frame ──────
    if (draggedRopeNode && draggedRope && mouseDownLeft)
        draggedRope->DragNode(draggedRopeNode, mouseWorld);

    // ── Thruster: move with mouse + find snap target ──────────────
    if (draggingThruster && mouseDownLeft)
    {
        draggingThruster->pos = mouseWorld;

        physics::Rigidbody *lastTarget = thrusterSnapTarget;
        const float snapRadius = 60.0f;
        const Vec2 snapped = world.SnapToNearestDynamicObject(mouseWorld, snapRadius);

        if ((snapped - mouseWorld).Length() < snapRadius)
        {
            physics::Rigidbody *candidate = world.PickBody(snapped);
            if (candidate && candidate != draggingThruster &&
                candidate->bodyType == physics::RigidbodyType::Dynamic &&
                !dynamic_cast<shape::Thruster *>(candidate))
            {
                thrusterSnapTarget = candidate;
            }
            else
            {
                thrusterSnapTarget = nullptr;
            }
        }
        else
        {
            thrusterSnapTarget = nullptr;
        }

        if (lastTarget && lastTarget != thrusterSnapTarget)
            lastTarget->isHighlighted = false;
    }

    // ── Normal body dragging ──────────────────────────────────────
    if (draggedBody && !isPanning && !draggingThruster)
    {
        if (draggedBody->bodyType == physics::RigidbodyType::Static)
        {
            if (!(dynamic_cast<shape::Cannon *>(draggedBody)))
                draggedBody->TranslateTo(mouseWorld + staticDragOffset);
            else
                draggedBody->TranslateTo(mouseWorld);
        }
        else if (draggedBody->bodyType == physics::RigidbodyType::Dynamic)
        {
            // if (dynamic_cast<shape::Spring *>(draggedBody))
            // {
            //     // const float stiffness = DragConstants::DRAG_STIFNESS;
            //     // const float damping = 30.0f * std::sqrt(
            //     //                                   stiffness * math::Clamp(10, 20.f, 100.f) / 20.f);
            //     // Vec2 delta = mouseWorld - draggedBody->pos;
            //     // draggedBody->dragForce =
            //     //     (delta * stiffness - draggedBody->linearVel * damping) *
            //     //     draggedBody->mass;
            // }

            switch (settings.dragMode)
            {
            case DragMode::percisionDrag:
                draggedBody->TranslateTo(mouseWorld);
                break;
            case DragMode::physicsDrag:
            {
                const float stiffness = DragConstants::DRAG_STIFNESS;
                const float damping = 30.0f * std::sqrt(
                                                  stiffness * math::Clamp(draggedBody->mass, 20.f, 100.f) / 20.f);
                Vec2 delta = mouseWorld - draggedBody->pos;
                draggedBody->dragForce =
                    (delta * stiffness - draggedBody->linearVel * damping) *
                    draggedBody->mass;
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

        simulationTimeMs += scaledDelta;

        for (auto &obj : world.GetObjects())
        {
            auto *t = dynamic_cast<shape::Thruster *>(obj.get());
            if (!t || !t->IsAttached())
                continue;

            t->SyncToAttachedBody();
            t->isActiveThisFrame = t->enabled &&
                                   (!t->keyHold || glfwGetKey(window, t->fireKey) == GLFW_PRESS);
        }

        if (settings.recording)
        {
            if (++recordFrameCounter % settings.recordInterval == 0)
                recorder.Record(world.CaptureSnapshot(), simulationTimeMs);
        }

        world.Update(scaledDelta, iterations, selectedBody, draggedBody);

        CheckSpawning();
        CheckCannon();
        CheckSpring();
    }
}

void Engine::CheckSpring()
{
    SpringReleaseSettings rel;
    if (!uiManager.ConsumeSpringReleaseRequest(rel))
        return;

    constexpr float LAUNCH_RADIUS = 80.0f; // pixels, tune to taste
    for (auto &rb : world.GetObjects())
    {
        if (rb->bodyType != physics::RigidbodyType::Dynamic)
            continue;
        math::Vec2 toBody = rb->pos - rel.tipPos;
        float dist = toBody.Length();
        if (dist > LAUNCH_RADIUS)
            continue;

        // Dot with launch direction to reject bodies behind the spring
        float alignment = toBody.x * rel.direction.x + toBody.y * rel.direction.y;
        if (alignment < -LAUNCH_RADIUS * 0.25f)
            continue;

        // Apply impulse:  Δv = impulse / mass
        float speed = rel.impulseStrength / rb->mass;
        rb->linearVel += rel.direction * speed;
    }
}

void Engine::CheckCannon()
{
    CannonFireSettings fire;
    if (!uiManager.ConsumeCannonFireRequest(fire))
        return;

    if (settings.autoRecordOnFire && !settings.recording)
    {
        settings.recording = true;
        recorder.Clear();
        simulationTimeMs = 0.0f;
        settings.scrubIndex = -1;
    }

    math::Vec2 vel{fire.vx, fire.vy};

    if (fire.projectileType == CannonFireSettings::ProjectileType::Ball)
    {
        auto ball = std::make_unique<shape::Ball>(
            fire.cannonPos, vel, Vec2(0, 0),
            fire.radius, fire.color, fire.mass, fire.restitution, RigidbodyType::Dynamic);
        physics::Rigidbody *ballPtr = ball.get();
        world.Add(std::move(ball));
        world.StartTrail(ballPtr, 5.0f);
    }
    else if (fire.projectileType == CannonFireSettings::ProjectileType::Box)
    {
        auto box = std::make_unique<shape::Box>(
            fire.cannonPos, vel, Vec2(0, 0),
            fire.boxWidth, fire.boxHeight, fire.color, fire.mass,
            fire.restitution, RigidbodyType::Dynamic);
        physics::Rigidbody *boxPtr = box.get();
        world.Add(std::move(box));
        world.StartTrail(boxPtr, 5.0f);
    }
}

// ================================================================
// RENDER
// ================================================================
void Engine::Render()
{
    if (settings.showFBDLabels)
        renderer.ClearTextLabels();

    renderer.BeginFrame();

    world.SetCameraInfo(cameraOffset, settings.zoom);
    world.Draw(renderer);

    // ── Thruster top-pass ──────────────────────────────────────────
    // Draw every thruster again AFTER all other geometry so it is
    // always visible regardless of spawn order. world.Draw() already
    // drew them once in order; this second pass overpaints them on top.
    // DrawThruster() is cheap (4 triangles + optional flame).
    for (auto &obj : world.GetObjects())
    {
        if (auto *t = dynamic_cast<shape::Thruster *>(obj.get()))
            renderer.DrawThruster(*t);
    }

    // ── ImGui ─────────────────────────────────────────────────────
    uiManager.BeginImGuiFrame();

    if (settings.showFBDLabels)
        renderer.FlushTextLabels();

    uiManager.RenderMainControls(world.RigidbodyCount(), selectedBody);

    physics::Rigidbody *removedObject = uiManager.ConsumeRemovedObject();
    if (removedObject == selectedBody)
        selectedBody = nullptr;

    // If the removed object was the snap target or the thruster being
    // dragged, clear those pointers too so we don't hold dangling refs
    if (removedObject)
    {
        if (removedObject == thrusterSnapTarget)
            thrusterSnapTarget = nullptr;
        if (removedObject == draggingThruster)
        {
            draggingThruster = nullptr;
            thrusterSnapTarget = nullptr;
        }
    }

    uiManager.RenderSpawner();

    ImGuiIO &io = ImGui::GetIO();
    const bool overUI = io.WantCaptureMouse;

    if (!overUI)
    {
        GLFWwindow *window = renderer.GetWindow();
        int winW2, winH2;
        glfwGetWindowSize(window, &winW2, &winH2);
        const float z2 = settings.zoom;
        const float cx2 = static_cast<float>(winW2) * 0.5f;
        const float cy2 = static_cast<float>(winH2) * 0.5f;

        // Shared world→screen helper used by both the measurement
        // overlay and the thruster snap preview below
        auto worldToScreen = [&](const math::Vec2 &w) -> math::Vec2
        {
            float sx = (w.x - cameraOffset.x - cx2) * z2 + cx2;
            float sy = ((static_cast<float>(winH2) - w.y) - cy2 + cameraOffset.y) * z2 + cy2;
            return math::Vec2(sx, sy);
        };

        // Measurement overlay
        const float snapRadius = 25.0f;
        math::Vec2 snappedStartWorld = world.SnapToNearestDynamicObject(mouseInitialPos, snapRadius);
        math::Vec2 snappedEndWorld = world.SnapToNearestDynamicObject(mouseWorld, snapRadius);
        math::Vec2 snappedStartScreen = worldToScreen(snappedStartWorld);
        math::Vec2 snappedEndScreen = worldToScreen(snappedEndWorld);

        uiManager.RenderMeasurementOverlay(
            snappedStartScreen, snappedEndScreen,
            snappedStartWorld, snappedEndWorld,
            mouseDownRight);

        // ── Thruster snap preview ──────────────────────────────────
        // While dragging a thruster, highlight the snap target body and
        // draw a pulsing amber line + circle to show where it will attach.
        if (draggingThruster)
        {
            auto *dl = ImGui::GetForegroundDrawList();

            if (thrusterSnapTarget)
            {
                // Pulse-highlight the target body
                thrusterSnapTarget->isHighlighted = true;

                const math::Vec2 fromW = draggingThruster->pos;
                const math::Vec2 toW = thrusterSnapTarget->pos;
                const ImVec2 from = {worldToScreen(fromW).x, worldToScreen(fromW).y};
                const ImVec2 to = {worldToScreen(toW).x, worldToScreen(toW).y};

                // Pulsing amber dashed line
                const float t = static_cast<float>(glfwGetTime());
                const float alpha = 0.55f + 0.35f * std::sin(t * 6.0f);
                const int ia = static_cast<int>(alpha * 255.0f);

                dl->AddLine(from, to, IM_COL32(242, 153, 26, ia), 1.5f);

                // Circle + filled dot at snap point
                dl->AddCircle(to, 8.0f, IM_COL32(242, 153, 26, 200), 16, 1.5f);
                dl->AddCircleFilled(to, 3.5f, IM_COL32(242, 153, 26, 230));

                // Small label
                dl->AddText({to.x + 11.0f, to.y - 8.0f},
                            IM_COL32(242, 153, 26, 200), "attach here");
            }
            else
            {
                // No snap target in range — draw a soft grey circle around the
                // thruster so the user knows it's in free-drag mode
                const math::Vec2 tPos = draggingThruster->pos;
                const ImVec2 centre = {worldToScreen(tPos).x, worldToScreen(tPos).y};
                dl->AddCircle(centre, 14.0f, IM_COL32(180, 180, 180, 80), 24, 1.2f);
            }
        }
        else if (thrusterSnapTarget)
        {
            // Release happened — clear the highlight on the next frame
            thrusterSnapTarget->isHighlighted = false;
        }

        uiManager.RenderMousePositionOverlay(mouseWorld);
    }

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

    const std::array<float, 4> skyBlue{0.313726f, 0.627451f, 1.0f, 1.0f};

    world.Add(std::make_unique<Box>(
        Vec2(0, 0), Vec2(0, 0), Vec2(0, 0),
        20, 1, skyBlue, 100, 0, RigidbodyType::Static));
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
    // If a thruster was being dragged, clear that state first
    draggingThruster = nullptr;
    thrusterSnapTarget = nullptr;

    world.ClearObjects();
    recorder.Clear();
    simulationTimeMs = 0.0f;
    settings.scrubIndex = -1;
}
