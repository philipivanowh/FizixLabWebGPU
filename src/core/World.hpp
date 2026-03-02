#pragma once

#include "collision/CollisionPipeline.hpp"
#include "collision/CollisionSolver.hpp"
#include "physics/Rigidbody.hpp"
#include "core/Snapshot.hpp"
#include "math/Vec2.hpp"

#include "common/settings.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class Renderer;

// Simple trail point structure
struct TrailPoint
{
	math::Vec2 position;
	std::array<float, 4> color;
	float radius;
	float age;		// Time since created
	float lifetime; // Total lifetime before fading
};

// Trail data for a projectile with smart memory management
struct ProjectileTrail
{
	physics::Rigidbody *projectile; // Pointer to the projectile being tracked
	std::vector<TrailPoint> points;
	float lastPointTime = 0.0f;
	float pointSpacing = 0.03f; // Seconds between trail points (30ms = less frequent)
	float lifetime = 1.5f;		// Total lifetime for trail points (shorter lifespan)
	bool isActive = true;		// Track if trail is still being added to
};

class World
{
public:
	WorldSnapshot CaptureSnapshot() const;
	void RestoreSnapshot(const WorldSnapshot &snap);

	void Add(std::unique_ptr<physics::Rigidbody> body);
	void Initialize(Settings *settings);
	void Update(float deltaMs, int iterations, physics::Rigidbody *&selectedBody, physics::Rigidbody *&draggedBody);
	void Draw(Renderer &renderer) const;

	size_t RigidbodyCount() const;
	void ClearObjects();

	// Trail management
	void StartTrail(physics::Rigidbody *projectile, float lifetime = 2.0f);
	void UpdateTrails(float deltaMs);
	void DrawTrails(Renderer &renderer) const;
	void SetCameraInfo(const math::Vec2 &cameraPos, float zoom); // For LOD calculations

	physics::Rigidbody *PickBody(const math::Vec2 &p);

	// If `position` lies within `snapRadius` of any body’s center, return
	// the body's position; otherwise return the original coordinate.  This
	// helps the UI snap measurements when the user clicks near an object.
	math::Vec2 SnapToNearestDynamicObject(const math::Vec2 &position, float snapRadius);

	void RemoveObject(physics::Rigidbody *draggedBody);

private:
	// Fix 1
	int ComputeAdaptiveIterations(float deltaMs, int requestedIterations) const;

	// Fix 2
	void SweptContactCorrection(physics::Rigidbody &body,
								const math::Vec2 &prevPos,
								const math::Vec2 &integratedPos);
	int ClampIterations(int value) const;
	void RemoveObjects(physics::Rigidbody *&selectedBody, physics::Rigidbody *&draggedBody);
	void UpdateTriggerCollisions();
	float CalculateLODMultiplier(const math::Vec2 &trailPosition) const; // LOD helper

	std::vector<std::unique_ptr<physics::Rigidbody>> objects;
	std::vector<ProjectileTrail> trails; // Active projectile trails
	CollisionPipeline collisionPipeline;
	CollisionSolver collisionSolver;
	Settings *settings = nullptr;

	// Camera info for LOD calculations
	math::Vec2 cameraPos{0.0f, 0.0f};
	float cameraZoom = 1.0f;

	float viewBottom = -10000.0f;
};
