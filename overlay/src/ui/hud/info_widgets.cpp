#include "ui/hud/info_widgets.h"

#include "ui/hud/hud_glass.h"
#include "ui/fonts.h"

#include <cstdio>

namespace myiui::ui::hud {

namespace {

constexpr float kInfoScale = 2.f;
constexpr float kStackGap = 6.f;

ImVec2 AnchorOrigin(myiui::config::InfoAnchor anchor, float ox, float oy, float vw, float vh) {
    switch (anchor) {
        case myiui::config::InfoAnchor::TopRight:
            return ImVec2(vw + ox, oy);
        case myiui::config::InfoAnchor::BottomLeft:
            return ImVec2(ox, vh + oy);
        case myiui::config::InfoAnchor::BottomRight:
            return ImVec2(vw + ox, vh + oy);
        default:
            return ImVec2(ox, oy);
    }
}

float DrawCapsule(const AppConfig& cfg, ImVec2 pos, const char* text) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
    const float fs = 13.f * kInfoScale;
    const float padX = 10.f * kInfoScale;
    const float padY = 5.f * kInfoScale;
    const ImVec2 ts = font ? font->CalcTextSizeA(fs, FLT_MAX, 0.f, text) : ImGui::CalcTextSize(text);
    const ImVec2 min(pos.x, pos.y);
    const ImVec2 max(pos.x + ts.x + padX * 2.f, pos.y + ts.y + padY * 2.f);
    DrawGlassPanel(dl, min, max, 8.f * kInfoScale, cfg.theme.accent, 0.88f);
    if (font) {
        dl->AddText(font, fs, ImVec2(min.x + padX, min.y + padY), IM_COL32(245, 247, 250, 255), text);
    }
    return max.y - min.y;
}

struct StackCursor {
    float topLeftY = 0.f;
    float topRightY = 0.f;
    float bottomLeftY = 0.f;
    float bottomRightY = 0.f;
    bool topLeftInit = false;
    bool topRightInit = false;
    bool bottomLeftInit = false;
    bool bottomRightInit = false;
};

float& StackY(StackCursor& c, myiui::config::InfoAnchor anchor, float baseY, bool& initialized) {
    switch (anchor) {
        case myiui::config::InfoAnchor::TopRight:
            if (!c.topRightInit) {
                c.topRightY = baseY;
                c.topRightInit = true;
            }
            return c.topRightY;
        case myiui::config::InfoAnchor::BottomLeft:
            if (!c.bottomLeftInit) {
                c.bottomLeftY = baseY;
                c.bottomLeftInit = true;
            }
            return c.bottomLeftY;
        case myiui::config::InfoAnchor::BottomRight:
            if (!c.bottomRightInit) {
                c.bottomRightY = baseY;
                c.bottomRightInit = true;
            }
            return c.bottomRightY;
        default:
            if (!c.topLeftInit) {
                c.topLeftY = baseY;
                c.topLeftInit = true;
            }
            return c.topLeftY;
    }
}

void DrawStacked(const AppConfig& cfg, StackCursor& cursor, const myiui::config::InfoWidgetSettings& settings,
                 float viewportW, float viewportH, const char* text) {
    bool initialized = false;
    float& y = StackY(cursor, settings.anchor, settings.y, initialized);
    const ImVec2 origin = AnchorOrigin(settings.anchor, settings.x, y, viewportW, viewportH);
    ImVec2 pos = origin;
    if (settings.anchor == myiui::config::InfoAnchor::TopRight ||
        settings.anchor == myiui::config::InfoAnchor::BottomRight) {
        ImFont* font = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
        const float fs = 13.f * kInfoScale;
        const float padX = 10.f * kInfoScale;
        const ImVec2 ts = font ? font->CalcTextSizeA(fs, FLT_MAX, 0.f, text) : ImGui::CalcTextSize(text);
        const float w = ts.x + padX * 2.f;
        pos.x -= w;
    }
    if (settings.anchor == myiui::config::InfoAnchor::BottomLeft ||
        settings.anchor == myiui::config::InfoAnchor::BottomRight) {
        ImFont* font = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
        const float fs = 13.f * kInfoScale;
        const float padY = 5.f * kInfoScale;
        const ImVec2 ts = font ? font->CalcTextSizeA(fs, FLT_MAX, 0.f, text) : ImGui::CalcTextSize(text);
        const float h = ts.y + padY * 2.f;
        pos.y -= h;
    }
    const float h = DrawCapsule(cfg, pos, text);
    if (settings.anchor == myiui::config::InfoAnchor::BottomLeft ||
        settings.anchor == myiui::config::InfoAnchor::BottomRight) {
        y -= h + kStackGap * kInfoScale;
    } else {
        y += h + kStackGap * kInfoScale;
    }
}

}  // namespace

void RenderInfoWidgets(const AppConfig& cfg, const myiui::shared::InfoHudState& info,
                       const myiui::config::UserSettings& settings, float viewportW, float viewportH) {
    if (!info.valid) return;

    StackCursor cursor;
    char buf[128];
    if (settings.info_coords.enabled) {
        snprintf(buf, sizeof(buf), "XYZ %d %d %d", info.block_x, info.block_y, info.block_z);
        DrawStacked(cfg, cursor, settings.info_coords, viewportW, viewportH, buf);
    }
    if (settings.info_ping.enabled) {
        snprintf(buf, sizeof(buf), "Ping %d ms", info.ping_ms);
        DrawStacked(cfg, cursor, settings.info_ping, viewportW, viewportH, buf);
    }
    if (settings.info_speed.enabled) {
        snprintf(buf, sizeof(buf), "Speed %.1f b/s", info.speed_bps);
        DrawStacked(cfg, cursor, settings.info_speed, viewportW, viewportH, buf);
    }
}

}  // namespace myiui::ui::hud
