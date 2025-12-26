#pragma once

// Include the C++ wrapper instead of the raw header(s)
#include <webgpu/webgpu.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

using namespace wgpu;

namespace math {
class Vec2;
}

namespace physics {
class Body;
class Ball;
class Box;
}

class Renderer {
public:
	// Initialize everything and return true if it went all right
	bool Initialize();

	// Uninitialize everything that was initialized
	void Terminate();

	// Begin a frame, record draw calls, and submit.
	void BeginFrame();
	void EndFrame();

	// Return true as long as the main loop should keep on running
	bool IsRunning();

	void DrawShape(const physics::Body& body);
	void DrawBox(const physics::Box& box);
	void DrawBall(const physics::Ball& ball);
	void DrawTestTriangle();
	void DrawTest2Triangle();

private:
	wgpu::TextureView GetNextSurfaceTextureView();
	// Substep of Initialize() that creates the render pipeline
	void InitializePipeline();
	wgpu::RequiredLimits GetRequiredLimits(Adapter adapter) const;
	void InitializeBuffers();
	void EnsureVertexBufferSize(int size);
	uint32_t UpdateUniforms(const math::Vec2& position, const std::array<float, 4>& color);

private:
	// We put here all the variables that are shared between init and main loop
	GLFWwindow *window;
	wgpu::Device device;
	Queue queue;
	Surface surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;
	TextureFormat surfaceFormat = TextureFormat::Undefined;
	RenderPassEncoder renderPass;
	RenderPipeline pipeline;
	PipelineLayout pipelineLayout;
	BindGroupLayout uniformBindGroupLayout;
	BindGroup uniformBindGroup;
	Buffer uniformBuffer;
	Buffer vertexBuffer;
	uint32_t vertexCount;
	CommandEncoder encoder;
	TextureView targetView;
	//size_t vertexBufferSize = 0;
	uint32_t uniformAlignment = 1;
	size_t uniformBufferStride = 0;
	size_t uniformBufferSize = 0;
	size_t uniformBufferOffset = 0;
	uint32_t windowWidth = 1470;
	uint32_t windowHeight = 956;
	const WGPUColor backgroundColor = WGPUColor{0.015, 0.016, 0.02, 1.0};
};
