cmake_minimum_required(VERSION 3.0...3.25)



project(
	LearnWebGPU
	VERSION 0.1.0
	LANGUAGES CXX C
)

if (NOT EMSCRIPTEN)
	add_subdirectory(glfw)
else()
	add_library(glfw INTERFACE)
	target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)
add_subdirectory(imgui)




add_executable(App
	src/app/main.cpp
	src/core/UIManager.cpp
	src/core/Engine.cpp
	src/collision/CollisionPipeline.cpp
	src/collision/CollisionSolver.cpp
	src/core/Renderer.cpp
	src/core/World.cpp
	src/core/Utility.cpp
	src/shape/Ball.cpp
	src/physics/Rigidbody.cpp
	src/shape/Box.cpp
	src/shape/Cannon.cpp
	src/shape/Incline.cpp
	src/shape/Thruster.cpp
	src/shape/Trigger.cpp
	src/shape/Rope.cpp
	src/shape/Spring.cpp
	src/shape/Shape.cpp
	src/collision/Collisions.cpp
)


target_include_directories(App PRIVATE src)

target_link_libraries(App PRIVATE glfw webgpu glfw3webgpu imgui)


target_copy_webgpu_binaries(App)

set_target_properties(App PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
	target_compile_options(App PRIVATE /W4)
else()
	target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

if (EMSCRIPTEN)
	set_target_properties(App PROPERTIES SUFFIX ".html")

	# NB: --use-port is NOT set here. The 'webgpu' target (WebGPU-distribution,
	# EMDAWNWEBGPU backend) propagates its own pinned emdawnwebgpu port as
	# INTERFACE compile+link options to App, imgui and glfw3webgpu. Adding the
	# emsdk built-in port here as well would pull in a second, conflicting copy
	# of the WebGPU headers.
    target_link_options(App PRIVATE
        -sASYNCIFY
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        # Growable (resizable) ArrayBuffers are rejected by Chrome's
        # GPUQueue.writeBuffer ("value must not be resizable"), so grow memory
        # the classic way (detach + replace views) instead.
        -sGROWABLE_ARRAYBUFFERS=0
        -sNO_DISABLE_EXCEPTION_CATCHING
        -sSTACK_SIZE=5MB
        -sINITIAL_MEMORY=256MB
    )

	# Add shader file to embed
	target_link_options(App PRIVATE
		"SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders@/src/shaders"
	)

	# Custom minimal page: fullscreen canvas, no Emscripten branding
	target_link_options(App PRIVATE
		"SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
	)
	set_property(TARGET App APPEND PROPERTY LINK_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
	)
endif()
