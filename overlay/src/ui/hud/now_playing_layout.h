#pragma once

#include "config/user_settings.h"

#include "imgui.h"

namespace myiui::ui::hud {

constexpr float kNowPlayingScale = 2.f;

float NowPlayingEffectiveScale(const myiui::config::NowPlayingSettings& settings);
float NowPlayingCardWidth(const myiui::config::NowPlayingSettings& settings);
float NowPlayingCardHeight(const myiui::config::NowPlayingSettings& settings);
ImVec4 CalcNowPlayingBounds(const myiui::config::NowPlayingSettings& settings, float viewportW, float viewportH);

/** NowPlaying 卡片顶边 Y；无卡片时返回热键栏上方参考线。 */
float ComputeLyricsAnchorTop(const myiui::config::UserSettings& settings, float viewportW, float viewportH);

bool NowPlayingShouldShow(const myiui::config::NowPlayingSettings& settings, bool musicValid, bool playing,
                          bool paused);

}  // namespace myiui::ui::hud
