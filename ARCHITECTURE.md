# Architecture Decision Records

Documentation of significant architecture decisions and their rationale.

---

## ADR-001: Separation of Physics and Rendering

**Status**: Accepted

**Context**:
The engine needed to handle physics simulation independent of graphics rendering. This allows for:
- Different frame rates (physics at 60 Hz, rendering at 120 Hz)
- Offline physics simulation
- Easy testing without GPU

**Decision**:
Created separate `World` (physics) and `Renderer` (graphics) classes with clear data flow.

**Consequences**:
- ✅ Physics can run independently
- ✅ Easy to unit test physics
- ✅ Can swap renderers (CPU debug, GPU production)
- ⚠️ Need to sync transforms between systems
- ⚠️ Slightly more overhead than tightly coupled

**Code Pattern**:
```cpp
class Engine {
  World world;      // Physics
  Renderer renderer; // Graphics
  
  void RunFrame() {
    world.Update(deltaMs, iterations);  // Physics first
    renderer.BeginFrame();
    world.Draw(renderer);               // Physics tells renderer what to draw
    renderer.EndFrame();
  }
};
```

---

## ADR-002: GPU Memory Layout for Uniforms

**Status**: Accepted

**Context**:
Multiple objects per frame need different uniforms (position, color). Options:
1. One draw call per object, update uniforms each time (slow)
2. Batch all objects in one buffer, use dynamic offsets (current)
3. Use instancing (more complex)

**Decision**:
Use dynamic offsets with a single uniform buffer. Each object gets a stride-aligned slot.

**Rationale**:
- ✅ Fewer GPU state changes
- ✅ Simpler than instancing
- ✅ Still per-object data
- ⚠️ Requires alignment (256-byte stride)
- ⚠️ Limited buffer size (65KB / 256 bytes = 256 objects)

**Code Pattern**:
```cpp
// Single buffer, multiple offsets
uniformBufferSize = 256 * sizeof(Uniforms);  // 256 slots
uniformBuffer = device.createBuffer({.size = uniformBufferSize, ...});

// Per draw call:
uint32_t offset = UpdateUniforms(...);
renderPass.setBindGroup(0, group, 1, &offset);  // Dynamic offset
renderPass.draw(count);
```

---

## ADR-003: Vertex Data as Flat Float Arrays

**Status**: Accepted

**Context**:
Vertices needed to be stored efficiently for GPU transfer. Options:
1. Array of Vec2 structs
2. Flat float array [x0, y0, x1, y1, ...]
3. Separate position buffers

**Decision**:
Use flat float arrays matching GPU vertex buffer format directly.

**Rationale**:
- ✅ Zero conversion overhead on upload
- ✅ Matches WebGPU vertex buffer format exactly
- ✅ Cache-friendly for rendering
- ⚠️ Less type-safe than Vec2 arrays
- ⚠️ Requires careful size calculations

**Code Pattern**:
```cpp
// Generated locally as Vec2
vector<Vec2> vertices = {...};

// Converted to float array for GPU
vector<float> gpuVertices;
for (const auto& v : vertices) {
  gpuVertices.push_back(v.x);
  gpuVertices.push_back(v.y);
}

// Size in GPU terms:
vertexCount = gpuVertices.size() / 2;
```

---

## ADR-004: Triangle-Based Geometry Representation

**Status**: Accepted

**Context**:
WebGPU primitive type needed to be chosen. Options:
1. Triangle list (current)
2. Triangle strip
3. Line list (for debug)

**Decision**:
Use triangle list (`PrimitiveTopology::TriangleList`). Every 3 vertices form a triangle.

**Rationale**:
- ✅ Simple and predictable
- ✅ No state between triangles
- ✅ Efficient for most shapes
- ⚠️ More vertices for strips (but simpler code)
- ⚠️ Need 3 vertices per triangle

**Code Pattern**:
```cpp
// Ball as triangle fan (20 triangles, 60 vertices)
for (int i = 0; i < steps; ++i) {
  vertices.push_back({0.0f, 0.0f});      // Center
  vertices.push_back({prevX, prevY});    // Previous
  vertices.push_back({newX, newY});      // New
}
// WebGPU interprets as: triangle 0: {0,0, prev, new}
//                       triangle 1: {0,0, next, next2}
```

---

## ADR-005: Dynamic Vertex Allocation

**Status**: Accepted

**Context**:
Vertex buffer needed to handle different shape sizes. Options:
1. Fixed maximum size
2. Allocate per shape
3. Suballocate from pool

**Decision**:
Allocate fresh buffer for each shape (`EnsureVertexBufferSize`).

**Rationale**:
- ✅ Simple to implement
- ✅ No max shape complexity
- ✅ Works for prototyping
- ⚠️ GPU allocation overhead per shape
- ⚠️ Not optimal for many small objects

**Code Pattern**:
```cpp
void Renderer::EnsureVertexBufferSize(int size) {
  BufferDescriptor desc;
  desc.size = size * sizeof(float);
  desc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
  vertexBuffer = device.createBuffer(desc);
}

// Called before drawing each shape
EnsureVertexBufferSize(vertices.size());
```

**Future Optimization**:
Could implement ring buffer or memory pool for better performance.

---

## ADR-006: AABB-Only Collision Detection

**Status**: Accepted

**Context**:
Collision detection approach. Options:
1. AABB only (broad phase only)
2. AABB + SAT (precise)
3. Circle distance (for balls)

**Decision**:
Use AABB intersection only, no narrow phase.

**Rationale**:
- ✅ Very fast O(n²) broad phase
- ✅ Simple to understand
- ✅ Sufficient for demo
- ⚠️ May have false positives (overlapping AABBs that don't collide)
- ⚠️ No rotated rectangles

**Code Pattern**:
```cpp
bool AABB::Intersects(const AABB& other) const {
  return !(maxX < other.minX || minX > other.maxX ||
           maxY < other.minY || minY > other.maxY);
}
```

**Future Enhancement**:
Add SAT (Separating Axis Theorem) for precise polygon collision.

---

## ADR-007: Explicit Euler Time Integration

**Status**: Accepted

**Context**:
Physics integration method. Options:
1. Explicit Euler (current)
2. Semi-implicit Euler
3. Velocity Verlet
4. RK4

**Decision**:
Use explicit Euler with multiple iterations for stability.

**Rationale**:
- ✅ Simple to implement
- ✅ Easy to debug
- ✅ Fast enough for demo
- ⚠️ Can be unstable with large timesteps
- ⚠️ Energy dissipation over time

**Code Pattern**:
```cpp
void Rigidbody::Update(float dt) {
  // Apply forces
  linearVel += linearAcc * dt;
  pos += linearVel * dt;
  
  angularVel += angularAcc * dt;
  rotation += angularVel * dt;
}
```

**Why Multiple Iterations**:
```cpp
// With 10 iterations of dt/10:
// - More stable than single dt step
// - Better collision response
// - Minimal overhead
```

---

## ADR-008: Global Singleton for Graphics State

**Status**: Accepted (but not ideal)

**Context**:
WebGPU resources (device, queue, surface) are global state. Options:
1. Singleton pattern
2. Pass through Engine
3. Thread-local

**Decision**:
Stored in `Renderer` class, accessed via `Engine`.

**Rationale**:
- ✅ Clear ownership (Renderer manages)
- ✅ Single initialization point
- ⚠️ Not true singleton, but limited lifetime
- ⚠️ Could cause issues with multiple renderers

**Code Pattern**:
```cpp
class Renderer {
private:
  Device device;       // GPU context
  Queue queue;         // Command queue
  Surface surface;     // Window surface
};

class Engine {
  Renderer renderer;   // Owned by engine
};
```

**Better Approach**:
Could use dependency injection if needed for testing.

---

## ADR-009: Shape Inheritance from Rigidbody

**Status**: Accepted

**Context**:
Physics and geometry relationship. Options:
1. Shape inherits from Rigidbody (current)
2. Composition (Rigidbody contains Shape)
3. Separate hierarchy

**Decision**:
Shape extends Rigidbody, adding geometry methods.

**Rationale**:
- ✅ Type hierarchy mirrors domain
- ✅ Single object represents physics + graphics
- ✅ Easy to add shape-specific physics
- ⚠️ Tight coupling of concerns
- ⚠️ Harder to share shapes between bodies

**Code Pattern**:
```cpp
class Shape : public Rigidbody {
  virtual std::vector<float> GetVertexLocalPos() const = 0;
  virtual AABB GetAABB() const = 0;
};

class Ball : public Shape {
  float radius;
  // Implements geometry + physics
};
```

**Alternative** (better separation):
```cpp
class Rigidbody { /*physics*/ };
class Geometry { /*vertices*/ };
class PhysicsObject {
  unique_ptr<Rigidbody> physics;
  unique_ptr<Geometry> geometry;
};
```

---

## ADR-010: Manual Resource Management with RAII

**Status**: Accepted

**Context**:
GPU resources need cleanup. Options:
1. Manual cleanup in destructors (RAII, current)
2. Reference counting
3. Garbage collection

**Decision**:
Use RAII: initialize in constructors, cleanup in destructors. No raw pointers.

**Rationale**:
- ✅ Exception-safe
- ✅ No memory leaks
- ✅ Clear ownership
- ✅ Works well with unique_ptr
- ⚠️ Must not copy objects with GPU resources

**Code Pattern**:
```cpp
class Renderer {
public:
  bool Initialize() { /*allocate GPU resources*/ }
  void Terminate() { /*free GPU resources*/ }
  
private:
  Buffer vertexBuffer;      // Auto-freed on destruction
  BindGroup uniformGroup;   // Auto-freed on destruction
};

// Safer alternative for complex resources:
class Renderer {
private:
  unique_ptr<Buffer> vertexBuffer;  // Explicit deletion
  unique_ptr<BindGroup> group;
};
```

---

## Future Decisions to Make

### Performance
- ADR-011: Memory pooling strategy for allocations
- ADR-012: Spatial partitioning (octree/grid for collision)
- ADR-013: Compute shader usage for physics

### Scalability
- ADR-014: Multi-threaded physics simulation
- ADR-015: Network synchronization for multiplayer
- ADR-016: Serialization format for worlds

### Flexibility
- ADR-017: Physics backend abstraction
- ADR-018: Renderer backend abstraction
- ADR-019: Scripting language integration

---

## How to Record a New Decision

When making an architecture decision:

```markdown
## ADR-NNN: [Short Title]

**Status**: Proposed/Accepted/Deprecated

**Context**:
- What problem are we solving?
- What constraints exist?
- What options were considered?

**Decision**:
What we decided to do.

**Rationale**:
Why this was chosen. Include trade-offs.

**Consequences**:
- What becomes easier?
- What becomes harder?
- What's the cost?

**Code Pattern**:
Example code showing the decision in practice.
```

Then reference this document in commit messages:
```
git commit -m "Implement feature X

Implements ADR-010 (RAII for resources)
Follows pattern in ADR-003 (flat arrays)
See ARCHITECTURE.md for rationale"
```

---

