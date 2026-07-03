#pragma once

#include "config/config_loader.h"
#include "ipc/shm_reader.h"

namespace myiui::ui::island {

void IslandRender(const ThemeConfig& theme, const ShmReader& shm, float viewportW, float viewportH, float dt);

} // namespace myiui::ui::island
