#include "core/Engine.hpp"

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Canon.hpp"
#include "shape/Incline.hpp"
#include "shape/Shape.hpp"
#include "math/Math.hpp"

#include <array>
#include <memory>
using math::PI;
using math::Vec2;
using physics::RigidbodyType;

bool Engine::Initialize()
{
	if (!renderer.Initialize())
	{
		return false;
	}
	uiManager.InitializeImGui(renderer);
	// AddDefaultObjects();
	//  ComparisonScene();
	InclineProblemScene();
	return true;
}

void Engine::Shutdown()
{
	renderer.Terminate();
	uiManager.TerminateImGui();
}

void Engine::CheckSpawning()
{
	SpawnSettings req;
	if (uiManager.ConsumeSpawnRequest(req))
	{
		if (req.shapeType == shape::ShapeType::Box)
		{
			world.Add(std::make_unique<shape::Box>(
				req.position,
				req.velocity,
				math::Vec2(0.0f, 0.0f),
				req.boxWidth,
				req.boxHeight,
				req.color,
				req.mass,
				req.restitution,
				req.bodyType));
		}
		else if (req.shapeType == shape::ShapeType::Ball)
		{
			world.Add(std::make_unique<shape::Ball>(
				req.position,
				req.velocity,
				math::Vec2(0.0f, 0.0f),
				req.radius,
				req.color,
				req.mass,
				req.restitution,
				req.bodyType));
		}
		else if (req.shapeType == shape::ShapeType::Incline)
		{
			world.Add(std::make_unique<shape::Incline>(
				req.position,
				req.velocity,
				math::Vec2(0.0f, 0.0f),
				req.base,
				req.angle,
				req.flip,
				req.color));
		}
		else if (req.shapeType == shape::ShapeType::Canon)
		{
			world.Add(std::make_unique<shape::Canon>(
				req.position,
				req.angle,
				req.color));
		}
	}
}

void Engine::Update(float deltaMs, int iterations)
{
	world.Update(deltaMs, iterations);

	CheckSpawning();
}

void Engine::Render()
{
	renderer.BeginFrame();
	world.Draw(renderer);
	// renderer.UpdateGUI();

	// renderer.DrawTestTriangle();
	// renderer.DrawTest2Triangle();
	// renderer.UpdateGUI();
	uiManager.BeginImGuiFrame();
	uiManager.RenderMainControls(world.RigidbodyCount());
	uiManager.RenderSpawner();
	uiManager.EndImGuiFrame(renderer);

	renderer.EndFrame();
}

void Engine::RunFrame(float deltaMs, int iterations)
{
	Update(deltaMs, iterations);
	Render();
}

bool Engine::IsRunning()
{
	return renderer.IsRunning();
}

Renderer &Engine::GetRenderer()
{
	return renderer;
}

void Engine::AddDefaultObjects()
{
	using physics::RigidbodyType;
	using shape::Ball;
	using shape::Box;

	const std::array<float, 4> warmRed{18.0f, 46.0f, 56.0f, 1.0f};
	const std::array<float, 4> white{255.0f, 255.0f, 255.0f, 1.0f};
	const std::array<float, 4> skyBlue{80.0f, 160.0f, 255.0f, 1.0f};
	const std::array<float, 4> yellow{255.0f, 200.0f, 20.0f, 1.0f};

	world.Add(std::make_unique<shape::Box>(
		Vec2(700.0f, 200.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		1600.0f,
		50.0f,
		skyBlue,
		100.0f,
		0.0f,
		RigidbodyType::Static));

	world.Add(std::make_unique<shape::Box>(
		Vec2(700.0f, 300.0f),
		Vec2(0.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		80.0f,
		80.0f,
		white,
		1.0f,
		0.5f,
		RigidbodyType::Dynamic));

	auto slope = std::make_unique<shape::Box>(
		Vec2(1000.0f, 400.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		500.0f,
		40.0f,
		skyBlue,
		100.0f,
		0.0f,
		RigidbodyType::Static);
	const float slopeAngle = -PI / 6.0f;
	slope->RotateTo(slopeAngle);
	world.Add(std::move(slope));

	world.Add(std::make_unique<shape::Box>(
		Vec2(500.0f, 700.0f),
		Vec2(5.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		200.0f,
		200.0f,
		white,
		2000.0f,
		1.0f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Box>(
		Vec2(896.0f, 670.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		70.0f,
		70.0f,
		yellow,
		15.0f,
		0.5f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Box>(
		Vec2(906.0f, 570.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		70.0f,
		70.0f,
		warmRed,
		50.0f,
		0.5f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Ball>(
		Vec2(906.0f, 770.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		50.0f,
		warmRed,
		50.0f,
		0.5f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Ball>(
		Vec2(500.0f, 700.0f),
		Vec2(-1.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		20.0f,
		warmRed,
		5.0f,
		0.2f,
		RigidbodyType::Dynamic));
}

void Engine::ComparisonScene()
{
	const std::array<float, 4> warmRed{18.0f, 46.0f, 56.0f, 1.0f};
	const std::array<float, 4> white{255.0f, 255.0f, 255.0f, 1.0f};
	const std::array<float, 4> skyBlue{80.0f, 160.0f, 255.0f, 1.0f};
	const std::array<float, 4> yellow{255.0f, 200.0f, 20.0f, 1.0f};

	world.Add(std::make_unique<shape::Box>(
		Vec2(700.0f, 400.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		1600.0f,
		50.0f,
		skyBlue,
		2000.0f,
		0.0f,
		RigidbodyType::Static));

	world.Add(std::make_unique<shape::Box>(
		Vec2(100.0f, 500.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		25.0f,
		25.0f,
		warmRed,
		10.0f,
		0.0f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Box>(
		Vec2(150.0f, 500.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		25.0f,
		25.0f,
		white,
		1000.0f,
		0.0f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Box>(
		Vec2(200.0f, 500.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		25.0f,
		25.0f,
		skyBlue,
		100.0f,
		0.0f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Box>(
		Vec2(250.0f, 500.0f),
		Vec2(1.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		25.0f,
		25.0f,
		yellow,
		100.0f,
		0.0f,
		RigidbodyType::Dynamic));
}

void Engine::InclineProblemScene()
{
	const std::array<float, 4> warmRed{180.0f, 42.0f, 102.0f, 1.0f};
	const std::array<float, 4> white{255.0f, 255.0f, 255.0f, 1.0f};
	const std::array<float, 4> cannonBlue{90.0f, 140.0f, 220.0f, 1.0f};

	world.Add(std::make_unique<shape::Box>(
		Vec2(700.0f, 200.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		1600.0f,
		50.0f,
		warmRed,
		100.0f,
		0.0f,
		RigidbodyType::Static));

	world.Add(std::make_unique<shape::Incline>(
		Vec2(800.0f, 400.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		600.0f,
		20.0f, // degree input but it will be converted to radian in the simulation
		true,
		warmRed));

	world.Add(std::make_unique<shape::Canon>(
		Vec2(520.0f, 280.0f),
		30.0f,
		cannonBlue));

	world.Add(std::make_unique<shape::Box>(
		Vec2(970.0f, 590.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		100.0f,
		100.0f,
		white,
		100.0f,
		1.0f,
		RigidbodyType::Dynamic));
}

// void Engine::SpawnRandomBox()
// {
// }

// void Engine::SpawnRandomBall()
// {
// }
