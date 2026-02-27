#pragma once

#include "collision/CollisionPipeline.hpp"
#include "collision/CollisionSolver.hpp"
#include "physics/Rigidbody.hpp"
#include "core/Snapshot.hpp"

#include "common/settings.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class Renderer;

class World
{
public:
	WorldSnapshot CaptureSnapshot() const;
	void RestoreSnapshot(const WorldSnapshot &snap);

	void Add(std::unique_ptr<physics::Rigidbody> body);
	void Update(float deltaMs, int iterations, Settings &settings,physics::Rigidbody *selectedBody,physics::Rigidbody *draggedBody);
	void Draw(Renderer &renderer) const;

	size_t RigidbodyCount() const;
	void ClearObjects();

	physics::Rigidbody *PickBody(const math::Vec2 &p);

	// If `position` lies within `snapRadius` of any body’s center, return
	// the body's position; otherwise return the original coordinate.  This
	// helps the UI snap measurements when the user clicks near an object.
	math::Vec2 SnapToNearestDynamicObject(const math::Vec2 &position, float snapRadius);

private : 
	int ClampIterations(int value);
	void RemoveObjects(physics::Rigidbody *selectedBody,physics::Rigidbody *draggedBody);

	std::vector<std::unique_ptr<physics::Rigidbody>> objects;
	CollisionPipeline collisionPipeline;
	CollisionSolver collisionSolver;

	float viewBottom = -10000.0f;
};
