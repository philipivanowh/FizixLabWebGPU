FizixLabWebGPU - Physics Engine with WebGPU Renderer
==================================================

A modular 2D physics simulation engine with real-time WebGPU graphics rendering. Built with C++17, GLFW, and WebGPU.

**Perfect for learning**: Game physics, GPU programming, rendering architecture, and software design patterns.

## 📚 Documentation

New to this codebase? **Start here:**

1. **[CODEBASE_OVERVIEW.md](./CODEBASE_OVERVIEW.md)** - Complete architecture guide
   - High-level system design
   - Data flow diagrams
   - Key components explained
   - Development best practices
   - Great for understanding the big picture

2. **[QUICK_REFERENCE.md](./QUICK_REFERENCE.md)** - Fast lookup guide
   - Find files and functions quickly
   - Common code patterns
   - Debugging checklist
   - Configuration values
   - Perfect for daily development

## Building
--------

```bash
cmake . -B build
cmake --build build
```

Run with: `./build/App` (macOS/Linux) or `build\Debug\App.exe` (Windows MSVC)

## Project Structure

```
src/
├── app/          # Application entry point
├── core/         # Engine, World (physics), Renderer
├── shape/        # Ball, Box shape definitions
├── physics/      # Rigidbody physics simulation
├── collision/    # AABB, collision detection
├── math/         # Vec2 and math utilities
└── shaders/      # WGPU shader code (WGSL)
```

## Key Features

- **Physics Simulation**: Rigidbody dynamics with gravity, friction, collision response
- **Collision Detection**: AABB-based broad phase, response resolution
- **Real-time Rendering**: WebGPU GPU rendering with GLFW windowing
- **Extensible Design**: Easy to add new shape types and physics features
- **Educational**: Well-structured code with clear separation of concerns

## Quick Start

The engine automatically creates a physics world with objects. Modify objects in `src/core/Engine.cpp`:

```cpp
world.Add(std::make_unique<shape::Ball>(
  Vec2(500.0f, 700.0f),  // Position
  Vec2(0.0f, 0.0f),      // Velocity
  Vec2(0.0f, 0.0f),      // Acceleration
  50.0f,                  // Radius
  color,                  // RGBA color
  mass,                   // Mass in kg
  restitution,            // Bounciness
  RigidbodyType::Dynamic  // Physics type
));
```

## Resources

- [Learn WebGPU Tutorial](https://eliemichel.github.io/LearnWebGPU/)
- [GLFW Documentation](https://www.glfw.org/documentation.html)
- [WebGPU Specification](https://www.w3.org/TR/webgpu/)

See [CODEBASE_OVERVIEW.md](./CODEBASE_OVERVIEW.md) for architecture details.
