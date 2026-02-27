#pragma once

#include "core/Renderer.hpp"
#include "core/World.hpp"
#include "math/Vec2.hpp"
#include "core/UIManager.hpp"
#include "common/settings.hpp"
#include "core/Recorder.hpp"

class UIManager;
class Engine
{
public:
	// Delete constructors and destructor to prevent instantiation
	Engine() = delete;
	Engine(const Engine &) = delete;
	Engine &operator=(const Engine &) = delete;
	~Engine() = delete;

	static bool Initialize();
	static void Shutdown();

	static void Update(float deltaMs, int iterations);
	static void Render();
	static void RunFrame(float deltaMs, int iterations);

	static bool IsRunning();
	static Renderer &GetRenderer();

	static void ComparisonScene();

	static void AddDefaultObjects();
	static void InclineProblemScene();
	// void SpawnBox(const math::Vec2& pos, const math::Vec2& vel, float width, float height, float mass, float restitution);
	// void SpawnBall(const math::Vec2& pos, const math::Vec2& vel, float radius, float mass, float restitution);
	static void CheckSpawning();
	static void CheckCannon();
	static void ClearBodies();

	static void Scroll_Feedback(GLFWwindow *window, double xoffset, double yoffset);

	static Recorder &GetRecorder() { return recorder; }

private:
	static Recorder recorder;

	static int recordFrameCounter;
	static int spawnNudgeFrames;

	static Renderer renderer;
	// static TextRenderer textRenderer;
	static Settings settings;
	static World world;
	static UIManager uiManager;
	static physics::Rigidbody *draggedBody;
	static physics::Rigidbody *selectedBody;
	static math::Vec2 mouseWorld;
	static bool mouseDownLeft;
	static bool mouseDownRight;
	static math::Vec2 mouseInitialPos;
	static math::Vec2 mouseDeltaScale;

	static math::Vec2 mouseScreen;

	static math::Vec2 mouseInitialScreen;

	static math::Vec2 staticDragOffset;

	// Camera panning
	static math::Vec2 cameraOffset;	  // Current camera pan offset
	static math::Vec2 panStartMouse;  // Mouse position when pan started
	static math::Vec2 panStartCamera; // Camera offset when pan started
	static bool isPanning;			  // Is currently panning the camera

	// Key state tracking for press-only detection
	static bool prevKeyP; // Previous frame state of P key
	static bool prevKeyO; // Previous frame state of O key
	static bool prevKeyR; // Previous frame state of R key
};