#pragma once

#include "imgui.h"

#include "bridge/ui_state_types.h"
#include "config/config_loader.h"
#include "config/user_settings.h"

struct AppConfig;

namespace myiui::ui::hud {

void RenderNowPlaying(const AppConfig& cfg, const myiui::shared::MusicHudState& music,
                      const myiui::config::NowPlayingSettings& settings, float viewportW, float viewportH, float dt);

void RenderMiniWaveform(ImDrawList* dl, ImVec2 min, ImVec2 max, const float* bins, int count, const int accent[4],
                        float alpha);

}  // namespace myiui::ui::hud
