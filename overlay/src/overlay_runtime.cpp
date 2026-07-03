#include "overlay_runtime.h"

#include "config/config_loader.h"

#include <windows.h>

#include <fstream>

namespace myiui::overlay {

void OverlayLog(const wchar_t* message) {
    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) == 0) {
        return;
    }
    const std::wstring dir = std::wstring(localAppData) + L"\\MyiUI";
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::wstring logPath = dir + L"\\overlay.log";
    std::wofstream out(logPath, std::ios::app);
    if (out) {
        out << message << L"\n";
    }
}

std::wstring ResolveProjectRoot() {
    wchar_t env[1024]{};
    if (GetEnvironmentVariableW(L"MYIUI_ROOT", env, 1024) > 0) {
        return env;
    }

    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) > 0) {
        const std::wstring marker = std::wstring(localAppData) + L"\\MyiUI\\project_root.txt";
        std::wifstream in(marker);
        if (in) {
            std::wstring root;
            std::getline(in, root);
            if (!root.empty()) {
                return root;
            }
        }

        const std::wstring runtimeRoot = std::wstring(localAppData) + L"\\MyiUI\\runtime";
        const std::wstring theme = runtimeRoot + L"\\config\\menu\\theme.json";
        if (GetFileAttributesW(theme.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return runtimeRoot;
        }
    }

    wchar_t dllPath[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ResolveProjectRoot), &self);
    if (self && GetModuleFileNameW(self, dllPath, MAX_PATH) > 0) {
        std::wstring path(dllPath);
        const auto pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) path.resize(pos);
        if (path.ends_with(L"Release") || path.ends_with(L"Debug")) {
            path = path.substr(0, path.find_last_of(L"\\/"));
        }
        if (path.ends_with(L"overlay")) {
            path = path.substr(0, path.find_last_of(L"\\/"));
        }
        if (path.ends_with(L"build")) {
            path = path.substr(0, path.find_last_of(L"\\/"));
        }
        if (GetFileAttributesW((path + L"\\assets\\logos\\png").c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }

    return L"";
}

static void ApplyDefaultConfig(AppConfig& cfg) {
    cfg.root_path.clear();
    cfg.theme.blur_radius = 18.f;
    cfg.theme.corner_radius = 14.f;
    cfg.theme.glass_tint[0] = 12;
    cfg.theme.glass_tint[1] = 14;
    cfg.theme.glass_tint[2] = 22;
    cfg.theme.glass_tint[3] = 140;
    cfg.theme.border_color[0] = 255;
    cfg.theme.border_color[1] = 255;
    cfg.theme.border_color[2] = 255;
    cfg.theme.border_color[3] = 38;
    cfg.theme.text_primary[0] = 255;
    cfg.theme.text_primary[1] = 255;
    cfg.theme.text_primary[2] = 255;
    cfg.theme.text_primary[3] = 255;
    cfg.theme.text_secondary[0] = 180;
    cfg.theme.text_secondary[1] = 186;
    cfg.theme.text_secondary[2] = 198;
    cfg.theme.text_secondary[3] = 220;
    cfg.theme.accent[0] = 120;
    cfg.theme.accent[1] = 168;
    cfg.theme.accent[2] = 255;
    cfg.theme.accent[3] = 255;
    cfg.theme.hover_alpha_mul = 1.12f;
    cfg.theme.brand_size = 22.f;
    cfg.theme.nav_size = 15.f;
    cfg.theme.profile_name_size = 18.f;
    cfg.theme.profile_sub_size = 13.f;
    cfg.theme.font_regular = L"Alibaba PuHuiTi";
    cfg.theme.font_bold = L"Alibaba PuHuiTi SemiBold";
    cfg.layout.logo_x = 0.0167f;
    cfg.layout.logo_y = 0.022f;
    cfg.layout.manager_x = 0.983f;
    cfg.layout.manager_y = 0.022f;
    cfg.layout.manager_label = "Manager";
    cfg.layout.profile_x = 0.08f;
    cfg.layout.profile_y = 0.42f;
    cfg.layout.profile_w = 0.20f;
    cfg.layout.profile_h = 0.15f;
    cfg.layout.nav_x = 0.92f;
    cfg.layout.nav_y = 0.42f;
    cfg.layout.nav_gap = 12;
    cfg.layout.nav_button_w = 220.f;
    cfg.layout.nav_button_h = 44.f;
    cfg.layout.nav_items = {
        {"single", "Singleplayer", "OPEN_SINGLEPLAYER"},
        {"multi", "Multiplayer", "OPEN_MULTIPLAYER"},
        {"options", "Options", "OPEN_OPTIONS"},
        {"quit", "Quit Game", "QUIT"},
    };
    cfg.motion.hover_duration_ms = 180.f;
    cfg.motion.hover_scale = 1.03f;
    cfg.motion.hover_brightness = 1.08f;
    cfg.background.vignette_strength = 1.f;
    cfg.background.fallback_color[0] = 18;
    cfg.background.fallback_color[1] = 20;
    cfg.background.fallback_color[2] = 28;
    cfg.background.fallback_color[3] = 255;
    cfg.background.max_image_bytes = 15LL * 1024 * 1024;
    cfg.background.max_video_bytes = 80LL * 1024 * 1024;
}

bool LoadAppConfigWithFallback(AppConfig& out) {
    const std::wstring root = ResolveProjectRoot();
    if (!root.empty()) {
        out.root_path = root;
        if (LoadAppConfig(out)) {
            OverlayLog(L"Config loaded from project root.");
            return true;
        }
    }
    ApplyDefaultConfig(out);
    if (!root.empty()) {
        out.root_path = root;
    }
    OverlayLog(L"Config fallback: using built-in defaults.");
    return true;
}

}  // namespace myiui::overlay
