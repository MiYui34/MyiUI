#pragma once

#include "config/config_loader.h"

#include "imgui.h"

namespace myiui::ui {

ImU32 ColorFromRGBA(const int rgba[4], float alphaMul = 1.f);

void DrawGlassSurface(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const int tint[4], const int border[4],
                      float rounding, float borderWidth, float alphaMul = 1.f);

void DrawGlassRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ThemeConfig& theme, float alphaMul = 1.f);

} // namespace myiui::ui
