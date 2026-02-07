#include "UIManager.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
EM_JS(void, UIManager_PublishStats, (int bodyCount), {
    if (window.FizixUI && window.FizixUI.setStats) {
        window.FizixUI.setStats(bodyCount);
    }
});
#endif

void UIManager::InitializeImGui(Renderer &renderer)
{
    (void)renderer;
}

void UIManager::TerminateImGui()
{
}

void UIManager::BeginImGuiFrame()
{
}

void UIManager::EndImGuiFrame(Renderer &renderer)
{
    (void)renderer;
}

void UIManager::RenderMainControls(std::size_t bodyCount, Settings &settings)
{
    (void)settings;
#ifdef __EMSCRIPTEN__
    UIManager_PublishStats(static_cast<int>(bodyCount));
#else
    (void)bodyCount;
#endif
}

void UIManager::RenderSpawner()
{
}

void UIManager::QueueSpawn(const SpawnSettings &settings)
{
    spawnSettings = settings;
    spawnRequestPending = true;
}

bool UIManager::ConsumeSpawnRequest(SpawnSettings &out)
{
    if (!spawnRequestPending)
    {
        return false;
    }

    out = spawnSettings;
    spawnRequestPending = false;
    return true;
}

#ifdef __EMSCRIPTEN__
extern "C"
{
    EMSCRIPTEN_KEEPALIVE void UIManager_SetDragMode(int mode)
    {
        Engine::settings.dragMode = static_cast<DragMode>(mode);
    }

    EMSCRIPTEN_KEEPALIVE void UIManager_ClearBodies()
    {
        Engine::ClearBodies();
    }

    EMSCRIPTEN_KEEPALIVE void UIManager_RequestSpawn(
        int shapeType,
        float positionX,
        float positionY,
        float velocityX,
        float velocityY,
        float boxWidth,
        float boxHeight,
        float base,
        float angle,
        float radius,
        float mass,
        float staticFriction,
        float kineticFriction,
        int flip,
        float colorR,
        float colorG,
        float colorB,
        float colorA,
        float restitution,
        int bodyType)
    {
        SpawnSettings settings;
        settings.shapeType = static_cast<shape::ShapeType>(shapeType);
        settings.position = math::Vec2(positionX, positionY);
        settings.velocity = math::Vec2(velocityX, velocityY);
        settings.boxWidth = boxWidth;
        settings.boxHeight = boxHeight;
        settings.base = base;
        settings.angle = angle;
        settings.radius = radius;
        settings.mass = mass;
        settings.staticFriction = staticFriction;
        settings.kineticFriction = kineticFriction;
        settings.flip = (flip != 0);
        settings.color = {colorR, colorG, colorB, colorA};
        settings.restitution = restitution;
        settings.bodyType = static_cast<physics::RigidbodyType>(bodyType);

        Engine::uiManager.QueueSpawn(settings);
    }
}
#endif
