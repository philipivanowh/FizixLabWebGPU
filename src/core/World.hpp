#pragma once

#include "physics/Rigidbody.hpp"
#include "collision/ContactManifold.hpp"
#include "math/Vec2.hpp"

#include <cstddef>
#include <memory>
#include <vector>

class Renderer;


class World {
public:
	void Add(std::unique_ptr<physics::Rigidbody> Rigidbody);
	void Update(float deltaMs, int iterations);
	void Draw(Renderer& renderer) const;

	size_t RigidbodyCount() const;

private:
	int ClampIterations(int value);
	void BroadPhaseDetection();
	void NarrowPhaseDetection();
	void SeperateBodies(physics::Rigidbody& RigidbodyA, physics::Rigidbody& RigidbodyB, const math::Vec2& mtv);
	void ResolveCollisionBasic(const collision::ContactManifold& contact);
	void ResolveCollisionWithRotation(const collision::ContactManifold& contact);
	void ResolveCollisionWithRotationAndFriction(const collision::ContactManifold& contact);
	void RemoveObjects();
	static bool ContainsContactPoint(const std::vector<math::Vec2>& list,
		const math::Vec2& candidate,
		float epsilon);

	std::vector<std::unique_ptr<physics::Rigidbody>> objects;
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
