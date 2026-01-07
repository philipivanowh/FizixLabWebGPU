#pragma once

#include "core/Renderer.hpp"
#include "core/World.hpp"
#include "math/Vec2.hpp"
#include "core/UIManager.hpp"


enum DragMode{
	percisionDrag,
	physicsDrag
};


class Engine
{
public:
	static bool Initialize();
	static void Shutdown();

	static void Update(float deltaMs, int iterations);
	static void Render();
	static void RunFrame(float deltaMs, int iterations);

	static bool IsRunning();
	static Renderer& GetRenderer();

	static void ComparisonScene();

	static void AddDefaultObjects();
	static void InclineProblemScene();
	//void SpawnBox(const math::Vec2& pos, const math::Vec2& vel, float width, float height, float mass, float restitution);
	//void SpawnBall(const math::Vec2& pos, const math::Vec2& vel, float radius, float mass, float restitution);
	static void CheckSpawning();

	//settings
	static DragMode dragMode;

private:
	static Renderer renderer;
	static World world;
	static UIManager uiManager;
	static physics::Rigidbody* draggedBody;
	static math::Vec2 mouseWorld;
	static bool mouseDown;
	
};