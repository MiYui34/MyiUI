#pragma once

#include <string>

namespace myiui::config {

enum class InfoAnchor : int {
    TopLeft = 0,
    TopRight = 1,
    BottomLeft = 2,
    BottomRight = 3,
};

struct ThemeSettings {
    int accent_preset = 0;
    bool material_you = true;
    bool show_profile = true;
    bool glass_enabled = true;
    bool spring_anim = true;
    bool hover_scale_enabled = true;
    float vignette_strength = 0.55f;
    float hover_scale = 1.05f;
    int anim_duration_ms = 320;
    int blur_strength = 20;
    float ui_brightness = 1.f;
};

struct InfoWidgetSettings {
    bool enabled = true;
    InfoAnchor anchor = InfoAnchor::TopLeft;
    float x = 8.f;
    float y = 8.f;
};

struct NowPlayingSettings {
    bool enabled = true;
    InfoAnchor anchor = InfoAnchor::BottomLeft;
    float x = 8.f;
    float y = -120.f;
    bool show_waveform = true;
};

struct IslandSettings {
    bool visible = true;
    float scale = 3.f;
    float opacity = 0.5f;
    bool blur = false;
    bool show_fps = true;
};

struct UserSettings {
    int version = 1;
    ThemeSettings theme;
    InfoWidgetSettings info_coords;
    InfoWidgetSettings info_ping;
    InfoWidgetSettings info_speed;
    InfoWidgetSettings info_fps;
    NowPlayingSettings now_playing;
    IslandSettings island;
    bool hud_visible = true;
    bool chat_visible = true;
};

UserSettings& GetUserSettings();
const UserSettings& GetUserSettingsConst();

void UserSettingsLoad();
void UserSettingsSave();
void UserSettingsRequestSave();

void UserSettingsTick(float dt);

}  // namespace myiui::config
