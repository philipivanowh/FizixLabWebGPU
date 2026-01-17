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

Also in Cmakelist
this line -> set(CMAKE_PREFIX_PATH "C:/Users/hahoa/tools/vcpkg/installed/x64-windows/share")
find the right vcpkg path for it
--------

```
# Build using Dawn backend
cmake -G "MinGW Makefiles" -S . -B build "-DCMAKE_TOOLCHAIN_FILE=C:\Users\hahoa\tools\vcpkg\scripts\buildsystems\vcpkg.cmake" 
cmake --build build

# Build using emscripten
NOT YET
```

Then run either `./build/App`
