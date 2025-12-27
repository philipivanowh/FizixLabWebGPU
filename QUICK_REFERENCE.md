# FizixLabWebGPU - Quick Reference Guide

Fast lookup guide for common tasks and key code locations.

---

## 🎯 Find It Fast

### Entry Points
- **Main Entry**: `src/app/main.cpp` - Program start, main loop setup
- **Engine Init**: `Engine::Initialize()` - Renderer, World, Objects setup
- **Frame Loop**: `Engine::RunFrame()` - Called every frame

### Physics
- **Physics Step**: `World::Update()` - All physics simulation
- **Force Application**: `Rigidbody::ApplyForce()` - Add forces
- **Integration**: `Rigidbody::Update()` - Physics calculation
- **Collision Detect**: `World` class - Collision pipeline
- **Constants**: `src/physics/PhysicsConstants.hpp` - Gravity, friction, etc.

### Rendering
- **GPU Setup**: `Renderer::Initialize()` - Device, pipeline creation
- **Draw Loop**: `Renderer::BeginFrame()` / `EndFrame()`
- **Shape Drawing**: `Renderer::DrawBox()`, `DrawBall()` - Render functions
- **Shader**: `src/shaders/triangle.wgsl` - GPU shader code
- **Buffers**: `uniformBuffer`, `vertexBuffer` in Renderer

### Shapes
- **Base Class**: `src/shape/Shape.hpp` - Interface
- **Box**: `src/shape/Box.hpp/cpp` - Rectangle
- **Ball**: `src/shape/Ball.hpp/cpp` - Circle
- **Vertices**: `GetVertexLocalPos()` - Get geometry data

### Collision
- **Bounding Boxes**: `src/collision/AABB.hpp` - Axis-aligned boxes
- **Detection**: `Collisions::Detect()` - Find collisions
- **Resolution**: `Collisions::Resolve()` - Fix overlaps

---

## 📝 Common Code Patterns

### Create a Physics Object

```cpp
// In Engine::AddDefaultObjects():
world.Add(std::make_unique<shape::Box>(
  Vec2(700.0f, 300.0f),      // Position
  Vec2(0.0f, 1.0f),          // Linear velocity
  Vec2(0.0f, 0.0f),          // Linear acceleration
  80.0f,                      // Width
  80.0f,                      // Height
  std::array{255.0f, 255.0f, 255.0f, 1.0f},  // Color (RGBA)
  15.0f,                      // Mass
  0.5f,                       // Restitution (bounciness)
  physics::RigidbodyType::Dynamic  // Static or Dynamic
));
```

### Add Force to Object

```cpp
// In some update loop:
Rigidbody* obj = ...;
Vec2 force(100.0f, 50.0f);
obj->ApplyForce(force);
```

### Modify Physics Simulation

```cpp
// In World::Update(deltaMs, iterations):
// Increase iterations for more stability
for(int iter = 0; iter < iterations; ++iter) {
  // Each rigidbody updates
  // Collision detection
  // Collision resolution
}
```

### Add New Shape to Rendering

```cpp
void Renderer::DrawShape(const physics::Rigidbody &rb) {
  if(auto box = dynamic_cast<const shape::Box*>(&rb)) {
    DrawBox(*box);
    return;
  }
  if(auto ball = dynamic_cast<const shape::Ball*>(&rb)) {
    DrawBall(*ball);
    return;
  }
  if(auto myshape = dynamic_cast<const shape::MyShape*>(&rb)) {
    DrawMyShape(*myshape);
    return;
  }
}
```

### Generate Circle Vertices

```cpp
void Ball::GenerateVertices() const {
  vertices.clear();
  vertices.reserve(steps * 3);
  
  float prevX = radius * cos(0.0f);
  float prevY = radius * sin(0.0f);
  
  for(int i = 1; i <= steps; ++i) {
    float theta = angle * static_cast<float>(i);
    float newX = radius * cos(theta);
    float newY = radius * sin(theta);
    
    // Create triangle from center to edge
    vertices.push_back({0.0f, 0.0f});      // Center
    vertices.push_back({prevX, prevY});    // Previous edge
    vertices.push_back({newX, newY});      // New edge
    
    prevX = newX;
    prevY = newY;
  }
}
```

---

## 🔧 Key Configuration Values

### In `PhysicsConstants.hpp`:
```cpp
const float GRAVITY = 1000.0f;              // m/s²
const float AIR_RESISTANCE = 0.99f;         // Damping
const float DEFAULT_STATIC_FRICTION = 0.6f;
const float DEFAULT_KINETIC_FRICTION = 0.3f;
```

### In `Renderer::Initialize()`:
```cpp
const uint32_t windowWidth = 1600;
const uint32_t windowHeight = 900;
const float uniformAlignment = 256;         // GPU alignment requirement
```

### In `Ball::GenerateVertices()`:
```cpp
steps = 40;  // Circle smoothness (increase for smoother circles)
```

---

## 🐛 Debugging Checklist

- [ ] Shader loads: Check `Utility::LoadFileToString()` output
- [ ] Objects render: Add debug output in `DrawBox()`, `DrawBall()`
- [ ] Physics updates: Print velocities in `Rigidbody::Update()`
- [ ] Collisions work: Print in `World::DetectCollisions()`
- [ ] Colors correct: Verify RGB normalization in `UpdateUniforms()`
- [ ] Rotation shows: Check `Ball::GetVertexLocalPos()` returns rotation line

---

## 📊 Data Structures

### Vec2 (2D Vector)
```cpp
struct Vec2 {
  float x, y;
  
  Vec2 operator+(const Vec2& other) const;
  Vec2 operator-(const Vec2& other) const;
  Vec2 operator*(float scalar) const;
  float Dot(const Vec2& other) const;
  float Cross(const Vec2& other) const;
  float Length() const;
  Vec2 Normalize() const;
};
```

### AABB (Bounding Box)
```cpp
struct AABB {
  float minX, minY, maxX, maxY;
  
  bool Intersects(const AABB& other) const;
  bool Contains(float x, float y) const;
};
```

### Uniforms (GPU Data)
```cpp
struct Uniforms {
  float resolution[2];   // Screen size
  float translation[2];  // Object position
  float color[4];        // RGBA color
};
```

---

## 🎨 Shader Pipeline

### Vertex Shader Input
```glsl
@location(0) in_vertex_position: vec2f  // Vertex position from buffer
```

### Vertex Shader Output
```glsl
@builtin(position) vec4f  // Clip-space position
```

### Fragment Shader Input
- Uniforms: `resolution`, `translation`, `color`

### Fragment Shader Output
```glsl
@location(0) vec4f  // RGBA color to framebuffer
```

---

## 📈 Performance Tips

1. **Reduce vertex count**: Lower `Ball.steps` for fewer triangles
2. **Reduce iterations**: Lower physics iterations for speed (trade stability)
3. **Batch rendering**: Combine multiple objects into one draw call
4. **Use simpler collision**: AABB-only (no SAT) is faster
5. **Profile**: Time `World::Update()` vs `Renderer::EndFrame()`

---

## 🚨 Common Issues & Fixes

| Issue | Cause | Fix |
|-------|-------|-----|
| Black screen | Shader not loaded | Check `src/shaders/triangle.wgsl` path |
| Objects don't move | Physics not updating | Check `world.Update()` is called |
| Objects overlap | Collision not resolving | Check `DetectCollisions()` and `ResolveCollisions()` |
| Wrong colors | Not normalized to 0-1 | Color values > 1 should be / 255 |
| GPU crash | Uniform buffer too small | Check `uniformBufferSize` calculation |
| Line doesn't rotate | `GenerateVertices()` not called | Call in `Ball::GetVertexLocalPos()` override |

---

## 📁 File Organization by Feature

### To Change Physics Behavior
1. `src/physics/PhysicsConstants.hpp` - Edit constants
2. `src/physics/Rigidbody.cpp` - Edit integration method
3. `src/core/World.cpp` - Edit collision resolution

### To Change Graphics
1. `src/shaders/triangle.wgsl` - Edit shader
2. `src/core/Renderer.cpp` - Edit pipeline setup
3. `src/app/main.cpp` - Edit camera/view (if added)

### To Add New Shape
1. Create `src/shape/MyShape.hpp` and `.cpp`
2. Implement `GetVertexLocalPos()`, `GetAABB()`, `ComputeInertia()`
3. Add to `Renderer::DrawShape()` dispatch
4. Instantiate in `Engine::AddDefaultObjects()`

### To Debug
1. Add output to shape's `Draw*` function
2. Print in `World::Update()` for physics
3. Check device error callback for GPU errors
4. Look at shader compilation in `Renderer::InitializePipeline()`

---

## 🔗 Dependencies & Integration

```
main.cpp
  ↓
Engine (orchestrator)
  ├─ Renderer (graphics)
  │   ├─ GLFW (window/input)
  │   ├─ WebGPU (GPU)
  │   └─ glfw3webgpu (integration)
  │
  └─ World (physics)
      ├─ Rigidbody (physics objects)
      ├─ Shape (geometry)
      │   ├─ Box
      │   └─ Ball
      ├─ Collision (AABB, detection, resolution)
      └─ Math (Vec2, operations)
```

---

## ✅ Building & Running

```bash
# Configure
cmake . -B build

# Build
cmake --build build

# Run
./build/App

# Clean
rm -rf build
```

---

## 📚 Key Methods to Know

| Task | Method | Location |
|------|--------|----------|
| Create object | `World::Add()` | `src/core/World.cpp` |
| Update physics | `Rigidbody::Update()` | `src/physics/Rigidbody.cpp` |
| Detect collisions | `Collisions::Detect()` | `src/collision/Collisions.cpp` |
| Render object | `Renderer::DrawBox/Ball()` | `src/core/Renderer.cpp` |
| Get vertices | `Shape::GetVertexLocalPos()` | `src/shape/*.cpp` |
| Upload to GPU | `queue.writeBuffer()` | WebGPU API |
| Issue draw call | `renderPass.draw()` | WebGPU API |

---

## 💾 Important File Formats

### Vertex Data Format
```
Float array: [x0, y0, x1, y1, x2, y2, ...]
Size: num_vertices * 2 floats
```

### Uniform Data Format
```cpp
struct {
  float[2] resolution,
  float[2] translation,
  float[4] color
}
```

### Shader Format
```wgsl
@vertex
fn vs_main(@location(0) pos: vec2f) -> @builtin(position) vec4f {
  // Must output clip-space position (-1 to 1)
}

@fragment
fn fs_main() -> @location(0) vec4f {
  // Must output color
}
```

---

## 🎓 Study Path

1. **Understand main loop**: Read `src/app/main.cpp`
2. **See orchestration**: Read `Engine::RunFrame()`
3. **Learn physics**: Read `World::Update()` and `Rigidbody::Update()`
4. **Learn rendering**: Read `Renderer::DrawBox()` then `Renderer::Initialize()`
5. **Understand shader**: Read `src/shaders/triangle.wgsl`
6. **Add feature**: Create new shape type following existing patterns

