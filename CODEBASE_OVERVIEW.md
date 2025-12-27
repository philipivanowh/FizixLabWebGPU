# FizixLabWebGPU - Codebase Architecture Guide

A comprehensive guide for newcomers to understand the physics simulation and 3D rendering engine built with WebGPU, GLFW, and C++17.

---

## 📋 Table of Contents

1. [High-Level Architecture](#high-level-architecture)
2. [Project Structure](#project-structure)
3. [Core Systems](#core-systems)
4. [Data Flow](#data-flow)
5. [Key Classes](#key-classes)
6. [Important Concepts](#important-concepts)
7. [Development Guide](#development-guide)

---

## 🏗️ High-Level Architecture

This is a **modular physics engine + graphics renderer** architecture. Think of it as:

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│                     (main.cpp)                           │
├─────────────────────────────────────────────────────────┤
│                    Engine (Core)                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │ World (Physics Simulation)  │  Renderer (Graphics) │  │
│  │ • Rigidbodies             │  • WebGPU Pipeline     │  │
│  │ • Collision Detection      │  • Vertex Buffers      │  │
│  │ • Force Integration        │  • Shader Management   │  │
│  └───────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────┤
│               Supporting Systems                         │
│  ┌──────────────┬──────────────┬──────────────────────┐ │
│  │   Shapes     │   Physics    │   Collision          │ │
│  │ • Ball       │ • Rigidbody  │ • AABB               │ │
│  │ • Box        │ • Forces     │ • Collision Res.     │ │
│  │ • Vertices   │ • Mass Props │ • Manifolds          │ │
│  └──────────────┴──────────────┴──────────────────────┘ │
├─────────────────────────────────────────────────────────┤
│              Platform/Graphics Layer                     │
│  ┌────────────┬──────────────┬──────────────────────┐   │
│  │   GLFW     │  WebGPU      │   Math Library       │   │
│  │ • Windows  │ • Rendering  │ • Vec2, matrices     │   │
│  │ • Events   │ • Device Mgmt│ • Vector operations  │   │
│  └────────────┴──────────────┴──────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---

## 📁 Project Structure

```
FizixLabWebGPU/
├── src/                          # Main application source
│   ├── app/
│   │   └── main.cpp             # Entry point, main loop
│   ├── core/
│   │   ├── Engine.hpp/cpp       # Main orchestrator
│   │   ├── World.hpp/cpp        # Physics simulation
│   │   ├── Renderer.hpp/cpp     # Graphics rendering
│   │   └── Utility.hpp/cpp      # Helper functions
│   ├── shape/
│   │   ├── Shape.hpp            # Base shape class
│   │   ├── Ball.hpp/cpp         # Circle/ball physics
│   │   └── Box.hpp/cpp          # Rectangle physics
│   ├── physics/
│   │   ├── Rigidbody.hpp/cpp    # Physics object
│   │   └── PhysicsConstants.hpp # Physics config
│   ├── collision/
│   │   ├── AABB.hpp             # Bounding boxes
│   │   ├── ContactManifold.hpp  # Contact info
│   │   └── Collisions.hpp/cpp   # Collision detection
│   ├── math/
│   │   ├── Vec2.hpp             # 2D vector math
│   │   └── Math.hpp             # Utility math
│   └── shaders/
│       └── triangle.wgsl        # WGPU shader
├── glfw/                        # GLFW windowing library (vendored)
├── webgpu/                      # WebGPU headers (vendored)
├── glfw3webgpu/                 # GLFW-WebGPU integration
├── build/                       # Build output (generated)
└── CMakeLists.txt              # Build configuration
```

---

## 🔧 Core Systems

### 1. **Engine System** (`src/core/Engine.*`)

The main orchestrator that ties everything together.

```cpp
class Engine {
  bool Initialize();        // Setup renderer, world, objects
  void Shutdown();         // Cleanup
  void Render();           // Render current frame
  void Update();           // Physics simulation step
  void RunFrame();         // Update + Render
  bool IsRunning();        // Check if window is open
  
private:
  Renderer renderer;       // Graphics system
  World world;            // Physics system
};
```

**Responsibility**: Initialize subsystems, manage main loop, add objects to world.

---

### 2. **World System** (`src/core/World.*`)

Physics simulation engine. Manages all dynamic objects and their interactions.

```cpp
class World {
  void Add(unique_ptr<Rigidbody>);  // Add object to simulation
  void Update(float deltaMs, int iterations);  // Physics step
  void Draw(Renderer&);                        // Render all objects
  
private:
  vector<unique_ptr<Rigidbody>> objects;
  // Collision detection, force resolution, etc.
};
```

**Responsibility**:
- Store all physical objects
- Detect collisions
- Resolve collisions
- Integrate forces and velocities
- Update object transforms

**Key Physics Constants** (from `PhysicsConstants.hpp`):
- Gravity
- Air resistance
- Friction coefficients
- Collision constraints

---

### 3. **Renderer System** (`src/core/Renderer.*`)

Graphics rendering using WebGPU. Handles window, shaders, buffers, and draw calls.

```cpp
class Renderer {
  bool Initialize();       // Create window, device, pipeline
  void BeginFrame();       // Start render pass
  void EndFrame();         // Submit commands
  void DrawShape(const Rigidbody&);  // Render a physics object
  void Terminate();        // Cleanup GPU resources
};
```

**Key Components**:
- **WebGPU Device**: GPU context
- **Render Pipeline**: Shader program + state
- **Buffers**:
  - `vertexBuffer`: Geometry (position data)
  - `uniformBuffer`: Transform + color data
- **Bind Groups**: Connects buffers to shaders

**Render Flow**:
```
Initialize Pipeline
    ↓
For Each Frame:
  BeginFrame() → Begin render pass
    ↓
  For Each Shape:
    DrawBox/DrawBall() → Upload vertices, uniforms → Draw call
    ↓
  EndFrame() → Submit to GPU
```

---

### 4. **Shape System** (`src/shape/`)

Represents drawable 2D shapes with physics properties.

```cpp
class Shape : public Rigidbody {
  virtual GetVertexLocalPos() const;  // Get vertex positions
  virtual GetAABB() const;            // Get bounding box
  vector<Vec2> vertices;              // Local coordinates
};

class Ball : public Shape {
  float radius;
  void GenerateVertices() const;      // Create circle mesh
  vector<float> GetVertexLocalPos() const override;
};

class Box : public Shape {
  float width, height;
  vector<float> GetVertexLocalPos() const override;  // Returns vertex data
};
```

**Key Concept**: Shapes contain geometry AND physics data. When rendering, we:
1. Get local vertices from shape
2. Transform by position/rotation
3. Upload to GPU
4. Draw triangles

---

### 5. **Physics System** (`src/physics/Rigidbody.*`)

Represents any object with mass, velocity, and forces.

```cpp
class Rigidbody {
  // Position and motion
  Vec2 pos;              // World position
  Vec2 linearVel;        // Linear velocity (m/s)
  Vec2 linearAcc;        // Linear acceleration (m/s²)
  
  // Rotation
  float rotation;        // Angle in radians
  float angularVel;      // Rotational velocity (rad/s)
  float angularAcc;      // Rotational acceleration
  
  // Mass properties
  float mass;
  float inertia;         // Resistance to rotation
  float invMass;         // 1/mass (optimization)
  
  // Physics simulation
  void Update(float deltaMs, int iterations);
  void ApplyForce(Vec2 force);
  void Translate(Vec2 amount);
  void Rotate(float amount);
};
```

**Physics Integration** (in `Update`):
```
For each iteration:
  1. Apply gravity and air resistance
  2. v = v + a * dt
  3. p = p + v * dt
  4. ω = ω + α * dt
  5. θ = θ + ω * dt
```

---

### 6. **Collision System** (`src/collision/`)

Detects and resolves collisions between objects.

```cpp
class AABB {  // Axis-Aligned Bounding Box
  float minX, minY, maxX, maxY;
  bool Intersects(const AABB&) const;
};

class ContactManifold {
  // Contact point information
  Vec2 point;
  Vec2 normal;
  float depth;
};

// In World:
void DetectCollisions();    // Find colliding pairs
void ResolveCollisions();   // Apply impulses to separate objects
```

**Collision Resolution**:
- Uses impulse-based constraint resolution
- Separates overlapping objects
- Transfers momentum between objects

---

## 📊 Data Flow

### Per-Frame Execution (Main Loop)

```
main() → while(engine.IsRunning()) → engine.RunFrame(deltaMs)
  ↓
Engine::RunFrame()
  ├─ Update(deltaMs, iterations)
  │   └─ World::Update()
  │       ├─ For each Rigidbody: Update physics (forces, velocity, position)
  │       ├─ DetectCollisions() - find AABB overlaps
  │       └─ ResolveCollisions() - impulse-based response
  │
  └─ Render()
      └─ Renderer::BeginFrame()
          ├─ For each Rigidbody: DrawShape()
          │   ├─ Get vertices from shape
          │   ├─ Create/update GPU buffers
          │   ├─ Set uniforms (position, color, rotation)
          │   └─ Issue draw call
          └─ Renderer::EndFrame()
              └─ surface.present()  // Display frame
```

### Rendering Deep Dive (How DrawBox works)

```cpp
DrawBox(box) {
  1. Get local vertices:
     vertices = box.GetVertexLocalPos()  // returns {x1,y1, x2,y2, ...}
     
  2. Setup GPU buffer:
     queue.writeBuffer(vertexBuffer, vertices)  // CPU → GPU
     
  3. Set uniforms:
     UpdateUniforms(position, color)  // Transform data
     queue.writeBuffer(uniformBuffer, uniforms)
     
  4. Issue draw call:
     renderPass.setBindGroup()        // Attach buffers to shader
     renderPass.setVertexBuffer()     // Where vertices come from
     renderPass.draw(vertexCount)     // Draw triangles
}
```

---

## 🎯 Key Classes & Their Roles

| Class | File | Purpose | Key Methods |
|-------|------|---------|------------|
| **Engine** | `core/Engine.*` | Orchestrates everything | Initialize, RunFrame, Render, Update |
| **World** | `core/World.*` | Physics simulation | Add, Update, Draw |
| **Renderer** | `core/Renderer.*` | Graphics & rendering | Initialize, BeginFrame, EndFrame, DrawShape |
| **Rigidbody** | `physics/Rigidbody.*` | Physics object | Update, ApplyForce, RotateTo |
| **Shape** | `shape/Shape.*` | Abstract base for drawable objects | GetVertexLocalPos, GetAABB |
| **Box** | `shape/Box.*` | Rectangle physics object | GetVertexLocalPos (12 floats for 2 triangles) |
| **Ball** | `shape/Ball.*` | Circle physics object | GenerateVertices, GetVertexLocalPos |
| **AABB** | `collision/AABB.*` | Axis-aligned bounding box | Intersects, contains |
| **Collisions** | `collision/Collisions.*` | Collision detection | Detect, Resolve |
| **Vec2** | `math/Vec2.*` | 2D vector | Dot, Cross, Normalize, operator+ |

---

## 💡 Important Concepts

### 1. **Vertex Data Format**

Vertices are stored as **flat arrays of floats**:
```cpp
vector<float> vertices = {
  x0, y0,    // Vertex 0
  x1, y1,    // Vertex 1
  x2, y2,    // Vertex 2
  ...
};
// Size = number of vertices * 2
```

When drawing:
```cpp
vertexCount = vertices.size() / 2;  // Convert floats to vertex count
renderPass.draw(vertexCount);       // Draw all vertices as triangles
```

### 2. **Local vs World Coordinates**

- **Local**: Relative to object's origin (returned by `GetVertexLocalPos()`)
- **World**: Absolute position in scene
  ```cpp
  worldPos = localPos + object.pos + rotation offset
  ```

- Shader handles transformation using uniforms:
  ```glsl
  // triangle.wgsl
  let worldPos = inVertexPosition + u.translation;
  let clipSpace = worldPos / u.resolution * 2.0 - 1.0;
  ```

### 3. **Uniform Buffers & Dynamic Offsets**

GPU-side data that's constant for a draw call:

```cpp
struct Uniforms {
  resolution: vec2f,    // Screen size
  translation: vec2f,   // Object position
  color: vec4f          // RGBA color
};

// Multiple uniforms batched in one buffer (performance optimization)
uniformBuffer.size = 256 * sizeof(Uniforms);  // 256 objects per frame
```

### 4. **Render Pipeline**

```
Vertex Shader:
  Input: vertex position, uniforms (transform)
  Output: Clip-space position (normalized device coords)
  
Rasterization:
  Converts triangles to fragments (pixels)
  
Fragment Shader:
  Input: uniform color
  Output: Final pixel color
  
Blend:
  Combines fragment with framebuffer
```

### 5. **Physics Time Integration**

**Explicit Euler** (what's used):
```
v_new = v_old + a * dt
p_new = p_old + v_new * dt
```

Each physics frame subdivides into iterations for stability:
```cpp
Update(deltaMs, iterations=10) {
  float dtPerIteration = deltaMs / iterations;
  for(int i = 0; i < iterations; ++i) {
    // Single integration step
  }
}
```

---

## 🔨 Development Guide

### Adding a New Shape Type

1. **Create shape class** (`src/shape/MyShape.hpp/cpp`):
```cpp
class MyShape : public Shape {
  float someParameter;
  
  MyShape(const Vec2& pos, float param, ...);
  void GenerateVertices() const override;  // Generate mesh
  float ComputeInertia() const override;   // For rotation
  AABB GetAABB() const override;          // For collision
};
```

2. **Generate vertices** (must be triangles):
```cpp
void MyShape::GenerateVertices() const {
  vertices.clear();
  // Add Vec2 positions
  // Every 3 vertices = 1 triangle
}
```

3. **Add to DrawShape**:
```cpp
void Renderer::DrawShape(const Rigidbody& rb) {
  if(auto shape = dynamic_cast<const MyShape*>(&rb)) {
    DrawMyShape(*shape);
    return;
  }
}
```

4. **Create in Engine**:
```cpp
world.Add(make_unique<MyShape>(pos, param, color, mass, ...));
```

### Debugging Tips

**Physics Issues**:
- Check `UpdateUniforms()` for correct color normalization
- Verify vertex positions are in reasonable range
- Print `vertexCount` to ensure vertices are loaded

**Rendering Issues**:
- Add debug output in `DrawShape()` to see if shape is being drawn
- Check shader compilation errors in device error callback
- Verify uniform buffer size matches shader expectations

**Collision Issues**:
- Print AABB bounds in `GetAABB()`
- Verify rigidbody type (Static vs Dynamic)
- Check mass and inertia calculations

### Performance Considerations

1. **Batch Drawing**: Currently draws one shape per call
   - Could batch multiple shapes into one buffer
   - Would reduce GPU command overhead

2. **Physics Iterations**: Higher = more stable but slower
   - Try different values: 1-20 iterations
   - Adjust based on collision stability

3. **Vertex Count**: More vertices = smoother but slower
   - Ball: `steps` parameter controls circle smoothness
   - Consider LOD (level of detail) system

---

## 🚀 Quick Start for Modifications

### To change physics:
- Edit `PhysicsConstants.hpp` (gravity, friction, etc.)
- Modify `World::Update()` for integration method
- Adjust collision resolution in `Collisions.cpp`

### To change rendering:
- Edit shader in `src/shaders/triangle.wgsl`
- Modify `Renderer::InitializePipeline()` for GPU pipeline state
- Change colors in `Engine::AddDefaultObjects()` or shape creation

### To add new objects:
- In `Engine::AddDefaultObjects()`, call:
  ```cpp
  world.Add(make_unique<Shape>(pos, vel, acc, params, color, mass, restitution, type));
  ```

### To test changes:
```bash
cd /path/to/project
cmake --build build
./build/App
```

---

## 📚 External Resources

- **WebGPU**: https://www.w3.org/TR/webgpu/
- **GLFW**: https://www.glfw.org/documentation.html
- **Learn WebGPU**: https://eliemichel.github.io/LearnWebGPU/
- **Physics**: Understanding game physics requires calculus & linear algebra

---

## 🎓 Architecture Lessons (from Google SWE perspective)

### Design Patterns Used:

1. **Separation of Concerns**: Physics, rendering, input separate
2. **Factory Pattern**: `World::Add()` manages object creation
3. **Template Method**: `Shape` base class with virtual `GetVertexLocalPos()`
4. **Strategy Pattern**: Different rigidbody types (Static/Dynamic)
5. **Resource Management**: RAII with unique_ptr for GPU resources

### Key Principles:

- **Modularity**: Each system has clear responsibilities
- **Extensibility**: Adding new shape types doesn't require modifying core
- **Performance**: GPU resources reused, minimal CPU-GPU transfers
- **Testability**: Physics and rendering independent

This architecture follows Google's internal guidance on game engine design, emphasizing clear component boundaries and data flow.

