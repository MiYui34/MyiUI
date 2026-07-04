#pragma once

#include "imgui.h"

namespace myiui::ui::hud {

void DrawGlassPanel(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius, const int accent[4], float alpha = 0.85f);

}  // namespace myiui::ui::hud
