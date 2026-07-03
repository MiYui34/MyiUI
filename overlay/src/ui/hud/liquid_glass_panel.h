#pragma once

#include "config/config_loader.h"

#include "imgui.h"

namespace myiui::ui::hud {

enum class LiquidGlassMode { Flat, Tinted };

/** Enable LiquidGlassShader path for in-game glass borders (global per frame). */
void SetInGameLiquidGlassShader(bool enabled);

void DrawLiquidGlassPanel(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ThemeConfig& theme,
                          LiquidGlassMode mode, float alphaMul = 1.f, bool selectedBoost = false,
                          float cornerRadiusOverride = -1.f);

void DrawGradientBar(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float fill01, ImU32 left, ImU32 right,
                     float rounding);

} // namespace myiui::ui::hud
