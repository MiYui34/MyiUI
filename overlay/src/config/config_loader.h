#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui/screen_id.h"

struct ThemeConfig {
    float blur_radius = 18.f;
    float corner_radius = 14.f;
    float profile_corner_radius = 20.f;
    float border_width = 2.f;
    float hover_alpha_mul = 1.12f;
    int glass_tint[4]{18, 22, 32, 165};
    int glass_tint_strong[4]{14, 18, 28, 210};
    int border_color[4]{255, 255, 255, 110};
    int border_accent[4]{90, 200, 250, 115};
    int text_primary[4]{245, 247, 250, 255};
    int text_secondary[4]{178, 184, 196, 255};
    int text_dim[4]{128, 134, 148, 255};
    int accent[4]{90, 200, 250, 255};
    int accent_fill[4]{90, 200, 250, 184};
    int accent_hover_bg[4]{90, 200, 250, 31};
    int danger[4]{220, 90, 70, 255};
    float brand_size = 22.f;
    float nav_size = 19.f;
    float profile_name_size = 20.f;
    float profile_sub_size = 14.f;
    float caption_size = 14.f;
    int nav_gap = 10;
    std::wstring font_regular = L"Alibaba PuHuiTi";
    std::wstring font_bold = L"Alibaba PuHuiTi SemiBold";
};

struct NavItem {
    std::string id;
    std::string label;
    std::string pipe;
    std::string style;
};

struct LayoutConfig {
    float logo_x = 0.025f;
    float logo_y = 0.03f;
    float manager_x = 0.975f;
    float manager_y = 0.03f;
    std::string manager_label = "Manager";
    std::string brand_label = "MyiUI";
    std::string version_label = "Fabric 1.21";
    float main_center_y = 0.5f;
    float cluster_gap = 48.f;
    float profile_card_w = 280.f;
    float profile_x = 0.08f;
    float profile_y = 0.42f;
    float profile_w = 0.20f;
    float profile_h = 0.15f;
    float nav_x = 0.92f;
    float nav_y = 0.42f;
    int nav_gap = 10;
    float nav_button_w = 320.f;
    float nav_button_h = 44.f;
    std::vector<NavItem> nav_items;
};

struct MotionConfig {
    float hover_duration_ms = 180.f;
    float hover_scale = 1.05f;
    float hover_brightness = 1.08f;
    float press_duration_ms = 120.f;
    float press_scale = 0.97f;
};

struct PageTransitionConfig {
    float duration_ms = 180.f;
    float reduce_motion_ms = 80.f;
    float slide_px = 0.f;
    bool crossfade = true;
};

struct ComponentsConfig {
    float content_panel_w = 720.f;
    float content_padding = 24.f;
    float shell_header_h = 56.f;
    float nav_row_h = 52.f;
    float setting_row_h = 56.f;
    float world_card_h = 72.f;
    float toast_duration_ms = 2800.f;
};

enum class SettingRowType { Toggle, Slider, Enum, Keybind, PackList };

struct SettingRowSpec {
    SettingRowType type = SettingRowType::Toggle;
    std::string key;
    std::string label;
    float min_val = 0.f;
    float max_val = 100.f;
    std::string default_val;
    std::vector<std::string> options;
};

struct OptionsScreenSpec {
    std::string category;
    std::string title;
    std::vector<SettingRowSpec> rows;
};

struct OptionsHubNavItem {
    std::string id;
    std::string label;
    ScreenId screen = ScreenId::OptionsVideo;
};

struct BackgroundConfig {
    float vignette_strength = 0.35f;
    int fallback_color[4]{18, 20, 28, 255};
    std::string default_image;
    int64_t max_image_bytes = 15LL * 1024 * 1024;
    int64_t max_video_bytes = 80LL * 1024 * 1024;
};

struct AppConfig {
    ThemeConfig theme;
    LayoutConfig layout;
    MotionConfig motion;
    PageTransitionConfig page_transition;
    ComponentsConfig components;
    BackgroundConfig background;
    std::wstring root_path;
};

bool LoadAppConfig(AppConfig& out);
bool LoadOptionsScreenSpec(const std::wstring& root, const char* jsonName, OptionsScreenSpec& out);
bool LoadOptionsHubNav(const std::wstring& root, std::vector<OptionsHubNavItem>& out);
