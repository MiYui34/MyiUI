#pragma once

#include "config/config_loader.h"
#include "ipc/shm_reader.h"

#include "imgui.h"

namespace myiui::ui::hud {

void HudRender(const ThemeConfig& theme, const ShmReader& shm, float viewportW, float viewportH);

} // namespace myiui::ui::hud
