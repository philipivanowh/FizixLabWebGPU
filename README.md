LearnWebGPU - Code
==================

Thank you Eliemichel for the WebGPU C++ Guide [Learn WebGPU](https://eliemichel.github.io/LearnWebGPU/)

Video Demo
https://www.youtube.com/watch?v=WU2XR_y64CQ
https://www.youtube.com/watch?v=C9aZ5YHLRF0


Setup
Install Emscripten
  https://emscripten.org/docs/getting_started/downloads.html

Building Instruction


--------

```
MACOS
cmake -B build-dawn -DWEBGPU_BACKEND=DAWN -DWEBGPU_BUILD_FROM_SOURCE=ON
cmake --build build-dawn

Windows
# Build using Dawn backend
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build

# Build using emscripten 
NOT YET
```

Then run either `./build-dawn/App`
