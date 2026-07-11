#include "ui/clickgui/clickgui.h"

#include "config/user_settings.h"
#include "ui/fonts.h"
#include "ui/island/island_tokens.h"
#include "ui/logo_assets.h"
#include "ui/music/music_panel.h"

#include "imgui.h"

#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace myiui::ui::clickgui {

namespace {

// ── 简约毛玻璃 (Minimal Glass) Design tokens ──
int g_accR = 64, g_accG = 194, g_accB = 255;
constexpr int kGlassBgBase[4]   = { 12, 12, 16, 120 };
constexpr int kGlassBgTint[4]   = { 255, 255, 255, 8 };
constexpr int kGlassBorder[4]   = { 255, 255, 255, 25 };
int g_glassBorderAcc[4]         = { 64, 194, 255, 80 };

constexpr int kTextPrimary[4]   = { 250, 250, 250, 255 };
constexpr int kTextSecondary[4] = { 180, 180, 185, 255 };
constexpr int kTextDim[4]       = { 110, 110, 115, 255 };

constexpr float kRadiusLg = 32.f;
constexpr float kRadiusMd = 24.f;
constexpr float kRadiusSm = 16.f;

ImU32 RGBA(const int c[4], float aMul = 1.f) {
    return IM_COL32(c[0], c[1], c[2], static_cast<int>(std::min(255.f, c[3] * aMul)));
}

ImU32 AccCol(int alpha) { return IM_COL32(g_accR, g_accG, g_accB, alpha); }

// ── State ──
bool g_open = false;
float g_openAnim = 0.f;

int g_activeCategory = 0;
char g_searchBuf[256] = {};
float g_catHover[8] = {};

float LerpF(float t, float a, float b) { return a + t * (b - a); }
float EaseOutCubic(float t) { return 1.f - std::pow(1.f - t, 3.f); }
float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

ImFont* GetFont() { return GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont(); }
float TextW(ImFont* f, float sz, const char* t) { return f ? f->CalcTextSizeA(sz, FLT_MAX, 0.f, t).x : 0.f; }

// ── Item definitions ──
enum class ItemType { Toggle, Slider };

struct ModuleItem {
    const char* name;
    const char* desc;
    ItemType type;
    bool* boolValue;
    float* floatValue;
    float floatMin, floatMax;
    const char* suffix;
    bool hasPopup;
    int parent;   // -1 = top-level
    
    // [FIX]: 动画状态与具体Item绑定，防止筛选/折叠时发生状态错位
    float animHover = 0.f;
    float animToggle = 0.f;
};

ModuleItem g_hudItems[] = {
    {"灵动岛",   "显示灵动岛",         ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, true,  -1, 0.f, 0.f},
    {"FPS显示",  "在灵动岛显示FPS",    ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, 0, 0.f, 0.f},
    {"缩放",     "灵动岛大小",         ItemType::Slider, nullptr, nullptr,       1.f, 6.f, "x", false, 0, 0.f, 0.f},
    {"透明度",   "灵动岛不透明度",     ItemType::Slider, nullptr, nullptr,       0.1f, 0.9f, "", false, 0, 0.f, 0.f},
    {"背景模糊", "灵动岛毛玻璃效果",   ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, 0, 0.f, 0.f},
    {"HUD显示",  "显示游戏HUD",        ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, -1, 0.f, 0.f},
    {"聊天显示", "显示游戏聊天",       ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, -1, 0.f, 0.f},
    {"布局编辑器", "拖动缩放HUD组件",  ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, -1, 0.f, 0.f},
};

ModuleItem g_infoItems[] = {
    {"NowPlaying", "音乐播放卡片",     ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, -1, 0.f, 0.f},
    {"波形",     "音乐波形动画",       ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, 0, 0.f, 0.f},
    {"沉浸歌词", "HUD上方卡拉OK歌词",  ItemType::Toggle, nullptr, nullptr,       0, 0, nullptr, false, -1, 0.f, 0.f},
};

ModuleItem g_visualItems[] = {
    {"亮度", "游戏画面亮度", ItemType::Slider, nullptr, nullptr, 0.3f, 1.5f, "x", false, -1, 0.f, 0.f},
    {"Material You", "封面取色主题", ItemType::Toggle, nullptr, nullptr, 0, 0, nullptr, false, -1, 0.f, 0.f},
    {"Web面板", "Ultralight HTML 悬浮窗", ItemType::Toggle, nullptr, nullptr, 0, 0, nullptr, false, -1, 0.f, 0.f},
};

struct Category {
    const char* name;
    const char* icon;
    ModuleItem* items;
    int itemCount;
    bool isMusic;   // 音乐分类走自定义渲染
};

Category g_categories[] = {
    {"HUD", "H", g_hudItems, 8, false},
    {"信息", "I", g_infoItems, 3, false},
    {"视觉", "V", g_visualItems, 3, false},
    {"音乐", "M", nullptr, 0, true},
};
constexpr int kCategoryCount = 4;

void BindSettingsPointers() {
    auto& s = myiui::config::GetUserSettings();
    g_hudItems[0].boolValue = &s.island.visible;
    g_hudItems[1].boolValue = &s.island.show_fps;
    g_hudItems[2].floatValue = &s.island.scale;
    g_hudItems[3].floatValue = &s.island.opacity;
    g_hudItems[4].boolValue = &s.island.blur;
    g_hudItems[5].boolValue = &s.hud_visible;
    g_hudItems[6].boolValue = &s.chat_visible;
    g_hudItems[7].boolValue = &s.layout_editor_enabled;
    g_infoItems[0].boolValue = &s.now_playing.enabled;
    g_infoItems[1].boolValue = &s.now_playing.show_waveform;
    g_infoItems[2].boolValue = &s.now_playing.immersive_lyrics;
    g_visualItems[0].floatValue = &s.theme.ui_brightness;
    g_visualItems[1].boolValue = &s.theme.material_you;
    g_visualItems[2].boolValue = &s.web_panel_enabled;
}

// ── 核心毛玻璃渲染 ──
void DrawGlass(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius, float alpha, bool accent = false) {
    dl->AddRectFilled(min, max, RGBA(kGlassBgBase, alpha), radius);
    const int tintAlpha = accent ? 25 : kGlassBgTint[3];
    dl->AddRectFilled(min, max, IM_COL32(255, 255, 255, static_cast<int>(tintAlpha * alpha)), radius);
    const int* bc = accent ? g_glassBorderAcc : kGlassBorder;
    dl->AddRect(min, max, RGBA(bc, alpha), radius, 0, 1.0f);
    if (!accent) {
        ImVec2 topEdgeMax = ImVec2(max.x, min.y + radius + 1.f);
        dl->AddRect(min, topEdgeMax, IM_COL32(255, 255, 255, static_cast<int>(15 * alpha)), radius, ImDrawFlags_RoundCornersTop, 1.0f);
    }
}

// ── 精致化开关 ──
void DrawToggle(ImDrawList* dl, ImVec2 pos, float size, bool on, float animVal, float alpha) {
    const float w = size * 1.8f;
    const float h = size * 0.9f;
    const float r = h * 0.5f;
    ImU32 trackCol = on ? AccCol(static_cast<int>(230 * alpha))
                        : IM_COL32(255, 255, 255, static_cast<int>(20 * alpha));
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), trackCol, r);
    dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), IM_COL32(0,0,0, static_cast<int>(40 * alpha)), r, 0, 1.0f);

    const float pad = 4.0f;
    const float knobR = r - pad;
    const float knobX = pos.x + r + (w - 2.f * r) * animVal;
    ImU32 knobCol = on ? IM_COL32(255, 255, 255, static_cast<int>(255 * alpha))
                       : IM_COL32(220, 220, 225, static_cast<int>(255 * alpha));
    dl->AddCircleFilled(ImVec2(knobX, pos.y + r), knobR, knobCol, 24);
}

// ── 简约模块卡片 ──
void DrawCard(const char* id, ModuleItem& item, float alpha, float dt, ImVec2 cMin, ImVec2 cMax) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = GetFont();

    ImGui::SetCursorScreenPos(cMin);
    ImGui::InvisibleButton(id, ImVec2(cMax.x - cMin.x, cMax.y - cMin.y));
    bool hovered = ImGui::IsItemHovered();
    bool lClick = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool rClick = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    bool active = ImGui::IsItemActive();
    
    item.animHover = LerpF(std::min(1.f, dt * 10.f), item.animHover, hovered ? 1.f : 0.f);

    bool isOn = false;
    if (item.type == ItemType::Toggle && item.boolValue) {
        isOn = *item.boolValue;
        item.animToggle = LerpF(std::min(1.f, dt * 15.f), item.animToggle, isOn ? 1.f : 0.f);
    }

    DrawGlass(dl, cMin, cMax, kRadiusMd, alpha * (0.8f + 0.2f * item.animHover), isOn);

    const float pad = 32.f;
    const float nameSize = 32.f;
    const float descSize = 24.f;

    ImU32 nameCol = isOn ? RGBA(kTextPrimary, alpha) : RGBA(kTextSecondary, alpha);
    dl->AddText(font, nameSize, ImVec2(cMin.x + pad, cMin.y + 20.f), nameCol, item.name);

    if (item.type == ItemType::Toggle) {
        if (item.desc) {
            dl->AddText(font, descSize, ImVec2(cMin.x + pad, cMin.y + 54.f),
                        RGBA(kTextDim, alpha * 0.9f), item.desc);
        }
        const float tSz = 36.f;
        const float tX = cMax.x - (tSz * 1.8f) - pad;
        const float tY = cMin.y + ((cMax.y - cMin.y) - tSz * 0.9f) * 0.5f;
        DrawToggle(dl, ImVec2(tX, tY), tSz, isOn, item.animToggle, alpha);

        if (item.hasPopup && hovered) {
            dl->AddText(font, 24.f, ImVec2(tX - 120.f, tY + 2.f),
                        AccCol(static_cast<int>(255 * alpha * item.animHover)), "设置");
        }
        if (lClick && item.boolValue) {
            *item.boolValue = !*item.boolValue;
            myiui::config::UserSettingsRequestSave();
        }
        if (rClick && item.hasPopup) ImGui::OpenPopup("##settings_popup");

    } else if (item.type == ItemType::Slider && item.floatValue) {
        char valBuf[64];
        if (item.suffix && item.suffix[0]) snprintf(valBuf, sizeof(valBuf), "%.1f%s", *item.floatValue, item.suffix);
        else if (item.floatMax <= 1.5f) snprintf(valBuf, sizeof(valBuf), "%.0f%%", *item.floatValue * 100);
        else snprintf(valBuf, sizeof(valBuf), "%.1f", *item.floatValue);
        
        float valW = TextW(font, nameSize, valBuf);
        dl->AddText(font, nameSize, ImVec2(cMax.x - valW - pad, cMin.y + 28.f), RGBA(g_glassBorderAcc, alpha), valBuf);

        const float sH = 8.f;
        const float sR = sH * 0.5f;
        const float sX = cMin.x + pad;
        const float sW = (cMax.x - cMin.x) - 2.f * pad;
        const float sY = cMax.y - 36.f;
        
        dl->AddRectFilled(ImVec2(sX, sY), ImVec2(sX + sW, sY + sH), IM_COL32(255, 255, 255, static_cast<int>(20 * alpha)), sR);
        float pct = ClampF((*item.floatValue - item.floatMin) / (item.floatMax - item.floatMin), 0.f, 1.f);
        dl->AddRectFilled(ImVec2(sX, sY), ImVec2(sX + sW * pct, sY + sH), AccCol(static_cast<int>(230 * alpha)), sR);
        dl->AddCircleFilled(ImVec2(sX + sW * pct, sY + sR), 12.f, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 24);
        
        // [FIX]: 移除 IsMouseDragging，允许点击轨道直接跳转位置
        if (active) {
            float mp = ClampF((ImGui::GetMousePos().x - sX) / sW, 0.f, 1.f);
            *item.floatValue = item.floatMin + (item.floatMax - item.floatMin) * mp;
            myiui::config::UserSettingsRequestSave();
        }
    }
}

// ── 子项行 ──
void DrawChildRow(const char* id, ModuleItem& item, float alpha, float dt, ImVec2 cMin, ImVec2 cMax) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = GetFont();

    ImGui::SetCursorScreenPos(cMin);
    ImGui::InvisibleButton(id, ImVec2(cMax.x - cMin.x, cMax.y - cMin.y));
    bool hovered = ImGui::IsItemHovered();
    bool lClick = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool active = ImGui::IsItemActive();
    
    item.animHover = LerpF(std::min(1.f, dt * 12.f), item.animHover, hovered ? 1.f : 0.f);

    bool isOn = false;
    if (item.type == ItemType::Toggle && item.boolValue) {
        isOn = *item.boolValue;
        item.animToggle = LerpF(std::min(1.f, dt * 15.f), item.animToggle, isOn ? 1.f : 0.f);
    }

    // 子项背景 — 更淡，半宽
    const float bgAlpha = 0.5f + 0.5f * item.animHover;
    dl->AddRectFilled(cMin, cMax, IM_COL32(255, 255, 255, static_cast<int>(10 * alpha * bgAlpha)), kRadiusSm);
    if (item.animHover > 0.01f) {
        dl->AddRect(cMin, cMax, AccCol(static_cast<int>(40 * alpha * item.animHover)), kRadiusSm, 0, 1.f);
    }

    const float pad = 28.f;
    const float nameSize = 28.f;
    ImU32 nameCol = isOn ? RGBA(kTextPrimary, alpha) : RGBA(kTextSecondary, alpha * 0.9f);
    dl->AddText(font, nameSize, ImVec2(cMin.x + pad, cMin.y + (cMax.y - cMin.y - nameSize) * 0.5f), nameCol, item.name);

    if (item.type == ItemType::Toggle) {
        const float tSz = 30.f;
        const float tX = cMax.x - (tSz * 1.8f) - pad;
        const float tY = cMin.y + ((cMax.y - cMin.y) - tSz * 0.9f) * 0.5f;
        DrawToggle(dl, ImVec2(tX, tY), tSz, isOn, item.animToggle, alpha);
        if (lClick && item.boolValue) {
            *item.boolValue = !*item.boolValue;
            myiui::config::UserSettingsRequestSave();
        }

    } else if (item.type == ItemType::Slider && item.floatValue) {
        char valBuf[64];
        if (item.suffix && item.suffix[0]) snprintf(valBuf, sizeof(valBuf), "%.1f%s", *item.floatValue, item.suffix);
        else if (item.floatMax <= 1.5f) snprintf(valBuf, sizeof(valBuf), "%.0f%%", *item.floatValue * 100);
        else snprintf(valBuf, sizeof(valBuf), "%.1f", *item.floatValue);
        
        float valW = TextW(font, 26.f, valBuf);
        dl->AddText(font, 26.f, ImVec2(cMax.x - valW - pad, cMin.y + (cMax.y - cMin.y - 26.f) * 0.5f), RGBA(g_glassBorderAcc, alpha), valBuf);

        const float sH = 6.f;
        const float sR = sH * 0.5f;
        const float nameW = TextW(font, nameSize, item.name);
        const float sX = cMin.x + pad + nameW + 32.f;
        const float sW = (cMax.x - valW - pad - 32.f) - sX;
        const float sY = cMin.y + (cMax.y - cMin.y - sH) * 0.5f;
        
        if (sW > 60.f) {
            dl->AddRectFilled(ImVec2(sX, sY), ImVec2(sX + sW, sY + sH), IM_COL32(255, 255, 255, static_cast<int>(20 * alpha)), sR);
            float pct = ClampF((*item.floatValue - item.floatMin) / (item.floatMax - item.floatMin), 0.f, 1.f);
            dl->AddRectFilled(ImVec2(sX, sY), ImVec2(sX + sW * pct, sY + sH), AccCol(static_cast<int>(230 * alpha)), sR);
            dl->AddCircleFilled(ImVec2(sX + sW * pct, sY + sR), 10.f, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), 20);
            
            if (active) {
                float mp = ClampF((ImGui::GetMousePos().x - sX) / sW, 0.f, 1.f);
                *item.floatValue = item.floatMin + (item.floatMax - item.floatMin) * mp;
            }
        }
    }
}

// ── Settings popup ──
void DrawSettingsPopup(float alpha) {
    ImFont* font = GetFont();
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(12, 12, 16, 230));
    ImGui::PushStyleColor(ImGuiCol_Border, RGBA(kGlassBorder, alpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(48, 40));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRadiusLg);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(16, 32));

    if (ImGui::BeginPopup("##settings_popup")) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 tp = ImGui::GetCursorScreenPos();
        dl->AddText(font, 36.f, tp, RGBA(kTextPrimary, alpha), "灵动岛 · 高级");
        ImGui::Dummy(ImVec2(480, 48));

        ImVec2 sp = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sp.x - 24, sp.y), ImVec2(sp.x + 480, sp.y), RGBA(kGlassBorder, alpha * 0.3f), 1.f);
        ImGui::Dummy(ImVec2(1, 8));

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, alpha * 0.8f), "基础设置请在灵动岛子项中调整");
        ImGui::Dummy(ImVec2(1, 12));
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.92f, alpha), "重置为默认");
        ImGui::SameLine(360);
        if (ImGui::SmallButton("重置")) {
            auto& s = myiui::config::GetUserSettings().island;
            s.scale = 3.f;
            s.opacity = 0.5f;
            s.blur = false;
            s.show_fps = true;
            myiui::config::UserSettingsRequestSave();
        }

        ImGui::Dummy(ImVec2(1, 4));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, alpha * 0.6f), "点击外部区域关闭");
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// ── Sidebar ──
void DrawSidebar(ImVec2 min, ImVec2 max, float alpha, float dt) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = GetFont();

    DrawGlass(dl, min, max, kRadiusLg, alpha, false);

    const float logoY = min.y + 48.f;
    // MyiUI logo (使用真实 logo 纹理，回退到字母 M)
    const LogoSet& logos = GetLogos();
    const ImVec2 logoMin(min.x + 28.f, logoY);
    const ImVec2 logoMax(min.x + 100.f, logoY + 72.f);
    if (logos.mark.valid()) {
        DrawLogoFit(dl, logos.mark, logoMin, logoMax, alpha);
    } else {
        dl->AddCircleFilled(ImVec2(min.x + 64.f, logoY + 36.f), 36.f,
                            AccCol(static_cast<int>(255 * alpha)), 32);
        dl->AddText(font, 40.f, ImVec2(min.x + 50.f, logoY + 16.f),
                    IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), "M");
    }
    dl->AddText(font, 44.f, ImVec2(min.x + 124.f, logoY + 8.f), RGBA(kTextPrimary, alpha), "MyiUI");
    dl->AddText(font, 24.f, ImVec2(min.x + 124.f, logoY + 56.f), RGBA(kTextDim, alpha), "v2 JVMTI");

    dl->AddLine(ImVec2(min.x + 48.f, logoY + 128.f), ImVec2(max.x - 48.f, logoY + 128.f), RGBA(kGlassBorder, alpha * 0.3f), 1.f);

    const float catStartY = logoY + 144.f;
    const float catH = 80.f;
    const float catPad = 24.f; 

    for (int i = 0; i < kCategoryCount; ++i) {
        const float cy = catStartY + i * (catH + 4.f);
        const ImVec2 cMin(min.x + catPad, cy);
        const ImVec2 cMax(max.x - catPad, cy + catH);
        const bool act = (g_activeCategory == i);

        float hT = (act || (ImGui::IsMouseHoveringRect(cMin, cMax) && g_open)) ? 1.f : 0.f;
        g_catHover[i] = LerpF(std::min(1.f, dt * 15.f), g_catHover[i], hT);

        if (g_catHover[i] > 0.01f) {
            ImU32 bg = act ? AccCol(static_cast<int>(30 * alpha * g_catHover[i]))
                           : IM_COL32(255, 255, 255, static_cast<int>(12 * alpha * g_catHover[i]));
            dl->AddRectFilled(cMin, cMax, bg, kRadiusSm);
        }

        ImU32 iconCol = act ? AccCol(static_cast<int>(255 * alpha)) : RGBA(kTextDim, alpha);
        dl->AddText(font, 32.f, ImVec2(cMin.x + 36.f, cMin.y + catH * 0.5f - 16.f), iconCol, g_categories[i].icon);

        ImU32 nc = act ? RGBA(kTextPrimary, alpha) : RGBA(kTextSecondary, alpha);
        dl->AddText(font, 32.f, ImVec2(cMin.x + 88.f, cMin.y + (catH - 32.f) * 0.5f), nc, g_categories[i].name);

        ImGui::SetCursorScreenPos(cMin);
        ImGui::InvisibleButton(("##cat_" + std::to_string(i)).c_str(), ImVec2(cMax.x - cMin.x, catH));
        if (ImGui::IsItemClicked()) g_activeCategory = i;
    }

    const float profY = max.y - 140.f;
    dl->AddLine(ImVec2(min.x + 48.f, profY - 24.f), ImVec2(max.x - 48.f, profY - 24.f), RGBA(kGlassBorder, alpha * 0.3f), 1.f);
    dl->AddCircleFilled(ImVec2(min.x + 64.f, profY + 36.f), 32.f, IM_COL32(255, 255, 255, static_cast<int>(20 * alpha)), 32);
    dl->AddText(font, 32.f, ImVec2(min.x + 54.f, profY + 20.f), IM_COL32(255, 255, 255, static_cast<int>(220 * alpha)), "U");
    dl->AddText(font, 30.f, ImVec2(min.x + 116.f, profY + 16.f), RGBA(kTextPrimary, alpha * 0.9f), "Local User");
    dl->AddText(font, 24.f, ImVec2(min.x + 116.f, profY + 52.f), RGBA(kTextDim, alpha * 0.8f), "Online");
}

void ApplyAccentInternal(const int accent[4]) {
    if (!accent) return;
    g_accR = accent[0];
    g_accG = accent[1];
    g_accB = accent[2];
    g_glassBorderAcc[0] = accent[0];
    g_glassBorderAcc[1] = accent[1];
    g_glassBorderAcc[2] = accent[2];
}

}  // namespace

void SyncTheme(const int accent[4]) { ApplyAccentInternal(accent); }

void Toggle() { g_open = !g_open; }
bool IsOpen() { return g_open; }

bool HandleKey(int key, int, int action) {
    if (key == 340 && action == 1) { Toggle(); return true; }
    if (g_open && key == 256 && action == 1) { g_open = false; return true; }
    return false;
}

bool IslandVisible() { return myiui::config::GetUserSettingsConst().island.visible; }
float IslandScale() { return myiui::config::GetUserSettingsConst().island.scale; }
bool IslandBlur() { return myiui::config::GetUserSettingsConst().island.blur; }
float IslandOpacity() { return myiui::config::GetUserSettingsConst().island.opacity; }
bool ShowFps() { return myiui::config::GetUserSettingsConst().island.show_fps; }
bool HudVisible() { return myiui::config::GetUserSettingsConst().hud_visible; }
bool ChatVisible() { return myiui::config::GetUserSettingsConst().chat_visible; }
bool WebPanelVisible() { return myiui::config::GetUserSettingsConst().web_panel_enabled; }

bool g_suppressEscUp = false;
void RequestSuppressEscUp() { g_suppressEscUp = true; }
bool ConsumeSuppressEscUp() {
    if (g_suppressEscUp) { g_suppressEscUp = false; return true; }
    return false;
}

void Render(float viewportW, float viewportH, float dt) {
    BindSettingsPointers();
    if (!g_open && g_openAnim < 0.01f) return;

    // GUI 刚打开时清除鼠标按下状态，防止游戏中的鼠标按下残留导致首次点击无效
    static bool s_wasOpen = false;
    static int s_openClearFrames = 0;
    if (g_open && !s_wasOpen) {
        s_openClearFrames = 3;
    }
    s_wasOpen = g_open;
    if (s_openClearFrames > 0) {
        ImGui::GetIO().MouseDown[0] = false;
        ImGui::GetIO().MouseDown[1] = false;
        s_openClearFrames--;
    }

    float target = g_open ? 1.f : 0.f;
    g_openAnim = LerpF(std::min(1.f, dt * 10.f), g_openAnim, target);
    if (!g_open && g_openAnim < 0.01f) { g_openAnim = 0.f; return; }

    float alpha = EaseOutCubic(g_openAnim);
    ImFont* font = GetFont();

    ImDrawList* bgDl = ImGui::GetBackgroundDrawList();
    bgDl->AddRectFilled(ImVec2(0, 0), ImVec2(viewportW, viewportH), IM_COL32(0, 0, 0, static_cast<int>(160 * alpha)));

    const float guiW = std::min(viewportW - 80.f, 1520.f);
    const float guiH = std::min(viewportH - 80.f, 1040.f);
    const float guiX = (viewportW - guiW) * 0.5f;
    const float guiY = (viewportH - guiH) * 0.5f;
    const float sidebarW = 380.f;
    const float contentW = guiW - sidebarW - 32.f;

    ImGui::SetNextWindowPos(ImVec2(guiX, guiY));
    ImGui::SetNextWindowSize(ImVec2(guiW, guiH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::Begin("##ClickGui", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);

    const ImVec2 winMin = ImGui::GetWindowPos();
    const ImVec2 sidebarMin = winMin;
    const ImVec2 sidebarMax(winMin.x + sidebarW, winMin.y + guiH);
    const ImVec2 contentMin(winMin.x + sidebarW + 32.f, winMin.y);
    const ImVec2 contentMax(winMin.x + guiW, winMin.y + guiH);

    DrawSidebar(sidebarMin, sidebarMax, alpha, dt);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    DrawGlass(dl, contentMin, contentMax, kRadiusLg, alpha, false);

    // 音乐分类：走自定义渲染面板
    if (g_categories[g_activeCategory].isMusic) {
        myiui::ui::music::MusicPanelRender(contentMin, contentMax, alpha, dt);
        DrawSettingsPopup(alpha);
        const char* hint = "R-SHIFT 开关   ·   L-CLICK 播放   ·   R-CLICK 喜欢   ·   ESC 退出";
        float hintW = TextW(font, 24.f, hint);
        bgDl->AddText(font, 24.f, ImVec2((viewportW - hintW) * 0.5f, viewportH - 64.f),
                      RGBA(kTextDim, alpha * 0.6f), hint);
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    const float headerH = 128.f;
    const char* catName = g_categories[g_activeCategory].name;
    dl->AddText(font, 40.f, ImVec2(contentMin.x + 64.f, contentMin.y + 44.f), RGBA(kTextPrimary, alpha), catName);

    // Search bar
    const float searchW = 360.f;
    const float searchH = 68.f;
    const ImVec2 sPos(contentMax.x - searchW - 48.f, contentMin.y + 30.f);
    dl->AddRectFilled(sPos, ImVec2(sPos.x + searchW, sPos.y + searchH), IM_COL32(255,255,255, static_cast<int>(15*alpha)), kRadiusSm);
    dl->AddRect(sPos, ImVec2(sPos.x + searchW, sPos.y + searchH), RGBA(kGlassBorder, alpha * 0.5f), kRadiusSm, 0, 1.f);

    {
        const float icx = sPos.x + 28.f;
        const float icy = sPos.y + 28.f;
        dl->AddCircle(ImVec2(icx, icy), 9.f, RGBA(kTextDim, alpha), 12, 1.4f);
        dl->AddLine(ImVec2(icx + 6.4f, icy + 6.4f), ImVec2(icx + 13.f, icy + 13.f), RGBA(kTextDim, alpha), 1.6f);
    }
    const float textOriginX = sPos.x + 68.f;

    if (g_searchBuf[0] == 0) {
        dl->AddText(font, 28.f, ImVec2(textOriginX, sPos.y + 18.f), RGBA(kTextDim, alpha), "搜索...");
    }

    ImGui::SetCursorScreenPos(ImVec2(sPos.x + 56.f, sPos.y + 6.f));
    ImGui::PushItemWidth(searchW - 56.f - 20.f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.97f, 0.98f, alpha));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, AccCol(80));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRadiusSm);
    ImGui::InputText("##search", g_searchBuf, sizeof(g_searchBuf));
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
    ImGui::PopItemWidth();

    dl->AddLine(ImVec2(contentMin.x + 64.f, contentMin.y + headerH),
                ImVec2(contentMax.x - 64.f, contentMin.y + headerH),
                RGBA(kGlassBorder, alpha * 0.2f), 1.f);

    // [FIX]: 引入局部滚动区域 (BeginChild) 处理内容溢出，同时利用ImGui自带规则实现完美滚轮支持
    ImGui::SetCursorScreenPos(ImVec2(contentMin.x, contentMin.y + headerH + 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    // 创建一个透明、不显示滚动条但响应鼠标滚轮的子窗口
    ImGui::BeginChild("##ItemList", ImVec2(contentW, (contentMax.y - contentMin.y) - headerH - 1.f), false, 
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    Category& cat = g_categories[g_activeCategory];
    const float cardPadX = 64.f;
    const float cardW = contentW - cardPadX * 2.f;
    const float parentH = 88.f;
    const float childH  = 52.f;
    const float childGap = 0.f;
    const float parentGap = 0.f;
    const float childIndent = 56.f;

    ImGui::Dummy(ImVec2(0, 20.f)); // 顶部边距

    for (int i = 0; i < cat.itemCount; ++i) {
        ModuleItem& item = cat.items[i];

        if (item.parent >= 0) {
            bool parentOn = cat.items[item.parent].boolValue && *cat.items[item.parent].boolValue;
            if (!parentOn) continue;
        }

        if (g_searchBuf[0]) {
            bool match = strstr(item.name, g_searchBuf) ||
                         (item.parent >= 0 && strstr(cat.items[item.parent].name, g_searchBuf));
            if (!match) continue;
        }

        if (item.parent < 0) {
            ImGui::SetCursorPosX(cardPadX);
            ImVec2 cMin = ImGui::GetCursorScreenPos();
            ImVec2 cMax(cMin.x + cardW, cMin.y + parentH);
            DrawCard(("##mod_" + std::to_string(i)).c_str(), item, alpha, dt, cMin, cMax);
            // InvisibleButton 已推进 parentH，Dummy 只补间距
            if (parentGap > 0.f) ImGui::Dummy(ImVec2(0, parentGap));
        } else {
            ImGui::SetCursorPosX(cardPadX + childIndent);
            ImVec2 cMin = ImGui::GetCursorScreenPos();
            ImVec2 cMax(cMin.x + cardW - childIndent, cMin.y + childH);
            DrawChildRow(("##child_" + std::to_string(i)).c_str(), item, alpha, dt, cMin, cMax);
            if (childGap > 0.f) ImGui::Dummy(ImVec2(0, childGap));
        }
    }
    
    ImGui::Dummy(ImVec2(0, 16.f)); // 底部边距留白
    ImGui::EndChild();
    ImGui::PopStyleVar(); // 恢复 ItemSpacing

    DrawSettingsPopup(alpha);

    const char* hint = "R-SHIFT 开关   ·   L-CLICK 切换   ·   R-CLICK 高级设置   ·   ESC 退出";
    float hintW = TextW(font, 24.f, hint);
    bgDl->AddText(font, 24.f, ImVec2((viewportW - hintW) * 0.5f, viewportH - 64.f), RGBA(kTextDim, alpha * 0.6f), hint);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

}  // namespace myiui::ui::clickgui