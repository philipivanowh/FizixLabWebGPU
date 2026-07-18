# This file is part of the "Learn WebGPU for C++" book.
#   https://eliemichel.github.io/LearnWebGPU
#
# MIT License
# Copyright (c) 2022-2024 Elie Michel and the wgpu-native authors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Unified WebGPU-distribution (v0.3.0-gamma). It defines the 'webgpu' target,
# the WEBGPU_BACKEND cache option (defaults: EMDAWNWEBGPU under emcmake, WGPU
# natively; DAWN and EMSCRIPTEN are also valid), the WEBGPU_BACKEND_* compile
# definitions, and the target_copy_webgpu_binaries() helper. Under Emscripten
# it propagates a pinned emdawnwebgpu port as INTERFACE compile+link options
# to every target that links 'webgpu', along with a matching webgpu.hpp.
include(FetchContent)

if (NOT TARGET webgpu)
	FetchContent_Declare(
		webgpu-distribution
		URL https://github.com/eliemichel/WebGPU-distribution/archive/refs/tags/v0.3.0-gamma.zip
	)
	FetchContent_MakeAvailable(webgpu-distribution)
endif()
