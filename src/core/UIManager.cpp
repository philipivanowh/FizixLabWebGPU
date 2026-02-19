#include "UIManager.hpp"
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <iterator>

namespace
{
    ImVec4 ToImGuiColor(const std::array<float, 4> &color)
    {
        return ImVec4(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, color[3] / 255.0f);
    }

    void FromImGuiColor(const ImVec4 &color, std::array<float, 4> &outColor)
    {
        outColor[0] = color.x * 255.0f;
        outColor[1] = color.y * 255.0f;
        outColor[2] = color.z * 255.0f;
        outColor[3] = color.w * 255.0f;
    }
}

void UIManager::InitializeImGui(Renderer &renderer)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(renderer.GetWindow(), true);
    ImGui_ImplWGPU_Init(renderer.GetDevice(), 3, renderer.GetSurfaceFormat());
}

void UIManager::TerminateImGui()
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::BeginImGuiFrame()
{
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::EndImGuiFrame(Renderer &renderer)
{
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderer.GetRenderPass());
}

void UIManager::RenderMainControls(std::size_t bodyCount, Settings &settings)
{
    RenderSimulationControlsWindow(bodyCount, settings);
}

void UIManager::RenderSpawner()
{
    RenderSpawnerWindow();
}

void UIManager::RenderSimulationControlsWindow(std::size_t bodyCount, Settings &settings)
{
    ImGui::Begin("Simulation Controls");

    ImGui::Text("Bodies: %zu", bodyCount);
    ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

    ImGui::Separator();

    // ── Time Control ──────────────────────────────────────────
    ImGui::Text("Time Control");

    if (ImGui::Button(settings.recording ? "⏹ Stop Recording" : "⏺ Start Recording"))
    {
        settings.recording = !settings.recording;
        if (!settings.recording)
            Engine::GetRecorder().Clear(); // optional: clear on stop
    }

    ImGui::SameLine();

    // Show what interval means in plain terms
    const float fps = ImGui::GetIO().Framerate;
    const float secondsOfRewind = (Engine::GetRecorder().HistorySize() * settings.recordInterval) / fps;

    ImGui::SliderInt("Record Interval", &settings.recordInterval, 1, 10);

    // Helper text so the user understands the tradeoff
    if (settings.recordInterval == 1)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                           "Full detail — high memory use");
    else if (settings.recordInterval <= 3)
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                           "Balanced");
    else
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Low detail — longer rewind window");

    // Live stats
    ImGui::Text("Captured frames : %zu", Engine::GetRecorder().HistorySize());
    ImGui::Text("Rewind window   : %.1fs", secondsOfRewind);
    

        // Show history size so user knows something is being captured
        if (settings.recording)
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Recording... (%zu frames)",
                               Engine::GetRecorder().HistorySize());

    // Only show rewind button if there's something to rewind
    if (Engine::GetRecorder().HasHistory())
    {
        ImGui::Button("◀◀ Rewind (hold)");
        settings.rewinding = ImGui::IsItemActive();
    }
    else
    {
        ImGui::TextDisabled("◀◀ Rewind (no history)");
        settings.rewinding = false;
    }

    ImGui::SameLine();
    // Pause / Resume button
    if (ImGui::Button(settings.paused ? "Resume" : "Pause"))
        settings.paused = !settings.paused;

    ImGui::SameLine();

    // Step one frame forward while paused
    if (ImGui::Button("Step") && settings.paused)
        settings.stepOneFrame = true; // consumed below

    // Speed slider  0.01x ──────────────── 3x
    ImGui::SliderFloat("Speed", &settings.timeScale, 0.01f, 3.0f, "%.2fx");

    // Preset buttons for common speeds
    if (ImGui::Button("0.1x"))
        settings.timeScale = 0.1f;
    ImGui::SameLine();
    if (ImGui::Button("0.25x"))
        settings.timeScale = 0.25f;
    ImGui::SameLine();
    if (ImGui::Button("0.5x"))
        settings.timeScale = 0.5f;
    ImGui::SameLine();
    if (ImGui::Button("1x"))
        settings.timeScale = 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("2x"))
        settings.timeScale = 2.0f;
    // ─────────────────────────────────────────────────────────

    ImGui::Separator();

    const char *dragType[] = {"PerciseDrag", "PhysicsDrag"};
    int dragIndex = static_cast<int>(settings.dragMode);
    ImGui::Combo("Drag Mode:", &dragIndex, dragType, std::size(dragType));
    settings.dragMode = static_cast<DragMode>(dragIndex);

    if (ImGui::Button("Clear Bodies"))
    {
        Engine::ClearBodies();
    }

    ImGui::End();
}

void UIManager::RenderSpawnerWindow()
{
    ImGui::Begin("Object Spawner");

    ImGui::Text("Spawn Settings");
    ImGui::Separator();

    RenderSpawnBasics();
    RenderSpawnPhysicsControls();
    RenderSpawnSizeControls();
    RenderSpawnActions();

    ImGui::End();
}

void UIManager::RenderSpawnBasics()
{
    const char *shapes[] = {"Ball", "Incline", "Box", "Canon"};
    int shapeIndex = static_cast<int>(spawnSettings.shapeType);
    ImGui::Combo("Shape", &shapeIndex, shapes, std::size(shapes));
    spawnSettings.shapeType = static_cast<shape::ShapeType>(shapeIndex);

    ImGui::DragFloat2("Position", &spawnSettings.position.x, 1.0f);
    ImGui::DragFloat2("Velocity", &spawnSettings.velocity.x, 0.1f);
}

void UIManager::RenderSpawnPhysicsControls()
{
    const char *rbType[] = {"Static", "Dynamic"};

    if (spawnSettings.shapeType == shape::ShapeType::Box || spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        ImGui::DragFloat("Mass", &spawnSettings.mass, 0.5f, 0.1f, 1000.0f);
        ImGui::SliderFloat("Restitution", &spawnSettings.restitution, 0.0f, 1.0f);
    }

    ImVec4 bodyColor = ToImGuiColor(spawnSettings.color);
    ImGui::ColorEdit3("Body color", reinterpret_cast<float *>(&bodyColor));
    FromImGuiColor(bodyColor, spawnSettings.color);

    if (spawnSettings.shapeType == shape::ShapeType::Incline)
    {
        ImGui::DragFloat("Static Friction", &spawnSettings.staticFriction, 0.05f, 0.0f, 2.0f);
        ImGui::DragFloat("Kinetic Friction", &spawnSettings.kineticFriction, 0.05f, 0.0f, 2.0f);
    }

    if (spawnSettings.shapeType == shape::ShapeType::Box || spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        int bodyTypeIndex = static_cast<int>(spawnSettings.bodyType);
        ImGui::Combo("Rigidbody Type", &bodyTypeIndex, rbType, 2);
        spawnSettings.bodyType = static_cast<physics::RigidbodyType>(bodyTypeIndex);
    }
}

void UIManager::RenderSpawnSizeControls()
{
    if (spawnSettings.shapeType == shape::ShapeType::Box)
    {
        ImGui::DragFloat("Width", &spawnSettings.boxWidth, 1.0f, 1.0f, 500.0f);
        ImGui::DragFloat("Height", &spawnSettings.boxHeight, 1.0f, 1.0f, 500.0f);
        return;
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        ImGui::DragFloat("Radius", &spawnSettings.radius, 1.0f, 1.0f, 250.0f);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Incline)
    {
        ImGui::DragFloat("Base", &spawnSettings.base, 1.0f, 1.0f, 500.0f);
        ImGui::DragFloat("Angle(Degree)", &spawnSettings.angle, 1.0f, 1.0f, 500.0f);
        ImGui::Checkbox("Flip", &spawnSettings.flip);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Canon)
    {
        ImGui::DragFloat("Anlge(Degree)", &spawnSettings.angle, 1.0f, 1.0f, 500.0f);
    }
}

void UIManager::RenderSpawnActions()
{
    if (ImGui::Button("Spawn Object"))
    {
        spawnRequestPending = true;
    }
}

bool UIManager::ConsumeSpawnRequest(SpawnSettings &out)
{
    if (!spawnRequestPending)
    {
        return false;
    }

    out = spawnSettings;
    spawnRequestPending = false;
    return true;
}
