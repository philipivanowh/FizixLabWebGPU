# FizixLabWebGPU Architecture

This document explains how the codebase is organized and how the runtime systems fit together. It is aimed at new contributors who want to understand where to make changes.

## High-level overview

The project is a small physics + rendering engine built on WebGPU. The entry point sets up an `Engine` that owns a `World` (physics and collision) and a `Renderer` (WebGPU + GLFW). Each frame runs physics updates and then draws the current world state.

Key flow:

```
main (src/app/main.cpp)
  -> Engine::Initialize
     -> Renderer::Initialize
     -> Engine::AddDefaultObjects
  -> per-frame loop
     -> Engine::RunFrame
        -> World::Update
        -> World::Draw (via Renderer)
  -> Engine::Shutdown
```

## Source tree map

- `src/app/main.cpp`
  - Program entry. Handles native vs Emscripten loop and calls into the engine.
- `src/core/Engine.*`
  - Orchestrates the frame loop and owns `Renderer` + `World`.
- `src/core/World.*`
  - Physics simulation loop, collision detection, and collision resolution.
- `src/core/Renderer.*`
  - WebGPU/GLFW setup, shaders, pipelines, buffers, and draw calls.
- `src/physics/Rigidbody.*`
  - Base physics body with integration, forces, mass properties.
- `src/shape/*`
  - Concrete shapes (Box, Ball) that extend `Rigidbody` with geometry.
- `src/collision/*`
  - AABB broad-phase, shape intersection tests, contact generation.
- `src/math/*`
  - Vector math, transforms, utility math helpers.
- `src/common/settings.hpp`
  - Simulation and physics constants (gravity, iteration limits, scaling).
- `src/shaders/triangle.wgsl`
  - WGSL shader used by the renderer pipeline.

Vendor/third-party:

- `glfw/`, `webgpu/`, `glfw3webgpu/`
  - Local dependencies pulled in by CMake.
- `webBuild/`
  - Generated build output (can be deleted and re-generated).
- `src/third_party/webgpu/webgpu.h`
  - WebGPU C header used for builds that need it.

## Runtime flow

### Frame loop

`src/app/main.cpp` drives the loop and calls `Engine::RunFrame(deltaMs, iterations)`. Emscripten uses a callback-based loop, while native builds use a standard while loop.

### Engine

`Engine` (in `src/core/Engine.*`) is responsible for:

- Initializing the renderer and creating the initial world content.
- Advancing physics (`World::Update`).
- Rendering the world (`World::Draw`).

The default scene setup happens in `Engine::AddDefaultObjects`.

### Physics and collision

Physics is handled by `World` and `Rigidbody`:

- `World::Update` runs multiple sub-iterations per frame to improve stability.
- The physics update calls `Rigidbody::Update`, which integrates position/velocity and applies gravity.
- `World::BroadPhaseDetection` performs AABB tests to find potentially colliding pairs.
- `World::NarrowPhaseDetection` uses shape-specific tests in `collision::Collide` and computes contact points.
- Collision resolution uses impulses, rotation, and friction (see `World::ResolveCollisionWithRotationAndFriction`).

Shape-specific behavior is provided by classes in `src/shape`:

- `shape::Box` defines vertices, inertia, and AABB for boxes.
- `shape::Ball` defines radius-based AABB and vertices for circles.

### Rendering

`Renderer` manages WebGPU + GLFW:

- Creates the window, surface, adapter, and device.
- Builds pipelines and buffers with a WGSL shader.
- `BeginFrame`/`EndFrame` wrap each frame.
- `DrawShape` dispatches to `DrawBox` or `DrawBall` based on the runtime type of the body.

Geometry is currently built on the CPU and uploaded to a vertex buffer each frame.

## Data model

Physics bodies share a base class, `physics::Rigidbody`, which contains:

- Position, velocity, rotation, forces, mass/inertia.
- Integration in `Rigidbody::Update` using a semi-implicit scheme.

Shapes extend `Rigidbody` with geometry and rendering metadata:

- `shape::Box`: width/height, vertex generation, color.
- `shape::Ball`: radius, vertex generation, color.

Rendering uses the shape data to generate vertices for each draw call.

## Build and entry points

The CMake build target `App` uses `src/app/main.cpp` as the executable entry point. The root-level `main.cpp` is a standalone Learn WebGPU sample and is not compiled into `App`.

## Contribution paths

Common places to extend:

- Add a new shape: create a class in `src/shape`, implement AABB and vertex generation, and add handling in `Renderer::DrawShape`.
- Add a new force or integration: extend `physics::Rigidbody::UpdateForce` or add force helpers.
- Add new collision logic: update `collision::Collide` and `FindContactPoints` to support the new shape type.
- Add rendering features: extend `src/shaders/triangle.wgsl` and update `Renderer` pipelines.

## Notes and constraints

- Coordinates are in pixels; meters are converted using `SimulationConstants::PIXELS_PER_METER`.
- Physics iterations are clamped between min/max constants in `src/common/settings.hpp`.
- The renderer assumes a fixed window size; the swapchain config is not resized.

