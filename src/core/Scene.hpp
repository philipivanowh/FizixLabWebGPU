#pragma once

#include "physics/Body.hpp"
#include "physics/Manifold.hpp"
#include "math/Vec2.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class Renderer;

namespace physics {

class Scene {
public:
	void Add(std::unique_ptr<Body> body);
	void Update(float deltaMs, int iterations);
	void Draw(Renderer& renderer) const;

	size_t BodyCount() const;

private:
	int ClampIterations(int value);
	void BroadPhaseDetection();
	void NarrowPhaseDetection();
	void SeperateBodies(Body& bodyA, Body& bodyB, const math::Vec2& mtv);
	void ResolveCollisionBasic(const Manifold& contact);
	void ResolveCollisionWithRotation(const Manifold& contact);
	void ResolveCollisionWithRotationAndFriction(const Manifold& contact);
	void RemoveObjects();
	static bool ContainsContactPoint(const std::vector<math::Vec2>& list,
		const math::Vec2& candidate,
		float epsilon);

	std::vector<std::unique_ptr<Body>> objects;
	std::vector<std::tuple <int, int>> contactPairs;
	std::vector<math::Vec2> contactPointsList;

	math::Vec2 contactList[2];

	math::Vec2 impulseList[2];
	math::Vec2 impulseFrictionList[2];
	math::Vec2 raList[2];
	math::Vec2 rbList[2];
	float jList[2];

	float viewBottom = -200.0f;
};

} // namespace physics
