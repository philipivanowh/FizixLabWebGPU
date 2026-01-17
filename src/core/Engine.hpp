#pragma once

#include "core/Renderer.hpp"
#include "core/World.hpp"
#include "math/Vec2.hpp"
#include "core/UIManager.hpp"
#include "common/settings.hpp"

class UIManager;
class Engine
{
public:
	// Delete constructors and destructor to prevent instantiation
	Engine() = delete;
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	~Engine() = delete;

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
	static void ClearBodies();

private:
	static Renderer renderer;
	//static TextRenderer textRenderer;
	static Settings settings;
	static World world;
	static UIManager uiManager;
	static physics::Rigidbody* draggedBody;
	static math::Vec2 mouseWorld;
	static bool mouseDownLeft;
	static bool mouseDownRight;
	static math::Vec2 mouseInitialPos;
	static math::Vec2 mouseDeltaScale;
	
};