LearnWebGPU - Code
==================

Thank you Eliemichel for the WebGPU C++ Guide [Learn WebGPU](https://eliemichel.github.io/LearnWebGPU/)

Setup
Install Emscripten

Install vcpkg
    vcpkg install freetype

Building Instruction

Make sure when building the backend
Replace the VCPKG_PATH with your VCPKG_PATH
-DCMAKE_TOOLCHAIN_FILE=\VCPKG_PATH\scripts\buildsystems\vcpkg.cmake
--------

```
# Build using Dawn backend
cmake -G "MinGW Makefiles" -S . -B build  -DCMAKE_TOOLCHAIN_FILE=C:\Users\hahoa\tools\vcpkg\scripts\buildsystems\vcpkg.cmake

//Mac
cmake -B build DWEBGPU_BACKEND=DAWN
cmake --build build

# Build using emscripten

```

Then run either `./build/App` (linux/macOS/MinGW) or `build\Debug\App.exe` (MSVC).
