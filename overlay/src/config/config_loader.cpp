#include "config/config_loader.h"

#include <fstream>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <regex>
#include <sstream>

static std::wstring GetMyiuiRoot() {
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
        if (GetFileAttributesW((runtimeRoot + L"\\config\\menu\\theme.json").c_str()) != INVALID_FILE_ATTRIBUTES) {
            return runtimeRoot;
        }
    }

    wchar_t dllPath[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&GetMyiuiRoot), &self);
    GetModuleFileNameW(self, dllPath, MAX_PATH);
    std::wstring path(dllPath);
    const auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    // overlay/build -> project root
    if (path.ends_with(L"Release") || path.ends_with(L"Debug")) {
        path = path.substr(0, path.find_last_of(L"\\/"));
    }
    if (path.ends_with(L"overlay")) {
        path = path.substr(0, path.find_last_of(L"\\/"));
    }
    return path;
}

static std::string ReadFileUtf8(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static float ParseFloat(const std::string& json, const std::string& key, float fallback) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9.+-]+)");
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 1) {
        return std::stof(m[1].str());
    }
    return fallback;
}

static int ParseInt(const std::string& json, const std::string& key, int fallback) {
    return static_cast<int>(ParseFloat(json, key, static_cast<float>(fallback)));
}

static std::string ParseString(const std::string& json, const std::string& key, const std::string& fallback) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 1) {
        return m[1].str();
    }
    return fallback;
}

static void ParseColorArray(const std::string& json, const std::string& key, int out[4]) {
    std::regex re("\"" + key + "\"\\s*:\\s*\\[\\s*([0-9]+)\\s*,\\s*([0-9]+)\\s*,\\s*([0-9]+)\\s*,\\s*([0-9]+)\\s*\\]");
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 4) {
        for (int i = 0; i < 4; ++i) out[i] = std::stoi(m[i + 1].str());
    }
}

static void ParseNavItems(const std::string& json, LayoutConfig& layout) {
    layout.nav_items.clear();
    const std::regex itemRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"label\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"pipe\"\\s*:\\s*\"([^\"]*)\"(?:\\s*,\\s*\"style\"\\s*:\\s*\"([^\"]*)\")?\\s*\\}");
    auto begin = std::sregex_iterator(json.begin(), json.end(), itemRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        NavItem item;
        item.id = (*it)[1].str();
        item.label = (*it)[2].str();
        item.pipe = (*it)[3].str();
        item.style = (*it)[4].matched ? (*it)[4].str() : "";
        layout.nav_items.push_back(std::move(item));
    }
    if (layout.nav_items.empty()) {
        layout.nav_items = {
            {"single", "\xe5\x8d\x95\xe4\xba\xba\xe6\xb8\xb8\xe6\x88\x8f", "OPEN_SINGLEPLAYER", "primary"},
            {"multi", "\xe5\xa4\x9a\xe4\xba\xba\xe6\xb8\xb8\xe6\x88\x8f", "OPEN_MULTIPLAYER", "default"},
            {"options", "\xe9\x80\x89\xe9\xa1\xb9", "OPEN_OPTIONS", "default"},
            {"quit", "\xe9\x80\x80\xe5\x87\xba\xe6\xb8\xb8\xe6\x88\x8f", "QUIT", "danger"},
        };
    }
}

bool LoadAppConfig(AppConfig& out) {
    out.root_path = GetMyiuiRoot();
    const std::wstring themePath = out.root_path + L"\\config\\menu\\theme.json";
    const std::wstring layoutDesign = out.root_path + L"\\design\\v1\\layout.json";
    const std::wstring layoutRuntime = out.root_path + L"\\config\\menu\\layout.json";
    const std::wstring layoutPick = (GetFileAttributesW(layoutDesign.c_str()) != INVALID_FILE_ATTRIBUTES)
                                        ? layoutDesign
                                        : layoutRuntime;
    const std::wstring motionPath = out.root_path + L"\\design\\v1\\motion.json";
    const std::wstring bgPath = out.root_path + L"\\config\\menu\\background.json";

    const std::string themeJson = ReadFileUtf8(themePath);
    const std::string layoutJson = ReadFileUtf8(layoutPick);
    const std::string motionJson = ReadFileUtf8(motionPath);
    const std::string bgJson = ReadFileUtf8(bgPath);

    if (themeJson.empty() || layoutJson.empty()) return false;

    out.theme.blur_radius = ParseFloat(themeJson, "blur_radius", out.theme.blur_radius);
    out.theme.corner_radius = ParseFloat(themeJson, "corner_radius", out.theme.corner_radius);
    ParseColorArray(themeJson, "background_tint", out.theme.glass_tint);
    ParseColorArray(themeJson, "glass_strong_tint", out.theme.glass_tint_strong);
    ParseColorArray(themeJson, "border_color", out.theme.border_color);
    ParseColorArray(themeJson, "border_accent", out.theme.border_accent);
    ParseColorArray(themeJson, "text_primary", out.theme.text_primary);
    ParseColorArray(themeJson, "text_secondary", out.theme.text_secondary);
    ParseColorArray(themeJson, "text_dim", out.theme.text_dim);
    ParseColorArray(themeJson, "accent", out.theme.accent);
    ParseColorArray(themeJson, "accent_fill", out.theme.accent_fill);
    ParseColorArray(themeJson, "accent_hover_bg", out.theme.accent_hover_bg);
    ParseColorArray(themeJson, "danger", out.theme.danger);
    out.theme.border_width = ParseFloat(themeJson, "border_width", out.theme.border_width);
    out.theme.profile_corner_radius = ParseFloat(themeJson, "profile_corner_radius", out.theme.profile_corner_radius);
    out.theme.brand_size = ParseFloat(themeJson, "brand_size", out.theme.brand_size);
    out.theme.nav_size = ParseFloat(themeJson, "nav_size", out.theme.nav_size);
    out.theme.profile_name_size = ParseFloat(themeJson, "profile_name_size", out.theme.profile_name_size);
    out.theme.profile_sub_size = ParseFloat(themeJson, "profile_sub_size", out.theme.profile_sub_size);
    out.theme.caption_size = ParseFloat(themeJson, "caption_size", out.theme.caption_size);
    out.theme.hover_alpha_mul = ParseFloat(themeJson, "hover_alpha_mul", out.theme.hover_alpha_mul);

    const std::string fontRegular = ParseString(themeJson, "font_regular", "Alibaba PuHuiTi");
    const std::string fontBold = ParseString(themeJson, "font_bold", "Alibaba PuHuiTi SemiBold");
    out.theme.font_regular.assign(fontRegular.begin(), fontRegular.end());
    out.theme.font_bold.assign(fontBold.begin(), fontBold.end());

    auto parseSection = [&](const std::string& section, const std::string& key, float fallback) {
        const auto pos = layoutJson.find(section);
        if (pos == std::string::npos) return fallback;
        return ParseFloat(layoutJson.substr(pos), key, fallback);
    };

    out.layout.logo_x = parseSection("\"logo_bar\"", "x", 0.0167f);
    out.layout.logo_y = parseSection("\"logo_bar\"", "y", 0.022f);
    out.layout.manager_x = parseSection("\"manager\"", "x", 0.983f);
    out.layout.manager_y = parseSection("\"manager\"", "y", 0.022f);
    out.layout.manager_label = ParseString(layoutJson, "label", out.layout.manager_label);
    out.layout.brand_label = ParseString(layoutJson, "brand_label", out.layout.brand_label);
    out.layout.version_label = ParseString(layoutJson, "version_label", out.layout.version_label);
    out.layout.main_center_y = parseSection("\"main_cluster\"", "center_y", out.layout.main_center_y);
    out.layout.cluster_gap = parseSection("\"main_cluster\"", "gap", out.layout.cluster_gap);
    out.layout.profile_card_w = parseSection("\"main_cluster\"", "profile_w", out.layout.profile_card_w);
    out.layout.nav_button_w = parseSection("\"main_cluster\"", "menu_w", out.layout.nav_button_w);
    out.layout.profile_x = parseSection("\"profile_card\"", "x", 0.08f);
    out.layout.profile_y = parseSection("\"profile_card\"", "y", 0.42f);
    out.layout.profile_w = parseSection("\"profile_card\"", "w", 0.20f);
    out.layout.profile_h = parseSection("\"profile_card\"", "h", 0.15f);
    out.layout.nav_x = parseSection("\"nav_stack\"", "x", 0.92f);
    out.layout.nav_y = parseSection("\"nav_stack\"", "y", 0.42f);
    out.layout.nav_gap = static_cast<int>(parseSection("\"nav_stack\"", "gap", 12.f));
    out.layout.nav_button_w = parseSection("\"nav_stack\"", "button_w", 220.f);
    out.layout.nav_button_h = parseSection("\"nav_stack\"", "button_h", 40.f);
    ParseNavItems(layoutJson, out.layout);

    if (!motionJson.empty()) {
        const auto hoverPos = motionJson.find("\"hover\"");
        const auto pressPos = motionJson.find("\"press\"");
        if (hoverPos != std::string::npos) {
            const std::string hoverSection = motionJson.substr(hoverPos);
            out.motion.hover_duration_ms = ParseFloat(hoverSection, "duration_ms", out.motion.hover_duration_ms);
            out.motion.hover_scale = ParseFloat(hoverSection, "scale", out.motion.hover_scale);
            out.motion.hover_brightness = ParseFloat(hoverSection, "brightness_mul", out.motion.hover_brightness);
        }
        if (pressPos != std::string::npos) {
            const std::string pressSection = motionJson.substr(pressPos);
            out.motion.press_duration_ms = ParseFloat(pressSection, "duration_ms", out.motion.press_duration_ms);
            out.motion.press_scale = ParseFloat(pressSection, "scale", out.motion.press_scale);
        }
        const auto pagePos = motionJson.find("\"page_transition\"");
        if (pagePos != std::string::npos) {
            const std::string pageSection = motionJson.substr(pagePos);
            out.page_transition.duration_ms = ParseFloat(pageSection, "duration_ms", out.page_transition.duration_ms);
            out.page_transition.reduce_motion_ms =
                ParseFloat(pageSection, "reduce_motion_ms", out.page_transition.reduce_motion_ms);
            out.page_transition.slide_px = ParseFloat(pageSection, "slide_px", out.page_transition.slide_px);
        }
    }

    const std::wstring componentsPath = out.root_path + L"\\design\\v1\\components.json";
    const std::string componentsJson = ReadFileUtf8(componentsPath);
    if (!componentsJson.empty()) {
        const auto panelPos = componentsJson.find("\"content_panel\"");
        if (panelPos != std::string::npos) {
            const std::string sec = componentsJson.substr(panelPos);
            out.components.content_panel_w = ParseFloat(sec, "width", out.components.content_panel_w);
            out.components.content_padding = ParseFloat(sec, "padding", out.components.content_padding);
        }
        const auto shellPos = componentsJson.find("\"screenShell\"");
        const auto shellPosLegacy = componentsJson.find("\"screen_shell\"");
        const size_t shellAt = shellPos != std::string::npos ? shellPos : shellPosLegacy;
        if (shellAt != std::string::npos) {
            const std::string shellSec = componentsJson.substr(shellAt);
            out.components.content_panel_w =
                ParseFloat(shellSec, "contentPanelWidth", out.components.content_panel_w);
            out.components.content_padding =
                ParseFloat(shellSec, "contentPanelPadding", out.components.content_padding);
            out.components.shell_header_h =
                ParseFloat(shellSec, "topBarHeight", out.components.shell_header_h);
            if (out.components.shell_header_h <= 0.f) {
                out.components.shell_header_h =
                    ParseFloat(shellSec, "header_height", out.components.shell_header_h);
            }
        }
        const auto navPos = componentsJson.find("\"navList\"");
        const auto navPosLegacy = componentsJson.find("\"nav_list\"");
        const size_t navAt = navPos != std::string::npos ? navPos : navPosLegacy;
        if (navAt != std::string::npos) {
            const std::string navSec = componentsJson.substr(navAt);
            out.components.nav_row_h = ParseFloat(navSec, "itemMinHeight", out.components.nav_row_h);
            if (out.components.nav_row_h <= 0.f) {
                out.components.nav_row_h = ParseFloat(navSec, "row_height", out.components.nav_row_h);
            }
        }
        const auto toastPos = componentsJson.find("\"toast\"");
        if (toastPos != std::string::npos) {
            out.components.toast_duration_ms =
                ParseFloat(componentsJson.substr(toastPos), "duration_ms", out.components.toast_duration_ms);
        }
    }

    if (!bgJson.empty()) {
        out.background.vignette_strength = ParseFloat(bgJson, "vignette_strength", out.background.vignette_strength);
        ParseColorArray(bgJson, "fallback_color", out.background.fallback_color);
        out.background.default_image = ParseString(bgJson, "default_image", out.background.default_image);
        const float maxImageMb = ParseFloat(bgJson, "max_image_mb", 15.f);
        const float maxVideoMb = ParseFloat(bgJson, "max_video_mb", 80.f);
        out.background.max_image_bytes = static_cast<int64_t>(maxImageMb * 1024.f * 1024.f);
        out.background.max_video_bytes = static_cast<int64_t>(maxVideoMb * 1024.f * 1024.f);
    }

    return true;
}

static SettingRowType ParseRowType(const std::string& type) {
    if (type == "slider") return SettingRowType::Slider;
    if (type == "enum") return SettingRowType::Enum;
    if (type == "keybind") return SettingRowType::Keybind;
    if (type == "pack_list") return SettingRowType::PackList;
    return SettingRowType::Toggle;
}

static void ApplyRowChunkDefaults(SettingRowSpec& row, const std::string& chunk) {
    row.min_val = ParseFloat(chunk, "min", row.min_val);
    row.max_val = ParseFloat(chunk, "max", row.max_val);
    const std::regex defaultRe("\"default\"\\s*:\\s*(\"([^\"]*)\"|(true|false)|([0-9.+-]+))");
    std::smatch m;
    if (std::regex_search(chunk, m, defaultRe)) {
        if (m[2].matched) {
            row.default_val = m[2].str();
        } else if (m[3].matched) {
            row.default_val = m[3].str();
        } else if (m[4].matched) {
            row.default_val = m[4].str();
        }
    }
    row.options.clear();
    const std::regex optionsRe("\"options\"\\s*:\\s*\\[([^\\]]*)\\]");
    if (std::regex_search(chunk, m, optionsRe) && m[1].matched) {
        const std::string optionsBody = m[1].str();
        const std::regex optRe("\"([^\"]*)\"");
        for (auto it = std::sregex_iterator(optionsBody.begin(), optionsBody.end(), optRe);
             it != std::sregex_iterator(); ++it) {
            row.options.push_back((*it)[1].str());
        }
    }
}

static std::wstring ScreenJsonPath(const std::wstring& root, const char* jsonName) {
    return root + L"\\design\\v1\\screens\\" + std::wstring(jsonName, jsonName + strlen(jsonName)) + L".json";
}

static std::string ReadScreenJson(const std::wstring& root, const char* jsonName) {
    std::string json = ReadFileUtf8(ScreenJsonPath(root, jsonName));
    if (!json.empty()) return json;

    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) > 0) {
        const std::wstring runtimeRoot = std::wstring(localAppData) + L"\\MyiUI\\runtime";
        json = ReadFileUtf8(ScreenJsonPath(runtimeRoot, jsonName));
        if (!json.empty()) return json;
    }
    return {};
}

static ScreenId ScreenIdFromToken(const std::string& token) {
    if (token == "OptionsVideo" || token == "options-video.html" || token == "options_video")
        return ScreenId::OptionsVideo;
    if (token == "OptionsSound" || token == "options-sound.html" || token == "options_sound")
        return ScreenId::OptionsSound;
    if (token == "OptionsControls" || token == "options-controls.html" || token == "options_controls")
        return ScreenId::OptionsControls;
    if (token == "OptionsLanguage" || token == "options-language.html" || token == "options_language")
        return ScreenId::OptionsLanguage;
    if (token == "OptionsChat" || token == "options-chat.html" || token == "options_chat")
        return ScreenId::OptionsChat;
    if (token == "OptionsAccessibility" || token == "options-accessibility.html" ||
        token == "options_accessibility")
        return ScreenId::OptionsAccessibility;
    if (token == "OptionsSkin" || token == "options-skin.html" || token == "options_skin")
        return ScreenId::OptionsSkin;
    if (token == "OptionsResourcePacks" || token == "options-resource-packs.html" ||
        token == "options_resource_packs")
        return ScreenId::OptionsResourcePacks;
    return ScreenId::OptionsHub;
}

bool LoadOptionsScreenSpec(const std::wstring& root, const char* jsonName, OptionsScreenSpec& out) {
    const std::string json = ReadScreenJson(root, jsonName);
    if (json.empty()) return false;

    out.category = ParseString(json, "category", out.category);
    out.title = ParseString(json, "title", out.title);
    if (out.category.empty()) {
        out.category = ParseString(json, "id", out.category);
    }
    out.rows.clear();

    const std::regex rowKeyRe(
        "\\{\\s*\"type\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"key\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"label\"\\s*:\\s*\"([^\"]*)\"");
    const std::regex rowIdRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"type\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"label\"\\s*:\\s*\"([^\"]*)\"");
    auto begin = std::sregex_iterator(json.begin(), json.end(), rowKeyRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        SettingRowSpec row;
        row.type = ParseRowType((*it)[1].str());
        row.key = (*it)[2].str();
        row.label = (*it)[3].str();
        const auto keyPos = json.find("\"key\": \"" + row.key + "\"");
        if (keyPos == std::string::npos) {
            const auto keyPos2 = json.find("\"key\":\"" + row.key + "\"");
            if (keyPos2 != std::string::npos) {
                ApplyRowChunkDefaults(row, json.substr(keyPos2, 512));
            }
        } else {
            ApplyRowChunkDefaults(row, json.substr(keyPos, 512));
        }
        out.rows.push_back(std::move(row));
    }
    if (out.rows.empty()) {
        begin = std::sregex_iterator(json.begin(), json.end(), rowIdRe);
        for (auto it = begin; it != end; ++it) {
            SettingRowSpec row;
            row.key = (*it)[1].str();
            row.type = ParseRowType((*it)[2].str());
            row.label = (*it)[3].str();
            const auto idPos = json.find("\"id\": \"" + row.key + "\"");
            if (idPos == std::string::npos) {
                const auto idPos2 = json.find("\"id\":\"" + row.key + "\"");
                if (idPos2 != std::string::npos) {
                    ApplyRowChunkDefaults(row, json.substr(idPos2, 512));
                }
            } else {
                ApplyRowChunkDefaults(row, json.substr(idPos, 512));
            }
            out.rows.push_back(std::move(row));
        }
    }
    return !out.rows.empty() || !out.title.empty();
}

bool LoadOptionsHubNav(const std::wstring& root, std::vector<OptionsHubNavItem>& out) {
    const std::string json = ReadScreenJson(root, "options_hub");
    if (json.empty()) return false;

    out.clear();
    const std::regex navScreenRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"label\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"screen\"\\s*:\\s*\"([^\"]*)\"\\s*\\}");
    const std::regex navTargetRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"label\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"target\"\\s*:\\s*\"([^\"]*)\"");
    auto begin = std::sregex_iterator(json.begin(), json.end(), navScreenRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        OptionsHubNavItem item;
        item.id = (*it)[1].str();
        item.label = (*it)[2].str();
        item.screen = ScreenIdFromToken((*it)[3].str());
        out.push_back(std::move(item));
    }
    if (out.empty()) {
        begin = std::sregex_iterator(json.begin(), json.end(), navTargetRe);
        for (auto it = begin; it != end; ++it) {
            OptionsHubNavItem item;
            item.id = (*it)[1].str();
            item.label = (*it)[2].str();
            item.screen = ScreenIdFromToken((*it)[3].str());
            out.push_back(std::move(item));
        }
    }
    if (out.empty()) {
        out = {
            {"options_video", "\xe8\xa7\x86\xe9\xa2\x91\xe8\xae\xbe\xe7\xbd\xae", ScreenId::OptionsVideo},
            {"options_sound", "\xe9\x9f\xb3\xe4\xb9\x90\xe5\x92\x8c\xe5\xa3\xb0\xe9\x9f\xb3", ScreenId::OptionsSound},
            {"options_controls", "\xe6\x8e\xa7\xe5\x88\xb6", ScreenId::OptionsControls},
            {"options_language", "\xe8\xaf\xad\xe8\xa8\x80", ScreenId::OptionsLanguage},
            {"options_chat", "\xe8\x81\x8a\xe5\xa4\xa9", ScreenId::OptionsChat},
            {"options_accessibility", "\xe6\x97\xa0\xe9\x9a\x9c\xe7\xa2\x8d", ScreenId::OptionsAccessibility},
            {"options_skin", "\xe7\x9a\xae\xe8\x82\xa4\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89", ScreenId::OptionsSkin},
            {"options_resource_packs", "\xe8\xb5\x84\xe6\xba\x90\xe5\x8c\x85", ScreenId::OptionsResourcePacks},
        };
    }
    return !out.empty();
}
