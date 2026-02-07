#include "core/Engine.hpp"


# include <chrono>
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <emscripten/html5.h>
#endif // __EMSCRIPTEN__

#ifdef __EMSCRIPTEN__
extern "C"
{
	EMSCRIPTEN_KEEPALIVE void StartSimulation()
	{
		static bool started = false;
		if (started)
		{
			return;
		}
		started = true;

		if (!Engine::Initialize())
		{
			return;
		}

		struct LoopState
		{
			double lastTimeMs = 0.0;
		};
		static LoopState loopState{emscripten_get_now()};
		auto callback = [](void *arg)
		{
			auto *state = reinterpret_cast<LoopState *>(arg);
			const double nowMs = emscripten_get_now();
			const float deltaMs = static_cast<float>(nowMs - state->lastTimeMs) * 0.5f;
			state->lastTimeMs = nowMs;
			Engine::RunFrame(deltaMs, 10);
		};
		emscripten_set_main_loop_arg(callback, &loopState, 0, true);
	}
}
#endif


int main() {
#ifdef __EMSCRIPTEN__
	return 0;
#else
	if (!Engine::Initialize()) {
		return 1;
	}

	auto lastTime = std::chrono::steady_clock::now();
	while (Engine::IsRunning()) {
		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<float, std::milli> delta = now - lastTime;
		lastTime = now;
		Engine::RunFrame(delta.count() * 0.5f, 10);
	}

	Engine::Shutdown();

	return 0;
#endif // __EMSCRIPTEN__
}
