#pragma once

#include "config/config_loader.h"
#include "ipc/shm_reader.h"

#include "imgui.h"

namespace myiui::ui::island {

void IslandRender(const ThemeConfig& theme, const ShmReader& shm, float viewportW, float viewportH, float dt);

ImVec4 CalcIslandIdleBounds(float viewportW, float viewportH);

}  // namespace myiui::ui::island
