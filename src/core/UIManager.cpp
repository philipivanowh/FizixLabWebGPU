#include "UIManager.hpp"
#include "core/Engine.hpp"
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <cmath>
#include <cstdarg>
#include <algorithm>
#include <iterator>

// ================================================================
//  COLOUR PALETTE  —  Alan Becker "Animation vs ___" whiteboard
//
//  The entire look is: a bright, clean notebook page with bold
//  primary-colour marker strokes.  Think whiteboard, not terminal.
//
//  Rule of thumb for every colour decision:
//    • Backgrounds: warm off-whites / very light greys
//    • Text:        near-black ink, never pure white on bright bg
//    • Accents:     exactly one saturated hue per semantic role
//    • No glow      — contrast comes from clarity, not bloom
// ================================================================
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
    //static constexpr ImVec4 AmberSoft{0.900f, 0.600f, 0.150f, 0.12f};

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
void UIManager::InitializeImGui(Renderer &renderer, Settings* settings)
{
    this->settings = settings;
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
                Engine::GetRecorder().Clear();
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
        scrubChanged = ImGui::SliderInt("##tl", &sliderVal, 0, maxFrame,
                                        settings->scrubIndex >= 0 ? "Frame %d" : "LIVE");
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

    KVRowVec("Position", body->pos, Col::ToU32(Col::Ink));
    KVRowVec("Velocity", body->linearVel, Col::ToU32(Col::Blue));
    KVRowVec("Accel", body->linearAcc, Col::ToU32(Col::InkMid));
    {
        const float deg = body->rotation * (180.0f / 3.14159265f);
        KVRow("Rotation", Col::ToU32(Col::Ink), "%.1f deg", deg);
    }

    ImGui::Spacing();
    SectionHead("PROPERTIES");

    KVRow("Mass", Col::ToU32(Col::Ink), "%.2f kg", body->mass);
    KVRow("Restitution", Col::ToU32(Col::Ink), "%.2f", body->restitution);
    KVRow("s-Friction", Col::ToU32(Col::Ink), "%.2f", body->staticFriction);
    KVRow("k-Friction", Col::ToU32(Col::Ink), "%.2f", body->kineticFriction);

    ImGui::Spacing();
    SectionHead("FORCES");

    const auto &forces = body->GetForcesForDisplay();
    // When displaying to the user:
//const float pixelsPerMeter = SimulationConstants::PIXELS_PER_METER;
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
            ImGui::TextColored(Col::InkMid, "(%.0f, %.0f)", fi.force.x, fi.force.y);
            ImGui::SameLine();
            ImGui::TextColored(col, "  %.0f N", fi.force.Length());
        }
    }

    ImGui::Spacing();
    SectionHead("SPEED");

    const float speed = body->linearVel.Length();
    const float frac = std::min(speed / 2000.0f, 1.0f);
    const float bw = ImGui::GetContentRegionAvail().x;

    AnimBar(frac, bw, 8.0f, Col::ToU32(Col::Green), Col::ToU32(Col::Red));
    ImGui::TextColored(Col::InkMid, "%.1f px/s", speed);

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
    ImGui::TextColored(Col::Ink, "(%.0f,  %.0f)", cannon->pos.x, cannon->pos.y);

    ImGui::Spacing();

    // ── Projectile type toggle ────────────────────────────────────
    SectionHead("PROJECTILE TYPE");

    auto TypeButton = [&](const char *label, CannonFireSettings::ProjectileType t)
    {
        const bool active = (cannonFireSettings.projectileType == t);
        const float halfW = (ImGui::GetContentRegionAvail().x -
                             ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Button,
                              active ? Col::BlueSoft : Col::WidgetBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::HoverBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Col::ActiveBg);
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

    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       Col::Blue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, Col::BlueHov);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##cangle",
                       &cannon->barrelAngleDegrees,
                       0.0f, 360.0f, "%.1f deg");
    ImGui::PopStyleColor(2);

    cannonFireSettings.angleDegrees = cannon->barrelAngleDegrees;

    // Compact angle arrow diagram
    {
        const float rad    = cannon->barrelAngleDegrees * 3.14159265f / 180.0f;
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
                     1.0f, 1.0f, 500.0f, "%.0f vel/s");

    cannonFireSettings.Recompute();

    ImGui::Spacing();

    // Vx / Vy card
    {
        const float cw    = ImGui::GetContentRegionAvail().x;
        const float lineH = ImGui::GetTextLineHeightWithSpacing();
        const float cardH = lineH * 2.0f + 10.0f;
        ImVec2 cMin = ImGui::GetCursorScreenPos();
        ImVec2 cMax = {cMin.x + cw, cMin.y + cardH};
        ImGui::GetWindowDrawList()->AddRectFilled(
            cMin, cMax, Col::ToU32(Col::WidgetBg), 5.0f);
        ImGui::GetWindowDrawList()->AddRect(
            cMin, cMax, Col::ToU32(Col::Border), 5.0f, 0, 1.0f);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6.0f, 2.0f});
    ImGui::Dummy({4.0f, 4.0f});

    ImGui::TextColored(Col::InkFaint, "  Vx");
    ImGui::SameLine(42.0f);
    ImGui::TextColored(Col::Green,  "%+.1f", cannonFireSettings.vx);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::InkFaint, "px/s");

    ImGui::TextColored(Col::InkFaint, "  Vy");
    ImGui::SameLine(42.0f);
    ImGui::TextColored(Col::Amber, "%+.1f", cannonFireSettings.vy);
    ImGui::SameLine(0, 4);
    ImGui::TextColored(Col::InkFaint, "px/s");

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
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       Col::Blue);
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
        1.0f
    };
    ImGui::SetNextItemWidth(-1);
    if (ImGui::ColorEdit3("##ccol", reinterpret_cast<float *>(&pickerCol)))
    {
        cannonFireSettings.color[0] = pickerCol.x * 255.0f;
        cannonFireSettings.color[1] = pickerCol.y * 255.0f;
        cannonFireSettings.color[2] = pickerCol.z * 255.0f;
    }

    // Size — adapts to ball vs box
    ImGui::Spacing();
    if (isBall)
    {
        ImGui::TextColored(Col::InkMid, "Radius");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##crad",
                         &cannonFireSettings.radius,
                         0.5f, 1.0f, 250.0f, "%.0f px");
    }
    else
    {
        ImGui::TextColored(Col::InkMid, "Width");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##cbw",
                         &cannonFireSettings.boxWidth,
                         0.5f, 1.0f, 500.0f, "%.0f px");

        ImGui::TextColored(Col::InkMid, "Height");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##cbh",
                         &cannonFireSettings.boxHeight,
                         0.5f, 1.0f, 500.0f, "%.0f px");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // ── FIRE button ───────────────────────────────────────────────
    {
        const float rad   = cannon->barrelAngleDegrees * 3.14159265f / 180.0f;
        cannonFireSettings.cannonPos = {
            cannon->pos.x + cannon->barrelLength * std::cos(rad),
            cannon->pos.y + cannon->barrelLength * std::sin(rad)
        };
    }

    const float t    = static_cast<float>(ImGui::GetTime());
    const float beat = Col::Smooth(0.5f + 0.5f * sinf(t * 3.5f));
    ImGui::PushStyleColor(ImGuiCol_Button,
                          Col::A(Col::Amber, 0.15f + 0.07f * beat));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Col::A(Col::Amber, 0.28f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Col::A(Col::Amber, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Text,          Col::Amber);
    ImGui::PushStyleColor(ImGuiCol_Border,
                          Col::A(Col::Amber, 0.45f + 0.30f * beat));

    if (ImGui::Button("  FIRE  ", {-1, 0}))
        cannonFirePending = true;

    ImGui::PopStyleColor(5);
    ImGui::Spacing();
    ImGui::Spacing();
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
    RenderSpawnActions();

    ImGui::End();
    ImGui::PopStyleColor(2);
}

// ================================================================
//  SPAWNER SUB-SECTIONS
// ================================================================
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

    const char *shapes[] = {"Ball", "Incline", "Box", "Cannon"};
    int si = (int)spawnSettings.shapeType;
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##shape", &si, shapes, std::size(shapes));
    spawnSettings.shapeType = (shape::ShapeType)si;

    ImGui::Spacing();
    ImGui::TextColored(Col::InkMid, "Position");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat2("##pos", &spawnSettings.position.x, 1.0f);

    ImGui::TextColored(Col::InkMid, "Velocity");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat2("##vel", &spawnSettings.velocity.x, 0.1f);
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
    SectionHead("GEOMETRY");

    if (spawnSettings.shapeType == shape::ShapeType::Box)
    {
        ImGui::TextColored(Col::InkMid, "Width");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##bw", &spawnSettings.boxWidth, 1.0f, 1.0f, 500.0f);
        ImGui::TextColored(Col::InkMid, "Height");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##bh", &spawnSettings.boxHeight, 1.0f, 1.0f, 500.0f);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Ball)
    {
        ImGui::TextColored(Col::InkMid, "Radius");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##rad", &spawnSettings.radius, 1.0f, 1.0f, 250.0f);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Incline)
    {
        ImGui::TextColored(Col::InkMid, "Base");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##base", &spawnSettings.base, 1.0f, 1.0f, 1000.0f);
        ImGui::TextColored(Col::InkMid, "Angle (deg)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ang", &spawnSettings.angle, 1.0f, 0.0f, 89.0f);
        ImGui::Checkbox("Flip", &spawnSettings.flip);
    }
    else if (spawnSettings.shapeType == shape::ShapeType::Cannon)
    {
        ImGui::TextColored(Col::InkMid, "Angle (deg)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##cang", &spawnSettings.angle, 1.0f, 0.0f, 89.0f);
    }
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
    const float wdx = wEnd.x - wStart.x;
    const float wdy = wEnd.y - wStart.y;
    const float wdist = sqrtf(wdx * wdx + wdy * wdy);

    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    char lx[40], ly[40], ld[40];
    snprintf(lx, sizeof(lx), "dx      %+.1f", wdx);
    snprintf(ly, sizeof(ly), "dy      %+.1f", wdy);
    snprintf(ld, sizeof(ld), "dist     %.1f", wdist);

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

    char xb[32], yb[32];
    snprintf(xb, sizeof(xb), "%.1f", worldPos.x);
    snprintf(yb, sizeof(yb), "%.1f", worldPos.y);

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