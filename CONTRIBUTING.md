# FizixLabWebGPU - Contributor's Guide

Guidelines for contributing to the physics engine and renderer codebase.

---

## 🎯 Before You Start

Read these in order:
1. **README.md** - Project overview
2. **CODEBASE_OVERVIEW.md** - Architecture and design
3. **QUICK_REFERENCE.md** - Find what you need fast
4. **This file** - Contribution guidelines

---

## 📋 Development Workflow

### 1. Understanding the Feature You Want to Build

Ask yourself:
- **What system does this affect?** (Physics, rendering, collision, shapes)
- **What existing patterns are similar?** (Study similar code)
- **Will this be a hot path?** (Called every frame? Optimize if yes)
- **Does it require GPU work?** (Shader changes? New buffers?)

### 2. Design Phase

Before coding:
- **Draw a diagram** showing data flow
- **Identify dependencies** on other systems
- **Check for existing implementations** you can extend
- **Plan the interface** - what will callers see?

### 3. Implementation Strategy

Follow these principles:

```
┌─────────────────────────────────────────┐
│ 1. Single Responsibility                │
│    Each class does ONE thing well       │
└─────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────┐
│ 2. Minimal Coupling                     │
│    Depend on abstractions, not concrete │
└─────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────┐
│ 3. Maximal Cohesion                     │
│    Related code stays together          │
└─────────────────────────────────────────┘
         ↓
┌─────────────────────────────────────────┐
│ 4. Test as You Go                       │
│    Verify behavior at each step         │
└─────────────────────────────────────────┘
```

---

## 🔨 Common Development Tasks

### Task 1: Add a New Shape Type

**Files to create/modify:**
```
src/shape/
├── MyShape.hpp          (new)
├── MyShape.cpp          (new)
└── Shape.hpp            (modify - if adding new virtual methods)

src/core/
└── Renderer.cpp         (modify - add DrawMyShape)

src/core/
└── Engine.cpp           (modify - add creation code)
```

**Step-by-step:**

```cpp
// 1. MyShape.hpp
namespace shape {
class MyShape : public Shape {
public:
  MyShape(const math::Vec2& pos,
          const math::Vec2& vel,
          const math::Vec2& acc,
          float myParam,
          const std::array<float, 4>& color,
          float mass,
          float restitution,
          physics::RigidbodyType type);
  
  ~MyShape() override = default;

private:
  float myParam;
  
  void GenerateVertices() const override;
  std::vector<float> GetVertexLocalPos() const override;
  float ComputeInertia() const override;
  collision::AABB GetAABB() const override;
};
}
```

```cpp
// 2. MyShape.cpp - Implement vertices
void MyShape::GenerateVertices() const {
  vertices.clear();
  
  // Create triangles
  // Remember: 3 vertices per triangle
  // Format: local coordinates (relative to origin)
  
  vertices.emplace_back(-1.0f, -1.0f);
  vertices.emplace_back(1.0f, -1.0f);
  vertices.emplace_back(0.0f, 1.0f);
}

std::vector<float> MyShape::GetVertexLocalPos() const {
  // Called every frame by renderer
  // Return flat array: [x0, y0, x1, y1, ...]
  GenerateVertices();  // Update based on rotation
  
  std::vector<float> out;
  out.reserve(vertices.size() * 2);
  for (const auto& v : vertices) {
    out.push_back(v.x);
    out.push_back(v.y);
  }
  return out;
}

float MyShape::ComputeInertia() const {
  // Moment of inertia for rotation
  // Formula depends on shape
  if (mass <= 0.0f) return 0.0f;
  return mass;  // Simplified
}

collision::AABB MyShape::GetAABB() const {
  // Conservative bounding box
  float minX = pos.x - 1.0f;  // Adjust bounds
  float minY = pos.y - 1.0f;
  float maxX = pos.x + 1.0f;
  float maxY = pos.y + 1.0f;
  return collision::AABB(minX, minY, maxX, maxY);
}
```

```cpp
// 3. Renderer.cpp - Add drawing function
void Renderer::DrawMyShape(const shape::MyShape& shape) {
  const std::vector<float> vertices = shape.GetVertexLocalPos();
  EnsureVertexBufferSize(vertices.size());
  
  queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));
  
  vertexCount = static_cast<uint32_t>(vertices.size() / 2);
  uint32_t uniformOffset = UpdateUniforms(shape.pos, shape.color);
  
  if (!uniformBindGroup) {
    std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
    return;
  }
  renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
  renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
  renderPass.draw(vertexCount, 1, 0, 0);
  
  std::cout << "Drawing MyShape with " << vertexCount << " vertices." << std::endl;
}

// Also update DrawShape dispatcher:
void Renderer::DrawShape(const physics::Rigidbody &rb) {
  if (auto box = dynamic_cast<const shape::Box *>(&rb)) {
    DrawBox(*box);
    return;
  }
  if (auto ball = dynamic_cast<const shape::Ball *>(&rb)) {
    DrawBall(*ball);
    return;
  }
  if (auto myshape = dynamic_cast<const shape::MyShape *>(&rb)) {
    DrawMyShape(*myshape);
    return;
  }
}
```

```cpp
// 4. Renderer.hpp - Add method declaration
void DrawMyShape(const shape::MyShape& shape);
```

```cpp
// 5. Engine.cpp - Create instances
void Engine::AddDefaultObjects() {
  // ... existing code ...
  
  world.Add(std::make_unique<shape::MyShape>(
    Vec2(500.0f, 500.0f),
    Vec2(0.0f, 0.0f),
    Vec2(0.0f, 0.0f),
    2.5f,  // myParam
    std::array{255.0f, 100.0f, 50.0f, 1.0f},
    10.0f,
    0.5f,
    physics::RigidbodyType::Dynamic
  ));
}
```

---

### Task 2: Modify Physics Behavior

**Files to modify:**
```
src/physics/PhysicsConstants.hpp    - Tweak constants
src/physics/Rigidbody.cpp          - Change integration
src/core/World.cpp                 - Collision response
```

**Example: Add air resistance**

```cpp
// In PhysicsConstants.hpp
const float AIR_RESISTANCE = 0.98f;  // Changed from 0.99f

// In Rigidbody::Update()
linearVel *= AIR_RESISTANCE;
angularVel *= AIR_RESISTANCE;
```

**Example: Change time integration**

```cpp
// Current: Explicit Euler (fast, less stable)
void Rigidbody::Update(float dt) {
  linearVel += linearAcc * dt;
  pos += linearVel * dt;
}

// Alternative: Semi-implicit Euler (more stable)
void Rigidbody::Update(float dt) {
  linearVel += linearAcc * dt;  // Update velocity first
  pos += linearVel * dt;         // Then position with new velocity
}
```

---

### Task 3: Modify Rendering

**Files to modify:**
```
src/shaders/triangle.wgsl              - GPU computation
src/core/Renderer.cpp                  - CPU-side rendering
src/core/Renderer.hpp                  - Structures
```

**Example: Change how colors are computed**

```glsl
// In triangle.wgsl

// Current:
@fragment
fn fs_main() -> @location(0) vec4f {
  return u.color;
}

// New: Add brightness variation
@fragment
fn fs_main() -> @location(0) vec4f {
  let brightness = 0.8 + 0.2 * sin(u.translation.x * 0.01);
  return u.color * vec4f(brightness, brightness, brightness, 1.0);
}
```

**Example: Add texture support**

Would require:
1. New uniform for texture handle
2. New vertex attribute for UV coordinates
3. Texture binding in bind group layout
4. Sampling in fragment shader

---

## ✅ Code Review Checklist

Before submitting changes:

### Design
- [ ] Single responsibility - does this class do one thing?
- [ ] No circular dependencies between modules
- [ ] Interface is minimal and clear
- [ ] Follows existing patterns in codebase

### Implementation
- [ ] No raw pointers (use unique_ptr, references)
- [ ] Resources are RAII (cleanup on destruction)
- [ ] Error handling present where needed
- [ ] Comments explain "why", not "what"

### Performance
- [ ] No unnecessary allocations in hot paths
- [ ] GPU calls are batched where possible
- [ ] Physics iterations are appropriate
- [ ] No n² algorithms where O(n log n) exists

### Testing
- [ ] Feature works as intended (manual test)
- [ ] No regressions in existing features
- [ ] Edge cases handled (empty objects, max values)
- [ ] Debug output added for verification

### Code Quality
- [ ] Consistent naming with existing code
- [ ] Proper const correctness
- [ ] Follows C++17 idioms
- [ ] No compiler warnings

---

## 🐛 Debugging Techniques

### GPU Errors

```cpp
// Already in Renderer::Initialize() but check it:
device.setUncapturedErrorCallback([](ErrorType type, const char* message) {
  std::cout << "GPU Error: " << message << std::endl;
});
```

### Physics Debugging

```cpp
// Add to Rigidbody::Update():
std::cout << "Body at " << pos.x << "," << pos.y
          << " vel=" << linearVel.x << "," << linearVel.y << std::endl;
```

### Rendering Debugging

```cpp
// Add to DrawShape:
std::cout << "Drawing shape with " << vertexCount << " vertices"
          << " at (" << pos.x << "," << pos.y << ")" << std::endl;
```

### Validation

```cpp
// Check assumptions
assert(vertexCount > 0);
assert(mass > 0.0f || bodyType == Static);
assert(vertices.size() % 3 == 0);  // Multiple of 3 for triangles
```

---

## 📏 Code Style Guidelines

### Naming

```cpp
// Classes: PascalCase
class Rigidbody { };
struct AABB { };

// Functions: camelCase
void updatePosition();
bool intersects(const AABB& other);

// Constants: SCREAMING_SNAKE_CASE or constexpr PascalCase
const float GRAVITY = 1000.0f;
constexpr int MAX_OBJECTS = 1000;

// Member variables: camelCase with no prefix
float linearVel;
Vec2 position;

// Private members: same as public
class Renderer {
private:
  Buffer vertexBuffer;  // Not m_vertexBuffer
};
```

### Comments

```cpp
// BAD: Explains what the code does
x = y + 1;  // Add 1 to y and assign to x

// GOOD: Explains why
// Account for off-by-one error in physics engine
x = y + 1;

// GOOD: Explains non-obvious logic
// Higher damping for stability in collisions
linearVel *= 0.95f;

// GOOD: Marks areas needing work
// TODO: This quadratic check should be spatial hash
if (collides) { /*...*/ }
```

### Formatting

```cpp
// 1. Use 2-space indent (check .editorconfig)
class Shape {
  float ComputeInertia() const override {
    // 2 spaces per level
    if (mass > 0.0f) {
      return 0.5f * mass * radius * radius;
    }
    return 0.0f;
  }
};

// 2. Brace style: opening brace on same line
if (condition) {
  // ...
} else {
  // ...
}

// 3. Line length: keep under 100 chars where reasonable
std::vector<float> vertices = shape.GetVertexLocalPos();  // OK: ~80 chars
```

---

## 🔄 Typical Contribution Flow

```
1. Fork repo (or branch)
   └─ git checkout -b feature/my-feature

2. Understand existing code
   └─ Read CODEBASE_OVERVIEW.md

3. Design your change
   └─ Sketch on paper, check dependencies

4. Implement incrementally
   └─ git commit -m "Add new shape class"
   └─ git commit -m "Implement vertices generation"
   └─ git commit -m "Add to renderer"

5. Test frequently
   └─ Build after each commit
   └─ Run manually to verify

6. Clean up
   └─ Review code quality
   └─ Add comments
   └─ Remove debug output

7. Submit PR
   └─ Describe what changed and why
   └─ Reference related issues
```

---

## 📚 Design Patterns Used (Extend These)

### Strategy Pattern
```cpp
// Existing:
enum RigidbodyType { Static, Dynamic };

// For new features:
class CollisionResponse {
  virtual void Resolve(ContactManifold&) = 0;
};

class ImpulseResponse : public CollisionResponse { };
class ConstraintResponse : public CollisionResponse { };
```

### Template Method Pattern
```cpp
// Existing:
class Shape : public Rigidbody {
  virtual std::vector<float> GetVertexLocalPos() const = 0;
};

// Extended by Ball, Box, MyShape
```

### Factory Pattern
```cpp
// Could add:
class ShapeFactory {
  static unique_ptr<Shape> CreateShape(ShapeType type, ...);
};
```

---

## 🚀 Performance Tips

### Profile First
- Use system profiler to identify hot paths
- CPU time vs GPU time
- Memory allocation frequency

### Optimization Order
1. Algorithm improvements (biggest gains)
2. Memory layout (better cache)
3. Low-level optimizations (compiler often does these)

### Common Bottlenecks
- Frequent allocations → Use object pools
- GPU stalls → Batch commands
- Physics checks → Use spatial partitioning
- Rendering → Reduce draw calls

---

## 📖 Resources for Contributors

### Physics
- *Game Physics Engine Development* - Ian Millington
- GDC talks on physics simulation
- PhysX source code

### Graphics
- [Learn WebGPU](https://eliemichel.github.io/LearnWebGPU/)
- [WebGPU Spec](https://www.w3.org/TR/webgpu/)
- Graphics Programming Black Book

### Software Design
- *Design Patterns* - Gang of Four
- *Code Complete* - Steve McConnell
- Google's SWE practices (google.com/careers/engineering)

---

## 🤝 Getting Help

1. **Question about code?**
   - Check QUICK_REFERENCE.md first
   - Search existing code for similar patterns
   - Read relevant CODEBASE_OVERVIEW section

2. **Stuck on physics?**
   - Check PhysicsConstants.hpp comments
   - Review Rigidbody::Update() step by step
   - Look at Box/Ball implementation

3. **GPU issues?**
   - Enable GPU error callback (already in code)
   - Check shader compilation (in Renderer.cpp output)
   - Verify buffer sizes and binding layout

---

Good luck! Happy hacking! 🚀

