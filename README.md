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

- CMake 3.16+
- A C++20 compiler (MSVC, clang, gcc)
- Git and an internet connection for the first configure (CMake fetches the
  WebGPU backend, and on web builds the emdawnwebgpu port)

### Native on macOS

You need the Xcode Command Line Tools (`xcode-select --install`) and CMake
(`brew install cmake`).

From the repo root:

```bash
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j8
```

Run it:

```bash
./build-native/App
```

The first configure downloads the WebGPU implementation (wgpu-native) via
FetchContent; after that, rebuilds are offline. The `libwgpu_native.dylib` is
copied next to the binary automatically.

The same two commands work on Windows (`build-native\App.exe`, add
`--config Release` with the Visual Studio generator) and Linux.

### Choosing a native WebGPU backend

wgpu-native is the default. Dawn also works:

```bash
cmake -S . -B build-native -DWEBGPU_BACKEND=DAWN
cmake --build build-native
```

### Web (Emscripten + emdawnwebgpu)

The web build uses Emscripten with the [emdawnwebgpu](https://dawn.googlesource.com/dawn)
port — no flags needed, `emcmake` selects it automatically.

1. **Install Emscripten** (4.0.10 or newer) via [emsdk](https://emscripten.org/docs/getting_started/downloads.html):

   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd path/emsdk 
   ./emsdk install latest

   **Activate Emscripten** 
   cd path/emsdk 
   ./emsdk activate latest
   source ./emsdk_env.sh   # do this in every new shell before building
   ```

2. **Build** from the repo root:

   ```bash
   emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
   cmake --build build-web -j8
   ```

   This produces `build-web/App.html` (the page, using the custom shell in
   `web/shell.html`) plus `App.js`, `App.wasm` and `App.data` (preloaded
   shaders).

3. **Launch it locally** — browsers won't load wasm from `file://`, so serve
   the folder over HTTP:

   ```bash
   python3 -m http.server 8123 --directory build-web
   ```

   Then open <http://localhost:8123/App.html> in a WebGPU-capable browser
   (current Chrome or Edge work best). The sandbox fills the window and
   adapts to any window size.

4. **Deploy** (optional): the four files in `build-web/` are a fully static
   site. Copy them to any static host (GitHub Pages, itch.io, Netlify, …),
   renaming `App.html` to `index.html`. No special server headers are needed.

## Contributing

PRs are welcome.

Suggested workflow:

1. Open an issue describing the physics scenario / bug / feature.
2. Keep changes focused (one feature or fix per PR).
3. Build locally and verify behavior in the sandbox.

Codebase map and "where to change what" lives in `ARCHITECTURE.md`.

