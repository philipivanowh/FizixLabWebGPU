#include "core/Engine.hpp"

#include <chrono>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <emscripten/html5.h>
#endif


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
            static_cast<float>(nowMs - state->lastTimeMs);

        state->lastTimeMs = nowMs;

        Engine::RunFrame(deltaMs, 1000);

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

        Engine::RunFrame(delta.count(), 1000);
        
    }

#endif

    Engine::Shutdown();
    return 0;
}
