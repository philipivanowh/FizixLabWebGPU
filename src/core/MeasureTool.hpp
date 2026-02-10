#pragma once
#include <imgui.h>

struct MeasureState {
    bool active = false;
    bool hasLast = false;
    ImVec2 start{};
    ImVec2 end{};
};
