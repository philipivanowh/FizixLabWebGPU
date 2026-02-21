LearnWebGPU - Code
==================

Thank you Eliemichel for the WebGPU C++ Guide [Learn WebGPU](https://eliemichel.github.io/LearnWebGPU/)

Video Demo
https://www.youtube.com/watch?v=WU2XR_y64CQ
https://www.youtube.com/watch?v=C9aZ5YHLRF0


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
MACOS
cmake -B build-dawn -DWEBGPU_BACKEND=DAWN -DWEBGPU_BUILD_FROM_SOURCE=ON
cmake --build build-dawn
```
# Build using Dawn backend
cmake -G "MinGW Makefiles" -S . -B build "-DCMAKE_TOOLCHAIN_FILE=C:\Users\hahoa\tools\vcpkg\scripts\buildsystems\vcpkg.cmake" 
cmake --build build

# Build using emscripten
NOT YET
```

Then run either `./build/App`
