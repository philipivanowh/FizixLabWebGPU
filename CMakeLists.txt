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

if (EMSCRIPTEN AND NOT DEFINED WEBGPU_BACKEND)
	set(WEBGPU_BACKEND "DAWN" CACHE STRING "Backend implementation of WebGPU.")
elseif (NOT EMSCRIPTEN AND NOT DEFINED WEBGPU_BACKEND)
	set(WEBGPU_BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU.")
endif()

add_subdirectory(webgpu)
add_subdirectory(glfw3webgpu)
set(CMAKE_PREFIX_PATH "C:/Users/hahoa/tools/vcpkg/installed/x64-windows/share")
find_package(Freetype CONFIG REQUIRED)


add_executable(App
	src/app/main.cpp
	src/core/UIManager.cpp
	src/core/Engine.cpp
	src/collision/CollisionPipeline.cpp
	src/collision/CollisionSolver.cpp
	src/core/Renderer.cpp
	src/core/TextRenderer.cpp
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


add_custom_target(copy_fonts ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/fonts
            $<TARGET_FILE_DIR:App>/fonts
)

add_dependencies(App copy_fonts)

target_include_directories(App PRIVATE src)

target_link_libraries(App PRIVATE glfw webgpu glfw3webgpu Freetype::Freetype)

# For Emscripten, embed the fonts
if (EMSCRIPTEN)
    target_link_options(App PRIVATE
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/fonts@/fonts"
    )
endif()

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
			"SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/web/index.html"
			"SHELL:-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
			"SHELL:-sEXPORTED_FUNCTIONS=_StartSimulation"
		)

	# Add shader file to embed
	target_link_options(App PRIVATE
		"SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders@/src/shaders"
	)
endif()

add_custom_target(copy_web_assets ALL
	COMMAND ${CMAKE_COMMAND} -E make_directory
		$<TARGET_FILE_DIR:App>
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/src/web/ui.js
		$<TARGET_FILE_DIR:App>/ui.js
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_SOURCE_DIR}/src/web/ui.css
		$<TARGET_FILE_DIR:App>/ui.css
)

add_dependencies(App copy_web_assets)
