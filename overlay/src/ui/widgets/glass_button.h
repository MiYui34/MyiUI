#pragma once

#include "config/config_loader.h"
#include "ui/menu_icons.h"

#include "imgui.h"

enum class GlassButtonStyle { Default, Primary, Danger };

bool GlassButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, const AppConfig& cfg,
                 float& hoverT, bool& pressed, ImFont* font, GlassButtonStyle style, MenuIcon icon, float scale,
                 bool enabled = true);
