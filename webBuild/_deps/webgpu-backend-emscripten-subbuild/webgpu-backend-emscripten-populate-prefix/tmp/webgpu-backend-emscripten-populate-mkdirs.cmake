# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-src")
  file(MAKE_DIRECTORY "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-src")
endif()
file(MAKE_DIRECTORY
  "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-build"
  "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix"
  "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix/tmp"
  "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix/src/webgpu-backend-emscripten-populate-stamp"
  "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix/src"
  "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix/src/webgpu-backend-emscripten-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix/src/webgpu-backend-emscripten-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/lansmachine/Documents/GitHub/LearnWebGPU-Code/webBuild/_deps/webgpu-backend-emscripten-subbuild/webgpu-backend-emscripten-populate-prefix/src/webgpu-backend-emscripten-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
