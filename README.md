LearnWebGPU - Code
==================

Thank you Eliemichel for the WebGPU C++ Guide [Learn WebGPU](https://eliemichel.github.io/LearnWebGPU/)


Building Instruction
--------

```
# Build using Dawn backend
cmake -B build-dawn -DWEBGPU_BACKEND=DAWN
cmake --build build-dawn

# Build using emscripten
emcmake cmake -B build-emscripten
cmake --build build-emscripten
```

Then run either `./build/App` (linux/macOS/MinGW) or `build\Debug\App.exe` (MSVC).
