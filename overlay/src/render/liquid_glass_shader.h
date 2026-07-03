#pragma once

#include "config/config_loader.h"

#include "imgui.h"

namespace myiui::render {

/** Capture backbuffer + mipmap blur. Call once per in-game frame before glass draws. */
bool LiquidGlassBeginFrame(int screenW, int screenH);

/** Draw rounded liquid-glass panel (LiquidGlassShader V2 tinted). Screen coords, top-left origin. */
void LiquidGlassDrawPanel(const ImVec2& min, const ImVec2& max, const ThemeConfig& theme, float alphaMul,
                          bool selectedBoost, float cornerRadius);

bool LiquidGlassReady();
void LiquidGlassShutdown();

} // namespace myiui::render
