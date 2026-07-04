#include "ui/theme/theme_runtime.h"

#include "config/config_loader.h"
#include "config/user_settings.h"
#include "ui/music/cover_loader.h"
#include "ui/theme/material_you.h"

#include <cstring>

namespace myiui::ui::theme {

namespace {

char g_coverUrl[128]{};
float g_reseedCooldown = 0.f;

void ApplyStaticPreset(AppConfig& cfg, int preset) {
    static const int kPresets[4][3] = {
        {90, 200, 250},
        {191, 90, 242},
        {48, 209, 88},
        {255, 214, 10},
    };
    preset = std::max(0, std::min(3, preset));
    MaterialYouPalette palette = BuildPaletteFromSeed({kPresets[preset][0], kPresets[preset][1], kPresets[preset][2]});
    ApplyPaletteToTheme(cfg.theme, palette);
    cfg.theme.blur_radius = static_cast<float>(myiui::config::GetUserSettingsConst().theme.blur_strength);
}

}  // namespace

void ThemeRuntimeInit() {}

void ThemeRuntimeSetCoverSeedUrl(const char* url) {
    if (!url) return;
    if (std::strncmp(g_coverUrl, url, sizeof(g_coverUrl) - 1) == 0) return;
    std::strncpy(g_coverUrl, url, sizeof(g_coverUrl) - 1);
    g_reseedCooldown = 0.f;
}

void ThemeRuntimeApplyPreset(AppConfig& cfg, int preset) {
    ApplyStaticPreset(cfg, preset);
}

void ThemeRuntimeTick(AppConfig& cfg, const myiui::config::UserSettings& settings, float dt) {
    g_reseedCooldown -= dt;
    if (!settings.theme.material_you) {
        ApplyStaticPreset(cfg, settings.theme.accent_preset);
        return;
    }
    if (g_coverUrl[0] && g_reseedCooldown <= 0.f) {
        int w = 0, h = 0;
        const uint8_t* rgba = myiui::ui::music::CoverLoaderSampleRgba(g_coverUrl, w, h);
        if (rgba && w > 0 && h > 0) {
            RgbColor seed = QuantizeDominantFromRgba(rgba, w, h);
            MaterialYouPalette palette = BuildPaletteFromSeed(seed);
            ApplyPaletteToTheme(cfg.theme, palette, settings.theme.ui_brightness);
            cfg.theme.blur_radius = static_cast<float>(settings.theme.blur_strength);
            g_reseedCooldown = 2.f;
            return;
        }
    }
    ApplyStaticPreset(cfg, settings.theme.accent_preset);
}

}  // namespace myiui::ui::theme
