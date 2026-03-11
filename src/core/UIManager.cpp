#include "UIManager.hpp"
#include "core/Engine.hpp"
#include "common/settings.hpp"
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <cmath>
#include <cstdarg>
#include <algorithm>
#include <iterator>

namespace Col
{
    // ── Backgrounds ──────────────────────────────────────────────
    // Deep space with slight blue bias (not true black)
    static constexpr ImVec4 Void{0.055f, 0.070f, 0.110f, 1.00f};     // #0E121C
    static constexpr ImVec4 PanelBg{0.075f, 0.095f, 0.150f, 1.00f};  // #131826
    static constexpr ImVec4 WidgetBg{0.095f, 0.120f, 0.185f, 1.00f}; // #182030

    // Hover feels like faint gravitational distortion
    static constexpr ImVec4 HoverBg{0.140f, 0.185f, 0.290f, 1.00f};  // subtle lift
    static constexpr ImVec4 ActiveBg{0.180f, 0.240f, 0.380f, 1.00f}; // deeper press

    // ── Borders ───────────────────────────────────────────────────
    // Almost invisible separation
    static constexpr ImVec4 Border{0.150f, 0.180f, 0.250f, 0.50f};
    static constexpr ImVec4 BorderFg{0.380f, 0.580f, 0.950f, 0.65f}; // focus ring glow

    // ── Physics Blue — primary action, vectors ────────────────────
    // Inspired by simulation lines
    static constexpr ImVec4 Blue{0.380f, 0.580f, 0.950f, 1.00f}; // cool luminous blue
    static constexpr ImVec4 BlueSoft{0.380f, 0.580f, 0.950f, 0.12f};
    static constexpr ImVec4 BlueHov{0.450f, 0.650f, 1.000f, 1.00f};

    // ── Event Horizon Red — danger / recording ────────────────────
    // Not bright red. More distant star collapse.
    static constexpr ImVec4 Red{0.800f, 0.250f, 0.320f, 1.00f};
    static constexpr ImVec4 RedSoft{0.800f, 0.250f, 0.320f, 0.12f};

    // ── Quantum Green — stable / live ─────────────────────────────
    static constexpr ImVec4 Green{0.300f, 0.750f, 0.550f, 1.00f};
    static constexpr ImVec4 GreenSoft{0.300f, 0.750f, 0.550f, 0.12f};

    // ── Stellar Amber — warning / friction ────────────────────────
    // Muted plasma tone
    static constexpr ImVec4 Amber{0.900f, 0.600f, 0.150f, 1.00f};
    // static constexpr ImVec4 AmberSoft{0.900f, 0.600f, 0.150f, 0.12f};

    // ── Ink — text hierarchy ──────────────────────────────────────
    // No pure white. Ever.
    static constexpr ImVec4 Ink{0.880f, 0.910f, 0.980f, 1.00f}; // soft starlight
    static constexpr ImVec4 InkMid{0.650f, 0.700f, 0.800f, 1.00f};
    static constexpr ImVec4 InkFaint{0.420f, 0.460f, 0.560f, 1.00f};

    // ── Utils ─────────────────────────────────────────────────────
    static ImU32 ToU32(ImVec4 c) { return ImGui::ColorConvertFloat4ToU32(c); }

    // Blend v4 alpha by scalar
    static ImVec4 A(ImVec4 c, float a)
    {
        c.w *= a;
        return c;
    }

    // Smoothstep for easing animations  t in [0,1]
    static float Smooth(float t)
    {
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        return t * t * (3.0f - 2.0f * t);
    }

    // Lerp two ImU32 colours
    static ImU32 LerpU32(ImU32 a, ImU32 b, float t)
    {
        ImVec4 va = ImGui::ColorConvertU32ToFloat4(a);
        ImVec4 vb = ImGui::ColorConvertU32ToFloat4(b);
        return ImGui::ColorConvertFloat4ToU32({va.x + (vb.x - va.x) * t,
                                               va.y + (vb.y - va.y) * t,
                                               va.z + (vb.z - va.z) * t,
                                               va.w + (vb.w - va.w) * t});
    }
}

// ================================================================
//  INTERNAL DRAWING HELPERS
// ================================================================

// Thin vertical rule used as a separator in the top bar
static void VSep()
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    ImGui::GetWindowDrawList()->AddLine(
        {p.x, p.y + 4}, {p.x, p.y + h - 4},
        Col::ToU32(Col::Border), 1.0f);
    ImGui::Dummy({1.0f, h});
}

// All-caps micro-label with a ruled underline that extends to the panel edge
static void SectionHead(const char *text, ImVec4 col = Col::InkFaint)
{
    ImGui::TextColored(col, "%s", text);
    ImVec2 tl = ImGui::GetItemRectMin();
    ImVec2 br = ImGui::GetItemRectMax();
    float rEdge = ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x;
    ImGui::GetWindowDrawList()->AddLine(
        {tl.x, br.y + 2}, {rEdge, br.y + 2},
        Col::ToU32(Col::Border), 1.0f);
    ImGui::Dummy({0, 3.0f});
}

// Two-column label / value row.  keyW is the left column width.
static void KVRow(const char *key, ImU32 valCol, const char *fmt, ...)
{
    ImGui::TextColored(Col::InkMid, "%s", key);
    ImGui::SameLine(92.0f);
    va_list ap;
    va_start(ap, fmt);
    char buf[80];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetCursorScreenPos(), valCol, buf);
    ImGui::Dummy({ImGui::CalcTextSize(buf).x, ImGui::GetTextLineHeight()});
}

static void KVRowVec(const char *key, const math::Vec2 &v, ImU32 valCol)
{
    ImGui::TextColored(Col::InkMid, "%s", key);
    ImGui::SameLine(92.0f);
    char buf[64];
    snprintf(buf, sizeof(buf), "(%.1f,  %.1f)", v.x, v.y);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetCursorScreenPos(), valCol, buf);
    ImGui::Dummy({ImGui::CalcTextSize(buf).x, ImGui::GetTextLineHeight()});
}

// Animated progress bar that smoothly colour-shifts from lowCol to highCol
static void AnimBar(float frac, float w, float h, ImU32 lowCol, ImU32 highCol)
{
    frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
    const float t = static_cast<float>(ImGui::GetTime());
    // Subtle breathing shimmer on the filled segment
    const float shim = 0.94f + 0.06f * Col::Smooth(0.5f + 0.5f * sinf(t * 2.2f));

    ImVec2 p = ImGui::GetCursorScreenPos();
    auto *dl = ImGui::GetWindowDrawList();

    // Track (empty groove)
    dl->AddRectFilled({p.x, p.y}, {p.x + w, p.y + h},
                      Col::ToU32(Col::WidgetBg), h * 0.5f);

    if (frac > 0.001f)
    {
        ImU32 fillCol = Col::LerpU32(lowCol, highCol, frac);
        ImVec4 fc = ImGui::ColorConvertU32ToFloat4(fillCol);
        fc.w *= shim;
        dl->AddRectFilled({p.x, p.y}, {p.x + w * frac, p.y + h},
                          ImGui::ColorConvertFloat4ToU32(fc), h * 0.5f);
    }
    ImGui::Dummy({w, h});
}

// Drop-shadow rectangle helper (draws shadow then fills bg)
static void ShadowRect(ImDrawList *dl, ImVec2 min, ImVec2 max,
                       ImU32 bgCol, ImU32 borderCol, float rounding = 5.0f)
{
    // 3-layer soft shadow
    dl->AddRectFilled({min.x + 3, min.y + 3}, {max.x + 3, max.y + 3},
                      IM_COL32(0, 0, 0, 20), rounding + 1);
    dl->AddRectFilled({min.x + 1.5f, min.y + 1.5f}, {max.x + 1.5f, max.y + 1.5f},
                      IM_COL32(0, 0, 0, 12), rounding);
    dl->AddRectFilled(min, max, bgCol, rounding);
    dl->AddRect(min, max, borderCol, rounding, 0, 1.0f);
}

// ================================================================
//  THEME APPLICATION
// ================================================================
void UIManager::ApplyNeonTheme() // name unchanged so Engine.cpp compiles
{
    ImGuiStyle &s = ImGui::GetStyle();

    // Spacing — roomy, readable, nothing crammed
    s.WindowPadding = {16.0f, 13.0f};
    s.FramePadding = {9.0f, 6.0f};
    s.ItemSpacing = {8.0f, 7.0f};
    s.ItemInnerSpacing = {6.0f, 5.0f};
    s.ScrollbarSize = 8.0f;
    s.GrabMinSize = 14.0f;
    s.IndentSpacing = 18.0f;

    // Rounding — soft but not bubbly
    s.WindowRounding = 0.0f; // panels flush to edges
    s.ChildRounding = 6.0f;
    s.FrameRounding = 6.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding = 6.0f;
    s.TabRounding = 6.0f;

    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;

    ImVec4 *c = s.Colors;

    c[ImGuiCol_WindowBg] = Col::PanelBg;
    c[ImGuiCol_ChildBg] = Col::Void;
    c[ImGuiCol_PopupBg] = Col::Void;

    c[ImGuiCol_Border] = Col::Border;
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};

    c[ImGuiCol_FrameBg] = Col::WidgetBg;
    c[ImGuiCol_FrameBgHovered] = Col::HoverBg;
    c[ImGuiCol_FrameBgActive] = Col::ActiveBg;

    c[ImGuiCol_TitleBg] =
        c[ImGuiCol_TitleBgActive] =
            c[ImGuiCol_TitleBgCollapsed] =
                c[ImGuiCol_MenuBarBg] = Col::PanelBg;

    c[ImGuiCol_ScrollbarBg] = Col::WidgetBg;
    c[ImGuiCol_ScrollbarGrab] = Col::Border;
    c[ImGuiCol_ScrollbarGrabHovered] = Col::InkFaint;
    c[ImGuiCol_ScrollbarGrabActive] = Col::Blue;

    c[ImGuiCol_CheckMark] = Col::Blue;
    c[ImGuiCol_SliderGrab] = Col::Blue;
    c[ImGuiCol_SliderGrabActive] = Col::BlueHov;

    c[ImGuiCol_Button] = Col::WidgetBg;
    c[ImGuiCol_ButtonHovered] = Col::HoverBg;
    c[ImGuiCol_ButtonActive] = Col::ActiveBg;

    c[ImGuiCol_Header] = Col::BlueSoft;
    c[ImGuiCol_HeaderHovered] = Col::HoverBg;
    c[ImGuiCol_HeaderActive] = Col::ActiveBg;

    c[ImGuiCol_Separator] = Col::Border;
    c[ImGuiCol_SeparatorHovered] =
        c[ImGuiCol_SeparatorActive] = Col::Blue;

    c[ImGuiCol_ResizeGrip] = Col::Border;
    c[ImGuiCol_ResizeGripHovered] = Col::Blue;
    c[ImGuiCol_ResizeGripActive] = Col::BlueHov;

    c[ImGuiCol_Tab] = Col::WidgetBg;
    c[ImGuiCol_TabHovered] = Col::HoverBg;
    c[ImGuiCol_TabActive] = Col::BlueSoft;
    c[ImGuiCol_TabUnfocused] = Col::PanelBg;
    c[ImGuiCol_TabUnfocusedActive] = Col::WidgetBg;

    c[ImGuiCol_PlotLines] =
        c[ImGuiCol_PlotHistogram] = Col::Blue;

    c[ImGuiCol_Text] = Col::Ink;
    c[ImGuiCol_TextDisabled] = Col::InkFaint;

    c[ImGuiCol_DragDropTarget] =
        c[ImGuiCol_NavHighlight] = Col::Blue;
}

// ================================================================
//  LIFECYCLE
// ================================================================
void UIManager::InitializeImGui(Renderer &renderer, Settings *settings, World *world)
{
    this->world = world;
    this->settings = settings;
    this->glfwWindow = renderer.GetWindow();
    screenW = settings->windowWidth;
    screenH = settings->windowHeight;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ApplyNeonTheme();
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
    if (glfwWindow)
        KeybindUI::PollKeys(glfwWindow);
    KeybindUI::Update();

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::EndImGuiFrame(Renderer &renderer)
{
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderer.GetRenderPass());
}

// ================================================================
//  PUBLIC ENTRY POINTS
// ================================================================
void UIManager::RenderMainControls(std::size_t bodyCount,
                                   physics::Rigidbody *selectedBody)
{
    ImGuiIO &io = ImGui::GetIO();
    screenW = io.DisplaySize.x;
    screenH = io.DisplaySize.y;
    RenderTopTimelineBar();
    RenderSimPanel(bodyCount);
    RenderInspectorPanel(selectedBody);
}

void UIManager::RenderVisualizationSetting()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    SectionHead("VISUALIZATION");

    // ── Force Display ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, Col::InkMid);
    ImGui::Text("Force Diagrams");
    ImGui::PopStyleColor();

    ImGui::Indent(12.0f);

    // Show FBD Arrows
    bool showArrows = settings->showFBDArrows;
    if (ImGui::Checkbox("##fbd_arrows", &showArrows))
    {
        settings->showFBDArrows = showArrows;
    }
    ImGui::SameLine();
    ImGui::TextColored(showArrows ? Col::Ink : Col::InkFaint, "Force Arrows");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Display free body diagram force vectors");
    }

    // Show FBD Labels
    bool showLabels = settings->showFBDLabels;
    if (ImGui::Checkbox("##fbd_labels", &showLabels))
    {
        settings->showFBDLabels = showLabels;
    }
    ImGui::SameLine();
    ImGui::TextColored(showLabels ? Col::Ink : Col::InkFaint, "Force Labels");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Display force magnitude and symbol (e.g., Fg = 98.1 N)");
    }

    ImGui::Unindent(12.0f);

    ImGui::Spacing();
}

void UIManager::RenderSpawner() { RenderSpawnerPanel(); }

// ================================================================
//  SMALL HELPERS
// ================================================================
bool UIManager::NeonButton(const char *label, bool active)
{
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, Col::BlueSoft);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text, Col::Blue);
        ImGui::PushStyleColor(ImGuiCol_Border, Col::BorderFg);
    }
    bool clicked = ImGui::Button(label);
    if (active)
        ImGui::PopStyleColor(5);
    return clicked;
}

void UIManager::PulsingRecordDot()
{
    const float t = static_cast<float>(ImGui::GetTime());
    // Smooth ease-in/out beat — not harsh sin
    const float beat = Col::Smooth(0.5f + 0.5f * sinf(t * 4.0f));
    const float r = 4.5f + 1.2f * beat;

    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.x += 7.0f;
    p.y += ImGui::GetTextLineHeight() * 0.5f;

    // Outer halo
    ImVec4 halo = Col::A(Col::Red, 0.15f * beat);
    dl->AddCircleFilled(p, r + 3.5f, Col::ToU32(halo));
    // Core dot
    ImVec4 core = Col::A(Col::Red, 0.80f + 0.20f * beat);
    dl->AddCircleFilled(p, r, Col::ToU32(core));

    ImGui::Dummy({18.0f, ImGui::GetTextLineHeight()});
}

// ================================================================
//  TOP TIMELINE BAR
// ================================================================
void UIManager::RenderTopTimelineBar()
{
    const float BAR_H = 72.0f;
    const float t = static_cast<float>(ImGui::GetTime());

    ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({screenW, BAR_H}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12.0f, 8.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.0f, 4.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::Void);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::Border);

    ImGui::Begin("##TopBar", nullptr, flags);

    // Hand-drawn bottom border — single crisp line
    ImGui::GetForegroundDrawList()->AddLine(
        {0, BAR_H - 1}, {screenW, BAR_H - 1},
        Col::ToU32(Col::Border), 1.0f);

    // ── Row 1: transport controls ─────────────────────────────────

    // REC — red when active, animates border intensity
    const bool isRec = settings->recording;
    {
        // Animated border alpha: pulses when recording
        float borderA = isRec
                            ? (0.45f + 0.35f * Col::Smooth(0.5f + 0.5f * sinf(t * 4.0f)))
                            : 0.0f;
        ImGui::PushStyleColor(ImGuiCol_Button,
                              isRec ? Col::RedSoft : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              isRec ? Col::A(Col::Red, 0.22f) : Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              isRec ? Col::A(Col::Red, 0.35f) : Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              isRec ? Col::Red : Col::Ink);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              isRec ? Col::A(Col::Red, borderA) : Col::Border);

        if (ImGui::Button("  REC  "))
        {
            settings->recording = !settings->recording;
            if (!settings->recording)
            {
                Engine::GetRecorder().Clear();
                Engine::ResetSimTime();
            }
            settings->scrubIndex = -1;
        }
        ImGui::PopStyleColor(5);
    }

    if (isRec)
    {
        ImGui::SameLine(0, 4);
        PulsingRecordDot();
    }

    ImGui::SameLine(0, 12);
    VSep();
    ImGui::SameLine(0, 12);

    // PAUSE / PLAY — blue tinted border when paused
    {
        const bool paused = settings->paused;
        // Smooth colour lerp: border animates to blue when paused
        ImGui::PushStyleColor(ImGuiCol_Text,
                              paused ? Col::Blue : Col::Ink);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              paused ? Col::A(Col::Blue, 0.55f) : Col::Border);
        ImGui::PushStyleColor(ImGuiCol_Button,
                              paused ? Col::BlueSoft : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              paused ? Col::A(Col::Blue, 0.20f) : Col::HoverBg);

        if (ImGui::Button(paused ? "  PLAY  " : " PAUSE  "))
            settings->paused = !settings->paused;
        ImGui::PopStyleColor(4);
    }

    ImGui::SameLine(0, 6);

    // STEP — greyed when not paused
    if (!settings->paused)
        ImGui::BeginDisabled();
    if (ImGui::Button(" STEP ") && settings->paused)
        settings->stepOneFrame = true;
    if (!settings->paused)
        ImGui::EndDisabled();

    ImGui::SameLine(0, 12);
    VSep();
    ImGui::SameLine(0, 12);

    // Speed
    ImGui::TextColored(Col::InkMid, "SPEED");
    ImGui::SameLine(0, 8);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(130.0f);
    ImGui::SliderFloat("##spd", &settings->timeScale, 0.01f, 3.0f, "%.2fx");
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 6);
    if (ImGui::Button("0.1"))
        settings->timeScale = 0.1f;
    ImGui::SameLine(0, 3);
    if (ImGui::Button("0.5"))
        settings->timeScale = 0.5f;
    ImGui::SameLine(0, 3);
    if (ImGui::Button(" 1x"))
        settings->timeScale = 1.0f;
    ImGui::SameLine(0, 3);
    if (ImGui::Button(" 2x"))
        settings->timeScale = 2.0f;

    ImGui::SameLine(0, 12);
    VSep();
    ImGui::SameLine(0, 12);

    // LIVE / SCRUB indicator — gentle breathing when live
    if (settings->scrubIndex < 0)
    {
        float breath = 0.75f + 0.25f * Col::Smooth(0.5f + 0.5f * sinf(t * 1.8f));
        ImGui::TextColored(Col::A(Col::Green, breath), "● LIVE");
    }
    else
    {
        // Amber, steady
        ImGui::TextColored(Col::Amber, "◆ SCRUB");
    }

    // ── Row 2: timeline scrubber ──────────────────────────────────
    const size_t histSize = Engine::GetRecorder().HistorySize();
    const int maxFrame = histSize > 0 ? (int)histSize - 1 : 0;
    int sliderVal = (settings->scrubIndex >= 0) ? settings->scrubIndex : maxFrame;

    // Thin accent line above the slider track
    {
        ImVec2 sp = ImGui::GetCursorScreenPos();
        float tw = screenW - sp.x - 12.0f;
        ImGui::GetWindowDrawList()->AddLine(
            {sp.x, sp.y}, {sp.x + tw, sp.y},
            Col::ToU32(Col::Border), 1.0f);
    }

    ImGui::SetNextItemWidth(screenW - ImGui::GetCursorPosX() - 12.0f);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Col::WidgetBg);

    bool scrubChanged = false;
    if (histSize == 0)
    {
        ImGui::BeginDisabled();
        int dummy = 0;
        ImGui::SliderInt("##tl", &dummy, 0, 1, "No recorded frames");
        ImGui::EndDisabled();
    }
    else
    {
        if (settings->scrubIndex >= 0 || histSize > 0)
        {
            float frameTimeMs = Engine::GetRecorder().GetFrameTime((size_t)sliderVal);
            char timeFmt[64];
            // %%d becomes %d after ImGui processes it — time is pre-filled
            snprintf(timeFmt, sizeof(timeFmt),
                     settings->scrubIndex >= 0 ? "Frame %%d  |  t = %.3fs" : "LIVE  |  t = %.3fs",
                     frameTimeMs / 1000.0f);
            scrubChanged = ImGui::SliderInt("##tl", &sliderVal, 0, maxFrame, timeFmt);
        }
    }
    ImGui::PopStyleColor(3);

    if (scrubChanged && histSize > 0)
    {
        if (sliderVal >= maxFrame)
        {
            Engine::GetRecorder().TruncateAfter((size_t)sliderVal);
            settings->scrubIndex = -1;
        }
        else
        {
            settings->scrubIndex = sliderVal;
        }
    }

    // RESUME HERE — appears only while scrubbing
    if (settings->scrubIndex >= 0)
    {
        ImGui::SameLine(0, 10);
        ImGui::PushStyleColor(ImGuiCol_Button, Col::GreenSoft);
        ImGui::PushStyleColor(ImGuiCol_Text, Col::Green);
        ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Green, 0.55f));
        if (ImGui::Button("RESUME HERE"))
        {
            Engine::GetRecorder().TruncateAfter((size_t)settings->scrubIndex);
            settings->scrubIndex = -1;
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// ================================================================
//  SIM PANEL
// ================================================================
void UIManager::RenderSimPanel(std::size_t bodyCount)
{
    const float W = 270.0f;
    const float TOP = 72.0f;
    const float HEIGHT = (screenH - TOP) * 0.52f;

    ImGui::SetNextWindowPos({screenW - W, TOP}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({W, HEIGHT}, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::Void);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::Border);
    ImGui::Begin("##SimPanel", nullptr, flags);

    SectionHead("SIMULATION");

    // Bodies + FPS on a single line
    ImGuiIO &io = ImGui::GetIO();
    ImGui::TextColored(Col::Ink, "%zu", bodyCount);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::InkMid, "bodies");
    ImGui::SameLine(0, 12);
    ImGui::TextColored(Col::InkFaint, "|");
    ImGui::SameLine(0, 12);

    const float fps = io.Framerate;
    ImVec4 fpsCol = fps > 55 ? Col::Green : fps > 30 ? Col::Amber
                                                     : Col::Red;
    ImGui::TextColored(fpsCol, "%.0f fps", fps);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::InkMid, "/ %.2f ms", 1000.0f / (fps > 0 ? fps : 1));

    ImGui::Spacing();
    SectionHead("DRAG MODE");
    const char *dragType[] = {"Precision", "Physics"};
    int dragIdx = (int)settings->dragMode;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##drag", &dragIdx, dragType, 2);
    settings->dragMode = (DragMode)dragIdx;

    ImGui::Spacing();
    SectionHead("RECORDING");

    ImGui::TextColored(Col::InkMid, "Interval");
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##ri", &settings->recordInterval, 1, 10);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::TextColored(Col::InkMid, "Auto-Record");
    ImGui::SameLine(0, 8);
    ImGui::Checkbox("##autoRec", &settings->autoRecordOnFire);
    ImGui::SameLine(0, 6);
    ImGui::TextColored(Col::InkFaint, "on cannon fire");

    const float fps2 = fps > 0 ? fps : 60.0f;
    const float rewSecs = (Engine::GetRecorder().HistorySize() * settings->recordInterval) / fps2;

    ImGui::TextColored(Col::InkMid, "Frames");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::Blue, "%zu", Engine::GetRecorder().HistorySize());
    ImGui::SameLine(0, 16);
    ImGui::TextColored(Col::InkMid, "Window");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::Blue, "%.1fs", rewSecs);

    const char *qualStr = settings->recordInterval == 1   ? "Full detail"
                          : settings->recordInterval <= 3 ? "Balanced"
                                                          : "Long window";
    ImVec4 qualCol = settings->recordInterval == 1   ? Col::Amber
                     : settings->recordInterval <= 3 ? Col::Green
                                                     : Col::InkFaint;
    ImGui::SameLine(0, 10);
    ImGui::TextColored(qualCol, "— %s", qualStr);

    ImGui::Spacing();

    SectionHead("SCENE");

    ImGui::PushStyleColor(ImGuiCol_Button, Col::RedSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Red, 0.45f));
    if (ImGui::Button("  Clear All Bodies  ", {-1, 0}))
        Engine::ClearBodies();
    ImGui::PopStyleColor(4);

    RenderVisualizationSetting();

    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ================================================================
//  INSPECTOR PANEL
// ================================================================
void UIManager::RenderInspectorPanel(physics::Rigidbody *body)
{
    const float W = 270.0f;
    const float TOP = 72.0f;
    const float simH = (screenH - TOP) * 0.52f;
    const float H = (screenH - TOP) * 0.48f;
    const float Y = TOP + simH;

    ImGui::SetNextWindowPos({screenW - W, Y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({W, H}, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::Border);
    ImGui::Begin("##Inspector", nullptr, flags);

    SectionHead("INSPECTOR");

    if (!body)
    {
        ImGui::Spacing();
        ImGui::TextColored(Col::InkFaint, "  Click a body to inspect it.");
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }

    // ── Cannon fast-path ─────────────────────────────────────────
    // If the selected body is a Cannon we swap the entire lower half
    // of the inspector for the cannon control panel.
    if (auto *cannon = dynamic_cast<shape::Cannon *>(body))
    {
        RenderCannonInspector(cannon);
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }
    else if (auto *incline = dynamic_cast<shape::Incline *>(body))
    {
        RenderInclineInspector(incline);
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }
    else if (auto *trigger = dynamic_cast<shape::Trigger *>(body))
    {
        RenderTriggerInspector(trigger);
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }
    else if (auto *thruster = dynamic_cast<shape::Thruster *>(body))
    {
        RenderThrusterInspector(thruster);
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }
    else if (auto *spring = dynamic_cast<shape::Spring *>(body))
    {
        RenderSpringInspector(spring);
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }

    // Type badge — animated blue underline appears on Dynamic
    const bool isStatic = body->bodyType == physics::RigidbodyType::Static;
    ImGui::TextColored(Col::InkMid, "Type");
    ImGui::SameLine(92.0f);
    if (isStatic)
        ImGui::TextColored(Col::InkFaint, "Static");
    else
    {
        ImGui::TextColored(Col::Blue, "Dynamic");
        // Small animated tick under "Dynamic"
        ImVec2 tl = ImGui::GetItemRectMin();
        ImVec2 br = ImGui::GetItemRectMax();
        const float t = (float)ImGui::GetTime();
        float sw = 1.0f + 0.5f * Col::Smooth(0.5f + 0.5f * sinf(t * 2.5f));
        ImGui::GetWindowDrawList()->AddLine(
            {tl.x, br.y + 1}, {br.x, br.y + 1},
            Col::ToU32(Col::A(Col::Blue, 0.5f)), sw);
    }

    ImGui::Spacing();
    SectionHead("TRANSFORM");

    KVRowVec("Position", body->pos / SimulationConstants::PIXELS_PER_METER, Col::ToU32(Col::Ink));
    ImGui::SetNextItemWidth(-1);
    math::Vec2 posInMeters = body->pos / SimulationConstants::PIXELS_PER_METER;
    ImGui::DragFloat2("##pos", &posInMeters.x, 0.01f);

    // Update body position in pixels
    body->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;

    // Pause/unpause simulation when position is being edited
    positionBeingEdited = ImGui::IsItemActive();
    if (positionBeingEdited && !wasPositionEditedLastFrame)
    {
        // Just started editing position — pause
        settings->paused = true;
    }
    else if (!positionBeingEdited && wasPositionEditedLastFrame)
    {
        // Just finished editing position — unpause
        settings->paused = false;
    }
    wasPositionEditedLastFrame = positionBeingEdited;

    math::Vec2 velInMeters = body->linearVel / SimulationConstants::PIXELS_PER_METER;
    KVRowVec("Velocity", velInMeters, Col::ToU32(Col::Blue));
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat2("##vel", &velInMeters.x, 0.01f);

    // Update body velocity in pixels
    body->linearVel = velInMeters * SimulationConstants::PIXELS_PER_METER;

    KVRowVec("Accel", body->linearAcc, Col::ToU32(Col::InkMid));
    {
        const float deg = body->rotation * (180.0f / 3.14159265f);
        KVRow("Rotation", Col::ToU32(Col::Ink), "%.1f deg", deg);
    }

    ImGui::Spacing();
    SectionHead("PROPERTIES");
    if (auto box = dynamic_cast<shape::Box *>(body))
    {
        float widthMeters = box->width / SimulationConstants::PIXELS_PER_METER;

        KVRow("Width", Col::ToU32(Col::Ink), "%.2f m", widthMeters);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##width", &widthMeters, 0.01f, 0.1f, 1000.0f);
        box->width = std::max(widthMeters, 0.1f) * SimulationConstants::PIXELS_PER_METER;

        float heightMeters = box->height / SimulationConstants::PIXELS_PER_METER;
        KVRow("Height", Col::ToU32(Col::Ink), "%.2f m", heightMeters);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##height", &heightMeters, 0.01f, 0.1f, 1000.0f);
        box->height = std::max(heightMeters, 0.1f) * SimulationConstants::PIXELS_PER_METER;
    }
    else if (auto ball = dynamic_cast<shape::Ball *>(body))
    {
        KVRow("Radius", Col::ToU32(Col::Ink), "%.2f m", ball->radius / SimulationConstants::PIXELS_PER_METER);
    }

    float newMass = body->mass;
    KVRow("Mass", Col::ToU32(Col::Ink), "%.2f kg", newMass);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##mass", &body->mass, 0.01f, 0.01f, 1000.0f);

    KVRow("Restitution", Col::ToU32(Col::Ink), "%.2f", body->restitution);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##restitution", &body->restitution, 0.01f);

    KVRow("s-Friction", Col::ToU32(Col::Ink), "%.2f", body->staticFriction);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##staticFriction", &body->staticFriction, 0.01f, 0.0f, 2.0f);

    KVRow("k-Friction", Col::ToU32(Col::Ink), "%.2f", body->kineticFriction);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##kineticFriction", &body->kineticFriction, 0.01f, 0.0f, 2.0f);

    ImGui::Spacing();
    SectionHead("FORCES");

    const auto &forces = body->GetForcesForDisplay();
    // When displaying to the user:
    // const float pixelsPerMeter = SimulationConstants::PIXELS_PER_METER;
    if (forces.empty())
        ImGui::TextColored(Col::InkFaint, "  No active forces.");
    else
    {
        for (const auto &fi : forces)
        {
            const char *name = "Apply";
            ImVec4 col = Col::InkMid;
            switch (fi.type)
            {
            case physics::ForceType::Normal:
                name = "Normal";
                col = Col::Blue;
                break;
            case physics::ForceType::Frictional:
                name = "Friction";
                col = Col::Amber;
                break;
            case physics::ForceType::Gravitational:
                name = "Gravity";
                col = Col::Green;
                break;
            case physics::ForceType::Tension:
                name = "Tension";
                col = {0.65f, 0.28f, 0.90f, 1.0f};
                break;
            default:
                break;
            }
            ImGui::TextColored(col, "%-9s", name);
            ImGui::SameLine(92.0f);
            ImGui::TextColored(Col::InkMid, "(%.0f, %.0f)", fi.force.x / SimulationConstants::PIXELS_PER_METER, fi.force.y / SimulationConstants::PIXELS_PER_METER);
            ImGui::SameLine();
            ImGui::TextColored(col, "  %.0f N", fi.force.Length() / SimulationConstants::PIXELS_PER_METER);
        }
    }

    ImGui::Spacing();
    SectionHead("SPEED");

    const float speed = body->linearVel.Length();
    const float frac = std::min(speed / 2000.0f, 1.0f);
    const float bw = ImGui::GetContentRegionAvail().x;

    AnimBar(frac, bw, 8.0f, Col::ToU32(Col::Green), Col::ToU32(Col::Red));
    ImGui::TextColored(Col::InkMid, "%.1f px/s", speed);

    ImGui::Spacing();

    // ── Remove Button ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, Col::A(Col::Red, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::A(Col::Red, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Red, 0.6f));

    if (ImGui::Button("Remove Body", {-1, 0}))
    {
        removedObjectPointer = body;
        world->RemoveObject(body);
    }

    ImGui::PopStyleColor(5);

    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ================================================================
//  CANNON INSPECTOR
//  Shown in place of the generic inspector when the selected body
//  is a shape::Cannon.  Lets the user:
//    • choose Ball or Box as the projectile type
//    • aim the barrel with a slider (mirrors cannon->barrelAngleDegrees)
//    • set launch speed and see live Vx / Vy decomposition
//    • click FIRE to queue a CannonFireSettings request
// ================================================================
void UIManager::RenderCannonInspector(shape::Cannon *cannon)
{
    const bool isBall = (cannonFireSettings.projectileType ==
                         CannonFireSettings::ProjectileType::Ball);

    // ── Header ───────────────────────────────────────────────────
    SectionHead("CANNON");

    ImGui::TextColored(Col::InkFaint, "  Position");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Ink, "(%.0f,  %.0f)", cannon->pos.x / SimulationConstants::PIXELS_PER_METER, cannon->pos.y / SimulationConstants::PIXELS_PER_METER);
    math::Vec2 posInMeters = cannon->pos / SimulationConstants::PIXELS_PER_METER;
    ImGui::DragFloat2("##pos", &posInMeters.x, 0.01f);

    // Write back in pixels and pause while editing
    cannon->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;
    positionBeingEdited = ImGui::IsItemActive();
    if (positionBeingEdited && !wasPositionEditedLastFrame)
    {
        settings->paused = true;
    }
    else if (!positionBeingEdited && wasPositionEditedLastFrame)
    {
        settings->paused = false;
    }
    wasPositionEditedLastFrame = positionBeingEdited;

    cannon->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;

    ImGui::Spacing();

    // ── Projectile type toggle ────────────────────────────────────
    SectionHead("PROJECTILE TYPE");

    auto TypeButton = [&](const char *label, CannonFireSettings::ProjectileType t)
    {
        const bool active = (cannonFireSettings.projectileType == t);
        const float halfW = (ImGui::GetContentRegionAvail().x -
                             ImGui::GetStyle().ItemSpacing.x) *
                            0.5f;
        ImGui::PushStyleColor(ImGuiCol_Button,
                              active ? Col::BlueSoft : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              active ? Col::Blue : Col::InkMid);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              active ? Col::A(Col::Blue, 0.65f) : Col::Border);
        if (ImGui::Button(label, {halfW, 0}))
            cannonFireSettings.projectileType = t;
        ImGui::PopStyleColor(5);
    };

    TypeButton("  Ball  ", CannonFireSettings::ProjectileType::Ball);
    ImGui::SameLine();
    TypeButton("   Box  ", CannonFireSettings::ProjectileType::Box);

    ImGui::Spacing();

    // ── Barrel angle ──────────────────────────────────────────────
    SectionHead("BARREL ANGLE");

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    // DragFloat allows both dragging AND text input with 0.01f precision
    ImGui::DragFloat("##cangle",
                     &cannon->barrelAngleDegrees,
                     0.01f,  // Speed when dragging
                     0.0f,   // Min value
                     360.0f, // Max value
                     "%.2f deg");
    ImGui::PopStyleColor(2);

    // Clamp to 0-360 range
    if (cannon->barrelAngleDegrees < 0.0f)
        cannon->barrelAngleDegrees += 360.0f;
    if (cannon->barrelAngleDegrees >= 360.0f)
        cannon->barrelAngleDegrees = std::fmod(cannon->barrelAngleDegrees, 360.0f);

    cannonFireSettings.angleDegrees = cannon->barrelAngleDegrees;

    // Compact angle arrow diagram
    {
        const float rad = cannon->barrelAngleDegrees * 3.14159265f / 180.0f;
        const float armLen = 34.0f;
        ImVec2 origin = ImGui::GetCursorScreenPos();
        origin.x += 130.0f;
        origin.y += 32.0f;
        ImVec2 tip = {origin.x + armLen * std::cos(rad),
                      origin.y - armLen * std::sin(rad)};

        auto *dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(origin, 3.5f, Col::ToU32(Col::A(Col::Blue, 0.55f)));
        dl->AddLine(origin, tip, Col::ToU32(Col::Blue), 2.0f);
        dl->AddCircleFilled(tip, 3.0f, Col::ToU32(Col::Blue));

        char abuf[24];
        snprintf(abuf, sizeof(abuf), "%.0f deg", cannon->barrelAngleDegrees);
        dl->AddText({tip.x + 5.0f, tip.y - 7.0f}, Col::ToU32(Col::Ink), abuf);

        ImGui::Dummy({0, armLen + 8.0f});
    }

    ImGui::Spacing();

    // ── Launch speed + live Vx/Vy decomposition ───────────────────
    SectionHead("LAUNCH SPEED");

    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##cspeed",
                     &cannonFireSettings.speed,
                     0.01f, 1.0f, 100.0f, "%.02f m/s");

    cannonFireSettings.Recompute();

    ImGui::Spacing();

    // Vx / Vy card with detailed calculation info
    {
        const float cw = ImGui::GetContentRegionAvail().x;
        const float lineH = ImGui::GetTextLineHeightWithSpacing();
        const float cardH = lineH * 4.5f + 12.0f;
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        ImVec2 cMax = {cMin.x + cw, cMin.y + cardH};
        ImGui::GetWindowDrawList()->AddRectFilled(
            cMin, cMax, Col::ToU32(Col::WidgetBg), 5.0f);
        ImGui::GetWindowDrawList()->AddRect(
            cMin, cMax, Col::ToU32(Col::Border), 5.0f, 0, 1.0f);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.0f, 2.0f});
    ImGui::Dummy({4.0f, 4.0f});

    // Header
    ImGui::TextColored(Col::InkMid, "  VELOCITY DECOMPOSITION");

    // Vx calculation
    ImGui::TextColored(Col::InkFaint, "  Vx ");
    ImGui::SameLine(42.0f);
    ImGui::TextColored(Col::Green, "%+.2f", cannonFireSettings.vx);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::InkFaint, "m/s");
    ImGui::SameLine(0, 10);
    ImGui::TextColored(Col::InkFaint, "(speed × cos(%.1f°))", cannonFireSettings.angleDegrees);

    // Vy calculation
    ImGui::TextColored(Col::InkFaint, "  Vy ");
    ImGui::SameLine(42.0f);
    ImGui::TextColored(Col::Amber, "%+.2f", cannonFireSettings.vy);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::InkFaint, "m/s");
    ImGui::SameLine(0, 10);
    ImGui::TextColored(Col::InkFaint, "(speed × sin(%.1f°))", cannonFireSettings.angleDegrees);

    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Spacing();

    // ── Projectile properties ─────────────────────────────────────
    SectionHead("PROJECTILE");

    // Mass
    ImGui::TextColored(Col::InkMid, "Mass");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##cmass",
                     &cannonFireSettings.mass,
                     0.5f, 0.1f, 10000.0f, "%.1f kg");

    // Restitution
    ImGui::TextColored(Col::InkMid, "Restitution");
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##crest",
                       &cannonFireSettings.restitution,
                       0.0f, 1.0f, "%.2f");
    ImGui::PopStyleColor(2);

    // Color — stored as 0-255 floats, ImGui wants 0-1 floats
    ImGui::TextColored(Col::InkMid, "Color");
    ImVec4 pickerCol = {
        cannonFireSettings.color[0] / 255.0f,
        cannonFireSettings.color[1] / 255.0f,
        cannonFireSettings.color[2] / 255.0f,
        1.0f};
    ImGui::SetNextItemWidth(-1);
    if (ImGui::ColorEdit3("##ccol", reinterpret_cast<float *>(&pickerCol)))
    {
        cannonFireSettings.color[0] = pickerCol.x * 255.0f;
        cannonFireSettings.color[1] = pickerCol.y * 255.0f;
        cannonFireSettings.color[2] = pickerCol.z * 255.0f;
    }

    // Size — adapts to ball vs box.
    // DragFloat needs a real pointer so we round-trip through a local metre variable.
    ImGui::Spacing();
    if (isBall)
    {
        ImGui::TextColored(Col::InkMid, "Radius");
        ImGui::SetNextItemWidth(-1);
        // radius stored directly in metres — no PPM conversion needed here;
        // the Ball constructor scales it to pixels internally.
        if (ImGui::DragFloat("##crad", &cannonFireSettings.radius, 0.1f, 0.1f, 250.0f, "%.1f m"))
        {
        } // value written directly
    }
    else
    {
        ImGui::TextColored(Col::InkMid, "Width");
        ImGui::SetNextItemWidth(-1);
        // boxWidth/boxHeight stored directly in metres — no PPM conversion needed here.
        ImGui::DragFloat("##cbw", &cannonFireSettings.boxWidth, 0.1f, 0.1f, 500.0f, "%.1f m");

        ImGui::TextColored(Col::InkMid, "Height");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##cbh", &cannonFireSettings.boxHeight, 0.1f, 0.1f, 500.0f, "%.1f m");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::Spacing();

    // ── Auto-record toggle ────────────────────────────────────
    SectionHead("ON FIRE");

    {
        const bool autoRec = settings->autoRecordOnFire;
        const float t2 = static_cast<float>(ImGui::GetTime());

        // When enabled, the border breathes green like the pulsing REC dot
        float borderA = autoRec
                            ? (0.45f + 0.35f * Col::Smooth(0.5f + 0.5f * sinf(t2 * 2.5f)))
                            : 0.0f;

        ImGui::PushStyleColor(ImGuiCol_Button,
                              autoRec ? Col::GreenSoft : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              autoRec ? Col::A(Col::Green, 0.22f) : Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              autoRec ? Col::A(Col::Green, 0.35f) : Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              autoRec ? Col::Green : Col::InkMid);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              autoRec ? Col::A(Col::Green, borderA) : Col::Border);

        const char *label = autoRec ? "  AUTO-RECORD  ON  " : "  AUTO-RECORD  OFF  ";
        if (ImGui::Button(label, {-1, 0}))
            settings->autoRecordOnFire = !settings->autoRecordOnFire;

        ImGui::PopStyleColor(5);
    }

    // Small hint line so students know what it does
    ImGui::TextColored(Col::InkFaint, "  Starts recording automatically when fired.");

    ImGui::Spacing();
    ImGui::Spacing();

    // ── FIRE button ───────────────────────────────────────────────
    {
        // const float rad = cannon->barrelAngleDegrees * 3.14159265f / 180.0f;
        // Both terms must be in metres:
        //   cannon->pos is internal pixels  → divide by PPM
        //   barrelLength is a pixel-space visual size → divide by PPM
        // const float barrelLengthM = cannon->barrelLength / SimulationConstants::PIXELS_PER_METER;
        cannonFireSettings.cannonPos = {
            cannon->pos.x / SimulationConstants::PIXELS_PER_METER,
            cannon->pos.y / SimulationConstants::PIXELS_PER_METER};
    }

    const float t = static_cast<float>(ImGui::GetTime());

    const float beat = Col::Smooth(0.5f + 0.5f * sinf(t * 3.5f));
    ImGui::PushStyleColor(ImGuiCol_Button,
                          Col::A(Col::Amber, 0.15f + 0.07f * beat));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Amber, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::A(Col::Amber, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Amber);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          Col::A(Col::Amber, 0.45f + 0.30f * beat));

    if (ImGui::Button("  FIRE  ", {-1, 0}))
        cannonFirePending = true;

    ImGui::PopStyleColor(5);

    ImGui::Spacing();
    // ── Remove Button ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, Col::A(Col::Red, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::A(Col::Red, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Red, 0.6f));

    if (ImGui::Button("Remove Body", {-1, 0}))
    {
        removedObjectPointer = cannon;
        world->RemoveObject(cannon);
    }

    ImGui::PopStyleColor(5);
    ImGui::Spacing();
    ImGui::Spacing();
}

void UIManager::RenderInclineInspector(shape::Incline *incline)
{
    SectionHead("INCLINE");

    ImGui::TextColored(Col::InkFaint, "  Position");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Ink, "(%.2f,  %.2f)", incline->pos.x / SimulationConstants::PIXELS_PER_METER, incline->pos.y / SimulationConstants::PIXELS_PER_METER);
    math::Vec2 posInMeters = incline->pos / SimulationConstants::PIXELS_PER_METER;
    ImGui::DragFloat2("##pos", &posInMeters.x, 0.01f);

    // Write back in pixels and pause while editing
    incline->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;
    positionBeingEdited = ImGui::IsItemActive();
    if (positionBeingEdited && !wasPositionEditedLastFrame)
    {
        settings->paused = true;
    }
    else if (!positionBeingEdited && wasPositionEditedLastFrame)
    {
        settings->paused = false;
    }
    wasPositionEditedLastFrame = positionBeingEdited;
    incline->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;

    ImGui::Spacing();

    // ── Angle Section ──────────────────────────────────────────────
    SectionHead("INCLINE ANGLE");
    ImGui::TextColored(Col::InkMid, "Angle");
    ImGui::SameLine(92.0f);
    float currentAngle = incline->GetAngle();
    ImGui::TextColored(Col::Blue, "%.1f deg", currentAngle);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);

    if (ImGui::SliderFloat("##angle", &currentAngle, 0.0f, 89.0f, "%.1f°"))
    {
        incline->SetAngle(currentAngle);
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── Dimensions Section ─────────────────────────────────────────
    SectionHead("DIMENSIONS");

    ImGui::TextColored(Col::InkMid, "Base Width");
    ImGui::SameLine(92.0f);
    float baseMeters = incline->GetBase() / SimulationConstants::PIXELS_PER_METER;
    ImGui::TextColored(Col::Ink, "%.2f m", baseMeters);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);

    if (ImGui::SliderFloat("##base", &baseMeters, 50.0f / SimulationConstants::PIXELS_PER_METER, 1000.0f / SimulationConstants::PIXELS_PER_METER, "%.2f m"))
    {
        incline->SetBase(baseMeters * SimulationConstants::PIXELS_PER_METER);
    }
    ImGui::PopStyleColor(2);

    ImGui::TextColored(Col::InkFaint, "  Height");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Ink, "%.2f m", incline->GetHeight() / SimulationConstants::PIXELS_PER_METER);

    // Flip toggle
    bool isFlipped = incline->IsFlipped();
    if (ImGui::Checkbox("Flip Direction", &isFlipped))
    {
        incline->SetFlip(isFlipped);
    }

    ImGui::Spacing();

    // ── Friction Section ───────────────────────────────────────────
    SectionHead("FRICTION");

    // Static friction
    ImGui::TextColored(Col::InkMid, "Static coeff");
    ImGui::SameLine(92.0f);
    float staticFriction = incline->GetStaticFriction();
    ImVec4 orange_color = ImVec4(1.0f, 0.647f, 0.0f, 1.0f);
    ImGui::TextColored(orange_color, "%.3f", staticFriction);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, orange_color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    ImGui::SetNextItemWidth(-1);

    if (ImGui::SliderFloat("#static_friction", &staticFriction, 0.0f, 2.0f, "%.3f"))
    {
        incline->SetStaticFriction(staticFriction);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Coefficient of static friction\nTypical values: Ice 0.02-0.1, Wood 0.25-0.5, Rubber 0.6-1.0");
    }
    ImGui::PopStyleColor(2);

    // Kinetic friction
    ImGui::TextColored(Col::InkMid, "Kinetic coeff");
    ImGui::SameLine(92.0f);
    float kineticFriction = incline->GetKineticFriction();
    ImGui::TextColored(orange_color, "%.3f", kineticFriction);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, orange_color);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    ImGui::SetNextItemWidth(-1);

    if (ImGui::SliderFloat("#kinetic_friction", &kineticFriction, 0.0f, 2.0f, "%.3f"))
    {
        incline->SetKineticFriction(kineticFriction);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Coefficient of kinetic friction\nTypical values: Ice 0.01-0.05, Wood 0.2-0.4, Rubber 0.5-0.8\nMust be ≤ static friction");
    }
    ImGui::PopStyleColor(2);

    // Display note about friction
    if (kineticFriction > staticFriction)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "⚠ Kinetic must be ≤ static!");
    }

    ImGui::Spacing();

    // ── Remove Button ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, Col::A(Col::Red, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::A(Col::Red, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Red, 0.6f));

    if (ImGui::Button("Remove Body", {-1, 0}))
    {
        removedObjectPointer = incline;
        world->RemoveObject(incline);
    }

    ImGui::PopStyleColor(5);

    ImGui::Spacing();
}

void UIManager::RenderTriggerInspector(shape::Trigger *trigger)
{
    SectionHead("TRIGGER");

    ImGui::TextColored(Col::InkFaint, "  Position");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Ink, "(%.2f,  %.2f)", trigger->pos.x / SimulationConstants::PIXELS_PER_METER, trigger->pos.y / SimulationConstants::PIXELS_PER_METER);
    math::Vec2 posInMeters = trigger->pos / SimulationConstants::PIXELS_PER_METER;
    ImGui::DragFloat2("##pos", &posInMeters.x, 0.01f);

    // Write back in pixels and pause while editing
    trigger->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;
    positionBeingEdited = ImGui::IsItemActive();
    if (positionBeingEdited && !wasPositionEditedLastFrame)
    {
        settings->paused = true;
    }
    else if (!positionBeingEdited && wasPositionEditedLastFrame)
    {
        settings->paused = false;
    }
    wasPositionEditedLastFrame = positionBeingEdited;
    trigger->pos = posInMeters * SimulationConstants::PIXELS_PER_METER;

    ImGui::Spacing();

    // ── Dimensions Section ─────────────────────────────────────────
    SectionHead("DIMENSIONS");

    ImGui::TextColored(Col::InkMid, "Width");
    ImGui::SameLine(92.0f);
    float widthMeters = trigger->width / SimulationConstants::PIXELS_PER_METER;
    ImGui::TextColored(Col::Ink, "%.2f m", widthMeters);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);

    if (ImGui::SliderFloat("##width", &widthMeters, 10.0f / SimulationConstants::PIXELS_PER_METER, 500.0f / SimulationConstants::PIXELS_PER_METER, "%.2f m"))
    {
        trigger->width = widthMeters * SimulationConstants::PIXELS_PER_METER;
    }
    ImGui::PopStyleColor(2);

    ImGui::TextColored(Col::InkMid, "Height");
    ImGui::SameLine(92.0f);
    float heightMeters = trigger->height / SimulationConstants::PIXELS_PER_METER;
    ImGui::TextColored(Col::Ink, "%.2f m", heightMeters);

    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);

    if (ImGui::SliderFloat("##height", &heightMeters, 10.0f / SimulationConstants::PIXELS_PER_METER, 500.0f / SimulationConstants::PIXELS_PER_METER, "%.2f m"))
    {
        trigger->height = heightMeters * SimulationConstants::PIXELS_PER_METER;
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ───────────────── Color Section ──────────────────────────────────────────────
    SectionHead("COLORS");

    ImGui::TextColored(Col::InkMid, "Idle Color");
    ImGui::SameLine(92.0f);
    ImGui::ColorEdit4("##idleColor", trigger->originalColor.data(),
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

    ImGui::TextColored(Col::InkMid, "Trigger Color");
    ImGui::SameLine(92.0f);
    ImGui::ColorEdit4("##triggerColor", trigger->collisionColor.data(),
                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

    ImGui::Spacing();

    // ── Action Section ────────────────────────────────────────────
    SectionHead("TRIGGER ACTION");

    ImGui::TextColored(Col::InkMid, "On Trigger");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Blue, "%s",
                       trigger->action == shape::TriggerAction::DoNothing ? "Do Nothing" : "Pause Simulation");

    // Action buttons
    const float halfW = (ImGui::GetContentRegionAvail().x -
                         ImGui::GetStyle().ItemSpacing.x) *
                        0.5f;

    // Do Nothing button
    const bool isDoNothing = (trigger->action == shape::TriggerAction::DoNothing);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          isDoNothing ? Col::BlueSoft : Col::WidgetBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          isDoNothing ? Col::Blue : Col::InkMid);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          isDoNothing ? Col::A(Col::Blue, 0.65f) : Col::Border);
    if (ImGui::Button("Do Nothing##action", {halfW, 0}))
        trigger->action = shape::TriggerAction::DoNothing;
    ImGui::PopStyleColor(5);

    ImGui::SameLine();

    // Pause Simulation button
    const bool isPause = (trigger->action == shape::TriggerAction::PauseSimulation);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          isPause ? Col::BlueSoft : Col::WidgetBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          isPause ? Col::Blue : Col::InkMid);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          isPause ? Col::A(Col::Blue, 0.65f) : Col::Border);
    if (ImGui::Button("Pause Sim##action", {halfW, 0}))
        trigger->action = shape::TriggerAction::PauseSimulation;
    ImGui::PopStyleColor(5);

    ImGui::Spacing();

    // ── Status Section ────────────────────────────────────────────
    SectionHead("STATUS");

    ImGui::TextColored(Col::InkMid, "Collision State");
    ImGui::SameLine(92.0f);
    if (trigger->isColliding)
    {
        ImGui::TextColored(Col::Red, "TRIGGERED!");
    }
    else
    {
        ImGui::TextColored(Col::InkFaint, "Idle");
    }

    ImGui::Spacing();

    // ── Remove Button ──────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, Col::A(Col::Red, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::A(Col::Red, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Red, 0.6f));

    if (ImGui::Button("Remove Body", {-1, 0}))
    {
        removedObjectPointer = trigger;
        world->RemoveObject(trigger);
    }

    ImGui::PopStyleColor(5);
    ImGui::Spacing();
}

// ================================================================
//  THRUSTER INSPECTOR
//  Full replacement for RenderThrusterInspector().
// ================================================================
void UIManager::RenderThrusterInspector(shape::Thruster *thruster)
{
    const float t = static_cast<float>(ImGui::GetTime());
    auto *dl = ImGui::GetWindowDrawList();
    const float cw = ImGui::GetContentRegionAvail().x;

    // ── Header with live status badge ────────────────────────────
    SectionHead("THRUSTER");

    // Attachment status badge — right-aligned
    {
        const bool attached = thruster->IsAttached();
        ImVec4 badgeCol = attached ? Col::Green : Col::InkFaint;
        const char *label = attached ? "● ATTACHED" : "○ FLOATING";
        const float lw = ImGui::CalcTextSize(label).x;
        ImGui::SameLine(cw - lw - 4.0f);
        ImGui::TextColored(badgeCol, "%s", label);
    }

    // ── Position (read-only while attached, editable when floating) ──
    {
        math::Vec2 posMeters = thruster->pos / SimulationConstants::PIXELS_PER_METER;
        if (thruster->IsAttached())
        {
            // Read-only — position is driven by the attachment
            KVRow("Position", Col::ToU32(Col::InkFaint),
                  "(%.2f, %.2f) m", posMeters.x, posMeters.y);
        }
        else
        {
            ImGui::TextColored(Col::InkMid, "Position");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat2("##pos", &posMeters.x, 0.01f);
            thruster->pos = posMeters * SimulationConstants::PIXELS_PER_METER;
            positionBeingEdited = ImGui::IsItemActive();
            if (positionBeingEdited && !wasPositionEditedLastFrame)
                settings->paused = true;
            else if (!positionBeingEdited && wasPositionEditedLastFrame)
                settings->paused = false;
            wasPositionEditedLastFrame = positionBeingEdited;
        }
    }

    ImGui::Spacing();

    // ── Interactive compass dial ──────────────────────────────────
    SectionHead("DIRECTION");

    constexpr float R = 52.0f;
    const float dialX = (cw - R * 2.0f) * 0.5f; // centred in panel
    ImVec2 dialCentre = ImGui::GetCursorScreenPos();
    dialCentre.x += dialX + R;
    dialCentre.y += R + 4.0f;

    float currentAngle = thruster->GetAngleDegrees(); // +90° to make 0° point rightwards

    // ── Dial background ───────────────────────────────────────────
    // Outer ring glow (pulses amber when firing)
    if (thruster->isActiveThisFrame)
    {
        const float glow = 0.12f + 0.08f * std::sin(t * 14.0f);
        dl->AddCircleFilled(dialCentre, R + 5.0f,
                            Col::ToU32(Col::A(Col::Amber, glow)), 64);
    }
    dl->AddCircleFilled(dialCentre, R,
                        Col::ToU32(Col::WidgetBg), 64);
    dl->AddCircle(dialCentre, R,
                  Col::ToU32(thruster->isActiveThisFrame
                                 ? Col::A(Col::Amber, 0.6f)
                                 : Col::Border),
                  64, 1.4f);

    // Degree ring — thin marks every 30°
    for (int i = 0; i < 12; i++)
    {
        const float a = i * math::PI / 6.0f;
        const bool card = (i % 3 == 0);
        const float inner = card ? R - 7.0f : R - 4.0f;
        dl->AddLine(
            {dialCentre.x + inner * std::cos(a), dialCentre.y - inner * std::sin(a)},
            {dialCentre.x + R * std::cos(a), dialCentre.y - R * std::sin(a)},
            Col::ToU32(card ? Col::A(Col::InkFaint, 0.5f)
                            : Col::A(Col::Border, 0.6f)),
            card ? 1.2f : 0.7f);
    }

    // Cardinal labels
    const char *cards[] = {"E", "N", "W", "S"};
    for (int i = 0; i < 4; i++)
    {
        const float a = i * math::PI * 0.5f;
        const float lx = dialCentre.x + (R + 11.0f) * std::cos(a) - 3.5f;
        const float ly = dialCentre.y - (R + 11.0f) * std::sin(a) - 5.5f;
        dl->AddText({lx, ly}, Col::ToU32(Col::InkFaint), cards[i]);
    }

    // Body-relative indicator ring (dashed, only when bodyRelative=true)
    if (thruster->bodyRelative && thruster->IsAttached())
    {
        constexpr int DASHES = 20;
        for (int i = 0; i < DASHES; i++)
        {
            const float a0 = (i * 2.0f) / DASHES * math::PI * 2.0f;
            const float a1 = (i * 2.0f + 1.0f) / DASHES * math::PI * 2.0f;
            dl->PathArcTo(dialCentre, R - 2.0f, a0, a1, 6);
            dl->PathStroke(Col::ToU32(Col::A(Col::Blue, 0.25f)),
                           ImDrawFlags_None, 1.5f);
        }
    }

    // ── Thrust arrow ──────────────────────────────────────────────
    {
        const float rad = math::DegToRad(currentAngle);
        const float armLen = R - 8.0f;

        const ImVec2 tip{dialCentre.x + armLen * std::cos(rad),
                         dialCentre.y - armLen * std::sin(rad)};

        // Animated glow when firing
        if (thruster->isActiveThisFrame)
        {
            const float gA = 0.20f + 0.15f * std::sin(t * 12.0f);
            dl->AddLine(dialCentre, tip,
                        Col::ToU32(Col::A(Col::Amber, gA)), 9.0f);
        }

        // Shaft
        dl->AddLine(dialCentre, tip,
                    Col::ToU32(thruster->isActiveThisFrame ? Col::Amber : Col::Blue),
                    2.4f);

        // Arrowhead
        const float perp = rad + math::PI * 0.5f;
        const float hs = 6.0f;
        ImVec2 hl{tip.x - 10.0f * std::cos(rad) + hs * std::cos(perp),
                  tip.y + 10.0f * std::sin(rad) - hs * std::sin(perp)};
        ImVec2 hr{tip.x - 10.0f * std::cos(rad) - hs * std::cos(perp),
                  tip.y + 10.0f * std::sin(rad) + hs * std::sin(perp)};
        dl->AddTriangleFilled(tip, hl, hr,
                              Col::ToU32(thruster->isActiveThisFrame ? Col::Amber : Col::Blue));

        // Centre pivot dot
        dl->AddCircleFilled(dialCentre, 4.0f,
                            Col::ToU32(Col::A(
                                thruster->isActiveThisFrame ? Col::Amber : Col::Blue, 0.7f)));
    }

    // ── Invisible button over dial for drag interaction ───────────
    ImGui::SetCursorScreenPos({dialCentre.x - R, dialCentre.y - R});
    ImGui::InvisibleButton("##dialInspector ", {R * 2.0f, R * 2.0f});

    if (ImGui::IsItemActive())
    {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = mouse.x - dialCentre.x;
        float dy = dialCentre.y - mouse.y;
        if (dx != 0.0f || dy != 0.0f)
            currentAngle = std::fmod(
                std::atan2(dy, dx) * 180.0f / math::PI + 360.0f, 360.0f);

        // Write back immediately so the DragFloat below reads the
        // dial-updated value and the shape updates this same frame
        thruster->SetAngleDegrees(currentAngle);
    }

    // Advance cursor past the dial
    ImGui::SetCursorScreenPos(
        {ImGui::GetCursorScreenPos().x,
         dialCentre.y + R + 8.0f});

    // Numeric drag (below the dial, full width) — modifies currentAngle
    // directly; SetAngleDegrees at the end covers this path too.
    ImGui::SetNextItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,
                          thruster->isActiveThisFrame ? Col::Amber : Col::Blue);
    ImGui::DragFloat("##angle", &currentAngle,
                     0.5f, 0.0f, 360.0f, "%.1fÂ°");
    ImGui::PopStyleColor();

    // Final write-back covers the DragFloat path (and is a no-op if the
    // dial already wrote back above)
    thruster->SetAngleDegrees(currentAngle);

    ImGui::Spacing();

    // ── Force / Acceleration input ────────────────────────────────
    SectionHead("FORCE");

    // ── Mode toggle: Force (N) ↔ Acceleration (m/s²) ─────────────
    // magnitude is ALWAYS stored as Newtons internally.
    // Acceleration mode simply divides by the attached body's mass
    // on display and multiplies back on write — no physics change.
    {
        const float hw = (cw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        auto ModeBtn = [&](const char *label, bool active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, active ? Col::BlueSoft : Col::WidgetBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? Col::Blue : Col::InkMid);
            ImGui::PushStyleColor(ImGuiCol_Border, active ? Col::A(Col::Blue, 0.55f) : Col::Border);
            bool clicked = ImGui::Button(label, {hw, 0});
            ImGui::PopStyleColor(5);
            return clicked;
        };
        if (ModeBtn("  Force (N)  ", !thruster->accelerationMode))
            thruster->accelerationMode = false;
        ImGui::SameLine();
        if (ModeBtn("  Accel (m/sÂ²)  ", thruster->accelerationMode))
            thruster->accelerationMode = true;
    }

    ImGui::Spacing();

    // Resolve body mass — fall back to 1.0 if unattached so UI stays usable
    const float bodyMass = (thruster->IsAttached() && thruster->GetAttachedBody())
                               ? thruster->GetAttachedBody()->mass
                               : 1.0f;

    // The value shown and edited in the drag widget
    // In force mode   : Newtons  (stored directly)
    // In accel mode   : m/s²     (= magnitude / mass)
    float displayVal = thruster->accelerationMode
                           ? (thruster->magnitude / bodyMass) / SimulationConstants::PIXELS_PER_METER
                           : thruster->magnitude / SimulationConstants::PIXELS_PER_METER;
    ;

    const float maxDisplay = thruster->accelerationMode
                                 ? (MAX_THRUSTER_FORCE / bodyMass)
                                 : MAX_THRUSTER_FORCE;

    // Power meter bar — always based on the underlying force fraction
    {
        const float frac = math::Clamp(thruster->magnitude / MAX_THRUSTER_FORCE, 0.0f, 1.0f);
        ImVec2 barP = ImGui::GetCursorScreenPos();
        const float barH = 6.0f;

        dl->AddRectFilled(barP, {barP.x + cw, barP.y + barH},
                          Col::ToU32(Col::WidgetBg), 3.0f);
        if (frac > 0.001f)
        {
            const float shim = thruster->isActiveThisFrame
                                   ? (0.92f + 0.08f * std::sin(t * 10.0f))
                                   : 1.0f;
            ImU32 fillCol = frac < 0.5f
                                ? Col::LerpU32(Col::ToU32(Col::Green),
                                               Col::ToU32(Col::Amber), frac * 2.0f)
                                : Col::LerpU32(Col::ToU32(Col::Amber),
                                               Col::ToU32(Col::Red), (frac - 0.5f) * 2.0f);
            ImVec4 fc = ImGui::ColorConvertU32ToFloat4(fillCol);
            fc.w *= shim;
            dl->AddRectFilled(barP,
                              {barP.x + cw * frac, barP.y + barH},
                              ImGui::ColorConvertFloat4ToU32(fc), 3.0f);
        }
        ImGui::Dummy({cw, barH + 2.0f});
    }

    const char *dragFmt = thruster->accelerationMode ? "%.2f m/sÂ²" : "%.0f N";
    const float dragStep = thruster->accelerationMode ? 0.1f : 5.0f;

    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##forceinput", &displayVal, dragStep, 0.0f, maxDisplay, dragFmt);

    // Write back — convert to Newtons regardless of display mode
    thruster->magnitude = math::Clamp(
                              thruster->accelerationMode ? (displayVal * bodyMass) : displayVal,
                              0.0f, MAX_THRUSTER_FORCE) *
                          SimulationConstants::PIXELS_PER_METER;

    // Show equivalent value in the other unit as a hint
    {
        if (thruster->accelerationMode)
        {
            ImGui::TextColored(Col::InkFaint, "  = %.0f N  (mass: %.1f kg)",
                               thruster->magnitude, bodyMass);
        }
        else if (bodyMass > 0.001f)
        {
            ImGui::TextColored(Col::InkFaint, "  = %.2f m/sÂ²  (mass: %.1f kg)",
                               thruster->magnitude / bodyMass, bodyMass);
        }
    }

    // Fx / Fy decomposition card
    {
        const float rad = math::DegToRad(thruster->rotation);
        const float fx = thruster->magnitude * std::cos(rad);
        const float fy = thruster->magnitude * std::sin(rad);

        const float lineH = ImGui::GetTextLineHeightWithSpacing();
        const float cardH = lineH * 2.5f + 10.0f;
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        ImVec2 cMax = {cMin.x + cw, cMin.y + cardH};

        dl->AddRectFilled(cMin, cMax, Col::ToU32(Col::WidgetBg), 5.0f);
        dl->AddRect(cMin, cMax,
                    Col::ToU32(thruster->isActiveThisFrame
                                   ? Col::A(Col::Amber, 0.4f)
                                   : Col::Border),
                    5.0f, 0, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 1.0f});
        ImGui::Dummy({0.0f, 4.0f});

        if (thruster->accelerationMode)
        {
            ImGui::TextColored(Col::InkMid, "  ACCEL COMPONENTS");
            ImGui::TextColored(Col::InkFaint, "  ax");
            ImGui::SameLine(38.0f);
            ImGui::TextColored(Col::Green, "%+.2f m/sÂ²", fx / bodyMass);
            ImGui::SameLine(0, 14);
            ImGui::TextColored(Col::InkFaint, "  ay");
            ImGui::SameLine(0, 6);
            ImGui::TextColored(Col::Amber, "%+.2f m/sÂ²", fy / bodyMass);
        }
        else
        {
            ImGui::TextColored(Col::InkMid, "  FORCE COMPONENTS");
            ImGui::TextColored(Col::InkFaint, "  Fx");
            ImGui::SameLine(38.0f);
            ImGui::TextColored(Col::Green, "%+.1f N", fx);
            ImGui::SameLine(0, 14);
            ImGui::TextColored(Col::InkFaint, "  Fy");
            ImGui::SameLine(0, 6);
            ImGui::TextColored(Col::Amber, "%+.1f N", fy);
        }

        ImGui::PopStyleVar();
        ImGui::Dummy({0.0f, 4.0f});
    }

    ImGui::Spacing();

    // ── Reference frame toggle ────────────────────────────────────
    SectionHead("REFERENCE FRAME");

    {
        auto FrameButton = [&](const char *label, bool active)
        {
            const float hw = (cw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  active ? Col::BlueSoft : Col::WidgetBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  active ? Col::Blue : Col::InkMid);
            ImGui::PushStyleColor(ImGuiCol_Border,
                                  active ? Col::A(Col::Blue, 0.65f) : Col::Border);
            bool clicked = ImGui::Button(label, {hw, 0});
            ImGui::PopStyleColor(5);
            return clicked;
        };

        if (FrameButton("  Body-Relative  ", thruster->bodyRelative))
            thruster->bodyRelative = true;
        ImGui::SameLine();
        if (FrameButton("  World-Fixed  ", !thruster->bodyRelative))
            thruster->bodyRelative = false;

        ImGui::TextColored(Col::InkFaint,
                           thruster->bodyRelative
                               ? "  Angle rotates with the attached body."
                               : "  Angle is fixed in world space.");
    }

    ImGui::Spacing();

    // ── Firing mode ───────────────────────────────────────────────
    // ── Firing mode ───────────────────────────────────────────────
    SectionHead("FIRING MODE");

    {
        auto ModeButton = [&](const char *label, bool active)
        {
            const float hw = (cw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  active ? Col::GreenSoft : Col::WidgetBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  active ? Col::Green : Col::InkMid);
            ImGui::PushStyleColor(ImGuiCol_Border,
                                  active ? Col::A(Col::Green, 0.55f) : Col::Border);
            bool clicked = ImGui::Button(label, {hw, 0});
            ImGui::PopStyleColor(5);
            return clicked;
        };

        if (ModeButton("  Always On  ", !thruster->keyHold))
            thruster->keyHold = false;
        ImGui::SameLine();
        if (ModeButton("  Key Hold  ", thruster->keyHold))
            thruster->keyHold = true;

        if (thruster->keyHold)
        {
            ImGui::Spacing();

            // ── KeybindUI widget ──────────────────────────────────
            // Shows the current key as a clickable badge.
            // Click the badge or the ✎ button to enter listen mode;
            // press any key to rebind, Esc to cancel.
            // The widget writes directly into thruster->fireKey.
            const float pulse = 0.75f + 0.25f * sinf(t * 2.0f);
            KeybindUI::Widget("Fire Key", thruster->fireKey, 92.0f, pulse);
        }
    }

    ImGui::Spacing();

    // ── Enable / disable master toggle ───────────────────────────
    {
        const bool en = thruster->enabled;
        const float borderA = en
                                  ? (0.45f + 0.35f * Col::Smooth(0.5f + 0.5f * sinf(t * 4.0f)))
                                  : 0.0f;

        ImGui::PushStyleColor(ImGuiCol_Button,
                              en ? Col::GreenSoft : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              en ? Col::A(Col::Green, 0.22f) : Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              en ? Col::A(Col::Green, 0.35f) : Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              en ? Col::Green : Col::InkMid);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              en ? Col::A(Col::Green, borderA) : Col::Border);

        if (ImGui::Button(en ? "  THRUSTER  ON  " : "  THRUSTER  OFF  ", {-1, 0}))
            thruster->enabled = !thruster->enabled;

        ImGui::PopStyleColor(5);
    }

    ImGui::Spacing();

    // ── Remove button ─────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, Col::A(Col::Red, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::A(Col::Red, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Red, 0.6f));

    if (ImGui::Button("Remove Thruster", {-1, 0}))
    {
        removedObjectPointer = thruster;
        world->RemoveObject(thruster);
    }

    ImGui::PopStyleColor(5);
    ImGui::Spacing();
}

void UIManager::RenderSpringInspector(shape::Spring *spring)
{
    const float t  = static_cast<float>(ImGui::GetTime());
    const float cw = ImGui::GetContentRegionAvail().x;

    // ── Header with armed/idle badge ─────────────────────────────
    SectionHead("SPRING");

    {
        const char *badge    = spring->isLoaded ? "◆ ARMED" : "○ IDLE";
        const ImVec4 badgeC  = spring->isLoaded ? Col::Amber : Col::InkFaint;
        const float  bw      = ImGui::CalcTextSize(badge).x;
        ImGui::SameLine(cw - bw + 4.0f);
        ImGui::TextColored(badgeC, "%s", badge);
    }

    // ── Position ──────────────────────────────────────────────────
    ImGui::TextColored(Col::InkFaint, "  Position");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Ink, "(%.1f,  %.1f)",
        spring->pos.x / SimulationConstants::PIXELS_PER_METER,
        spring->pos.y / SimulationConstants::PIXELS_PER_METER);

    math::Vec2 posM = spring->pos / SimulationConstants::PIXELS_PER_METER;
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat2("##spos", &posM.x, 0.01f);
    spring->pos = posM * SimulationConstants::PIXELS_PER_METER;

    positionBeingEdited = ImGui::IsItemActive();
    if (positionBeingEdited && !wasPositionEditedLastFrame)
        settings->paused = true;
    else if (!positionBeingEdited && wasPositionEditedLastFrame)
        settings->paused = false;
    wasPositionEditedLastFrame = positionBeingEdited;

    ImGui::Spacing();

    // ── Angle ─────────────────────────────────────────────────────
    SectionHead("ANGLE");

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##sang", &spring->angleDegrees,
                     0.5f, 0.0f, 360.0f, "%.1f deg");
    ImGui::PopStyleColor(2);

    if (spring->angleDegrees < 0.0f)   spring->angleDegrees += 360.0f;
    if (spring->angleDegrees >= 360.0f) spring->angleDegrees = std::fmod(spring->angleDegrees, 360.0f);

    // Mini angle arrow diagram (same style as cannon inspector)
    {
        const float rad    = spring->angleDegrees * 3.14159265f / 180.0f;
        const float armLen = 32.0f;
        ImVec2 origin      = ImGui::GetCursorScreenPos();
        origin.x += 120.0f;
        origin.y += 28.0f;
        ImVec2 tip = { origin.x + armLen * std::cos(rad),
                       origin.y - armLen * std::sin(rad) };

        auto *dl = ImGui::GetWindowDrawList();
        // Base dot
        dl->AddCircleFilled(origin, 3.0f, Col::ToU32(Col::A(Col::Blue, 0.55f)));
        // Arrow shaft
        dl->AddLine(origin, tip, Col::ToU32(Col::Blue), 2.0f);
        // Arrowhead dot
        dl->AddCircleFilled(tip, 2.8f, Col::ToU32(Col::Blue));
        // Label
        char abuf[24];
        snprintf(abuf, sizeof(abuf), "%.0f°", spring->angleDegrees);
        dl->AddText({ tip.x + 5.0f, tip.y - 6.0f }, Col::ToU32(Col::Ink), abuf);

        // Faint direction label: shows axis name
        const float deg = spring->angleDegrees;
        const char *axisHint = (deg < 22.5f || deg >= 337.5f) ? "→ Right"  :
                               (deg < 67.5f)                  ? "↗ Up-Right":
                               (deg < 112.5f)                 ? "↑ Up"      :
                               (deg < 157.5f)                 ? "↖ Up-Left" :
                               (deg < 202.5f)                 ? "← Left"    :
                               (deg < 247.5f)                 ? "↙ Dn-Left" :
                               (deg < 292.5f)                 ? "↓ Down"    :
                                                                "↘ Dn-Right";
        ImGui::SetCursorScreenPos({ origin.x - 100.0f, origin.y - 8.0f });
        ImGui::TextColored(Col::InkFaint, "%s", axisHint);

        // Advance past diagram
        ImGui::Dummy({ 0.0f, armLen + 6.0f });
    }

    ImGui::Spacing();

    // ── Spring Properties ─────────────────────────────────────────
    SectionHead("SPRING PROPERTIES");

    // Rest length
    ImGui::TextColored(Col::InkMid, "Rest Length");
    float restM = spring->restLength / SimulationConstants::PIXELS_PER_METER;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##srl", &restM, 0.05f, 0.1f, 50.0f, "%.2f m"))
        spring->restLength = std::max(restM, 0.1f) * SimulationConstants::PIXELS_PER_METER;

    // Stiffness k
    ImGui::TextColored(Col::InkMid, "Stiffness k");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##sk", &spring->stiffness, 5.0f, 1.0f, 5000.0f, "%.0f N/m");

    // Damping
    ImGui::TextColored(Col::InkMid, "Damping");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##sdamp", &spring->damping, 0.1f, 0.0f, 100.0f, "%.1f");

    // Coil count
    ImGui::TextColored(Col::InkMid, "Coil Count");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragInt("##scoils", &spring->coilCount, 0.1f, 2, 20);

    ImGui::Spacing();

    // ── Live force readout ────────────────────────────────────────
    {
        const float PPM       = SimulationConstants::PIXELS_PER_METER;
        const float xMeters   = spring->restLength * spring->compressionFraction / PPM;
        const float forceN    = spring->stiffness * xMeters;
        const float energyJ   = 0.5f * spring->stiffness * xMeters * xMeters;

        const float cardH  = ImGui::GetTextLineHeightWithSpacing() * 2.8f + 10.0f;
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        ImVec2 cMax = { cMin.x + cw, cMin.y + cardH };
        ImGui::GetWindowDrawList()->AddRectFilled(cMin, cMax, Col::ToU32(Col::WidgetBg), 5.0f);
        ImGui::GetWindowDrawList()->AddRect(
            cMin, cMax,
            Col::ToU32(spring->isLoaded
                ? Col::A(Col::Amber, 0.40f)
                : Col::Border),
            5.0f, 0, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.0f, 2.0f});
        ImGui::Dummy({4.0f, 4.0f});
        ImGui::TextColored(Col::InkMid, "  STORED ENERGY");
        ImGui::TextColored(Col::InkFaint, "  F  ");
        ImGui::SameLine(38.0f);
        ImGui::TextColored(Col::Blue,  "%+.1f N", forceN);
        ImGui::SameLine(0, 14);
        ImGui::TextColored(Col::InkFaint, "E  ");
        ImGui::SameLine(0, 4);
        ImGui::TextColored(Col::Green, "%.4f J", energyJ);
        ImGui::PopStyleVar();
        ImGui::Dummy({0.0f, 4.0f});
    }

    ImGui::Spacing();

    // ── Compression section ───────────────────────────────────────
    SectionHead("COMPRESSION");

    // Compression fraction slider
    const float sliderGrabCol = spring->isLoaded ? Col::Amber.x : Col::Blue.x;
    (void)sliderGrabCol; // suppress warning; we push colour below

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,
                          spring->isLoaded ? Col::Amber : Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                          spring->isLoaded ? Col::A(Col::Amber, 0.9f) : Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    float comp = spring->compressionFraction;
    if (ImGui::SliderFloat("##scomp", &comp,
                            0.0f, spring->maxCompression, "%.2f"))
    {
        spring->SetCompression(comp);
    }
    ImGui::PopStyleColor(2);

    // Compression bar — colour shifts from green (low) to red (high)
    {
        const float bw = cw;
        const float frac = spring->compressionFraction / spring->maxCompression;
        AnimBar(frac, bw, 6.0f,
                Col::ToU32(Col::Green),
                Col::ToU32(Col::Red));
        ImGui::TextColored(Col::InkFaint,
            "  %.1f%% of %.2f m  (x = %.3f m)",
            spring->compressionFraction * 100.0f,
            spring->restLength / SimulationConstants::PIXELS_PER_METER,
            spring->restLength * spring->compressionFraction / SimulationConstants::PIXELS_PER_METER);
    }

    ImGui::Spacing();

    // ARM / DISARM toggle
    {
        const float halfW = (cw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        const bool  armed = spring->isLoaded;
        const bool  hasComp = (spring->compressionFraction > 0.001f);

        // ARM
        ImGui::PushStyleColor(ImGuiCol_Button,
                              armed ? Col::A(Col::Amber, 0.22f) : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              armed ? Col::Amber : (hasComp ? Col::InkMid : Col::InkFaint));
        ImGui::PushStyleColor(ImGuiCol_Border,
                              armed ? Col::A(Col::Amber, 0.60f) : Col::Border);
        if (!hasComp && !armed) ImGui::BeginDisabled();
        if (ImGui::Button(armed ? "  DISARM  " : "  ARM  ", { halfW, 0 }))
        {
            if (!armed && hasComp)
                spring->isLoaded = true;
            else
                spring->isLoaded = false;
        }
        if (!hasComp && !armed) ImGui::EndDisabled();
        ImGui::PopStyleColor(5);

        ImGui::SameLine();

        // RELEASE — big animated green when armed, disabled otherwise
        const float beat = Col::Smooth(0.5f + 0.5f * sinf(t * 3.5f));
        ImGui::PushStyleColor(ImGuiCol_Button,
                              armed ? Col::A(Col::Green, 0.15f + 0.07f * beat) : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              armed ? Col::A(Col::Green, 0.28f) : Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              armed ? Col::A(Col::Green, 0.45f) : Col::ActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              armed ? Col::Green : Col::InkFaint);
        ImGui::PushStyleColor(ImGuiCol_Border,
                              armed ? Col::A(Col::Green, 0.45f + 0.30f * beat) : Col::Border);
        if (!armed) ImGui::BeginDisabled();
        if (ImGui::Button("  RELEASE  ", { halfW, 0 }))
        {
            // Fill release request for Engine to consume
            springReleaseSettings.tipPos          = spring->GetTipWorldPos();
            springReleaseSettings.direction       = spring->GetTipDirection();
            springReleaseSettings.compressionDist =
                spring->restLength * spring->compressionFraction /
                SimulationConstants::PIXELS_PER_METER;
            springReleaseSettings.impulseStrength = spring->Release();
            springReleasePending = true;
        }
        if (!armed) ImGui::EndDisabled();
        ImGui::PopStyleColor(5);
    }

    ImGui::Spacing();
    ImGui::TextColored(Col::InkFaint,
        "  ARM holds compression; RELEASE fires stored energy.");

    ImGui::Spacing();
    ImGui::Spacing();

    // ── Remove Button ─────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        Col::A(Col::Red, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Red, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Col::A(Col::Red, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_Text,          Col::Red);
    ImGui::PushStyleColor(ImGuiCol_Border,        Col::A(Col::Red, 0.6f));
    if (ImGui::Button("Remove Body", { -1, 0 }))
    {
        removedObjectPointer = spring;
        world->RemoveObject(spring);
    }
    ImGui::PopStyleColor(5);
    ImGui::Spacing();
}

bool UIManager::ConsumeSpringReleaseRequest(SpringReleaseSettings &out)
{
    if (!springReleasePending) return false;
    out = springReleaseSettings;
    springReleasePending = false;
    return true;
}

bool UIManager::ConsumeCannonFireRequest(CannonFireSettings &out)
{
    if (!cannonFirePending)
        return false;
    out = cannonFireSettings;
    cannonFirePending = false;
    return true;
}

// ================================================================
//  SPAWNER PANEL
// ================================================================
void UIManager::RenderSpawnerPanel()
{
    const float W = 270.0f;
    const float TOP = 72.0f;

    ImGui::SetNextWindowPos({0.0f, TOP}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({W, screenH - TOP}, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, Col::Void);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::Border);
    ImGui::Begin("##Spawner", nullptr, flags);

    SectionHead("SPAWNER");

    RenderSpawnBasics();
    ImGui::Spacing();
    RenderSpawnPhysicsControls();
    ImGui::Spacing();
    RenderSpawnSizeControls();
    ImGui::Spacing();
    RenderSpawnConfigurationControls();
    ImGui::Spacing();
    RenderSpawnActions();

    ImGui::End();
    ImGui::PopStyleColor(2);
}

//--------------------------SPAWNER SUB-SECTIONS---------------------------
namespace
{
    ImVec4 ToImGuiColor(const std::array<float, 4> &c)
    {
        return {c[0] / 255.f, c[1] / 255.f, c[2] / 255.f, c[3] / 255.f};
    }

    void FromImGuiColor(const ImVec4 &c, std::array<float, 4> &out)
    {
        out = {c.x * 255.f, c.y * 255.f, c.z * 255.f, c.w * 255.f};
    }
}

void UIManager::RenderSpawnBasics()
{
    SectionHead("SHAPE & PLACEMENT");

    const char *shapes[] = {"Ball", "Incline", "Box", "Cannon", "Thruster", "Trigger", "Rope", "Spring"};
    int si = (int)spawnSettings.shapeType;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##shape", &si, shapes, std::size(shapes));
    spawnSettings.shapeType = (shape::ShapeType)si;

    ImGui::Spacing();
    ImGui::TextColored(Col::InkMid, "Position");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat2("##pos", &spawnSettings.position.x, 1.0f);

    if (spawnSettings.shapeType == shape::ShapeType::Box ||
        spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        ImGui::TextColored(Col::InkMid, "Velocity");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat2("##vel", &spawnSettings.velocity.x, 0.1f);
    }
}

void UIManager::RenderSpawnPhysicsControls()
{
    SectionHead("PHYSICS");

    if (spawnSettings.shapeType == shape::ShapeType::Box ||
        spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        ImGui::TextColored(Col::InkMid, "Mass");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##mass", &spawnSettings.mass, 0.5f, 0.1f, 10000.0f);

        ImGui::TextColored(Col::InkMid, "Restitution");
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##rest", &spawnSettings.restitution, 0.0f, 1.0f);
        ImGui::PopStyleColor(2);
    }

    if (spawnSettings.shapeType == shape::ShapeType::Incline)
    {
        ImGui::TextColored(Col::InkMid, "Static Friction");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##sf", &spawnSettings.staticFriction, 0.05f, 0.0f, 2.0f);

        ImGui::TextColored(Col::InkMid, "Kinetic Friction");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##kf", &spawnSettings.kineticFriction, 0.05f, 0.0f, 2.0f);
    }

    ImGui::TextColored(Col::InkMid, "Color");
    ImVec4 col = ToImGuiColor(spawnSettings.color);
    ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit3("##col", reinterpret_cast<float *>(&col));
    FromImGuiColor(col, spawnSettings.color);

    if (spawnSettings.shapeType == shape::ShapeType::Box ||
        spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        const char *rbType[] = {"Static", "Dynamic"};
        int bti = (int)spawnSettings.bodyType;
        ImGui::TextColored(Col::InkMid, "Body Type");
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##bt", &bti, rbType, 2);
        spawnSettings.bodyType = (physics::RigidbodyType)bti;
    }
}

void UIManager::RenderSpawnSizeControls()
{

    if (spawnSettings.shapeType == shape::ShapeType::Box)
    {
        SectionHead("GEOMETRY");
        // Initialize boxWidth/boxHeight only once (not every frame)
        static bool boxInitialized = false;
        if (!boxInitialized)
        {
            spawnSettings.boxWidth = 1.0f;
            spawnSettings.boxHeight = 1.0f;
            boxInitialized = true;
        }

        ImGui::TextColored(Col::InkMid, "Width");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##bw", &spawnSettings.boxWidth, 0.1f, 1.0f, 500.0f);
        ImGui::TextColored(Col::InkMid, "Height");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##bh", &spawnSettings.boxHeight, 0.1f, 1.0f, 500.0f);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        SectionHead("GEOMETRY");
        ImGui::TextColored(Col::InkMid, "Radius");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##rad", &spawnSettings.radius, 0.1f, 1.0f, 250.0f);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Incline)
    {
        SectionHead("GEOMETRY");
        ImGui::TextColored(Col::InkMid, "Base");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##base", &spawnSettings.base, 0.1f, 1.0f, 1000.0f);
    }
}

void UIManager::RenderSpawnConfigurationControls()
{

    if (spawnSettings.shapeType == shape::ShapeType::Incline)
    {
        SectionHead("CONFIGURATION");
        RenderInclineSpawnConfiguration();
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Cannon)
    {
        SectionHead("CONFIGURATION");
        RenderCannonSpawnConfiguration();
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Thruster)
    {
        RenderThrusterSpawnConfiguration();
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Trigger)
    {
        SectionHead("CONFIGURATION");
        RenderTriggerSpawnConfiguration();
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Rope)
    {
        SectionHead("CONFIGURATION");
        RenderRopeSpawnConfiguration();
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Spring)
    {
        SectionHead("CONFIGURATION");
        RenderSpringSpawnConfiguration();
    }
}

void UIManager::RenderRopeSpawnConfiguration()
{
    ImGui::DragFloat2("End Position", &spawnSettings.ropeEndPosition.x, 0.1f);
    ImGui::SliderInt("Segments", &spawnSettings.ropeSegments, 2, 40);
    ImGui::SliderFloat("Stiffness", &spawnSettings.ropeStiffness, 0.1f, 1.0f);
    ImGui::SliderFloat("Damping", &spawnSettings.ropeDamping, 0.9f, 1.0f);
    ImGui::SliderInt("Iterations", &spawnSettings.ropeStickIterations, 2, 20);
    ImGui::Checkbox("Pin Start", &spawnSettings.ropePinStart);
    ImGui::Checkbox("Pin End", &spawnSettings.ropePinEnd);
}

void UIManager::RenderSpawnActions()
{
    // Spawn — blue-tinted, full-width
    ImGui::PushStyleColor(ImGuiCol_Button, Col::BlueSoft);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Text, Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_Border, Col::A(Col::Blue, 0.55f));
    if (ImGui::Button("  Spawn Object  ", {-1, 0}))
        spawnRequestPending = true;
    ImGui::PopStyleColor(5);
}

void UIManager::RenderInclineSpawnConfiguration()
{
    ImGui::TextColored(Col::InkMid, "Angle (deg)");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##ang", &spawnSettings.angle, 0.1f, 0.0f, 89.0f);
    ImGui::Checkbox("Flip", &spawnSettings.flip);
}

void UIManager::RenderCannonSpawnConfiguration()
{
    ImGui::TextColored(Col::InkMid, "Angle (deg)");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##cang", &spawnSettings.angle, 0.1f, 0.0f, 89.0f);
}

void UIManager::RenderTriggerSpawnConfiguration()
{
    // Initialize boxWidth/boxHeight only once (not every frame)
    static bool triggerInitialized = false;
    if (!triggerInitialized)
    {
        spawnSettings.boxWidth = 2.0f;
        spawnSettings.boxHeight = 2.0f;
        triggerInitialized = true;
    }

    ImGui::TextColored(Col::InkMid, "Width");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##bw", &spawnSettings.boxWidth, 0.1f, 1.0f, 500.0f);
    ImGui::TextColored(Col::InkMid, "Height");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##bh", &spawnSettings.boxHeight, 0.1f, 1.0f, 500.0f);

    // ── Action Section ────────────────────────────────────────────
    SectionHead("TRIGGER ACTION");

    ImGui::TextColored(Col::InkMid, "On Trigger");
    ImGui::SameLine(92.0f);
    ImGui::TextColored(Col::Blue, "%s",
                       spawnSettings.triggerAction == shape::TriggerAction::DoNothing ? "Do Nothing" : "Pause Simulation");

    // Action buttons
    const float halfW = (ImGui::GetContentRegionAvail().x -
                         ImGui::GetStyle().ItemSpacing.x) *
                        0.5f;

    // Do Nothing button
    const bool isDoNothing = (spawnSettings.triggerAction == shape::TriggerAction::DoNothing);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          isDoNothing ? Col::BlueSoft : Col::WidgetBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          isDoNothing ? Col::Blue : Col::InkMid);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          isDoNothing ? Col::A(Col::Blue, 0.65f) : Col::Border);
    if (ImGui::Button("Do Nothing##action", {halfW, 0}))
        spawnSettings.triggerAction = shape::TriggerAction::DoNothing;
    ImGui::PopStyleColor(5);

    ImGui::SameLine();

    // Pause Simulation button
    const bool isPause = (spawnSettings.triggerAction == shape::TriggerAction::PauseSimulation);
    ImGui::PushStyleColor(ImGuiCol_Button,
                          isPause ? Col::BlueSoft : Col::WidgetBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          isPause ? Col::Blue : Col::InkMid);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          isPause ? Col::A(Col::Blue, 0.65f) : Col::Border);
    if (ImGui::Button("Pause Sim##action", {halfW, 0}))
        spawnSettings.triggerAction = shape::TriggerAction::PauseSimulation;
    ImGui::PopStyleColor(5);

    ImGui::Spacing();
}

void UIManager::RenderThrusterSpawnConfiguration()
{
    // const float t = static_cast<float>(ImGui::GetTime());
    auto *dl = ImGui::GetWindowDrawList();
    const float cw = ImGui::GetContentRegionAvail().x;

    // ── Compact compass dial ──────────────────────────────────────
    SectionHead("THRUST DIRECTION");

    constexpr float R = 42.0f; // dial radius (px)
    constexpr float CENTRE_OFFSET_X = 52.0f;
    ImVec2 dialCentre = ImGui::GetCursorScreenPos();
    dialCentre.x += CENTRE_OFFSET_X;
    dialCentre.y += R + 4.0f;

    // ── Draw the dial background ──────────────────────────────────
    dl->AddCircleFilled(dialCentre, R,
                        Col::ToU32(Col::WidgetBg), 64);
    dl->AddCircle(dialCentre, R,
                  Col::ToU32(Col::Border), 64, 1.2f);

    // Cardinal tick marks at 0/90/180/270
    for (int i = 0; i < 4; i++)
    {
        const float a = i * math::PI * 0.5f;
        const float ix = dialCentre.x + (R - 4.0f) * std::cos(a);
        const float iy = dialCentre.y - (R - 4.0f) * std::sin(a);
        const float ox = dialCentre.x + R * std::cos(a);
        const float oy = dialCentre.y - R * std::sin(a);
        dl->AddLine({ix, iy}, {ox, oy},
                    Col::ToU32(Col::A(Col::InkFaint, 0.5f)), 1.2f);
    }

    // 45° minor ticks (8 total, every 45°)
    for (int i = 0; i < 8; i++)
    {
        const float a = i * math::PI * 0.25f;
        const float ix = dialCentre.x + (R - 3.0f) * std::cos(a);
        const float iy = dialCentre.y - (R - 3.0f) * std::sin(a);
        const float ox = dialCentre.x + R * std::cos(a);
        const float oy = dialCentre.y - R * std::sin(a);
        dl->AddLine({ix, iy}, {ox, oy},
                    Col::ToU32(Col::A(Col::Border, 0.7f)), 0.8f);
    }

    // Cardinal labels — E/N/W/S using math convention (0°=right)
    const char *cardinals[] = {"E", "N", "W", "S"};
    for (int i = 0; i < 4; i++)
    {
        const float a = i * math::PI * 0.5f;
        const float lx = dialCentre.x + (R + 10.0f) * std::cos(a) - 3.5f;
        const float ly = dialCentre.y - (R + 10.0f) * std::sin(a) - 5.0f;
        dl->AddText({lx, ly},
                    Col::ToU32(Col::InkFaint), cardinals[i]);
    }

    // ── Draggable interaction zone ────────────────────────────────
    // Place an InvisibleButton over the dial so ImGui tracks hover/click.
    ImGui::SetCursorScreenPos({dialCentre.x - R, dialCentre.y - R});
    ImGui::InvisibleButton("##dialSpawner", {R * 2.0f, R * 2.0f});

    if (ImGui::IsItemActive())
    {
        // Drag inside the circle → update angle
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float dx = mouse.x - dialCentre.x;
        float dy = dialCentre.y - mouse.y; // flip Y so up = +Y
        if (dx != 0.0f || dy != 0.0f)
            spawnSettings.angle = std::fmod(
                std::atan2(dy, dx) * 180.0f / math::PI + 360.0f, 360.0f);
    }

    // ── Draw the thrust arrow ─────────────────────────────────────
    {
        const float rad = math::DegToRad(spawnSettings.angle);
        const float armLen = R - 6.0f;
        const ImVec2 tip{dialCentre.x + armLen * std::cos(rad),
                         dialCentre.y - armLen * std::sin(rad)};

        // Glow halo
        dl->AddLine(dialCentre, tip,
                    Col::ToU32(Col::A(Col::Amber, 0.18f)), 7.0f);
        // Shaft
        dl->AddLine(dialCentre, tip,
                    Col::ToU32(Col::Amber), 2.2f);
        // Arrowhead
        const float perpRad = rad + math::PI * 0.5f;
        const float hs = 5.0f;
        ImVec2 head_l{tip.x - 8.0f * std::cos(rad) + hs * std::cos(perpRad),
                      tip.y + 8.0f * std::sin(rad) - hs * std::sin(perpRad)};
        ImVec2 head_r{tip.x - 8.0f * std::cos(rad) - hs * std::cos(perpRad),
                      tip.y + 8.0f * std::sin(rad) + hs * std::sin(perpRad)};
        dl->AddTriangleFilled(tip, head_l, head_r,
                              Col::ToU32(Col::Amber));

        // Centre dot
        dl->AddCircleFilled(dialCentre, 3.2f,
                            Col::ToU32(Col::A(Col::Amber, 0.7f)));
    }

    // Advance cursor past the dial
    ImGui::SetCursorScreenPos(
        {ImGui::GetCursorScreenPos().x,
         dialCentre.y + R + 8.0f});

    // Numeric drag alongside dial
    ImGui::SameLine(CENTRE_OFFSET_X + R * 2.0f + 10.0f);
    ImGui::SetNextItemWidth(cw - CENTRE_OFFSET_X - R * 2.0f - 12.0f);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Amber);
    ImGui::DragFloat("##spawnerAngle", &spawnSettings.angle,
                     0.5f, 0.0f, 360.0f, "%.0f°");
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Force / Acceleration input ────────────────────────────────
    SectionHead("FORCE");

    // ── Mode toggle ───────────────────────────────────────────────
    // thrusterForce always stores Newtons. Acceleration mode uses
    // spawnSettings.mass as the reference body mass for conversion.
    {
        const float hw = (cw - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        auto ModeBtn = [&](const char *label, bool active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, active ? Col::BlueSoft : Col::WidgetBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Col::ActiveBg);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? Col::Blue : Col::InkMid);
            ImGui::PushStyleColor(ImGuiCol_Border, active ? Col::A(Col::Blue, 0.55f) : Col::Border);
            bool clicked = ImGui::Button(label, {hw, 0});
            ImGui::PopStyleColor(5);
            return clicked;
        };
        if (ModeBtn("  Force (N)  ", !spawnSettings.thrusterAccelMode))
            spawnSettings.thrusterAccelMode = false;
        ImGui::SameLine();
        if (ModeBtn("  Accel (m/sÂ²)  ", spawnSettings.thrusterAccelMode))
            spawnSettings.thrusterAccelMode = true;
    }

    ImGui::Spacing();

    const float refMass = std::max(spawnSettings.mass, 0.001f);
    const float maxDisplay = spawnSettings.thrusterAccelMode
                                 ? (MAX_THRUSTER_FORCE / refMass)
                                 : MAX_THRUSTER_FORCE;

    // Display value — convert for accel mode
    float displayForce = spawnSettings.thrusterAccelMode
                             ? (spawnSettings.thrusterForce / refMass)
                             : spawnSettings.thrusterForce;

    const float forceFrac = math::Clamp(spawnSettings.thrusterForce / MAX_THRUSTER_FORCE, 0.0f, 1.0f);

    // Power meter bar
    {
        ImVec2 barStart = ImGui::GetCursorScreenPos();
        const float barH = 4.0f;
        dl->AddRectFilled(barStart,
                          {barStart.x + cw, barStart.y + barH},
                          Col::ToU32(Col::WidgetBg), 2.0f);
        if (forceFrac > 0.001f)
        {
            ImU32 fillCol = forceFrac < 0.5f
                                ? Col::LerpU32(Col::ToU32(Col::Green),
                                               Col::ToU32(Col::Amber),
                                               forceFrac * 2.0f)
                                : Col::LerpU32(Col::ToU32(Col::Amber),
                                               Col::ToU32(Col::Red),
                                               (forceFrac - 0.5f) * 2.0f);
            dl->AddRectFilled(barStart,
                              {barStart.x + cw * forceFrac, barStart.y + barH},
                              fillCol, 2.0f);
        }
        ImGui::Dummy({cw, barH + 2.0f});
    }

    const char *dragFmt = spawnSettings.thrusterAccelMode ? "%.2f m/sÂ²" : "%.0f N";
    const float dragStep = spawnSettings.thrusterAccelMode ? 0.1f : 5.0f;

    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##spawnerForce", &displayForce, dragStep, 0.0f, maxDisplay, dragFmt);

    // Write back as Newtons
    spawnSettings.thrusterForce = math::Clamp(
        spawnSettings.thrusterAccelMode ? (displayForce * refMass) : displayForce,
        0.0f, MAX_THRUSTER_FORCE);

    // Cross-unit hint
    if (spawnSettings.thrusterAccelMode)
        ImGui::TextColored(Col::InkFaint, "  = %.0f N  (ref mass: %.1f kg)",
                           spawnSettings.thrusterForce, refMass);
    else
        ImGui::TextColored(Col::InkFaint, "  = %.2f m/sÂ²  (ref mass: %.1f kg)",
                           spawnSettings.thrusterForce / refMass, refMass);

    // Fx / Fy decomposition hint
    {
        const float rad = math::DegToRad(spawnSettings.angle);
        const float fx = spawnSettings.thrusterForce * std::cos(rad);
        const float fy = spawnSettings.thrusterForce * std::sin(rad);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.0f, 1.0f});
        if (spawnSettings.thrusterAccelMode)
        {
            ImGui::TextColored(Col::InkFaint, "  ax");
            ImGui::SameLine(56.0f);
            ImGui::TextColored(Col::Green, "%+.2f m/sÂ²", fx / refMass);
            ImGui::TextColored(Col::InkFaint, "  ay");
            ImGui::SameLine(56.0f);
            ImGui::TextColored(Col::Amber, "%+.2f m/sÂ²", fy / refMass);
        }
        else
        {
            ImGui::TextColored(Col::InkFaint, "  Fx");
            ImGui::SameLine(56.0f);
            ImGui::TextColored(Col::Green, "%+.0f N", fx);
            ImGui::TextColored(Col::InkFaint, "  Fy");
            ImGui::SameLine(56.0f);
            ImGui::TextColored(Col::Amber, "%+.0f N  ", fy);
        }
        ImGui::PopStyleVar();
    }
}

void UIManager::RenderSpringSpawnConfiguration()
{
    auto *dl = ImGui::GetWindowDrawList();
    const float cw = ImGui::GetContentRegionAvail().x;

    // ── Angle dial (reuses Thruster spawner dial style) ───────────
    SectionHead("LAUNCH DIRECTION");

    constexpr float R = 42.0f;
    constexpr float CX = 52.0f;
    ImVec2 dialC = ImGui::GetCursorScreenPos();
    dialC.x += CX;
    dialC.y += R + 4.0f;

    // Dial background
    dl->AddCircleFilled(dialC, R,     Col::ToU32(Col::WidgetBg), 64);
    dl->AddCircle(dialC, R,           Col::ToU32(Col::Border), 64, 1.2f);

    // Cardinal tick marks
    for (int i = 0; i < 4; i++)
    {
        const float a  = i * math::PI * 0.5f;
        dl->AddLine({ dialC.x + (R - 4.f) * std::cos(a), dialC.y - (R - 4.f) * std::sin(a) },
                    { dialC.x +  R        * std::cos(a), dialC.y -  R        * std::sin(a) },
                    Col::ToU32(Col::A(Col::InkFaint, 0.5f)), 1.2f);
    }
    const char *cards[] = { "E","N","W","S" };
    for (int i = 0; i < 4; i++)
    {
        const float a = i * math::PI * 0.5f;
        dl->AddText({ dialC.x + (R + 10.f) * std::cos(a) - 3.5f,
                      dialC.y - (R + 10.f) * std::sin(a) - 5.0f },
                    Col::ToU32(Col::InkFaint), cards[i]);
    }

    // Drag interaction zone
    ImGui::SetCursorScreenPos({ dialC.x - R, dialC.y - R });
    ImGui::InvisibleButton("##springDialSpawner", { R * 2.0f, R * 2.0f });
    if (ImGui::IsItemActive())
    {
        ImVec2 m = ImGui::GetIO().MousePos;
        float dx = m.x - dialC.x, dy = dialC.y - m.y;
        if (dx != 0.0f || dy != 0.0f)
            spawnSettings.springAngle = std::fmod(
                std::atan2(dy, dx) * 180.0f / math::PI + 360.0f, 360.0f);
    }

    // Arrow
    {
        const float rad = math::DegToRad(spawnSettings.springAngle);
        const float arm = R - 6.0f;
        const ImVec2 tip { dialC.x + arm * std::cos(rad),
                           dialC.y - arm * std::sin(rad) };
        // Halo + shaft
        dl->AddLine(dialC, tip, Col::ToU32(Col::A(Col::Blue, 0.18f)), 7.0f);
        dl->AddLine(dialC, tip, Col::ToU32(Col::Blue), 2.2f);
        // Arrowhead
        const float perp = rad + math::PI * 0.5f;
        constexpr float hs = 5.0f;
        ImVec2 hl { tip.x - 8.f * std::cos(rad) + hs * std::cos(perp),
                    tip.y + 8.f * std::sin(rad) - hs * std::sin(perp) };
        ImVec2 hr { tip.x - 8.f * std::cos(rad) - hs * std::cos(perp),
                    tip.y + 8.f * std::sin(rad) + hs * std::sin(perp) };
        dl->AddTriangleFilled(tip, hl, hr, Col::ToU32(Col::Blue));
        dl->AddCircleFilled(dialC, 3.2f, Col::ToU32(Col::A(Col::Blue, 0.7f)));
    }

    // Advance cursor past dial
    ImGui::SetCursorScreenPos({ ImGui::GetCursorScreenPos().x,
                                 dialC.y + R + 8.0f });
    // Numeric drag beside dial
    ImGui::SameLine(CX + R * 2.0f + 10.0f);
    ImGui::SetNextItemWidth(cw - CX - R * 2.0f - 12.0f);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, Col::Blue);
    ImGui::DragFloat("##springSpawnAngle", &spawnSettings.springAngle,
                     0.5f, 0.0f, 360.0f, "%.0f°");
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Geometry ──────────────────────────────────────────────────
    SectionHead("GEOMETRY");

    ImGui::TextColored(Col::InkMid, "Rest Length");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##srlen", &spawnSettings.springRestLength,
                     0.05f, 0.2f, 30.0f, "%.2f m");

    ImGui::TextColored(Col::InkMid, "Coil Count");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragInt("##srcoils", &spawnSettings.springCoilCount,
                   0.1f, 2, 20);

    ImGui::Spacing();

    // ── Physics ───────────────────────────────────────────────────
    SectionHead("PHYSICS");

    ImGui::TextColored(Col::InkMid, "Stiffness k");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##srk", &spawnSettings.springStiffness,
                     5.0f, 1.0f, 5000.0f, "%.0f N/m");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Spring constant k in Hooke's law  F = k·x\n"
                          "Typical: soft 50 N/m, medium 500 N/m, stiff 3000+ N/m");

    ImGui::TextColored(Col::InkMid, "Damping");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##srdamp", &spawnSettings.springDamping,
                     0.1f, 0.0f, 100.0f, "%.1f");

    ImGui::Spacing();

    // ── Initial compression ───────────────────────────────────────
    SectionHead("INITIAL COMPRESSION");

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,
        spawnSettings.springInitialCompression > 0.001f ? Col::Amber : Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
        spawnSettings.springInitialCompression > 0.001f
            ? Col::A(Col::Amber, 0.9f) : Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##sric", &spawnSettings.springInitialCompression,
                       0.0f, 0.85f, "%.2f  (0=free, 1=full)");
    ImGui::PopStyleColor(2);

    // Preview: show calculated force at this compression
    {
        //const float PPM      = SimulationConstants::PIXELS_PER_METER;
        const float xM       = spawnSettings.springRestLength
                               * spawnSettings.springInitialCompression;
        const float forceN   = spawnSettings.springStiffness * xM;
        const float energyJ  = 0.5f * spawnSettings.springStiffness * xM * xM;

        ImGui::TextColored(Col::InkFaint,
            "  F = %.1f N    E = %.4f J    x = %.3f m",
            forceN, energyJ, xM);

        // Small energy bar
        const float maxE = 0.5f * spawnSettings.springStiffness
                           * (spawnSettings.springRestLength * 0.85f)
                           * (spawnSettings.springRestLength * 0.85f);
        const float frac = (maxE > 0.001f)
                           ? std::min(energyJ / maxE, 1.0f) : 0.0f;
        AnimBar(frac, cw, 5.0f,
                Col::ToU32(Col::Green), Col::ToU32(Col::Amber));
    }

    ImGui::Spacing();
}


bool UIManager::ConsumeSpawnRequest(SpawnSettings &out)
{
    if (!spawnRequestPending)
        return false;
    out = spawnSettings;
    spawnRequestPending = false;
    return true;
}

// ================================================================
//  MEASUREMENT OVERLAY
//  Ink-on-Void look: blue hypotenuse, green dx, amber dy,
//  Void-white label card with a soft drop shadow.
// ================================================================
void UIManager::RenderMeasurementOverlay(const math::Vec2 &startScreen,
                                         const math::Vec2 &endScreen,
                                         const math::Vec2 &wStart,
                                         const math::Vec2 &wEnd,
                                         bool active)
{
    if (!active)
        return;
    const float dx = endScreen.x - startScreen.x;
    const float dy = endScreen.y - startScreen.y;
    if (sqrtf(dx * dx + dy * dy) < 2.0f)
        return;

    DrawMeasurementOverlay(
        {startScreen.x, startScreen.y},
        {endScreen.x, endScreen.y},
        wStart, wEnd);
}

void UIManager::DrawMeasurementOverlay(const math::Vec2 &screenA,
                                       const math::Vec2 &screenB,
                                       const math::Vec2 &wStart,
                                       const math::Vec2 &wEnd)
{
    ImGuiIO &io = ImGui::GetIO();
    auto *dl = ImGui::GetForegroundDrawList();

    ImVec2 A = {screenA.x, screenA.y};
    ImVec2 B = {screenB.x, screenB.y};
    const float dxs = B.x - A.x, dys = B.y - A.y;
    const float dist = sqrtf(dxs * dxs + dys * dys);
    if (dist < 2.0f)
        return;

    // Colours — ink strokes on Void
    const ImU32 cBlue = Col::ToU32(Col::Blue);
    const ImU32 cBlueDim = Col::ToU32(Col::A(Col::Blue, 0.18f));
    const ImU32 cGreen = Col::ToU32(Col::Green);
    const ImU32 cAmber = Col::ToU32(Col::Amber);
    const ImU32 cDot = cBlue;
    const ImU32 cSq = IM_COL32(140, 140, 128, 160);

    ImVec2 C = {B.x, A.y}; // right-angle corner

    // dx arm (green, horizontal)
    dl->AddLine(A, C, cGreen, 1.6f);
    // dy arm (amber, vertical)
    dl->AddLine(C, B, cAmber, 1.6f);

    // Right-angle marker
    const float sq = 8.0f;
    const float sx = (dxs >= 0) ? -sq : sq;
    const float sy = (dys >= 0) ? -sq : sq;
    const ImVec2 raMark[3] = {
        {C.x + sx, C.y},
        {C.x + sx, C.y + sy},
        {C.x, C.y + sy}};
    dl->AddPolyline(raMark, 3, cSq, ImDrawFlags_None, 1.2f);

    // Hypotenuse — glow layer + sharp ink line
    dl->AddLine(A, B, cBlueDim, 5.5f);
    dl->AddLine(A, B, cBlue, 1.8f);

    // Midpoint tick perpendicular to hyp
    {
        ImVec2 mid = {(A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f};
        const float inv = 1.0f / dist;
        const float px = -dys * inv, py = dxs * inv;
        const float tk = 9.0f;
        dl->AddLine({mid.x + px * tk, mid.y + py * tk},
                    {mid.x - px * tk, mid.y - py * tk}, cBlue, 1.5f);
    }

    // Endpoint dots
    dl->AddCircleFilled(A, 4.5f, cDot);
    dl->AddCircle(A, 8.5f, Col::ToU32(Col::A(Col::Blue, 0.22f)), 20, 1.2f);
    dl->AddCircleFilled(B, 4.5f, cDot);
    dl->AddCircle(B, 8.5f, Col::ToU32(Col::A(Col::Blue, 0.22f)), 20, 1.2f);

    // Label card — Void with drop shadow
    // Convert internal pixel world-space deltas to meters for display
    const float wdx = (wEnd.x - wStart.x) / SimulationConstants::PIXELS_PER_METER;
    const float wdy = (wEnd.y - wStart.y) / SimulationConstants::PIXELS_PER_METER;
    const float wdist = sqrtf(wdx * wdx + wdy * wdy);

    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    char lx[40], ly[40], ld[40];
    snprintf(lx, sizeof(lx), "dx      %+.2f m", wdx);
    snprintf(ly, sizeof(ly), "dy      %+.2f m", wdy);
    snprintf(ld, sizeof(ld), "dist     %.2f m", wdist);

    const float tw = std::max({ImGui::CalcTextSize(lx).x,
                               ImGui::CalcTextSize(ly).x,
                               ImGui::CalcTextSize(ld).x});
    const float th = lineH * 3.0f;
    const float padX = 11.0f, padY = 8.0f;
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;

    ImVec2 mid = {(A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f};
    ImVec2 lp = {mid.x + 20.0f, mid.y - th * 0.5f};
    if (lp.x + tw + padX * 2 > sw)
        lp.x = mid.x - tw - padX * 2 - 20.0f;
    if (lp.y + th + padY * 2 > sh)
        lp.y = sh - th - padY * 2 - 4.0f;
    if (lp.y < 0)
        lp.y = 4.0f;

    ImVec2 bMin = {lp.x - padX, lp.y - padY};
    ImVec2 bMax = {lp.x + tw + padX, lp.y + th + padY};

    const ImU32 cBg = IM_COL32(14.025, 17.85, 26, 200);
    ShadowRect(dl, bMin, bMax,
               cBg,
               Col::ToU32(Col::Border), 5.0f);

    dl->AddText({lp.x, lp.y}, cGreen, lx);
    dl->AddText({lp.x, lp.y + lineH}, cAmber, ly);
    dl->AddText({lp.x, lp.y + lineH * 2}, cBlue, ld);
}

// ================================================================
//  MOUSE POSITION OVERLAY
//  Void-card badge with a fine crosshair — same ink-on-Void look.
// ================================================================
void UIManager::RenderMousePositionOverlay(const math::Vec2 &worldPos)
{
    ImGuiIO &io = ImGui::GetIO();
    auto *dl = ImGui::GetForegroundDrawList();

    const ImU32 cBg = IM_COL32(14.025, 17.85, 26, 200);
    const ImU32 cBord = Col::ToU32(Col::Border);
    const ImU32 cLabel = Col::ToU32(Col::InkFaint);
    const ImU32 cX = Col::ToU32(Col::Green);
    const ImU32 cY = Col::ToU32(Col::Amber);
    const ImU32 cHair = Col::ToU32(Col::A(Col::Blue, 0.45f));

    // Convert internal pixel coords to meters for display
    const float mX = worldPos.x / SimulationConstants::PIXELS_PER_METER;
    const float mY = worldPos.y / SimulationConstants::PIXELS_PER_METER;

    char xb[32], yb[32];
    snprintf(xb, sizeof(xb), "%.2f m", mX);
    snprintf(yb, sizeof(yb), "%.2f m", mY);

    const float lineH = ImGui::GetTextLineHeight();
    const float lineGap = ImGui::GetTextLineHeightWithSpacing() - lineH;
    const float spacing = lineH + lineGap;
    const float lw = ImGui::CalcTextSize("X ").x;
    const float vw = std::max(ImGui::CalcTextSize(xb).x,
                              ImGui::CalcTextSize(yb).x);
    const float totW = lw + vw;
    const float totH = spacing + lineH;

    const ImVec2 m = io.MousePos;
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    const float ox = 19.0f, oy = 6.0f, px = 9.0f, py = 7.0f;

    ImVec2 lp = {m.x + ox, m.y + oy};
    if (lp.x + totW + px * 2 > sw)
        lp.x = m.x - totW - px * 2 - ox;
    if (lp.y + totH + py * 2 > sh)
        lp.y = m.y - totH - py * 2 - oy;
    lp.x = std::max(lp.x, 2.0f);
    lp.y = std::max(lp.y, 2.0f);

    ImVec2 bMin = {lp.x - px, lp.y - py};
    ImVec2 bMax = {lp.x + totW + px, lp.y + totH + py};

    ShadowRect(dl, bMin, bMax, cBg, cBord, 5.0f);

    // Row separator
    const float sepY = lp.y + spacing - lineGap * 0.5f;
    dl->AddLine({bMin.x + 5, sepY}, {bMax.x - 5, sepY}, cBord, 1.0f);

    dl->AddText({lp.x, lp.y}, cLabel, "X ");
    dl->AddText({lp.x + lw, lp.y}, cX, xb);
    dl->AddText({lp.x, lp.y + spacing}, cLabel, "Y ");
    dl->AddText({lp.x + lw, lp.y + spacing}, cY, yb);

    // Fine crosshair
    const float arm = 7.0f;
    dl->AddLine({m.x - arm, m.y}, {m.x - 2, m.y}, cHair, 1.2f);
    dl->AddLine({m.x + 2, m.y}, {m.x + arm, m.y}, cHair, 1.2f);
    dl->AddLine({m.x, m.y - arm}, {m.x, m.y - 2}, cHair, 1.2f);
    dl->AddLine({m.x, m.y + 2}, {m.x, m.y + arm}, cHair, 1.2f);
}