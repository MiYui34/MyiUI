#pragma once

#include "bridge/ui_state_types.h"
#include "config/config_loader.h"
#include "config/user_settings.h"

namespace myiui::ui::hud {

void RenderImmersiveLyrics(const AppConfig& cfg, const myiui::shared::MusicHudState& music,
                           const myiui::shared::IslandState& island, const myiui::config::UserSettings& settings,
                           float viewportW, float viewportH);

}  // namespace myiui::ui::hud
