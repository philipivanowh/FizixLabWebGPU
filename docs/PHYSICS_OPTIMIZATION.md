# Physics Engine Optimization — Box2D techniques applied to FizixLab

Working notes, July 2026. Techniques are borrowed from Box2D v2/v3
(Erin Catto). Rule #1 applies before any of them: **measure first**.

## 0. Measuring (do this before believing anything below)

```bash
cmake --build build-native -j8
cd build-native
FIZIX_SCENE=stress FIZIX_PERF=1 caffeinate -dius ./App | grep perf
```

- `FIZIX_SCENE=stress` loads a reproducible 143-body shower (walled, so the
  body count stays comparable across runs) — see `Engine::StressTestScene()`.
- `FIZIX_PERF=1` prints `world.Update` time averaged over 60 frames.
- `caffeinate` stops macOS App Nap from freezing an occluded benchmark window.

## 1. Where the time goes in this engine

The frame loop runs `World::Update` with **at least 200 sub-steps per rendered
frame** (`ComputeAdaptiveIterations` only ever raises it). Anything inside the
sub-step loop is therefore paid 200×. The measured hot spots were, in order:

1. Broadphase rebuilt **per sub-step** — 12,000 rebuilds/second — each one
   allocating an `unordered_map<cell, vector>` plus an `unordered_set`.
2. A fixed broadphase cell size of 200 units (pixel-scale) which collapses
   meter-scale scenes into a single cell — i.e. silently O(n²).
3. `dynamic_cast` chains in the per-pair filter (4 casts/pair/sub-step) and in
   the narrowphase dispatch (~10 casts per `Collide` call).
4. Per-sub-step vector allocations in `World::Update` itself.

## 2. Applied (this pass) — with the Box2D idea each one borrows

| Change | Box2D counterpart | Where |
|---|---|---|
| Broadphase built **once per frame** on AABBs fattened by each body's frame travel + slop; sub-steps reuse the pair list | Fat/predictive AABBs in `b2BroadPhase`; pairs persist while movement stays inside the margin | `CollisionPipeline::BuildPairs(bodies, dtSeconds)`, hoisted call in `World::Update` |
| Pair filtering (triggers, thrusters, rope adjacency) hoisted out of the sub-step loop into a frame-level `activePairs` list | Contact filtering happens at pair creation, not per solver iteration | `World::Update` |
| Flat sort-based grid: `(cell, body)` entries + two `std::sort`s over member scratch buffers; zero steady-state allocations | Box2D v3's flat, contiguous data structures; "allocate never" in the step | `CollisionPipeline` |
| Grid cell size adapts to average dynamic-body extent | BVH is scale-free; a grid must earn that property | `CollisionPipeline` |
| `shape::KindOf()` type tag (the `shapeType` field every shape already set!) replaces every `dynamic_cast` in hot paths | `b2Shape::Type` enum dispatch — no RTTI anywhere in Box2D | `Shape.hpp`, `Collisions.cpp`, `CollisionSolver.cpp`, `World.cpp` |
| Scratch vectors hoisted out of the sub-step loop | same allocation discipline | `World::Update` |

### Measured result (same scene, same machine, Release)

| | physics ms/frame (range over 5×60-frame windows) | bodies retained |
|---|---|---|
| before | 171 – 490 (avg ≈ 300) | 118/143 (25 tunnel out while settling) |
| after | 72 – 88 (avg ≈ 79) | 142/143 |

**≈ 4× faster, and fewer tunneling escapes** (the per-frame pair list with
predictive margins never misses the wall contacts mid-frame).

### Known tradeoff

A body that gains large velocity *mid-frame* (impulse from a fast impactor)
can outrun its fattened AABB for the remainder of that frame, missing a pair
until the next frame. Box2D v3 accepts the same one-step staleness for its
sub-steps. The 1.5× velocity factor + 5%-of-size slop in `BuildPairs` and the
existing `SweptContactCorrection` cover the realistic cases; bump the margin
constants if you ever see a fast body skip a thin wall.

## 3. Next tier — the solver itself (not done; do it as its own milestone)

The remaining ~79 ms is dominated by running narrowphase + resolve 200×. The
200 exists because the solver is a *one-shot* impulse resolver: stability
comes from tiny sub-steps. Box2D gets away with **4 sub-steps** because its
solver iterates with memory:

1. **Persistent contact manifolds + warm starting.** Key manifolds by body
   pair, carry accumulated normal/tangent impulses across frames, apply them
   as the initial guess. This is the single biggest stability-per-cost win in
   Box2D. It also makes the FBD force display *cleaner* (forces come from the
   accumulated impulse, which converges, instead of per-sub-step spikes).
2. **Velocity iterations over all contacts** (sequential impulses, 8–10
   iterations) instead of resolving each pair once per sub-step. Then cut
   sub-steps from 200 to 4–8. Expected another order of magnitude.
   ⚠ Validate the education features after: analytic static friction on
   inclines, normal-force display, the recorder.
3. **Sleeping + islands.** Bodies below a velocity threshold for ~0.5 s stop
   simulating until touched. Resting scenes then cost ~0. For a sandbox whose
   scenes spend most of their life at rest, this is the biggest end-user win —
   but sleeping bodies must still display their static forces (compute FBD
   values once at sleep time and freeze them).
4. **Narrowphase allocation diet.** `GetVertexWorldPos()` returns a fresh
   `std::vector` per call, several times per pair per sub-step. Cache world
   verts per body per sub-step (dirty flag already exists:
   `transformUpdateRequired`) or write into caller-provided scratch.

## 4. Tier 3 — only after Tier 2 exists

- Struct-of-arrays body storage (`objects` is `vector<unique_ptr>` — pointer
  chasing per access). Box2D v3 keeps bodies in contiguous arrays and got
  ~2× from data layout alone.
- SIMD-wide contact solving, multithreaded islands (Box2D v3's task graph).
  On the web build, check wasm SIMD support before bothering.
- Continuous collision (TOI) if fast projectiles ever need to be exact.
