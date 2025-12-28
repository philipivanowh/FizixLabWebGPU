#pragma once

#include "core/Renderer.hpp"
#include "core/World.hpp"
#include "math/Vec2.hpp"

class Engine {
public:
	bool Initialize();
	void Shutdown();

	void Update(float deltaMs, int iterations);
	void Render();
	void RunFrame(float deltaMs, int iterations);

	bool IsRunning();
	Renderer& GetRenderer();

	void ComparisonScene();

	void AddDefaultObjects();
	void SpawnRandomBox();
	void SpawnRandomBall();

private:
	Renderer renderer;
	World world;
};
