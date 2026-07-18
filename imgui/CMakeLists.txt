# Define an ImGui target that fits our use case
add_library(imgui STATIC
	# Among the different backends available, we are interested in connecting
	# the GUI to GLFW andWebGPU:
	backends/imgui_impl_wgpu.h
	backends/imgui_impl_wgpu.cpp
	backends/imgui_impl_glfw.h
	backends/imgui_impl_glfw.cpp

	# Bonus to add some C++ specific features (the core ImGUi is a C library)
	misc/cpp/imgui_stdlib.h
	misc/cpp/imgui_stdlib.cpp

	# The core ImGui files
	imconfig.h
	imgui.h
	imgui.cpp
	imgui_draw.cpp
	imgui_internal.h
	imgui_tables.cpp
	imgui_widgets.cpp
	imstb_rectpack.h
	imstb_textedit.h
	imstb_truetype.h)


target_include_directories(imgui PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC webgpu glfw)

# imgui_impl_wgpu needs to know which flavor of WebGPU implementation it is
# compiled against. Under Emscripten it defaults to the Dawn flavor (which is
# what --use-port=emdawnwebgpu provides); natively we use wgpu-native.
if (NOT EMSCRIPTEN)
	target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
endif()

# imgui_impl_wgpu.cpp contains a macOS surface helper that uses Objective-C
# (Cocoa/CAMetalLayer), so on Apple it must be built as Objective-C++
# (instruction from the top of that helper in imgui_impl_wgpu.cpp).
if (APPLE AND NOT EMSCRIPTEN)
	set_source_files_properties(backends/imgui_impl_wgpu.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()

set_target_properties(imgui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)