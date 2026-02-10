#include "MeasureTool.hpp"
#include <cmath>

static MeasureState gMeasure;

static float Distance(ImVec2 a, ImVec2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

void Measure_Update() {
    ImGuiIO& io = ImGui::GetIO();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        gMeasure.active = true;
        gMeasure.hasLast = true;
        gMeasure.start = io.MousePos;
        gMeasure.end = io.MousePos;
    }

    if (gMeasure.active) {
        gMeasure.end = io.MousePos;

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            gMeasure.active = false;
        }
    }
}

void Measure_Draw() {
    if (!gMeasure.hasLast)
        return;

    float dx = gMeasure.end.x - gMeasure.start.x;
    float dy = gMeasure.end.y - gMeasure.start.y;
    float dist = Distance(gMeasure.start, gMeasure.end);

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("MeasureHUD", nullptr, flags);
    ImGui::Text("Measurement (RMB)");
    ImGui::Separator();
    ImGui::Text("dx: %.1f px", dx);
    ImGui::Text("dy: %.1f px", dy);
    ImGui::Text("dist: %.1f px", dist);
    ImGui::End();

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddLine(gMeasure.start, gMeasure.end,
                IM_COL32(255, 255, 0, 255), 2.0f);
}
