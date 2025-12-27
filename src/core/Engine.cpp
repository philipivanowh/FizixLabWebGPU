#include "core/Engine.hpp"

#include "shape/Ball.hpp"
#include "shape/Box.hpp"

#include <array>
#include <memory>
using math::Vec2;

bool Engine::Initialize()
{
	if (!renderer.Initialize())
	{
		return false;
	}
	AddDefaultObjects();
	return true;
}

void Engine::Shutdown()
{
	renderer.Terminate();
}

void Engine::Update(float deltaMs, int iterations)
{
	world.Update(deltaMs, iterations);
}

void Engine::Render()
{
	renderer.BeginFrame();
	world.Draw(renderer);

	// renderer.DrawTestTriangle();
	// renderer.DrawTest2Triangle();

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
		Vec2(700.0f, 300.0f),
		Vec2(0.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		80.0f,
		80.0f,
		white,
		15.0f,
		0.5f,
		RigidbodyType::Dynamic));

	world.Add(std::make_unique<shape::Box>(
		Vec2(700.0f, 200.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		1600.0f,
		50.0f,
		skyBlue,
		100.0f,
		0.0f,
		RigidbodyType::Static
	));

	auto slope = std::make_unique<shape::Box>(
		Vec2(1000.0f, 400.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		500.0f,
		40.0f,
		skyBlue,
		100.0f,
		0.0f,
		RigidbodyType::Static
	);
	const float slopeAngle = -3.14159265358979323846f / 6.0f;
	slope->RotateTo(slopeAngle);
	world.Add(std::move(slope));

		world.Add(std::make_unique<shape::Box>(
		Vec2(500.0f, 300.0f),
		Vec2(5.0f, 1.0f),
		Vec2(0.0f, 0.0f),
		50.0f,
		50.0f,
		warmRed,
		5.0f,
		0.5f,
		RigidbodyType::Dynamic
	));

	world.Add(std::make_unique<shape::Box>(
		Vec2(896.0f, 670.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		70.0f,
		70.0f,
		yellow,
		15.0f,
		0.5f,
		RigidbodyType::Dynamic
	));

	world.Add(std::make_unique<shape::Box>(
		Vec2(906.0f, 570.0f),
		Vec2(0.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		70.0f,
		70.0f,
		warmRed,
		50.0f,
		0.5f,
		RigidbodyType::Dynamic
	));

	world.Add(std::make_unique<shape::Ball>(
		Vec2(500.0f, 700.0f),
		Vec2(-1.0f, 0.0f),
		Vec2(0.0f, 0.0f),
		50.0f,
		warmRed,
		5.0f,
		0.2f,
		RigidbodyType::Dynamic
	));
}

void Engine::SpawnRandomBox()
{
}

void Engine::SpawnRandomBall()
{
}
