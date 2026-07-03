#pragma once

#include "config/config_loader.h"

#include "imgui.h"

bool NavListRow(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, const AppConfig& cfg,
                float& hoverT, float scale);
