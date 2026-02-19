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
	src/shape/Canon.cpp
	src/shape/Incline.cpp
	src/shape/Shape.cpp
	src/collision/Collisions.cpp
)


target_include_directories(App PRIVATE src)

target_link_libraries(App PRIVATE glfw webgpu glfw3webgpu imgui)


target_copy_webgpu_binaries(App)

set_target_properties(App PROPERTIES
	CXX_STANDARD 17
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
	target_link_options(App PRIVATE 
		-sASYNCIFY
		-sUSE_WEBGPU=1
		-sUSE_GLFW=3
		-sALLOW_MEMORY_GROWTH=1
		-sNO_DISABLE_EXCEPTION_CATCHING
		-sSTACK_SIZE=5MB
		-sINITIAL_MEMORY=256MB
	)

	# Add shader file to embed
	target_link_options(App PRIVATE
		"SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders@/src/shaders"
	)
endif()
