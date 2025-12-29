#include "core/CollisionPipeline.hpp"

#include "collision/Collisions.hpp"
#include "physics/Rigidbody.hpp"

void CollisionPipeline::BuildPairs(const std::vector<std::unique_ptr<physics::Rigidbody>>& bodies)
{
	pairs.clear();

	for (size_t i = 0; i + 1 < bodies.size(); i++)
	{
		physics::Rigidbody& bodyA = *bodies[i];
		const collision::AABB bodyAABB = bodyA.GetAABB();

		for (size_t j = i + 1; j < bodies.size(); j++)
		{
			physics::Rigidbody& bodyB = *bodies[j];
			const collision::AABB bodyBABB = bodyB.GetAABB();

			if (bodyA.bodyType == physics::RigidbodyType::Static &&
				bodyB.bodyType == physics::RigidbodyType::Static)
			{
				continue;
			}

			if (!IntersectAABBs(bodyAABB, bodyBABB))
			{
				continue;
			}

			pairs.emplace_back(i, j);
		}
	}
}

const std::vector<CollisionPipeline::Pair>& CollisionPipeline::GetPairs() const
{
	return pairs;
}
