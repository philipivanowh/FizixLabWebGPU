#include "core/Engine.hpp"
#include <imgui.h>

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Canon.hpp"
#include "shape/Incline.hpp"
#include "shape/Shape.hpp"
#include "math/Math.hpp"

#include <array>
#include <memory>
#include <cmath>    // sqrtf
#include <string>

using math::PI;
using math::Vec2;
using physics::ForceType;
using physics::RigidbodyType;

Renderer Engine::renderer;
//TextRenderer Engine::textRenderer;
World Engine::world;
Settings Engine::settings;
UIManager Engine::uiManager;
physics::Rigidbody *Engine::draggedBody = nullptr;
math::Vec2 Engine::mouseWorld{};
bool Engine::mouseDownLeft = false;
bool Engine::mouseDownRight = false;
math::Vec2 Engine::mouseInitialPos;
math::Vec2 Engine::mouseDeltaScale;

static float DistanceVec2(const Vec2& a, const Vec2& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ============================================================
// Draw dx/dy tooltip at mouse (auto-flip at screen edges)
// ============================================================
static void DrawDxDyTooltipAtMouseCorner(float dx, float dy, float dist)
{
    ImGuiIO& io = ImGui::GetIO();

    // Mouse position in screen space
    ImVec2 m = io.MousePos;

    char buf[128];
    // 3-line text inside one tooltip box (keeps measurement compact)
    // If you want integers instead: change %.1f to %d with casts.
    snprintf(buf, sizeof(buf), "dx: %.1f\ndy: %.1f\ndist: %.1f", dx, dy, dist);

    // Measure multi-line size
    ImVec2 ts = ImGui::CalcTextSize(buf, nullptr, false, -1.0f);

    const float pad = 10.0f;
    ImVec2 p(m.x + pad, m.y + pad);

    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;

    // Flip horizontally if off right edge
    if (p.x + ts.x > sw) p.x = m.x - ts.x - pad;

    // Flip vertically if off bottom edge
    if (p.y + ts.y > sh) p.y = m.y - ts.y - pad;

    // Clamp to stay on-screen
    if (p.x < 0) p.x = 0;
    if (p.y < 0) p.y = 0;
    if (p.x + ts.x > sw) p.x = sw - ts.x;
    if (p.y + ts.y > sh) p.y = sh - ts.y;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 bg = IM_COL32(0, 0, 0, 200);
    ImU32 fg = IM_COL32(255, 255, 255, 255);

    const float boxPadX = 6.0f;
    const float boxPadY = 5.0f;

    ImVec2 boxMin(p.x - boxPadX, p.y - boxPadY);
    ImVec2 boxMax(p.x + ts.x + boxPadX, p.y + ts.y + boxPadY);

    dl->AddRectFilled(boxMin, boxMax, bg, 5.0f);
    dl->AddText(p, fg, buf);
}

bool Engine::Initialize()
{
    if (!renderer.Initialize())
    {
        return false;
    }

    // if(!textRenderer.Initialize_fonts(renderer.GetDevice()))
    // {
    //     return false;
    // }

    uiManager.InitializeImGui(renderer);
    AddDefaultObjects();
    // ComparisonScene();
    // InclineProblemScene();
    return true;
}

void Engine::Shutdown()
{
    renderer.Terminate();
    uiManager.TerminateImGui();
}

void Engine::CheckSpawning()
{
    SpawnSettings req;
    if (uiManager.ConsumeSpawnRequest(req))
    {
        if (req.shapeType == shape::ShapeType::Box)
        {
            world.Add(std::make_unique<shape::Box>(
                req.position,
                req.velocity,
                math::Vec2(0.0f, 0.0f),
                req.boxWidth,
                req.boxHeight,
                req.color,
                req.mass,
                req.restitution,
                req.bodyType));
        }
        else if (req.shapeType == shape::ShapeType::Ball)
        {
            world.Add(std::make_unique<shape::Ball>(
                req.position,
                req.velocity,
                math::Vec2(0.0f, 0.0f),
                req.radius,
                req.color,
                req.mass,
                req.restitution,
                req.bodyType));
        }
        else if (req.shapeType == shape::ShapeType::Incline)
        {
            world.Add(std::make_unique<shape::Incline>(
                req.position,
                req.velocity,
                math::Vec2(0.0f, 0.0f),
                req.base,
                req.angle,
                req.flip,
                req.color,
                req.staticFriction,
                req.kineticFriction));
        }
        else if (req.shapeType == shape::ShapeType::Canon)
        {
            world.Add(std::make_unique<shape::Canon>(
                req.position,
                req.angle,
                req.color));
        }
    }
}

void Engine::Update(float deltaMs, int iterations)
{
    GLFWwindow *window = renderer.GetWindow();

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    mouseWorld = Vec2(static_cast<float>(mx),
                      static_cast<float>(h - my));

    bool pressedLeft = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool pressedRight = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // Mouse down
    if (pressedLeft && !mouseDownLeft)
    {
        mouseDownLeft = true;
        draggedBody = world.PickBody(mouseWorld);
    }

    if (pressedRight && !mouseDownRight)
    {
        mouseDownRight = true;
        mouseInitialPos = mouseWorld.Clone();
    }

    // Mouse up
    if (!pressedLeft)
    {
        mouseDownLeft = false;
        draggedBody = nullptr;
    }

    if (!pressedRight)
    {
        mouseDownRight = false;
        mouseInitialPos = math::Vec2();
        mouseDeltaScale = math::Vec2();
    }

    if (mouseDownRight)
    {
        mouseDeltaScale = mouseWorld - mouseInitialPos;
    }

    // Apply drag force
    if (draggedBody && draggedBody->bodyType == RigidbodyType::Dynamic)
    {
        switch (settings.dragMode)
        {
        case DragMode::percisionDrag:
            draggedBody->TranslateTo(mouseWorld);
            break;
        case DragMode::physicsDrag:
        {
            const float stiffness = 2000.0f;
            // const float damping = 2.0f * std::sqrt(stiffness * draggedBody->mass);
            const float damping = 5.0f * std::sqrt(stiffness * math::Clamp(draggedBody->mass, 20, 100) / 20);

            Vec2 delta = mouseWorld - draggedBody->pos;
            draggedBody->dragForce = (delta * stiffness - draggedBody->linearVel * damping) * draggedBody->mass;
            std::cout << draggedBody->dragForce.x << std::endl;
            break;
        }
        }
    }

    world.Update(deltaMs, iterations);
    CheckSpawning();
}

void Engine::Render()
{
    renderer.BeginFrame();
    world.Draw(renderer);

    // Existing right-click rectangle (keep)
    if (mouseDeltaScale.Length() != 0.0f && mouseDownRight)
        renderer.DrawMeasuringRectangle(mouseInitialPos, mouseDeltaScale);

    // ------------------------------------------------------------
    // ImGui
    // ------------------------------------------------------------
    uiManager.BeginImGuiFrame();

    // Main UI
    uiManager.RenderMainControls(world.RigidbodyCount(), settings);
    uiManager.RenderSpawner();

    // ------------------------------------------------------------
    // Measurement tooltip at mouse corner (auto flip)
    // ------------------------------------------------------------
    if (mouseDownRight)
    {
        const float dx = mouseWorld.x - mouseInitialPos.x;
        const float dy = mouseWorld.y - mouseInitialPos.y;
        const float dist = DistanceVec2(mouseInitialPos, mouseWorld);

        DrawDxDyTooltipAtMouseCorner(dx, dy, dist);
    }

    uiManager.EndImGuiFrame(renderer);

    renderer.EndFrame();
}

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

void Engine::AddDefaultObjects()
{
    using physics::RigidbodyType;
    using shape::Ball;
    using shape::Box;

    const std::array<float, 4> warmRed{0.070588f, 0.180392f, 0.219608f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> skyBlue{0.313726f, 0.627451f, 1.0f, 1.0f};
    const std::array<float, 4> yellow{1.0f, 0.784314f, 0.078431f, 1.0f};

    world.Add(std::make_unique<shape::Box>(
        Vec2(700.0f, 200.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        1600.0f,
        50.0f,
        skyBlue,
        100.0f,
        0.0f,
        RigidbodyType::Static));

    world.Add(std::make_unique<shape::Box>(
        Vec2(700.0f, 300.0f),
        Vec2(0.0f, 1.0f),
        Vec2(0.0f, 0.0f),
        80.0f,
        80.0f,
        white,
        1.0f,
        0.5f,
        RigidbodyType::Dynamic));

    auto slope = std::make_unique<shape::Box>(
        Vec2(1000.0f, 400.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        500.0f,
        40.0f,
        skyBlue,
        100.0f,
        0.0f,
        RigidbodyType::Static);
    const float slopeAngle = -PI / 6.0f;
    slope->RotateTo(slopeAngle);
    world.Add(std::move(slope));

    world.Add(std::make_unique<shape::Box>(
        Vec2(500.0f, 700.0f),
        Vec2(5.0f, 1.0f),
        Vec2(0.0f, 0.0f),
        200.0f,
        200.0f,
        white,
        2000.0f,
        1.0f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(896.0f, 670.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        70.0f,
        70.0f,
        yellow,
        15.0f,
        0.5f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(906.0f, 570.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        70.0f,
        70.0f,
        warmRed,
        50.0f,
        0.5f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Ball>(
        Vec2(906.0f, 770.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        50.0f,
        warmRed,
        50.0f,
        0.5f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Ball>(
        Vec2(500.0f, 700.0f),
        Vec2(-1.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        20.0f,
        warmRed,
        5.0f,
        0.2f,
        RigidbodyType::Dynamic));
}

void Engine::ComparisonScene()
{
    const std::array<float, 4> warmRed{0.070588f, 0.180392f, 0.219608f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> skyBlue{0.313726f, 0.627451f, 1.0f, 1.0f};
    const std::array<float, 4> yellow{1.0f, 0.784314f, 0.078431f, 1.0f};

    world.Add(std::make_unique<shape::Box>(
        Vec2(700.0f, 400.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        1600.0f,
        50.0f,
        skyBlue,
        2000.0f,
        0.0f,
        RigidbodyType::Static));

    world.Add(std::make_unique<shape::Box>(
        Vec2(100.0f, 500.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        25.0f,
        25.0f,
        warmRed,
        10.0f,
        0.0f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(150.0f, 500.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        25.0f,
        25.0f,
        white,
        1000.0f,
        0.0f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(200.0f, 500.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        25.0f,
        25.0f,
        skyBlue,
        100.0f,
        0.0f,
        RigidbodyType::Dynamic));

    world.Add(std::make_unique<shape::Box>(
        Vec2(250.0f, 500.0f),
        Vec2(1.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        25.0f,
        25.0f,
        yellow,
        100.0f,
        0.0f,
        RigidbodyType::Dynamic));
}

void Engine::InclineProblemScene()
{
    const std::array<float, 4> warmRed{0.705882f, 0.164706f, 0.400000f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    const std::array<float, 4> cannonBlue{0.352941f, 0.549020f, 0.862745f, 1.0f};

    world.Add(std::make_unique<shape::Box>(
        Vec2(700.0f, 200.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        1600.0f,
        50.0f,
        warmRed,
        100.0f,
        0.0f,
        RigidbodyType::Static));

    world.Add(std::make_unique<shape::Incline>(
        Vec2(800.0f, 400.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        600.0f,
        20.0f,
        true,
        warmRed,
        0.1,
        0.1));

    world.Add(std::make_unique<shape::Canon>(
        Vec2(520.0f, 280.0f),
        30.0f,
        cannonBlue));

    world.Add(std::make_unique<shape::Box>(
        Vec2(970.0f, 590.0f),
        Vec2(0.0f, 0.0f),
        Vec2(0.0f, 0.0f),
        100.0f,
        100.0f,
        white,
        100.0f,
        1.0f,
        RigidbodyType::Dynamic));
}

void Engine::ClearBodies()
{
    world.ClearObjects();
}

// void Engine::SpawnRandomBox()
// {
// }

// void Engine::SpawnRandomBall()
// {
// }
