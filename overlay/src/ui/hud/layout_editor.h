#pragma once

#include "config/config_loader.h"
#include "ipc/shm_reader.h"

namespace myiui::ui::hud {

bool LayoutEditorActive();
bool LayoutEditorConsumesInput();
void LayoutEditorRender(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH);

}  // namespace myiui::ui::hud
