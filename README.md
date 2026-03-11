# FizixLabWebGPU

A physics visualization sandbox focused on kinematics + dynamics problems. Use it to sanity-check textbook answers by simulating scenarios and visualizing forces (FBD-style).

Built on WebGPU. This repo started from the Learn WebGPU C++ guide by Elie Michel: https://eliemichel.github.io/LearnWebGPU/

## Demo videos
- https://www.youtube.com/watch?v=WU2XR_y64CQ
- https://www.youtube.com/watch?v=C9aZ5YHLRF0

## Features

- Physics sandbox (rigidbodies, collisions, friction, rotation)
- Force visualization (useful for free-body diagrams)
- Recording + rewind: when recording, a timeline slider appears so you can scrub through history

## Objects you can spawn

- **Ball**: circle rigidbody (mass, radius)
- **Box**: box rigidbody (mass, width/height)
- **Incline**: incline plane helper (base/width, angle)
- **Thruster**: applies a controllable force (magnitude + direction, optional key to fire)
- **Trigger**: reacts when something collides with it (example: pause the sim when a projectile reaches a target zone)
- **Cannon**: projectile launcher (initial velocity + angle)
- **Spring**: Hooke's law spring
- **Rope**: exists but currently unstable/hidden

## Controls (quick)

- **Left click + drag**: move an object
- **Left click + drag empty space**: pan the camera
- **Right click**: measure tool (displacement ruler)
- **Mouse wheel**: zoom
- `=` / `+` and `-`: zoom in/out
- `P`: pause / play
- `O`: step one frame (useful while paused)
- `R`: start recording (stop via the UI)
- `Esc`: quit

Notes:
- On macOS, right click is `Ctrl + Left Click`.
- There is a UI setting for drag behavior (Precision vs Physics).

## Build (contributors)

### Requirements

- CMake
- A C++17 compiler (MSVC, clang, gcc)
- Git (CMake fetches the WebGPU backend dependencies)

### Native (recommended)

From the repo root:

```bash
cmake -S . -B build
cmake --build build
```

Run:
- Windows: `build/App.exe`
- macOS/Linux: `build/App`

Tip (Visual Studio generator): `cmake --build build --config Release`

### Choosing a WebGPU backend

The build supports multiple WebGPU backends via CMake:

```bash
cmake -S . -B build -DWEBGPU_BACKEND=WGPU
# or: -DWEBGPU_BACKEND=DAWN
cmake --build build
```

### Web (Emscripten)

Web builds are work-in-progress. If you want to experiment:

- Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html
- Configure with `emcmake cmake ...` and set `-DWEBGPU_BACKEND=EMSCRIPTEN`

## Contributing

PRs are welcome.

Suggested workflow:

1. Open an issue describing the physics scenario / bug / feature.
2. Keep changes focused (one feature or fix per PR).
3. Build locally and verify behavior in the sandbox.

Codebase map and "where to change what" lives in `ARCHITECTURE.md`.

