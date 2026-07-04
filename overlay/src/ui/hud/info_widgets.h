#pragma once

#include "bridge/ui_state_types.h"
#include "config/config_loader.h"
#include "config/user_settings.h"

struct AppConfig;

namespace myiui::ui::hud {

void RenderInfoWidgets(const AppConfig& cfg, const myiui::shared::InfoHudState& info,
                       const myiui::config::UserSettings& settings, float viewportW, float viewportH);

}  // namespace myiui::ui::hud
