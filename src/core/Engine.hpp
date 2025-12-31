#pragma once

#include "core/Renderer.hpp"
#include "core/World.hpp"
#include "math/Vec2.hpp"
#include "core/UIManager.hpp"

class Engine
{
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
	//void SpawnBox(const math::Vec2& pos, const math::Vec2& vel, float width, float height, float mass, float restitution);
	//void SpawnBall(const math::Vec2& pos, const math::Vec2& vel, float radius, float mass, float restitution);
	void CheckSpawning();

private:
	Renderer renderer;
	World world;
	UIManager uiManager;
};
