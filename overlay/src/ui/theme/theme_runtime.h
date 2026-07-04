#pragma once

struct AppConfig;

namespace myiui::config {
struct UserSettings;
}

namespace myiui::ui::theme {

void ThemeRuntimeInit();
void ThemeRuntimeTick(AppConfig& cfg, const myiui::config::UserSettings& settings, float dt);
void ThemeRuntimeApplyPreset(AppConfig& cfg, int preset);
void ThemeRuntimeSetCoverSeedUrl(const char* url);

}  // namespace myiui::ui::theme
