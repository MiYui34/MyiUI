#pragma once

#include "config/config_loader.h"
#include "ui/media_library.h"

#include "imgui.h"

struct UiSettings {
    bool show_profile = true;
    bool glass_enabled = true;
    bool spring_anim = true;
    bool hover_scale_enabled = true;
    float vignette_strength = 0.55f;
    float hover_scale = 1.05f;
    int accent_preset = 0;
    int anim_duration_ms = 320;
    int blur_strength = 20;
};

struct UiManagerAnim {
    float toggle_glass = 1.f;
    float toggle_profile = 1.f;
    float toggle_spring = 1.f;
    float toggle_hover = 1.f;
    float slider_blur = 0.25f;
    float slider_vignette = 0.55f;
    float slider_duration = 0.5f;
};

struct UiManagerState {
    int active_tab = 1;
    MediaLibraryState library;
    UiSettings settings;
    UiManagerAnim anim;
};

void UiManagerLoadSettings(UiManagerState& state);
void UiManagerSaveSettings(const UiManagerState& state);
void UiManagerApplyAccentPreset(AppConfig& cfg, int preset);
void UiManagerRenderPanel(const AppConfig& cfg, UiManagerState& state, float uiScale, bool* open);
