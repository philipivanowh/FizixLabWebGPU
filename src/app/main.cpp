#include "core/Engine.hpp"


# include <chrono>
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <emscripten/html5.h>
#endif // __EMSCRIPTEN__


int main() {

	if (!Engine::Initialize()) {
		return 1;
	}

#ifdef __EMSCRIPTEN__
	// Equivalent of the main loop when using Emscripten:
	struct LoopState {
		double lastTimeMs = 0.0;
	};
	LoopState loopState{emscripten_get_now()};
	auto callback = [](void *arg) {
		auto* state = reinterpret_cast<LoopState*>(arg);
		const double nowMs = emscripten_get_now();
		const float deltaMs = static_cast<float>(nowMs - state->lastTimeMs) * 0.5f;
		state->lastTimeMs = nowMs;
		Engine::RunFrame(deltaMs, 10); // 4. We can use the engine object
	};
	emscripten_set_main_loop_arg(callback, &loopState, 0, true);
#else // __EMSCRIPTEN__
	auto lastTime = std::chrono::steady_clock::now();
	while (Engine::IsRunning()) {
		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<float, std::milli> delta = now - lastTime;
		lastTime = now;
		Engine::RunFrame(delta.count() * 0.5f, 10);
	}
#endif // __EMSCRIPTEN__

	Engine::Shutdown();

	return 0;
}
