#include "core/World.hpp"

#include "core/Renderer.hpp"
#include "common/settings.hpp"

int World::ClampIterations(int value)
{
	if (value < SimulationConstants::MIN_PHYSICS_ITERATIONS)
	{
		return SimulationConstants::MIN_PHYSICS_ITERATIONS;
	}
	if (value > SimulationConstants::MAX_PHYSICS_ITERATIONS)
	{
		return SimulationConstants::MAX_PHYSICS_ITERATIONS;
	}
	return value;
}

void World::Add(std::unique_ptr<physics::Rigidbody> body)
{
	objects.push_back(std::move(body));
}
physics::Rigidbody* World::PickBody(const math::Vec2& p)
{
    for (auto& obj : objects)
    {
        auto aabb = obj->GetAABB();
        if (p.x >= aabb.min.x && p.x <= aabb.max.x &&
            p.y >= aabb.min.y && p.y <= aabb.max.y)
        {
            return obj.get();
        }
    }
    return nullptr;
}


void World::Update(float deltaMs, int iterations)
{
	iterations = ClampIterations(iterations);

	for (const auto &object : objects)
	{
		object->BeginFrameForces();
	}

	for (int currIteration = 0; currIteration < iterations; currIteration++)
	{
		for (const auto &object : objects)
		{
			object->Update(deltaMs, iterations);
		}
		collisionPipeline.BuildPairs(objects);
		for (const auto &pair : collisionPipeline.GetPairs())
		{
			physics::Rigidbody &bodyA = *objects[pair.first];
			physics::Rigidbody &bodyB = *objects[pair.second];
			collisionSolver.ResolveIfColliding(bodyA, bodyB);
		}
	}

	for (const auto &object : objects)
	{
		object->FinalizeForces(deltaMs);
	}

	RemoveObjects();
}

void World::Draw(Renderer &renderer) const
{
	for (const auto &object : objects)
	{
		renderer.DrawShape(*object);
	}
}

size_t World::RigidbodyCount() const
{
	return objects.size();
}

void World::RemoveObjects()
{
	for (size_t i = 0; i < objects.size();)
	{
		collision::AABB box = objects[i]->GetAABB();
		if (box.max.y < viewBottom)
		{
			objects.erase(objects.begin() + static_cast<long>(i));
		}
		else
		{
			++i;
		}
	}
}
