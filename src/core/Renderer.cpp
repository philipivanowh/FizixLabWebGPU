// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include "core/Renderer.hpp"
#include "core/Utility.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace wgpu;

namespace {
struct Uniforms {
	float resolution[2];
	float translation[2];
	float color[4];
};

size_t AlignTo(size_t value, size_t alignment) {
	return (value + alignment - 1) / alignment * alignment;
}
} // namespace

bool Renderer::Initialize()
{
	// Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(windowWidth, windowHeight, "FizixEngine", nullptr, nullptr);

	Instance instance = wgpuCreateInstance(nullptr);

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
	config.width = windowWidth;
	config.height = windowHeight;
	config.usage = TextureUsage::RenderAttachment;
	surfaceFormat = surface.getPreferredFormat(adapter);
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
	vertexBuffer.release();
	uniformBuffer.release();
	uniformBindGroup.release();
	uniformBindGroupLayout.release();
	pipelineLayout.release();
	linePipeline.release();
	pipeline.release();
	surface.unconfigure();
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
	WGPUBindGroupLayout bindGroupLayouts[] = { uniformBindGroupLayout };
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = bindGroupLayouts;
	pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);
	pipelineDesc.layout = pipelineLayout;

	pipeline = device.createRenderPipeline(pipelineDesc);
	if (!pipeline) {
		std::cout << "Failed to create render pipeline!" << std::endl;
	}
	pipelineDesc.primitive.topology = PrimitiveTopology::LineList;
	linePipeline = device.createRenderPipeline(pipelineDesc);
	if (!linePipeline) {
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
	if (!uniformBindGroup) {
		std::cout << "Failed to create uniform bind group." << std::endl;
	}

	// Release the shader module after pipeline creation is confirmed
	if (!pipeline) {
		std::cout << "Failed to create render pipeline." << std::endl;
	}
	shaderModule.release();
}

void Renderer::BeginFrame()
{
	glfwPollEvents();

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
	renderPassColorAttachment.clearValue = backgroundColor;
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
	adapter.getLimits(&supportedLimits);

	// Don't forget to = Default
	RequiredLimits requiredLimits = Default;

	// We use at most 1 vertex attribute for now
	requiredLimits.limits.maxVertexAttributes = 1;
	// We should also tell that we use 1 vertex buffers
	requiredLimits.limits.maxVertexBuffers = 1;
	// Allow larger buffers for uniforms and dynamic data.
	requiredLimits.limits.maxBufferSize = supportedLimits.limits.maxBufferSize;
	// Maximum stride between 2 consecutive vertices in the vertex buffer
	requiredLimits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);

	// These two limits are different because they are "minimum" limits,
	// they are the only ones we are may forward from the adapter's supported
	// limits.
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

	return requiredLimits;
}

void Renderer::InitializeBuffers()
{
	std::cout << "Initializing buffers..." << std::endl;
	// Vertex buffer data
	// There are 2 floats per vertex, one for x and one for y.
	std::vector<float> vertexData = {
		// Define a first triangle:
		-100,0,
		100,0,
		50,100
	
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

void Renderer::DrawShape(const physics::Rigidbody &Rigidbody)
{
	if (auto box = dynamic_cast<const shape::Box *>(&Rigidbody))
	{
		DrawBox(*box);
		return;
	}
	if (auto ball = dynamic_cast<const shape::Ball *>(&Rigidbody))
	{
		DrawBall(*ball);
		return;
	}
}

void Renderer::DrawBox(const shape::Box &box)
{
	renderPass.setPipeline(pipeline);

	const std::vector<float> vertices = box.GetVertexLocalPos();
	EnsureVertexBufferSize(vertices.size());

	queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

	vertexCount = static_cast<uint32_t>(vertices.size() / 2);
	uint32_t uniformOffset = UpdateUniforms(box.pos, box.color);

	if (!uniformBindGroup) {
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
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


void Renderer::DrawBall(const shape::Ball &ball)
{
	renderPass.setPipeline(pipeline);

	const std::vector<float> vertices = ball.GetVertexLocalPos();
	EnsureVertexBufferSize(vertices.size());
	
	queue.writeBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(float));

	vertexCount = static_cast<uint32_t>(vertices.size() / 2);
	uint32_t uniformOffset = UpdateUniforms(ball.pos, ball.color);

	if (!uniformBindGroup) {
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);
	
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
	std::cout << "Drawing ball with " << vertexCount << " vertices at position (" << ball.pos.x << ", " << ball.pos.y << ")" << std::endl;
}

void Renderer::DrawTestTriangle()
{
	renderPass.setPipeline(pipeline);
	const math::Vec2 translation(500,500);
	const std::array<float, 4> color{0.3f, 0.8f, 1.0f, 1.0f};
	uint32_t uniformOffset = UpdateUniforms(translation, color);

	if (!uniformBindGroup) {
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
	const math::Vec2 translation(700,500);
	const std::array<float, 4> color{0.1f, 0.8f, 1.0f, 1.0f};
	uint32_t uniformOffset = UpdateUniforms(translation, color);

	if (!uniformBindGroup) {
		std::cout << "Uniform bind group is invalid; skipping draw." << std::endl;
		return;
	}
	renderPass.setBindGroup(0, uniformBindGroup, 1, &uniformOffset);
	renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount * 2 * sizeof(float));
	renderPass.draw(vertexCount, 1, 0, 0);

	
}

uint32_t Renderer::UpdateUniforms(const math::Vec2& position, const std::array<float, 4>& color)
{
	Uniforms uniforms = {};
	uniforms.resolution[0] = static_cast<float>(windowWidth);
	uniforms.resolution[1] = static_cast<float>(windowHeight);
	uniforms.translation[0] = position.x;
	uniforms.translation[1] = position.y;

	const bool rgbNeedsNormalization = color[0] > 1.0f || color[1] > 1.0f ||
		color[2] > 1.0f;
	const float rgbScale = rgbNeedsNormalization ? (1.0f / 255.0f) : 1.0f;
	uniforms.color[0] = color[0] * rgbScale;
	uniforms.color[1] = color[1] * rgbScale;
	uniforms.color[2] = color[2] * rgbScale;
	uniforms.color[3] = (color[3] > 1.0f) ? (color[3] * (1.0f / 255.0f)) : color[3];

	if (uniformBufferOffset + uniformBufferStride > uniformBufferSize) {
		uniformBufferOffset = 0;
	}
	uint32_t offset = static_cast<uint32_t>(uniformBufferOffset);
	queue.writeBuffer(uniformBuffer, uniformBufferOffset, &uniforms, sizeof(Uniforms));
	uniformBufferOffset += uniformBufferStride;
	return offset;
}
