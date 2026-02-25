// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include "core/Renderer.hpp"
#include "core/Utility.hpp"
#include "math/Math.hpp"
#include "math/Mat4.hpp"
#include "common/settings.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace wgpu;

namespace
{
	struct Uniforms
	{
		float resolution[2];
		float translation[2];
		float color[4];
		float extras[4];
	};

	size_t AlignTo(size_t value, size_t alignment)
	{
		return (value + alignment - 1) / alignment * alignment;
	}
} // namespace

bool Renderer::Initialize(Settings &settings, GLFWscrollfun scrollCallback)
{
	// Open window
	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return false;
	}

	// settings.InitFromMonitor();

	// ── INSERT POINT A: native resolution ───────────────────────────────
	// Query the primary monitor native video mode so the window matches
	// the physical screen pixel count on every platform.

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	// On macOS, request a full Retina (HiDPI) framebuffer.
	settings.InitFromMonitor();
	windowWidth = settings.windowWidth;
	windowHeight = settings.windowHeight;
	framebufferWidth = settings.windowWidth;
	framebufferHeight = settings.windowHeight;

#ifdef __APPLE__
	glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif

	window = glfwCreateWindow(windowWidth, windowHeight, "FizixEngine", nullptr, nullptr);
	if (!window)
	{
		std::cerr << "Could not open window!" << std::endl;
		glfwTerminate();
		return false;
	}

	if (scrollCallback)
	{
		glfwSetScrollCallback(window, scrollCallback);
	}

	glfwGetFramebufferSize(window,
						   reinterpret_cast<int *>(&framebufferWidth),
						   reinterpret_cast<int *>(&framebufferHeight));

	Instance instance = wgpuCreateInstance(nullptr);
	if (!instance)
	{
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	surface = glfwGetWGPUSurface(instance, window);

	// Get adapter
	std::cout << "Requesting adapter..." << std::endl;
	RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = surface;
	Adapter adapter = instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	instance.release();

	std::cout << "Requesting device..." << std::endl;
	DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const *message, void * /* pUserData */)
	{
		std::cout << "Device lost: reason " << reason;
		if (message)
			std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	// Before adapter.requestDevice(deviceDesc)
	RequiredLimits requiredLimits = GetRequiredLimits(adapter);
	uniformAlignment = requiredLimits.limits.minUniformBufferOffsetAlignment;
	deviceDesc.requiredLimits = &requiredLimits;
	device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << device << std::endl;

	// Device error callback
	uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const *message)
																	  {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl; });

	queue = device.getQueue();

	// Configure the surface
	SurfaceConfiguration config = {};

	// Configuration of the textures created for the underlying swap chain
	config.width = framebufferWidth;
	config.height = framebufferHeight;
	config.usage = TextureUsage::RenderAttachment;
	// ── INSERT POINT B: surface format — prefer linear, detect sRGB ────
	// Windows/Linux: first format is usually BGRA8Unorm (linear). ✓
	// macOS/Metal  : first format is BGRA8UnormSrgb — the GPU will
	//                gamma-encode shader output automatically.
	// We scan the capability list for the linear variant first.
	// If unavailable we set surfaceIsSrgb so UpdateUniforms() can
	// pre-correct colours before upload.
	SurfaceCapabilities surfaceCapabilities;
	surface.getCapabilities(adapter, &surfaceCapabilities);

	surfaceFormat = surfaceCapabilities.formats[0]; // safe fallback
	for (size_t fi = 0; fi < surfaceCapabilities.formatCount; ++fi)
	{
		if (surfaceCapabilities.formats[fi] == TextureFormat::BGRA8Unorm)
		{
			surfaceFormat = TextureFormat::BGRA8Unorm; // prefer linear
			break;
		}
	}
	surfaceCapabilities.freeMembers();

	// Record whether we ended up with an sRGB surface (macOS fallback).

#ifdef __APPLE__
	surfaceIsSrgb = (surfaceFormat == TextureFormat::BGRA8UnormSrgb);
	if (surfaceIsSrgb)
		std::cout << "[Renderer] macOS: sRGB surface — enabling linear->sRGB colour pre-correction." << std::endl;
	else
		std::cout << "[Renderer] macOS: linear surface — no colour pre-correction needed." << std::endl;
#endif

	config.format = surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = device;
	config.presentMode = PresentMode::Fifo;
	config.alphaMode = CompositeAlphaMode::Auto;

	surface.configure(config);

	// Release the adapter only after it has been fully utilized
	adapter.release();

	InitializePipeline();
	InitializeBuffers();

	return true;
}

void Renderer::Terminate()
{
	// 2D Shape Pipeline
	vertexBuffer.release();
	uniformBuffer.release();
	uniformBindGroup.release();
	uniformBindGroupLayout.release();
	pipelineLayout.release();
	linePipeline.release();
	// Text Pipeline
	pipeline.release();
	textSampler.release();
	textVertexBuffer.release();
	textPipelineLayout.release();
	textBindGroupLayout.release();
	textPipeline.release();
	textShaderModule.release();
	surface.unconfigure();

	// WebGPU
	queue.release();
	surface.release();
	device.release();
	glfwDestroyWindow(window);
	glfwTerminate();
}

bool Renderer::IsRunning()
{
	return !glfwWindowShouldClose(window);
}

void Renderer::SetZoom(float value)
{
	currentZoom = value;
	if (currentZoom < 0.1f)
	{
		currentZoom = 0.1f;
	}
	else if (currentZoom > 4.0f)
	{
		currentZoom = 4.0f;
	}
}

void Renderer::SetCameraOffset(const math::Vec2 &offset)
{
	cameraOffset = offset;
}

math::Vec2 Renderer::GetCameraOffset() const
{
	return cameraOffset;
}

TextureView Renderer::GetNextSurfaceTextureView()
{
	// Get the surface texture
	SurfaceTexture surfaceTexture;
	surface.getCurrentTexture(&surfaceTexture);
	if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success)
	{
		return nullptr;
	}
	Texture texture = surfaceTexture.texture;

	// Create a view for this surface texture
	TextureViewDescriptor viewDescriptor;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = texture.getFormat();
	viewDescriptor.dimension = TextureViewDimension::_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = TextureAspect::All;
	TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
	// We no longer need the texture, only its view
	// (NB: with wgpu-native, surface textures must not be manually released)
	Texture(surfaceTexture.texture).release();
#endif // WEBGPU_BACKEND_WGPU
	return targetView;
}

void Renderer::InitializePipeline()
{
	// Load the shader module
	ShaderModuleDescriptor shaderDesc;
#ifdef WEBGPU_BACKEND_WGPU
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
#endif
	std::string shaderSource = Utility::LoadFileToString("src/shaders/triangle.wgsl");
	// We use the extension mechanism to specify the WGSL part of the shader module descriptor
	ShaderModuleWGSLDescriptor shaderCodeDesc;
	// Set the chained struct's header
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
	// Connect the chain
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	shaderCodeDesc.code = shaderSource.c_str();
	ShaderModule shaderModule = device.createShaderModule(shaderDesc);

	// Create the render pipeline
	RenderPipelineDescriptor pipelineDesc;

	// Configure the vertex pipeline
	// We use one vertex buffer
	VertexBufferLayout vertexBufferLayout;
	VertexAttribute positionAttrib;
	// == For each attribute, describe its layout, i.e., how to interpret the raw data ==
	// Corresponds to @location(...)
	positionAttrib.shaderLocation = 0;
	// Means vec2f in the shader
	positionAttrib.format = VertexFormat::Float32x2;
	// Index of the first element
	positionAttrib.offset = 0;

	vertexBufferLayout.attributeCount = 1;
	vertexBufferLayout.attributes = &positionAttrib;

	// == Common to attributes from the same buffer ==
	vertexBufferLayout.arrayStride = 2 * sizeof(float);
	vertexBufferLayout.stepMode = VertexStepMode::Vertex;

	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;

	// NB: We define the 'shaderModule' in the second part of this chapter.
	// Here we tell that the programmable vertex shader stage is described
	// by the function called 'vs_main' in that module.
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	// Each sequence of 3 vertices is considered as a triangle
	pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;

	// We'll see later how to specify the order in which vertices should be
	// connected. When not specified, vertices are considered sequentially.
	pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;

	// The face orientation is defined by assuming that when looking
	// from the front of the face, its corner vertices are enumerated
	// in the counter-clockwise (CCW) order.
	pipelineDesc.primitive.frontFace = FrontFace::CCW;

	// But the face orientation does not matter much because we do not
	// cull (i.e. "hide") the faces pointing away from us (which is often
	// used for optimization).
	pipelineDesc.primitive.cullMode = CullMode::None;

	// We tell that the programmable fragment shader stage is described
	// by the function called 'fs_main' in the shader module.
	FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	BlendState blendState;
	blendState.color.srcFactor = BlendFactor::SrcAlpha;
	blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = BlendOperation::Add;
	blendState.alpha.srcFactor = BlendFactor::Zero;
	blendState.alpha.dstFactor = BlendFactor::One;
	blendState.alpha.operation = BlendOperation::Add;

	ColorTargetState colorTarget;
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = ColorWriteMask::All; // We could write to only some of the color channels.

	// We have only one target because our render pass has only one output color
	// attachment.
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
	pipelineDesc.fragment = &fragmentState;

	// We do not use stencil/depth testing for now
	pipelineDesc.depthStencil = nullptr;

	// Samples per pixel
	pipelineDesc.multisample.count = 1;

	// Default value for the mask, meaning "all bits on"
	pipelineDesc.multisample.mask = ~0u;

	// Default value as well (irrelevant for count = 1 anyways)
	pipelineDesc.multisample.alphaToCoverageEnabled = false;
	BindGroupLayoutEntry uniformLayoutEntry = {};
	uniformLayoutEntry.binding = 0;
	uniformLayoutEntry.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
	uniformLayoutEntry.buffer.type = BufferBindingType::Uniform;
	uniformLayoutEntry.buffer.hasDynamicOffset = true;
	uniformLayoutEntry.buffer.minBindingSize = sizeof(Uniforms);

	BindGroupLayoutDescriptor uniformLayoutDesc = {};
	uniformLayoutDesc.entryCount = 1;
	uniformLayoutDesc.entries = &uniformLayoutEntry;
	uniformBindGroupLayout = device.createBindGroupLayout(uniformLayoutDesc);

	PipelineLayoutDescriptor pipelineLayoutDesc = {};
	WGPUBindGroupLayout bindGroupLayouts[] = {uniformBindGroupLayout};
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = bindGroupLayouts;
	pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);
	pipelineDesc.layout = pipelineLayout;

	pipeline = device.createRenderPipeline(pipelineDesc);
	if (!pipeline)
	{
		std::cout << "Failed to create render pipeline!" << std::endl;
	}
	pipelineDesc.primitive.topology = PrimitiveTopology::LineList;
	linePipeline = device.createRenderPipeline(pipelineDesc);
	if (!linePipeline)
	{
		std::cout << "Failed to create line render pipeline!" << std::endl;
	}

	uniformBufferStride = AlignTo(sizeof(Uniforms), static_cast<size_t>(uniformAlignment));
	constexpr size_t kMaxUniformsPerFrame = 256;
	uniformBufferSize = uniformBufferStride * kMaxUniformsPerFrame;

	BufferDescriptor uniformBufferDesc = {};
	uniformBufferDesc.size = uniformBufferSize;
	uniformBufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
	uniformBufferDesc.mappedAtCreation = false;
	uniformBuffer = device.createBuffer(uniformBufferDesc);

	BindGroupEntry uniformBindGroupEntry = {};
	uniformBindGroupEntry.binding = 0;
	uniformBindGroupEntry.buffer = uniformBuffer;
	uniformBindGroupEntry.offset = 0;
	uniformBindGroupEntry.size = sizeof(Uniforms); // Size of each uniform block

	BindGroupDescriptor uniformBindGroupDesc = {};
	uniformBindGroupDesc.layout = uniformBindGroupLayout;
	uniformBindGroupDesc.entryCount = 1;
	uniformBindGroupDesc.entries = &uniformBindGroupEntry;
	uniformBindGroup = device.createBindGroup(uniformBindGroupDesc);
	if (!uniformBindGroup)
	{
		std::cout << "Failed to create uniform bind group." << std::endl;
	}

	// Release the shader module after pipeline creation is confirmed
	if (!pipeline)
	{
		std::cout << "Failed to create render pipeline." << std::endl;
	}
	shaderModule.release();
}
void Renderer::BeginFrame()
{
	glfwPollEvents();

	uint32_t newFramebufferWidth = framebufferWidth;
	uint32_t newFramebufferHeight = framebufferHeight;
	glfwGetFramebufferSize(window,
						   reinterpret_cast<int *>(&newFramebufferWidth),
						   reinterpret_cast<int *>(&newFramebufferHeight));
	if (newFramebufferWidth != framebufferWidth || newFramebufferHeight != framebufferHeight)
	{
		framebufferWidth = newFramebufferWidth;
		framebufferHeight = newFramebufferHeight;
		if (framebufferWidth == 0 || framebufferHeight == 0)
		{
			return;
		}
		SurfaceConfiguration config = {};
		config.width = framebufferWidth;
		config.height = framebufferHeight;
		config.usage = TextureUsage::RenderAttachment;
		config.format = surfaceFormat;
		config.viewFormatCount = 0;
		config.viewFormats = nullptr;
		config.device = device;
		config.presentMode = PresentMode::Fifo;
		config.alphaMode = CompositeAlphaMode::Auto;
		surface.configure(config);
	}

	// Get the next target texture view
	targetView = GetNextSurfaceTextureView();
	if (!targetView)
		return;

	// Create a command encoder for the draw call
	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	// Create the render pass that clears the screen with our color
	RenderPassDescriptor renderPassDesc = {};

	// The attachment part of the render pass descriptor describes the target texture of the pass
	RenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	// 0-1 color (normalize if values are 0-255, then apply macOS-only brightness bump + sRGB pre-correction)
	WGPUColor clearColor = backgroundColor;
	renderPassColorAttachment.clearValue = clearColor;
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	renderPass = encoder.beginRenderPass(renderPassDesc);
	renderPass.setPipeline(pipeline);
	uniformBufferOffset = 0;

	DrawGrid();
}

void Renderer::DrawGrid()
{
	constexpr float kGridSpacing = 50.0f;
	constexpr float kMajorSpacing = 200.0f;
	std::array<float, 4> minorColor{0.1f, 0.5f, 0.6f, 0.05f};
	std::array<float, 4> majorColor{0.2f, 0.7f, 0.8f, 0.1f};

	for (float &value : minorColor)
	{
		value *= 1.2f;
	}
	for (float &value : majorColor)
	{
		value *= 1.2f;
	}

	std::vector<float> minorVertices;
	std::vector<float> majorVertices;

	// Calculate visible world space at current zoom
	const float visibleWorldWidth = windowWidth / currentZoom;
	const float visibleWorldHeight = windowHeight / currentZoom;

	const float padding = std::max(visibleWorldWidth, visibleWorldHeight) * 0.5f;

	const float startX = std::floor((cameraOffset.x - visibleWorldWidth * 0.5f - padding) / kGridSpacing) * kGridSpacing;
	const float endX = std::ceil((cameraOffset.x + visibleWorldWidth * 0.5f + padding) / kGridSpacing) * kGridSpacing;
	const float startY = std::floor((cameraOffset.y - visibleWorldHeight * 0.5f - padding) / kGridSpacing) * kGridSpacing;
	const float endY = std::ceil((cameraOffset.y + visibleWorldHeight * 0.5f + padding) / kGridSpacing) * kGridSpacing;

	// Vertical lines
	for (float x = startX; x <= endX; x += kGridSpacing)
	{
		const bool isMajor = std::fabs(std::fmod(x, kMajorSpacing)) < 0.5f;
		auto &vertices = isMajor ? majorVertices : minorVertices;
		vertices.push_back(x);
		vertices.push_back(startY);
		vertices.push_back(x);
		vertices.push_back(endY);
	}

	// Horizontal lines
	for (float y = startY; y <= endY; y += kGridSpacing)
	{
		const bool isMajor = std::fabs(std::fmod(y, kMajorSpacing)) < 0.5f;
		auto &vertices = isMajor ? majorVertices : minorVertices;
		vertices.push_back(startX);
		vertices.push_back(y);
		vertices.push_back(endX);
		vertices.push_back(y);
	}

	const math::Vec2 gridOrigin(0.0f, 0.0f);

	auto drawLines = [&](const std::vector<float> &vertices, const std::array<float, 4> &color)
	{
		if (vertices.empty())
			return;

		EnsureVertexBufferSize(static_cast<int>(vertices.size()));
		queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

		const uint32_t uniformOffset = UpdateUniforms(gridOrigin, color);
		renderPass.setPipeline(linePipeline);
		renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, vertices.size() * sizeof(float));
		renderPass.draw(static_cast<uint32_t>(vertices.size() / 2), 1, 0, 0);
	};

	drawLines(minorVertices, minorColor);
	drawLines(majorVertices, majorColor);

	// ── Draw destruction line at y = -10000.0f ────────────────────
	constexpr float kDestructionY = -10000.0f;

	// Only draw if it's in the visible range
	if (kDestructionY >= startY && kDestructionY <= endY)
	{
		std::array<float, 4> destructionColor{1.0f, 0.2f, 0.2f, 0.8f}; // Bright red, high opacity
		std::vector<float> destructionLine = {
			startX, kDestructionY,
			endX, kDestructionY};

		drawLines(destructionLine, destructionColor);
	}

	renderPass.setPipeline(pipeline);
}
void Renderer::EndFrame()
{

	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	queue.submit(1, &command);
	command.release();

	// At the end of the frame
	targetView.release();
#ifndef __EMSCRIPTEN__
	surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#endif
}

RequiredLimits Renderer::GetRequiredLimits(Adapter adapter) const
{
	// Get adapter supported limits, in case we need them
	SupportedLimits supportedLimits;
#ifdef __EMSCRIPTEN__
	// Error in Chrome: Aborted(TODO: wgpuAdapterGetLimits unimplemented)
	// (as of September 4, 2023), so we hardcode values:
	// These work for 99.95% of clients (source: https://web3dsurvey.com/webgpu)
	supportedLimits.limits.minStorageBufferOffsetAlignment = 32;
	supportedLimits.limits.minUniformBufferOffsetAlignment = 32;
#else
	adapter.getLimits(&supportedLimits);
#endif

	// Don't forget to = Default
	RequiredLimits requiredLimits = Default;

	// We use at most 1 vertex attribute for now
	requiredLimits.limits.maxVertexAttributes = supportedLimits.limits.maxVertexAttributes;
	;
	// We should also tell that we use 1 vertex buffers
	requiredLimits.limits.maxVertexBuffers = supportedLimits.limits.maxVertexBuffers;
	// Allow larger buffers for uniforms and dynamic data.
	requiredLimits.limits.maxBufferSize = supportedLimits.limits.maxBufferSize;
	// Maximum stride between 2 consecutive vertices in the vertex buffer
	requiredLimits.limits.maxVertexBufferArrayStride = supportedLimits.limits.maxVertexBufferArrayStride;
	;

	// These two limits are different because they are "minimum" limits,
	// they are the only ones we are may forward from the adapter's supported
	// limits.
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

	requiredLimits.limits.maxBindGroups = 2;

	return requiredLimits;
}

void Renderer::InitializeBuffers()
{
	std::cout << "Initializing buffers..." << std::endl;
	// Vertex buffer data
	// There are 2 floats per vertex, one for x and one for y.
	std::vector<float> vertexData = {
		// Define a first triangle:
		-100, 0,
		100, 0,
		50, 100

	};
	vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

	// Create vertex buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = vertexData.size() * sizeof(float);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex; // Vertex usage here!
	bufferDesc.mappedAtCreation = false;
	vertexBuffer = device.createBuffer(bufferDesc);

	// Upload geometry data to the buffer
	queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);
}

void Renderer::DrawHighlightOutline(physics::Rigidbody &body)
{
	float outlineThickness = 7.0f; // Thickness of the highlight outline in world units
	// Highlight color - bright yellow/white glow
	const std::array<float, 4> highlightColor{1.0f, 0.9f, 0.4f, 0.6f};

	if (auto box = dynamic_cast<const shape::Box *>(&body))
	{
		// Draw box slightly larger
		renderPass.setPipeline(pipeline);

		// Get vertices and scale them outward
		std::vector<float> vertices = box->GetVertexLocalPos();

		// Scale vertices outward from center by outline thickness
		for (size_t i = 0; i < vertices.size(); i += 2)
		{
			float x = vertices[i];
			float y = vertices[i + 1];

			// Calculate distance from center
			float dist = std::sqrt(x * x + y * y);
			if (dist > 0.001f)
			{
				// Push outward
				float scale = 1.0f + outlineThickness / dist;
				vertices[i] = x * scale;
				vertices[i + 1] = y * scale;
			}
		}

		EnsureVertexBufferSize(vertices.size());
		queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

		vertexCount = static_cast<uint32_t>(vertices.size() / 2);
		uint32_t uniformOffset = UpdateUniforms(box->pos, highlightColor);

		renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
		renderPass.draw(vertexCount, 1, 0, 0);
	}
	else if (auto ball = dynamic_cast<const shape::Ball *>(&body))
	{
		// Draw ball slightly larger
		renderPass.setPipeline(pipeline);

		std::vector<float> vertices = ball->GetVertexLocalPos();

		// Scale all vertices by a factor
		float scale = (ball->radius + outlineThickness * 0.7) / ball->radius;
		for (size_t i = 0; i < vertices.size(); i++)
		{
			vertices[i] *= scale;
		}

		EnsureVertexBufferSize(vertices.size());
		queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

		vertexCount = static_cast<uint32_t>(vertices.size() / 2);
		uint32_t uniformOffset = UpdateUniforms(ball->pos, highlightColor);

		renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
		renderPass.draw(vertexCount, 1, 0, 0);
	}
	else if (auto incline = dynamic_cast<const shape::Incline *>(&body))
	{
		renderPass.setPipeline(pipeline);

		std::vector<float> vertices = incline->GetVertexLocalPos();

		// Incline has exactly 3 vertices (6 floats)
		// Extract the 3 corner points
		math::Vec2 v0(vertices[0], vertices[1]);
		math::Vec2 v1(vertices[2], vertices[3]);
		math::Vec2 v2(vertices[4], vertices[5]);

		// Calculate centroid of the triangle
		math::Vec2 centroid = (v0 + v1 + v2) / 3.0f;

		// Scale each vertex away from centroid
		math::Vec2 outV0 = centroid + (v0 - centroid) * (1.0f + outlineThickness * 1.7 / (v0 - centroid).Length());
		math::Vec2 outV1 = centroid + (v1 - centroid) * (1.0f + outlineThickness * 1.7 / (v1 - centroid).Length());
		math::Vec2 outV2 = centroid + (v2 - centroid) * (1.0f + outlineThickness * 1.7 / (v2 - centroid).Length());

		// Create vertex array for the outline triangle
		std::vector<float> outlineVertices = {
			outV0.x, outV0.y,
			outV1.x, outV1.y,
			outV2.x, outV2.y};

		EnsureVertexBufferSize(outlineVertices.size());
		queue.writeBuffer(vertexBuffer, 0, outlineVertices.data(), outlineVertices.size() * sizeof(float));

		vertexCount = static_cast<uint32_t>(outlineVertices.size() / 2);
		uint32_t uniformOffset = UpdateUniforms(incline->pos, highlightColor);

		renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
		renderPass.draw(vertexCount, 1, 0, 0);
	}
	else if (auto cannon = dynamic_cast<const shape::Cannon *>(&body))
	{
		// For cannon, draw a simple circle around it
		const float cannonSize = 50.0f; // Approximate cannon size
		std::vector<float> circleVerts;
		const int segments = 32;

		for (int i = 0; i < segments; i++)
		{
			float angle1 = (float)i / segments * 2.0f * math::PI;
			float angle2 = (float)(i + 1) / segments * 2.0f * math::PI;

			circleVerts.push_back(std::cos(angle1) * cannonSize);
			circleVerts.push_back(std::sin(angle1) * cannonSize);
			circleVerts.push_back(std::cos(angle2) * cannonSize);
			circleVerts.push_back(std::sin(angle2) * cannonSize);
			circleVerts.push_back(0.0f);
			circleVerts.push_back(0.0f);
		}

		EnsureVertexBufferSize(circleVerts.size());
		queue.writeBuffer(vertexBuffer, 0, circleVerts.data(), circleVerts.size() * sizeof(float));

		uint32_t uniformOffset = UpdateUniforms(cannon->pos, highlightColor);
		renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, circleVerts.size() * sizeof(float));
		renderPass.draw(static_cast<uint32_t>(circleVerts.size() / 2), 1, 0, 0);
	}
}
void Renderer::DrawShape(physics::Rigidbody &body, bool highlight)
{
	if (highlight)
	{
		DrawHighlightOutline(body);
	}
	if (auto box = dynamic_cast<const shape::Box *>(&body))
	{
		DrawBox(*box);
		DrawFBD(body);
		return;
	}
	else if (auto ball = dynamic_cast<const shape::Ball *>(&body))
	{
		DrawBall(*ball);
		DrawFBD(body);
		return;
	}
	else if (auto Cannon = dynamic_cast<const shape::Cannon *>(&body))
	{
		DrawCannon(*Cannon);
		return;
	}
	else if (auto incline = dynamic_cast<const shape::Incline *>(&body))
	{
		DrawIncline(*incline);
		DrawFBD(body);
	}
}

void Renderer::DrawFBD(physics::Rigidbody &body)
{
	// Draw force arrow
	std::vector<math::Vec2> forcesToDraw;
	const auto &displayForces = body.GetForcesForDisplay();
	forcesToDraw.reserve(displayForces.size());

	for (const auto &forceInfo : displayForces)
	{
		forcesToDraw.push_back(forceInfo.force);
	}

	if (forcesToDraw.empty())
	{
		return;
	}

	auto forceColorForType = [](physics::ForceType type) -> std::array<float, 4>
	{
		switch (type)
		{
		case physics::ForceType::Normal:
			return {0.3f, 0.7f, 0.9f, 0.4f};
		case physics::ForceType::Frictional:
			return {0.9f, 0.5f, 0.2f, 0.4f};
		case physics::ForceType::Gravitational:
			return {0.2f, 0.6f, 0.3f, 0.4f};
		case physics::ForceType::Tension:
			return {0.9f, 0.9f, 0.2f, 0.4f};
		case physics::ForceType::Apply:
		default:
			return {0.5f, 0.3f, 0.5f, 0.4f};
		}
	};

	for (size_t i = 0; i < forcesToDraw.size(); i++)
	{
		const math::Vec2 force = forcesToDraw[i];
		const float angleRadians = force.GetRadian();

		const math::Vec2 scaledForce = force / SimulationConstants::PIXELS_PER_METER;
		const math::Vec2 arrowEnd = force.Normalize() * math::MapForceToLength(
															scaledForce,
															VisualizationConstants::FBD_FORCE_MIN,
															VisualizationConstants::FBD_FORCE_MAX,
															VisualizationConstants::FBD_ARROW_MIN,
															VisualizationConstants::FBD_ARROW_MAX,
															VisualizationConstants::FBD_CURVE_EXPONENT);
		const std::array<float, 4> forceColor = forceColorForType(displayForces[i].type);

		const float arrowHalfThickness = VisualizationConstants::FBD_ARROW_THICKNESS / 2.0f;
		const float arrowHeadHalfThickness = VisualizationConstants::FBD_ARROW_HEAD_THICKNESS / 2.0f;
		const float perpendicularX = std::cos(angleRadians + math::PI / 2.0f);
		const float perpendicularY = std::sin(angleRadians + math::PI / 2.0f);

		const std::vector<float> forceVertices = {

			// The rectangle part of the arrow
			arrowHalfThickness * perpendicularX, arrowHalfThickness * perpendicularY,
			-arrowHalfThickness * perpendicularX, -arrowHalfThickness * perpendicularY,
			arrowEnd.x - arrowHalfThickness * perpendicularX, arrowEnd.y - arrowHalfThickness * perpendicularY,

			arrowEnd.x - arrowHalfThickness * perpendicularX, arrowEnd.y - arrowHalfThickness * perpendicularY,
			arrowEnd.x + arrowHalfThickness * perpendicularX, arrowEnd.y + arrowHalfThickness * perpendicularY,
			arrowHalfThickness * perpendicularX, arrowHalfThickness * perpendicularY,

			// The pointing part of the arrow
			arrowEnd.x - arrowHeadHalfThickness * perpendicularX, arrowEnd.y - arrowHeadHalfThickness * perpendicularY,
			arrowEnd.x + arrowHeadHalfThickness * perpendicularX, arrowEnd.y + arrowHeadHalfThickness * perpendicularY,
			arrowEnd.x * VisualizationConstants::FBD_ARROW_HEAD_SCALE,
			arrowEnd.y * VisualizationConstants::FBD_ARROW_HEAD_SCALE};

		vertexCount = static_cast<uint32_t>(forceVertices.size() / 2);

		EnsureVertexBufferSize(forceVertices.size());
		queue.writeBuffer(vertexBuffer, 0, forceVertices.data(), forceVertices.size() * sizeof(float));

		const uint32_t forceUniformOffset = UpdateUniforms(body.pos, forceColor);
		renderPass.setPipeline(pipeline);
		renderPass.setBindGroup(0, uniformBindGroup, 1, &forceUniformOffset);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, forceVertices.size() * sizeof(float));
		renderPass.draw(vertexCount, 1, 0, 0);
	}
}

void Renderer::DrawMeasuringRectangle(math::Vec2 &start, math::Vec2 &size)
{
	const std::array<float, 4> rectangleColor = {0.7f, 0.7f, 0.0f, 0.1f};
	const std::vector<float> rectangleVertices = {

		// The rectangle part of the arrow
		0, 0,
		size.x, 0,
		size.x, size.y,

		size.x, size.y,
		0, size.y,
		0, 0};

	vertexCount = static_cast<uint32_t>(rectangleVertices.size() / 2);

	EnsureVertexBufferSize(rectangleVertices.size());
	queue.writeBuffer(vertexBuffer, 0, rectangleVertices.data(), rectangleVertices.size() * sizeof(float));

	math::Vec2 pos = math::Vec2(start.x, start.y);
	const uint32_t forceUniformOffset = UpdateUniforms(pos, rectangleColor);
	renderPass.setPipeline(pipeline);
	renderPass.setBindGroup(0, uniformBindGroup, 1, &forceUniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, rectangleVertices.size() * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);
}

void Renderer::EnsureVertexBufferSize(int size)
{
	BufferDescriptor bufferDesc;
	bufferDesc.size = size * sizeof(float);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
	bufferDesc.mappedAtCreation = false;
	vertexBuffer = device.createBuffer(bufferDesc);
}

void Renderer::DrawBox(const shape::Box &box)
{
	renderPass.setPipeline(pipeline);

	const std::vector<float> vertices = box.GetVertexLocalPos();
	EnsureVertexBufferSize(vertices.size());

	queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

	vertexCount = static_cast<uint32_t>(vertices.size() / 2);
	uint32_t uniformOffset = UpdateUniforms(box.pos, box.color);

	if (!uniformBindGroup)
	{
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);
}

void Renderer::DrawIncline(const shape::Incline &incline)
{
	renderPass.setPipeline(pipeline);

	const std::vector<float> verticies = incline.GetVertexLocalPos();
	EnsureVertexBufferSize(verticies.size());

	queue.writeBuffer(vertexBuffer, 0, verticies.data(), verticies.size() * sizeof(float));

	vertexCount = static_cast<uint32_t>(verticies.size() / 2);
	uint32_t uniformOffset = UpdateUniforms(incline.pos, incline.color);

	if (!uniformBindGroup)
	{
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);
}

void Renderer::DrawBall(const shape::Ball &ball)
{
	renderPass.setPipeline(pipeline);

	const std::vector<float> vertices = ball.GetVertexLocalPos();
	EnsureVertexBufferSize(vertices.size());

	queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

	vertexCount = static_cast<uint32_t>(vertices.size() / 2);
	uint32_t uniformOffset = UpdateUniforms(ball.pos, ball.color);

	if (!uniformBindGroup)
	{
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);

	DrawBallLine(ball);
}

void Renderer::DrawCannon(const shape::Cannon &cannon)
{
	if (!uniformBindGroup)
	{
		std::cout << "Uniform bind group is invalid; skipping DrawCannon." << std::endl;
		return;
	}

	renderPass.setPipeline(pipeline);

	// ── Layered draw helper ────────────────────────────────────────
	// Uploads vertices, binds the correct uniform offset, and issues
	// one draw call.  Called once per visual layer, back-to-front.
	auto DrawPart = [&](const std::vector<float> &verts,
						const std::array<float, 4> &color)
	{
		if (verts.empty())
			return;
		EnsureVertexBufferSize(verts.size());
		queue.writeBuffer(vertexBuffer, 0,
						  verts.data(), verts.size() * sizeof(float));
		const uint32_t count = static_cast<uint32_t>(verts.size() / 2);
		uint32_t off = UpdateUniforms(cannon.pos, color);
		renderPass.setBindGroup(0, uniformBindGroup, 1, &off);
		renderPass.setVertexBuffer(0, vertexBuffer, 0, count * 2 * sizeof(float));
		renderPass.draw(count, 1, 0, 0);
	};

	// ── Draw order: back → front ───────────────────────────────────
	//
	//  1. Carriage trail  (wooden base, always horizontal)
	//  2. Wheel rim       (outer annulus ring)
	//  3. Wheel spokes    (eight thin radial spokes)
	//  4. Breech block    (wide rear barrel section + cascabel knob)
	//  5. Barrel body     (tapered main tube, on top of breech)
	//  6. Barrel band     (reinforcing ring ~44 % along tube)
	//  7. Muzzle ring     (raised lip at barrel tip)
	//  8. Wheel hub       (small axle disc — drawn late so it sits
	//                      visually in front of both wheel & barrel)
	//  9. Bore            (near-black circle at muzzle face —
	//                      always the topmost element so the player
	//                      can always read where shots originate)

	DrawPart(cannon.GetCarriageVertexLocalPos(), cannon.carriageColor);
	DrawPart(cannon.GetWheelRimVertexLocalPos(), cannon.wheelColor);
	DrawPart(cannon.GetWheelSpokesVertexLocalPos(), cannon.spokesColor);
	DrawPart(cannon.GetBreechVertexLocalPos(), cannon.breechColor);
	DrawPart(cannon.GetBarrelBodyVertexLocalPos(), cannon.barrelColor);
	DrawPart(cannon.GetBarrelBandVertexLocalPos(), cannon.bandColor);
	DrawPart(cannon.GetMuzzleRingVertexLocalPos(), cannon.muzzleRingColor);
	DrawPart(cannon.GetWheelHubVertexLocalPos(), cannon.hubColor);
	DrawPart(cannon.GetBoreVertexLocalPos(), cannon.boreColor);
}

void Renderer::DrawBallLine(const shape::Ball &ball)
{
	const float radiusLineX = ball.radius * std::cos(ball.rotation);
	const float radiusLineY = ball.radius * std::sin(ball.rotation);
	const std::array<float, 4> radiusLineColor{1.0f, 0.2f, 0.2f, 1.0f};
	const std::vector<float> lineVertices = {0.0f, 0.0f, radiusLineX, radiusLineY};
	EnsureVertexBufferSize(lineVertices.size());
	queue.writeBuffer(vertexBuffer, 0, lineVertices.data(), lineVertices.size() * sizeof(float));

	const uint32_t lineUniformOffset = UpdateUniforms(ball.pos, radiusLineColor);
	renderPass.setPipeline(linePipeline);
	renderPass.setBindGroup(0, uniformBindGroup, 1, &lineUniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, lineVertices.size() * sizeof(float));
	renderPass.draw(2, 1, 0, 0);

	renderPass.setPipeline(pipeline);
}

void Renderer::DrawTestTriangle()
{
	renderPass.setPipeline(pipeline);
	const math::Vec2 translation(500, 500);
	const std::array<float, 4> color{0.3f, 0.8f, 1.0f, 1.0f};
	uint32_t uniformOffset = UpdateUniforms(translation, color);

	if (!uniformBindGroup)
	{
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);
}

void Renderer::DrawTest2Triangle()
{
	renderPass.setPipeline(pipeline);
	const math::Vec2 translation(700, 500);
	const std::array<float, 4> color{0.1f, 0.8f, 1.0f, 1.0f};
	uint32_t uniformOffset = UpdateUniforms(translation, color);

	if (!uniformBindGroup)
	{
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);
}

uint32_t Renderer::UpdateUniforms(const math::Vec2 &position, const std::array<float, 4> &color)
{
	Uniforms uniforms = {};
	uniforms.resolution[0] = static_cast<float>(windowWidth);
	uniforms.resolution[1] = static_cast<float>(windowHeight);
	uniforms.translation[0] = position.x;
	uniforms.translation[1] = position.y;
	uniforms.extras[0] = currentZoom;
	uniforms.extras[1] = cameraOffset.x;
	uniforms.extras[2] = cameraOffset.y;
	uniforms.extras[3] = 0.0f;

	// Normalise 0-255 inputs to 0-1 (no-op if already in 0-1 range)
	const bool rgbNeedsNormalization = color[0] > 1.0f || color[1] > 1.0f ||
									   color[2] > 1.0f;
	const float rgbScale = rgbNeedsNormalization ? (1.0f / 255.0f) : 1.0f;
	float r = color[0] * rgbScale;
	float g = color[1] * rgbScale;
	float b = color[2] * rgbScale;
	const float a = (color[3] > 1.0f) ? (color[3] * (1.0f / 255.0f)) : color[3];

	// ── INSERT POINT C: linear → sRGB pre-correction (macOS only) ───────
	// When surfaceIsSrgb is true the GPU automatically gamma-encodes every
	// value the fragment shader writes.  Our colours are linear floats, so
	// without this they get gamma-encoded twice and appear washed-out.
	// Pre-applying the inverse sRGB transfer function here means the GPU
	// pass produces the correct original linear value on screen.
	//
	// Windows/Linux: surfaceIsSrgb == false → this block is skipped entirely.
#ifdef __APPLE__
	if (surfaceIsSrgb)
	{
		auto linearToSrgb = [](float c) -> float
		{
			c = (c < 0.0f) ? 0.0f : (c > 1.0f) ? 1.0f
											   : c;
			return (c <= 0.0031308f)
					   ? 12.92f * c
					   : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
		};
		r = linearToSrgb(r);
		g = linearToSrgb(g);
		b = linearToSrgb(b);
	}
#endif

	uniforms.color[0] = r;
	uniforms.color[1] = g;
	uniforms.color[2] = b;
	uniforms.color[3] = a;

	if (uniformBufferOffset + uniformBufferStride > uniformBufferSize)
	{
		uniformBufferOffset = 0;
	}
	uint32_t offset = static_cast<uint32_t>(uniformBufferOffset);
	queue.writeBuffer(uniformBuffer, uniformBufferOffset, &uniforms, sizeof(Uniforms));
	uniformBufferOffset += uniformBufferStride;
	return offset;
}
