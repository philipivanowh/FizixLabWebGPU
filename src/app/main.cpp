#include "core/Engine.hpp"

#include <chrono>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <emscripten/html5.h>
#endif

// ============================================================
// DX / DY TOOLTIP (ImGui foreground, auto-flips at edges)
// ============================================================
static void DrawDxDyTooltipAtMouseCorner(float dx, float dy)
{
    ImGuiIO& io = ImGui::GetIO();

    // Mouse position (screen space)
    ImVec2 m = io.MousePos;

    char buf[64];
    snprintf(buf, sizeof(buf), "dx: %.1f, dy: %.1f", dx, dy);

    ImVec2 ts = ImGui::CalcTextSize(buf);

    const float pad = 8.0f;
    ImVec2 p(m.x + pad, m.y + pad);

    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;

    // Flip horizontally
    if (p.x + ts.x > sw) p.x = m.x - ts.x - pad;

    // Flip vertically
    if (p.y + ts.y > sh) p.y = m.y - ts.y - pad;

    // Clamp
    if (p.x < 0) p.x = 0;
    if (p.y < 0) p.y = 0;
    if (p.x + ts.x > sw) p.x = sw - ts.x;
    if (p.y + ts.y > sh) p.y = sh - ts.y;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImU32 bg = IM_COL32(0, 0, 0, 200);
    ImU32 fg = IM_COL32(255, 255, 255, 255);

    ImVec2 boxMin(p.x - 4.0f, p.y - 3.0f);
    ImVec2 boxMax(p.x + ts.x + 4.0f, p.y + ts.y + 3.0f);

    dl->AddRectFilled(boxMin, boxMax, bg, 4.0f);
    dl->AddText(p, fg, buf);
}

// ============================================================
// MAIN
// ============================================================
int main()
{
    if (!Engine::Initialize()) {
        return 1;
    }

#ifdef __EMSCRIPTEN__

    struct LoopState {
        double lastTimeMs = 0.0;
    };

    LoopState loopState{ emscripten_get_now() };

    auto callback = [](void* arg) {
        auto* state = reinterpret_cast<LoopState*>(arg);

        const double nowMs = emscripten_get_now();
        const float deltaMs =
            static_cast<float>(nowMs - state->lastTimeMs) * 0.5f;

        state->lastTimeMs = nowMs;

        Engine::RunFrame(deltaMs, 10);

        // ----------------------------------------------------
        // 🔴 CALL TOOLTIP HERE *ONLY IF YOU HAVE dx, dy*
        // Example:
        // if (dragging)
        //     DrawDxDyTooltipAtMouseCorner(dx, dy);
        // ----------------------------------------------------
    };

    emscripten_set_main_loop_arg(callback, &loopState, 0, true);

#else

    auto lastTime = std::chrono::steady_clock::now();

    while (Engine::IsRunning()) {
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float, std::milli> delta = now - lastTime;
        lastTime = now;

        Engine::RunFrame(delta.count() * 0.5f, 10);

        // ----------------------------------------------------
        // 🔴 CALL TOOLTIP HERE *ONLY IF YOU HAVE dx, dy*
        // Example:
        // if (dragging)
        //     DrawDxDyTooltipAtMouseCorner(dx, dy);
        // ----------------------------------------------------
    }

#endif

    Engine::Shutdown();
    return 0;
}
