#include "config/user_settings.h"

#include "ipc/pipe_client.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace myiui::config {

namespace {

UserSettings g_settings{};
float g_saveDelay = 0.f;
bool g_dirty = false;

std::wstring SettingsPath() {
    wchar_t appData[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData);
    return std::wstring(appData) + L"\\MyiUI\\user_settings.json";
}

std::wstring LegacyUiSettingsPath() {
    wchar_t appData[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData);
    return std::wstring(appData) + L"\\MyiUI\\ui_settings.json";
}

bool ParseBool(const std::string& json, const char* key, bool fallback) {
    const std::string needle = std::string("\"") + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    const auto colon = json.find(':', pos);
    if (colon == std::string::npos) return fallback;
    const auto end = json.find_first_of(",}\n", colon);
    const std::string slice = json.substr(colon + 1, end == std::string::npos ? std::string::npos : end - colon - 1);
    if (slice.find("true") != std::string::npos) return true;
    if (slice.find("false") != std::string::npos) return false;
    return fallback;
}

float ParseFloat(const std::string& json, const char* key, float fallback) {
    const std::string needle = std::string("\"") + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    const auto colon = json.find(':', pos);
    if (colon == std::string::npos) return fallback;
    try {
        return std::stof(json.substr(colon + 1));
    } catch (...) {
        return fallback;
    }
}

int ParseInt(const std::string& json, const char* key, int fallback) {
    return static_cast<int>(ParseFloat(json, key, static_cast<float>(fallback)));
}

InfoAnchor ParseAnchor(const std::string& json, const char* section, InfoAnchor fallback) {
    const std::string sec = std::string("\"") + section + "\"";
    const auto secPos = json.find(sec);
    if (secPos == std::string::npos) return fallback;
    const auto anchorPos = json.find("\"anchor\"", secPos);
    if (anchorPos == std::string::npos || anchorPos > json.find('}', secPos)) return fallback;
    if (json.find("top_right", anchorPos) != std::string::npos) return InfoAnchor::TopRight;
    if (json.find("bottom_left", anchorPos) != std::string::npos) return InfoAnchor::BottomLeft;
    if (json.find("bottom_right", anchorPos) != std::string::npos) return InfoAnchor::BottomRight;
    return InfoAnchor::TopLeft;
}

void ParseInfoWidget(const std::string& json, const char* section, InfoWidgetSettings& out) {
    const std::string sec = std::string("\"") + section + "\"";
    const auto secPos = json.find(sec);
    if (secPos == std::string::npos) return;
    const auto end = json.find('}', secPos);
    const std::string block = json.substr(secPos, end == std::string::npos ? std::string::npos : end - secPos);
    out.enabled = ParseBool(block, "enabled", out.enabled);
    out.x = ParseFloat(block, "x", out.x);
    out.y = ParseFloat(block, "y", out.y);
    out.anchor = ParseAnchor(json, section, out.anchor);
}

void MigrateLegacyUiSettings() {
    std::ifstream in(LegacyUiSettingsPath());
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string json = ss.str();
    g_settings.theme.show_profile = ParseBool(json, "show_profile", g_settings.theme.show_profile);
    g_settings.theme.glass_enabled = ParseBool(json, "glass_enabled", g_settings.theme.glass_enabled);
    g_settings.theme.spring_anim = ParseBool(json, "spring_enabled", g_settings.theme.spring_anim);
    g_settings.theme.hover_scale_enabled = ParseBool(json, "hover_scale_enabled", g_settings.theme.hover_scale_enabled);
    g_settings.theme.vignette_strength = ParseFloat(json, "vignette_strength", g_settings.theme.vignette_strength);
    g_settings.theme.hover_scale = ParseFloat(json, "hover_scale", g_settings.theme.hover_scale);
    g_settings.theme.accent_preset = ParseInt(json, "accent_preset", g_settings.theme.accent_preset);
    g_settings.theme.anim_duration_ms = ParseInt(json, "anim_duration_ms", g_settings.theme.anim_duration_ms);
    g_settings.theme.blur_strength = ParseInt(json, "blur_strength", g_settings.theme.blur_strength);
}

const char* AnchorName(InfoAnchor a) {
    switch (a) {
        case InfoAnchor::TopRight: return "top_right";
        case InfoAnchor::BottomLeft: return "bottom_left";
        case InfoAnchor::BottomRight: return "bottom_right";
        default: return "top_left";
    }
}

void WriteInfoWidget(std::ostringstream& out, const char* key, const InfoWidgetSettings& w) {
    out << "    \"" << key << "\": { \"enabled\": " << (w.enabled ? "true" : "false")
        << ", \"anchor\": \"" << AnchorName(w.anchor) << "\", \"x\": " << w.x << ", \"y\": " << w.y << " },\n";
}

void SyncAgentUiFlags() {
    const auto& s = g_settings;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "UI_FLAGS:chat=%d", s.chat_visible ? 1 : 0);
    PipeSendCommandAsync(buf);
}

}  // namespace

UserSettings& GetUserSettings() { return g_settings; }
const UserSettings& GetUserSettingsConst() { return g_settings; }

void UserSettingsLoad() {
    g_settings = UserSettings{};
    g_settings.info_fps.enabled = false;
    std::ifstream in(SettingsPath());
    if (!in) {
        MigrateLegacyUiSettings();
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string json = ss.str();
    if (json.empty()) {
        MigrateLegacyUiSettings();
        return;
    }

    g_settings.version = ParseInt(json, "version", 1);
    g_settings.theme.accent_preset = ParseInt(json, "accent_preset", g_settings.theme.accent_preset);
    if (json.find("\"theme\"") != std::string::npos) {
        g_settings.theme.accent_preset = ParseInt(json, "accent_preset", g_settings.theme.accent_preset);
        const auto tPos = json.find("\"theme\"");
        const std::string tBlock = json.substr(tPos, 400);
        g_settings.theme.material_you = ParseBool(tBlock, "material_you", g_settings.theme.material_you);
        g_settings.theme.blur_strength = ParseInt(tBlock, "blur_strength", g_settings.theme.blur_strength);
        g_settings.theme.ui_brightness = ParseFloat(tBlock, "ui_brightness", g_settings.theme.ui_brightness);
    }
    g_settings.theme.material_you = ParseBool(json, "material_you", g_settings.theme.material_you);

    ParseInfoWidget(json, "info_coords", g_settings.info_coords);
    ParseInfoWidget(json, "info_ping", g_settings.info_ping);
    ParseInfoWidget(json, "info_speed", g_settings.info_speed);
    ParseInfoWidget(json, "info_fps", g_settings.info_fps);
    ParseInfoWidget(json, "info_fps", g_settings.info_fps);

    const auto npPos = json.find("\"now_playing\"");
    if (npPos != std::string::npos) {
        const std::string block = json.substr(npPos, 320);
        g_settings.now_playing.enabled = ParseBool(block, "enabled", true);
        g_settings.now_playing.show_waveform = ParseBool(block, "show_waveform", true);
        g_settings.now_playing.x = ParseFloat(block, "x", g_settings.now_playing.x);
        g_settings.now_playing.y = ParseFloat(block, "y", g_settings.now_playing.y);
        g_settings.now_playing.anchor = ParseAnchor(json, "now_playing", g_settings.now_playing.anchor);
    }

    const auto iPos = json.find("\"island\"");
    if (iPos != std::string::npos) {
        const std::string block = json.substr(iPos, 256);
        g_settings.island.visible = ParseBool(block, "visible", true);
        g_settings.island.scale = ParseFloat(block, "scale", 3.f);
        g_settings.island.opacity = ParseFloat(block, "opacity", 0.5f);
        g_settings.island.blur = ParseBool(block, "blur", false);
        g_settings.island.show_fps = ParseBool(block, "show_fps", true);
    }

    g_settings.hud_visible = ParseBool(json, "visible", ParseBool(json, "hud_visible", true));
    if (json.find("\"hud\"") != std::string::npos) {
        const auto hPos = json.find("\"hud\"");
        g_settings.hud_visible = ParseBool(json.substr(hPos, 64), "visible", g_settings.hud_visible);
    }
    if (json.find("\"chat\"") != std::string::npos) {
        const auto cPos = json.find("\"chat\"");
        g_settings.chat_visible = ParseBool(json.substr(cPos, 64), "visible", g_settings.chat_visible);
    }

    MigrateLegacyUiSettings();
    SyncAgentUiFlags();
}

void UserSettingsSave() {
    CreateDirectoryW(([] {
        wchar_t appData[MAX_PATH]{};
        SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData);
        return std::wstring(appData) + L"\\MyiUI";
    })().c_str(), nullptr);

    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"theme\": {\n";
    out << "    \"accent_preset\": " << g_settings.theme.accent_preset << ",\n";
    out << "    \"material_you\": " << (g_settings.theme.material_you ? "true" : "false") << ",\n";
    out << "    \"blur_strength\": " << g_settings.theme.blur_strength << ",\n";
    out << "    \"ui_brightness\": " << g_settings.theme.ui_brightness << "\n";
    out << "  },\n";
    out << "  \"modules\": {\n";
    WriteInfoWidget(out, "info_coords", g_settings.info_coords);
    WriteInfoWidget(out, "info_ping", g_settings.info_ping);
    WriteInfoWidget(out, "info_speed", g_settings.info_speed);
    WriteInfoWidget(out, "info_fps", g_settings.info_fps);
    WriteInfoWidget(out, "info_fps", g_settings.info_fps);
    out << "    \"now_playing\": { \"enabled\": " << (g_settings.now_playing.enabled ? "true" : "false")
        << ", \"anchor\": \"" << AnchorName(g_settings.now_playing.anchor) << "\", \"x\": "
        << g_settings.now_playing.x << ", \"y\": " << g_settings.now_playing.y
        << ", \"show_waveform\": " << (g_settings.now_playing.show_waveform ? "true" : "false") << " }\n";
    out << "  },\n";
    out << "  \"island\": { \"visible\": " << (g_settings.island.visible ? "true" : "false")
        << ", \"scale\": " << g_settings.island.scale << ", \"opacity\": " << g_settings.island.opacity
        << ", \"blur\": " << (g_settings.island.blur ? "true" : "false")
        << ", \"show_fps\": " << (g_settings.island.show_fps ? "true" : "false") << " },\n";
    out << "  \"hud\": { \"visible\": " << (g_settings.hud_visible ? "true" : "false") << " },\n";
    out << "  \"chat\": { \"visible\": " << (g_settings.chat_visible ? "true" : "false") << " }\n";
    out << "}\n";

    std::ofstream file(SettingsPath(), std::ios::trunc);
    if (file) file << out.str();
    g_dirty = false;
    SyncAgentUiFlags();
}

void UserSettingsRequestSave() { g_dirty = true; g_saveDelay = 0.3f; }

void UserSettingsTick(float dt) {
    if (!g_dirty) return;
    g_saveDelay -= dt;
    if (g_saveDelay <= 0.f) UserSettingsSave();
}

}  // namespace myiui::config
