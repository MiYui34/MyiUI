#include "ui/ui_manager.h"

#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/media_library.h"
#include "ui/strings_zh.h"
#include "ui/ui_scale.h"
#include "ui/widgets/animated_toggle.h"
#include "ui/widgets/yc_slider.h"

#include <windows.h>
#include <shlobj.h>

#include <fstream>
#include <sstream>

#undef min
#undef max

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

float g_panelScale = 1.f;

std::wstring SettingsPath() {
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
    return json.find("true", colon) < json.find('}', colon);
}

float ParseFloatSetting(const std::string& json, const char* key, float fallback) {
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

int ParseIntSetting(const std::string& json, const char* key, int fallback) {
    return static_cast<int>(ParseFloatSetting(json, key, static_cast<float>(fallback)));
}

void DrawManagerSliderRow(const char* id, float* value, float minV, float maxV, float trackW, const int accent[4],
                          const char* valueFmt) {
    const auto sliderStyle = myiui::ui::YcSlider::StyleFromTheme(accent, 1.f);
    myiui::ui::YcSlider::Draw(id, value, minV, maxV, trackW, g_panelScale, sliderStyle);
    ImGui::SameLine(0.f, Px(8.f, g_panelScale));
    char buf[24]{};
    snprintf(buf, sizeof(buf), valueFmt, *value);
    ImGui::TextColored(ImVec4(0.70f, 0.72f, 0.77f, 1.f), "%s", buf);
}

bool DrawSettingRow(const char* title, const char* desc, float controlW, auto&& drawControl) {
    ImGui::PushID(title);
    const float rowH = Px(56.f, g_panelScale);
    const float rowW = ImGui::GetContentRegionAvail().x;
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    const ImVec2 rowEnd(rowStart.x + rowW, rowStart.y + rowH);
    auto* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(rowStart, rowEnd, IM_COL32(255, 255, 255, 8), Px(10.f, g_panelScale));
    dl->AddRect(rowStart, rowEnd, IM_COL32(255, 255, 255, 15), Px(10.f, g_panelScale), 0, 2.f);

    ImGui::BeginChild("##row_body", ImVec2(rowW, rowH), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(Px(16.f, g_panelScale), Px(10.f, g_panelScale)));
    ImGui::TextUnformatted(title);
    ImGui::SetCursorPos(ImVec2(Px(16.f, g_panelScale), Px(30.f, g_panelScale)));
    ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.f), "%s", desc);
    ImGui::SetCursorPos(ImVec2(rowW - controlW - Px(12.f, g_panelScale),
                               (rowH - Px(28.f, g_panelScale)) * 0.5f));
    drawControl();
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.f, Px(8.f, g_panelScale)));
    ImGui::PopID();
    return false;
}

}  // namespace

void UiManagerLoadSettings(UiManagerState& state) {
    std::ifstream in(SettingsPath());
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string json = ss.str();
    auto& s = state.settings;
    s.show_profile = ParseBool(json, "show_profile", s.show_profile);
    s.glass_enabled = ParseBool(json, "glass_enabled", s.glass_enabled);
    s.spring_anim = ParseBool(json, "spring_anim", s.spring_anim);
    s.hover_scale_enabled = ParseBool(json, "hover_scale_enabled", s.hover_scale_enabled);
    s.vignette_strength = ParseFloatSetting(json, "vignette_strength", s.vignette_strength);
    s.hover_scale = ParseFloatSetting(json, "hover_scale", s.hover_scale);
    s.accent_preset = ParseIntSetting(json, "accent_preset", s.accent_preset);
    s.anim_duration_ms = ParseIntSetting(json, "anim_duration_ms", s.anim_duration_ms);
    s.blur_strength = ParseIntSetting(json, "blur_strength", s.blur_strength);

    auto& a = state.anim;
    a.toggle_glass = s.glass_enabled ? 1.f : 0.f;
    a.toggle_profile = s.show_profile ? 1.f : 0.f;
    a.toggle_spring = s.spring_anim ? 1.f : 0.f;
    a.toggle_hover = s.hover_scale_enabled ? 1.f : 0.f;
    a.slider_blur = static_cast<float>(s.blur_strength - 18) / 6.f;
    a.slider_vignette = s.vignette_strength;
    a.slider_duration = static_cast<float>(s.anim_duration_ms - 180) / 300.f;
}

void UiManagerSaveSettings(const UiManagerState& state) {
    const std::wstring path = SettingsPath();
    CreateDirectoryW((path.substr(0, path.find_last_of(L'\\'))).c_str(), nullptr);
    std::ofstream out(path);
    if (!out) return;
    const auto& s = state.settings;
    out << "{\n"
        << "  \"show_profile\": " << (s.show_profile ? "true" : "false") << ",\n"
        << "  \"glass_enabled\": " << (s.glass_enabled ? "true" : "false") << ",\n"
        << "  \"spring_anim\": " << (s.spring_anim ? "true" : "false") << ",\n"
        << "  \"hover_scale_enabled\": " << (s.hover_scale_enabled ? "true" : "false") << ",\n"
        << "  \"vignette_strength\": " << s.vignette_strength << ",\n"
        << "  \"hover_scale\": " << s.hover_scale << ",\n"
        << "  \"accent_preset\": " << s.accent_preset << ",\n"
        << "  \"anim_duration_ms\": " << s.anim_duration_ms << ",\n"
        << "  \"blur_strength\": " << s.blur_strength << "\n"
        << "}\n";
}

void UiManagerApplyAccentPreset(AppConfig& cfg, int preset) {
    static const int kPresets[4][3] = {
        {90, 200, 250},
        {191, 90, 242},
        {48, 209, 88},
        {255, 214, 10},
    };
    const int idx = std::clamp(preset, 0, 3);
    const int r = kPresets[idx][0];
    const int g = kPresets[idx][1];
    const int b = kPresets[idx][2];

    auto fill = [&](int out[4], int alpha) {
        out[0] = r;
        out[1] = g;
        out[2] = b;
        out[3] = alpha;
    };
    fill(cfg.theme.accent, 255);
    fill(cfg.theme.accent_fill, 184);
    fill(cfg.theme.accent_hover_bg, 31);
    fill(cfg.theme.border_accent, 115);
}

void UiManagerRenderPanel(const AppConfig& cfg, UiManagerState& state, float uiScale, bool* open) {
    g_panelScale = uiScale;
    static bool settings_loaded = false;
    if (!settings_loaded) {
        UiManagerLoadSettings(state);
        settings_loaded = true;
    }

    MediaLibraryTick(state.library);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && open && *open) {
        *open = false;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 panelSize(Px(720.f, uiScale), Px(620.f, uiScale));
    ImVec2 panelPos(display.x * 0.5f - panelSize.x * 0.5f, display.y * 0.5f - panelSize.y * 0.5f);

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::SetNextWindowFocus();

    AppConfig liveCfg = cfg;
    UiManagerApplyAccentPreset(liveCfg, state.settings.accent_preset);
    const int* accent = liveCfg.theme.accent;
    auto& s = state.settings;
    auto& a = state.anim;

    const UiFonts& fonts = GetUiFonts();
    struct FontStackGuard {
        ImFont* font = nullptr;
        explicit FontStackGuard(ImFont* f) : font(f) {
            if (font) ImGui::PushFont(font);
        }
        ~FontStackGuard() {
            if (font) ImGui::PopFont();
        }
    } fontGuard(fonts.regular);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.10f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(20.f, uiScale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(24.f, uiScale), Px(20.f, uiScale)));

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("##UiManager", open, panelFlags)) {
        if (fonts.semibold) ImGui::PushFont(fonts.semibold);
        ImGui::TextUnformatted(myiui::strings::kUiManagerTitle);
        if (fonts.semibold) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - Px(88.f, uiScale));
        if (ImGui::Button("##ui_mgr_back", ImVec2(Px(80.f, uiScale), 0.f)) && open) {
            *open = false;
        }
        {
            const ImVec2 bmin = ImGui::GetItemRectMin();
            const ImVec2 bmax = ImGui::GetItemRectMax();
            const ImVec2 ts = ImGui::CalcTextSize(myiui::strings::kBack);
            ImGui::GetWindowDrawList()->AddText(
                ImVec2((bmin.x + bmax.x - ts.x) * 0.5f, (bmin.y + bmax.y - ts.y) * 0.5f),
                IM_COL32(245, 247, 252, 255), myiui::strings::kBack);
        }
        ImGui::TextColored(ImVec4(0.62f, 0.65f, 0.72f, 1.f), "%s", myiui::strings::kUiManagerSubtitle);

        ImGui::Dummy(ImVec2(0, Px(12.f, uiScale)));

        if (state.active_tab > 2) state.active_tab = 0;

        static const char* kTabs[] = {myiui::strings::kTabGeneral, myiui::strings::kTabBackground,
                                      myiui::strings::kTabMotion};
        const int tabCount = 3;
        const float tabW = (ImGui::GetContentRegionAvail().x - Px(8.f, uiScale) * (tabCount - 1)) / tabCount;
        for (int i = 0; i < tabCount; ++i) {
            ImGui::PushID(i);
            if (i > 0) ImGui::SameLine(0, Px(8.f, uiScale));
            const bool active = state.active_tab == i;
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent[0] / 255.f, accent[1] / 255.f, accent[2] / 255.f, 0.25f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(accent[0] / 255.f, accent[1] / 255.f, accent[2] / 255.f, 0.55f));
            }
            if (ImGui::Button("##tab", ImVec2(tabW, Px(34.f, uiScale)))) {
                state.active_tab = i;
            }
            if (active) ImGui::PopStyleColor(2);
            const ImVec2 tabMin = ImGui::GetItemRectMin();
            const ImVec2 tabMax = ImGui::GetItemRectMax();
            const ImVec2 ts = ImGui::CalcTextSize(kTabs[i]);
            ImGui::GetWindowDrawList()->AddText(
                ImVec2((tabMin.x + tabMax.x - ts.x) * 0.5f, (tabMin.y + tabMax.y - ts.y) * 0.5f),
                active ? IM_COL32(245, 247, 252, 255) : IM_COL32(178, 184, 196, 255), kTabs[i]);
            ImGui::PopID();
        }

        ImGui::Dummy(ImVec2(0, Px(12.f, uiScale)));
        ImGui::BeginChild("##TabBody", ImVec2(0, Px(470.f, uiScale)), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);

        const float controlW = Px(140.f, uiScale);
        const float sliderControlW = Px(196.f, uiScale);
        const float sliderTrackW = 108.f;

        switch (state.active_tab) {
            case 0:
                DrawSettingRow(myiui::strings::kGlassPanels, myiui::strings::kGlassPanelsDesc, controlW, [&]() {
                    myiui::ui::AnimatedToggle::Draw("glass", &s.glass_enabled, uiScale, accent, &a.toggle_glass);
                });
                DrawSettingRow(myiui::strings::kShowProfile, myiui::strings::kShowProfileDesc, controlW, [&]() {
                    myiui::ui::AnimatedToggle::Draw("profile", &s.show_profile, uiScale, accent, &a.toggle_profile);
                });
                DrawSettingRow(myiui::strings::kBlurStrength, myiui::strings::kBlurStrengthDesc, sliderControlW,
                               [&]() {
                                   float blur = static_cast<float>(s.blur_strength);
                                   DrawManagerSliderRow("blur", &blur, 18.f, 24.f, sliderTrackW, accent, "%.0fpx");
                                   s.blur_strength = static_cast<int>(std::lround(blur));
                               });
                liveCfg.theme.blur_radius = static_cast<float>(s.blur_strength);
                break;
            case 1:
                ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.f), "%s", myiui::strings::kMediaFormats);
                ImGui::Dummy(ImVec2(0, Px(6.f, uiScale)));
                MediaLibraryRenderGrid(liveCfg, state.library, uiScale, Px(360.f, uiScale));
                if (state.library.toast_timer > 0.f) {
                    state.library.toast_timer -= ImGui::GetIO().DeltaTime;
                    ImGui::TextColored(ImVec4(1.f, 0.85f, 0.45f, 1.f), "%s", state.library.toast_message.c_str());
                }
                DrawSettingRow(myiui::strings::kOverlayOpacity, myiui::strings::kOverlayOpacityDesc, sliderControlW,
                               [&]() {
                                   float pct = s.vignette_strength * 100.f;
                                   DrawManagerSliderRow("vig", &pct, 0.f, 100.f, sliderTrackW, accent, "%.0f%%");
                                   s.vignette_strength = pct / 100.f;
                               });
                break;
            case 2:
                DrawSettingRow(myiui::strings::kSpringAnim, myiui::strings::kSpringAnimDesc, controlW, [&]() {
                    myiui::ui::AnimatedToggle::Draw("spring", &s.spring_anim, uiScale, accent, &a.toggle_spring);
                });
                DrawSettingRow(myiui::strings::kHoverScale, myiui::strings::kHoverScaleDesc, controlW, [&]() {
                    myiui::ui::AnimatedToggle::Draw("hov", &s.hover_scale_enabled, uiScale, accent, &a.toggle_hover);
                });
                DrawSettingRow(myiui::strings::kAnimDuration, myiui::strings::kAnimDurationDesc, sliderControlW,
                               [&]() {
                                   float dur = static_cast<float>(s.anim_duration_ms);
                                   DrawManagerSliderRow("dur", &dur, 180.f, 480.f, sliderTrackW, accent, "%.0fms");
                                   s.anim_duration_ms = static_cast<int>(std::lround(dur));
                               });
                break;
            default:
                break;
        }

        ImGui::EndChild();
        UiManagerSaveSettings(state);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
