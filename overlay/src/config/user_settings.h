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

struct NowPlayingSettings {
    bool enabled = true;
    InfoAnchor anchor = InfoAnchor::BottomLeft;
    float x = 16.f;
    float y = -16.f;
    bool show_waveform = true;
    float scale = 1.f;
    bool immersive_lyrics = false;
};

struct IslandSettings {
    bool visible = true;
    float scale = 3.f;
    float opacity = 0.5f;
    bool blur = false;
    bool show_fps = true;
    float offset_x = 0.f;
    float offset_y = 0.f;
};

struct UserSettings {
    int version = 1;
    ThemeSettings theme;
    NowPlayingSettings now_playing;
    IslandSettings island;
    bool hud_visible = true;
    bool chat_visible = true;
    bool layout_editor_enabled = false;
};

UserSettings& GetUserSettings();
const UserSettings& GetUserSettingsConst();

void UserSettingsLoad();
void UserSettingsSave();
void UserSettingsRequestSave();

void UserSettingsTick(float dt);

}  // namespace myiui::config
