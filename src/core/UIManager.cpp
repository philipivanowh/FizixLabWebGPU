#include "UIManager.hpp"
#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

namespace
{
	ImVec4 ToImGuiColor(const std::array<float, 4>& color)
	{
		return ImVec4(color[0] / 255.0f, color[1] / 255.0f, color[2] / 255.0f, color[3] / 255.0f);
	}

	void FromImGuiColor(const ImVec4& color, std::array<float, 4>& outColor)
	{
		outColor[0] = color.x * 255.0f;
		outColor[1] = color.y * 255.0f;
		outColor[2] = color.z * 255.0f;
		outColor[3] = color.w * 255.0f;
	}
}

void UIManager::InitializeImGui(Renderer& renderer)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(renderer.GetWindow(), true);
	ImGui_ImplWGPU_Init(renderer.GetDevice(), 3, renderer.GetSurfaceFormat());
}

void UIManager::TerminateImGui()
{
	ImGui_ImplWGPU_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void UIManager::BeginImGuiFrame()
{
	ImGui_ImplWGPU_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void UIManager::EndImGuiFrame(Renderer& renderer)
{
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderer.GetRenderPass());
}

void UIManager::RenderMainControls(std::size_t bodyCount)
{
	RenderSimulationControlsWindow(bodyCount);
}

void UIManager::RenderSpawner()
{
	RenderSpawnerWindow();
}


void UIManager::RenderSimulationControlsWindow(std::size_t bodyCount)
{
	ImGui::Begin("Simulation Controls");


    ImGui::Text("Bodies: %zu", bodyCount);
	ImGuiIO &io = ImGui::GetIO();
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
	ImGui::End();
}

void UIManager::RenderSpawnerWindow()
{
	ImGui::Begin("Object Spawner");

	ImGui::Text("Spawn Settings");
	ImGui::Separator();

	RenderSpawnBasics();
	RenderSpawnPhysicsControls();
	RenderSpawnSizeControls();
	RenderSpawnActions();

	ImGui::End();
}

void UIManager::RenderSpawnBasics()
{
	const char* shapes[] = {"Ball", "Box"};
	int shapeIndex = static_cast<int>(spawnSettings.shapeType);
	ImGui::Combo("Shape", &shapeIndex, shapes, 2);
	spawnSettings.shapeType = static_cast<shape::ShapeType>(shapeIndex);

	ImGui::DragFloat2("Position", &spawnSettings.position.x, 1.0f);
	ImGui::DragFloat2("Velocity", &spawnSettings.velocity.x, 0.1f);
}

void UIManager::RenderSpawnPhysicsControls()
{
	const char* rbType[] = {"Static", "Dynamic"};

	ImGui::DragFloat("Mass", &spawnSettings.mass, 0.5f, 0.1f, 1000.0f);
	ImGui::SliderFloat("Restitution", &spawnSettings.restitution, 0.0f, 1.0f);

	ImVec4 bodyColor = ToImGuiColor(spawnSettings.color);
	ImGui::ColorEdit3("Body color", reinterpret_cast<float*>(&bodyColor));
	FromImGuiColor(bodyColor, spawnSettings.color);

	int bodyTypeIndex = static_cast<int>(spawnSettings.bodyType);
	ImGui::Combo("Rigidbody Type", &bodyTypeIndex, rbType, 2);
	spawnSettings.bodyType = static_cast<physics::RigidbodyType>(bodyTypeIndex);
}

void UIManager::RenderSpawnSizeControls()
{
	if (spawnSettings.shapeType == shape::ShapeType::Box)
	{
		ImGui::DragFloat("Width", &spawnSettings.boxWidth, 1.0f, 1.0f, 500.0f);
		ImGui::DragFloat("Height", &spawnSettings.boxHeight, 1.0f, 1.0f, 500.0f);
		return;
	}

	ImGui::DragFloat("Radius", &spawnSettings.radius, 1.0f, 1.0f, 250.0f);
}

void UIManager::RenderSpawnActions()
{
	if (ImGui::Button("Spawn Object"))
	{
		spawnRequestPending = true;
	}
}

bool UIManager::ConsumeSpawnRequest(SpawnSettings &out){
	if (!spawnRequestPending)
	{
		return false;
	}

	out = spawnSettings;
	spawnRequestPending = false;
	return true;
}
