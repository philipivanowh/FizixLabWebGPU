#pragma once

// Suppress warnings and errors from the third-party Emscripten WebGPU header
#ifdef __EMSCRIPTEN__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-w"
#endif

#include <webgpu/webgpu.hpp>

#ifdef __EMSCRIPTEN__
#pragma GCC diagnostic pop
#endif

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "shape/Ball.hpp"
#include "shape/Box.hpp"
#include "shape/Cannon.hpp"
#include "shape/Incline.hpp"
#include "shape/Trigger.hpp"
#include "physics/Rigidbody.hpp"
#include "math/Vec2.hpp"
#include "common/settings.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

using namespace wgpu;

class Renderer
{
public:
	// Initialize everything and return true if it went all right
	bool Initialize(Settings& settings, GLFWscrollfun scrollCallback = nullptr);

	// Uninitialize everything that was initialized
	void Terminate();

	// Begin a frame, record draw calls, and submit.
	void BeginFrame();
	void EndFrame();

	// Return true as long as the main loop should keep on running
	bool IsRunning();

	void DrawHighlightOutline(physics::Rigidbody &body);
	void DrawShape(physics::Rigidbody &body, bool highlight = false);
	void DrawBox(const shape::Box &box);
	void DrawBall(const shape::Ball &ball);
	void DrawCannon(const shape::Cannon &Cannon);
	void DrawIncline(const shape::Incline &incline);
	void DrawTrigger(const shape::Trigger &trigger);
	void DrawBallLine(const shape::Ball &ball);
	void DrawFBD(physics::Rigidbody &body);
	void DrawMeasuringRectangle(math::Vec2 &start, math::Vec2 &size);
	void DrawTestTriangle();
	void DrawTest2Triangle();

	//void RenderText(TextRenderer textRenderer, std::string& text, float x, float y, float scale, const std::array<float, 3>& color);

	GLFWwindow *GetWindow() const { return window; }
	Device GetDevice() const { return device; }
	Queue GetQueue() const { return queue; }
	RenderPassEncoder GetRenderPass() const { return renderPass; };
	TextureFormat GetSurfaceFormat() const { return surfaceFormat; };
	void SetZoom(float value);
	void SetCameraOffset(const math::Vec2& offset);  // Add this
    math::Vec2 GetCameraOffset() const;              // Add this
	float GetZoom() const { return currentZoom; }

private:
	wgpu::TextureView GetNextSurfaceTextureView();
	// Substep of Initialize() that creates the render pipeline
	void InitializePipeline();
	wgpu::RequiredLimits GetRequiredLimits(Adapter adapter) const;
	void InitializeBuffers();
	void EnsureVertexBufferSize(int size);
	uint32_t UpdateUniforms(const math::Vec2 &position, const std::array<float, 4> &color);
	void DrawGrid();

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
	RenderPipeline linePipeline;
	PipelineLayout pipelineLayout;
	BindGroupLayout uniformBindGroupLayout;
	BindGroup uniformBindGroup;
	Buffer uniformBuffer;
	Buffer vertexBuffer;
	uint32_t vertexCount;
	CommandEncoder encoder;
	TextureView targetView;
	uint32_t uniformAlignment = 1;
	size_t uniformBufferStride = 0;
	size_t uniformBufferSize = 0;
	size_t uniformBufferOffset = 0;
	uint32_t windowWidth = WindowConstants::defaultWindowWidth;
	uint32_t windowHeight = WindowConstants::defaultWindowHeight;
	uint32_t framebufferWidth = WindowConstants::defaultWindowWidth;
	uint32_t framebufferHeight = WindowConstants::defaultWindowHeight;

	bool surfaceIsSrgb = true;
	//Camera
	float currentZoom = 1.0f;
    math::Vec2 cameraOffset{0.0f, 0.0f}; 

	const WGPUColor backgroundColor = WGPUColor{0.0060*9.6, 0.0075*10.5, 0.010*9.8, 1.0};
	
		
	//Text
	ShaderModule textShaderModule;
	RenderPipeline textPipeline;
	BindGroupLayout textBindGroupLayout;
	PipelineLayout textPipelineLayout;
	Buffer textVertexBuffer;
	Sampler textSampler;
};
