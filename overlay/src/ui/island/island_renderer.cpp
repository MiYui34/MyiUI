#include "ui/island/island_renderer.h"
#include "bridge/native_state.h"
#include "bridge/ui_state_types.h"
#include "config/user_settings.h"
#include "ui/clickgui/clickgui.h"
#include "ui/fonts.h"
#include "ui/hud/now_playing.h"
#include "ui/island/island_tokens.h"
#include "spring_animator.h" // 引入弹簧动画

#include "imgui.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace myiui::ui::island {

namespace {

// ── Scale: 由 ClickGui 的"缩放"设置控制 (1x..6x)，默认 3x ──
float DynScale() { return myiui::ui::clickgui::IslandScale(); }
float DynOpacity() { return myiui::ui::clickgui::IslandOpacity(); }

// ── Animation state (物理弹簧) ──
Spring1D g_springIntro;   // 入场动画
Spring1D g_springExpand;  // 展开动画 (高度与内容)
Spring1D g_springSwitch;  // 切换开关
Spring1D g_springW;       // 动态宽度
Spring1D g_springH;       // 动态高度
Spring1D g_springHover;   // 鼠标悬停/按压形变
Spring1D g_springContent; // 内容淡入

// [新增] 流体分离气泡动画状态
Spring1D g_springBubbleCenter; // 气泡中心相对主岛右边缘的物理坐标
Spring1D g_springBubbleScale;  // 气泡的缩放

float g_idleW = 200.f;
bool g_initialized = false;

uint16_t g_lastSeq = 0;
char g_idleTitleBuf[256]{};

struct TabLayoutCache {
    uint16_t tabSeq = 0;
    uint8_t playerCount = 0;
    float viewportW = 0.f;
    float scale = 0.f;
    float targetW = 0.f;
    float targetH = 0.f;
    int colCount = 1;
    int rowCount = 0;
    float colW = 0.f;
    float rowH = 0.f;
    float headerH = 0.f;
    float pad = 0.f;
    float pingColW = 0.f;
};

TabLayoutCache g_tabLayout{};

float S(float v) { return v * DynScale(); }

float TextW(ImFont* font, float size, const char* text) {
    if (!font || !text) return 0.f;
    return font->CalcTextSizeA(size, FLT_MAX, 0.f, text).x;
}

float FontCapH(ImFont* font, float size) {
    if (!font) return size * 0.7f;
    return font->CalcTextSizeA(size, FLT_MAX, 0.f, "Ay").y * 0.65f;
}

void DrawOutlinedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos,
                      ImU32 color, const char* text, float alpha = 1.f) {
    if (!font || !text || !text[0]) return;
    const ImU32 outline = IM_COL32(0, 0, 0, static_cast<int>(180 * alpha));
    dl->AddText(font, size, ImVec2(pos.x - 1, pos.y), outline, text);
    dl->AddText(font, size, ImVec2(pos.x + 1, pos.y), outline, text);
    dl->AddText(font, size, ImVec2(pos.x, pos.y - 1), outline, text);
    dl->AddText(font, size, ImVec2(pos.x, pos.y + 1), outline, text);
    dl->AddText(font, size, pos, color, text);
}

float CalculateIdleWidth(ImFont* fP, ImFont* fS, float sizeP, float sizeS, const char* title) {
    float w = S(kIslandPad);
    w += TextW(fP, sizeP, "MyiUI");
    w += TextW(fS, sizeS, "  •  ");
    w += TextW(fS, sizeS, title);
    w += S(kIslandPad);
    return w;
}

const char* BuildIdleTitle(const myiui::shared::IslandState& island) {
    if (island.title[0] && island.lyrics_line[0]) {
        std::snprintf(g_idleTitleBuf, sizeof(g_idleTitleBuf), "%s • %s", island.title, island.lyrics_line);
        return g_idleTitleBuf;
    }
    if (island.title[0]) {
        return island.title;
    }
    if (myiui::ui::clickgui::ShowFps() && island.fps > 0) {
        std::snprintf(g_idleTitleBuf, sizeof(g_idleTitleBuf), "%d FPS", island.fps);
        return g_idleTitleBuf;
    }
    return "Ready";
}

void RenderLyricsExpanded(ImDrawList* dl, ImFont* fP, ImFont* fS, float sizeP, float sizeS,
                          const myiui::shared::IslandState& island,
                          const myiui::shared::MusicHudState& music,
                          float x, float y, float w, float h, float alpha,
                          const ThemeConfig& theme) {
    const float tx = x + S(42.f);
    DrawOutlinedText(dl, fP, sizeP, ImVec2(tx, y + S(11.f)),
                     IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)),
                     island.title[0] ? island.title : "正在播放", alpha);

    const char* lyric = island.lyrics_line[0] ? island.lyrics_line : island.subtitle;
    DrawOutlinedText(dl, fS, sizeS, ImVec2(tx, y + S(28.f)),
                     IM_COL32(170, 170, 170, static_cast<int>(255 * alpha)),
                     lyric[0] ? lyric : "暂无歌词", alpha);

    if (music.valid && music.playing) {
        const float waveY = y + h - S(18.f);
        const float waveH = S(10.f);
        myiui::ui::hud::RenderMiniWaveform(dl, ImVec2(tx, waveY), ImVec2(x + w - S(12.f), waveY + waveH),
                                           music.waveform, 32, theme.accent, alpha, music.playing);
    }
}

void RenderIdle(ImDrawList* dl, ImFont* fP, ImFont* fS, float sizeP, float sizeS,
                const myiui::shared::IslandState& island,
                float x, float y, float w, float h, float alpha,
                const ThemeConfig& theme) {
    const float cy = y + h * 0.5f;
    float curX = x + S(kIslandPad);

    const ImU32 themeCol = IM_COL32(theme.accent[0], theme.accent[1], theme.accent[2],
                                    static_cast<int>(255 * alpha));
    const char* clientName = "MyiUI";
    float nameW = TextW(fP, sizeP, clientName);
    DrawOutlinedText(dl, fP, sizeP, ImVec2(curX, cy - FontCapH(fP, sizeP)),
                     themeCol, clientName, alpha);
    curX += nameW;

    const ImU32 white = IM_COL32(255, 255, 255, static_cast<int>(200 * alpha));
    const ImU32 gray = IM_COL32(226, 232, 240, static_cast<int>(200 * alpha));

    DrawOutlinedText(dl, fS, sizeS, ImVec2(curX, cy - FontCapH(fS, sizeS)),
                     white, "  •  ", alpha);
    curX += TextW(fS, sizeS, "  •  ");

    const char* title = BuildIdleTitle(island);
    DrawOutlinedText(dl, fS, sizeS, ImVec2(curX, cy - FontCapH(fS, sizeS)),
                     gray, title, alpha);
}

void RenderHoverDetail(ImDrawList* dl, ImFont* fP, ImFont* fS, float sizeP, float sizeS,
                       const myiui::shared::IslandState& island,
                       float x, float y, float w, float h, float alpha,
                       const ThemeConfig& theme) {
    const float tx = x + S(42.f);
    const char* title = island.title[0] ? island.title : "MyiUI";
    DrawOutlinedText(dl, fP, sizeP, ImVec2(tx, y + S(11.f)),
                     IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), title, alpha);

    const char* detail = island.subtitle[0] ? island.subtitle : BuildIdleTitle(island);
    DrawOutlinedText(dl, fS, sizeS, ImVec2(tx, y + S(28.f)),
                     IM_COL32(170, 170, 170, static_cast<int>(255 * alpha)), detail, alpha);
}

void RenderExpanded(ImDrawList* dl, ImFont* fP, ImFont* fS, float sizeP, float sizeS,
                    const myiui::shared::IslandState& island,
                    float x, float y, float w, float h, float alpha,
                    const ThemeConfig& theme) {
    float switchVal = std::clamp(g_springSwitch.pos, 0.f, 1.f);

    const float sw = S(26.f);
    const float sh = S(14.f);
    const float sr = S(6.5f);
    const float sx = x + S(8.f);
    const float sy = y + S(13.f);

    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh),
                      IM_COL32(51, 51, 51, static_cast<int>(255 * alpha)), sr);

    const ImU32 trackCol = IM_COL32(
        static_cast<int>(theme.accent[0] * switchVal + 107 * (1 - switchVal)),
        static_cast<int>(theme.accent[1] * switchVal + 114 * (1 - switchVal)),
        static_cast<int>(theme.accent[2] * switchVal + 128 * (1 - switchVal)),
        static_cast<int>(255 * alpha));
    dl->AddRectFilled(ImVec2(sx + S(1), sy + S(1)), ImVec2(sx + sw - S(1), sy + sh - S(1)),
                      trackCol, sr - S(1));

    const float knobX = sx + S(2.f) + S(12.f) * switchVal;
    dl->AddRectFilled(ImVec2(knobX, sy + S(2.f)), ImVec2(knobX + S(10.f), sy + sh - S(2.f)),
                      IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), S(4.5f));

    const float tx = x + S(42.f);
    DrawOutlinedText(dl, fP, sizeP, ImVec2(tx, y + S(11.f)),
                     IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)),
                     island.title[0] ? island.title : "通知", alpha);

    const char* state = island.notify_count > 0 ? "已启用" : "已禁用";
    const ImU32 stateCol = island.notify_count > 0
        ? IM_COL32(16, 185, 129, static_cast<int>(255 * alpha))
        : IM_COL32(239, 68, 68, static_cast<int>(255 * alpha));

    char descBuf[256];
    if (island.subtitle[0]) {
        snprintf(descBuf, sizeof(descBuf), "%s ", island.subtitle);
    } else {
        snprintf(descBuf, sizeof(descBuf), "模块已切换 ");
    }

    DrawOutlinedText(dl, fS, sizeS, ImVec2(tx, y + S(24.f)),
                     IM_COL32(170, 170, 170, static_cast<int>(255 * alpha)),
                     descBuf, alpha);
    float descW = TextW(fS, sizeS, descBuf);
    DrawOutlinedText(dl, fS, sizeS, ImVec2(tx + descW, y + S(24.f)), stateCol, state, alpha);
}

ImU32 PingColor(int ping, float alpha) {
    if (ping < 0) {
        return IM_COL32(148, 163, 184, static_cast<int>(220 * alpha));
    }
    if (ping < 80) {
        return IM_COL32(52, 211, 153, static_cast<int>(255 * alpha));
    }
    if (ping < 150) {
        return IM_COL32(250, 204, 21, static_cast<int>(255 * alpha));
    }
    return IM_COL32(248, 113, 113, static_cast<int>(255 * alpha));
}

void UpdateTabLayout(const myiui::shared::TabListState& tab, ImFont* font, float sizeS,
                     float viewportW) {
    const float scale = DynScale();
    const bool dirty = g_tabLayout.tabSeq != tab.tab_seq
                       || g_tabLayout.playerCount != tab.player_count
                       || std::abs(g_tabLayout.viewportW - viewportW) > 1.f
                       || std::abs(g_tabLayout.scale - scale) > 0.01f;
    if (!dirty && g_tabLayout.targetW > 0.f) {
        return;
    }

    g_tabLayout.tabSeq = tab.tab_seq;
    g_tabLayout.playerCount = tab.player_count;
    g_tabLayout.viewportW = viewportW;
    g_tabLayout.scale = scale;
    g_tabLayout.pad = S(10.f);
    g_tabLayout.headerH = S(18.f);
    g_tabLayout.rowH = S(15.f);
    g_tabLayout.pingColW = S(44.f);

    const int count = static_cast<int>(tab.player_count);
    g_tabLayout.colCount = count > 10 ? 2 : 1;
    g_tabLayout.rowCount = count <= 0 ? 0 : (count + g_tabLayout.colCount - 1) / g_tabLayout.colCount;

    float maxNameW = TextW(font, sizeS, tab.header[0] ? tab.header : "Players");
    for (int i = 0; i < count; ++i) {
        maxNameW = (std::max)(maxNameW, TextW(font, sizeS, tab.players[i].name));
    }

    g_tabLayout.colW = maxNameW + g_tabLayout.pingColW + S(12.f);
    float totalW = g_tabLayout.pad * 2.f + g_tabLayout.colW * static_cast<float>(g_tabLayout.colCount);
    if (g_tabLayout.colCount > 1) {
        totalW += S(8.f);
    }
    float totalH = g_tabLayout.pad * 2.f + g_tabLayout.headerH;
    if (g_tabLayout.rowCount > 0) {
        totalH += g_tabLayout.rowH * static_cast<float>(g_tabLayout.rowCount);
    }

    const float minW = S(160.f);
    const float maxW = viewportW * 0.85f;
    g_tabLayout.targetW = (std::clamp)(totalW, minW, maxW);
    g_tabLayout.targetH = (std::max)(S(kIslandIdleH) + S(8.f), totalH);
}

void RenderTabList(ImDrawList* dl, ImFont* fP, ImFont* fS, float sizeP, float sizeS,
                   const myiui::shared::TabListState& tab,
                   float x, float y, float w, float h, float alpha, float expandVal,
                   float offsetX, float offsetY, const ThemeConfig& theme) {
    if (alpha <= 0.01f) {
        return;
    }

    const float contentScale = 0.92f + 0.08f * std::clamp(g_springContent.pos, 0.f, 1.f);
    const float slideY = (1.f - std::clamp(g_springContent.pos, 0.f, 1.f)) * S(6.f);

    const float innerX = x + offsetX;
    const float innerY = y + offsetY + slideY;
    const float cx = innerX + w * 0.5f;
    const float cy = innerY + h * 0.5f;
    const float scaledW = w * contentScale;
    const float scaledH = h * contentScale;
    const float drawX = cx - scaledW * 0.5f;
    const float drawY = cy - scaledH * 0.5f;

    const ImU32 headerCol = IM_COL32(theme.accent[0], theme.accent[1], theme.accent[2],
                                     static_cast<int>(255 * alpha));
    DrawOutlinedText(dl, fP, sizeP, ImVec2(drawX + g_tabLayout.pad, drawY + g_tabLayout.pad),
                     headerCol, tab.header[0] ? tab.header : "Players", alpha);

    const int count = static_cast<int>(tab.player_count);
    for (int i = 0; i < count; ++i) {
        const int col = i % g_tabLayout.colCount;
        const int row = i / g_tabLayout.colCount;
        const float stagger = static_cast<float>(i) * 0.035f;
        const float rowAlpha = alpha * std::clamp((expandVal - stagger) * 1.4f, 0.f, 1.f);
        if (rowAlpha <= 0.01f) {
            continue;
        }

        const float colOffset = static_cast<float>(col) * (g_tabLayout.colW + S(8.f));
        const float rowY = drawY + g_tabLayout.pad + g_tabLayout.headerH
                           + static_cast<float>(row) * g_tabLayout.rowH;
        const float nameX = drawX + g_tabLayout.pad + colOffset;
        const float pingX = nameX + g_tabLayout.colW - g_tabLayout.pingColW;

        DrawOutlinedText(dl, fS, sizeS, ImVec2(nameX, rowY),
                         IM_COL32(226, 232, 240, static_cast<int>(230 * rowAlpha)),
                         tab.players[i].name, rowAlpha);

        char pingBuf[16];
        const int ping = tab.players[i].ping;
        if (ping >= 0) {
            snprintf(pingBuf, sizeof(pingBuf), "%dms", ping);
        } else {
            snprintf(pingBuf, sizeof(pingBuf), "--");
        }
        DrawOutlinedText(dl, fS, sizeS, ImVec2(pingX, rowY), PingColor(ping, rowAlpha), pingBuf, rowAlpha);
    }
}

void SyncOverlayMousePos() {
    ImGuiIO& io = ImGui::GetIO();
    const HWND hwnd = reinterpret_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    if (!hwnd) {
        return;
    }
    POINT pt{};
    if (!::GetCursorPos(&pt)) {
        return;
    }
    if (!::ScreenToClient(hwnd, &pt)) {
        return;
    }
    io.AddMousePosEvent(static_cast<float>(pt.x), static_cast<float>(pt.y));
}

bool IslandHoverHitTest(float minX, float minY, float maxX, float maxY) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float pad = S(6.f);
    return mouse.x >= minX - pad && mouse.x <= maxX + pad && mouse.y >= minY - pad && mouse.y <= maxY + pad;
}

void ComputeIslandLayout(float viewportW, float introVal, float currentW, float currentH,
                         float& outDrawX, float& outDrawY, float& outCx, float& outCy,
                         float& outScaledW, float& outScaledH, float& outMinX, float& outMinY,
                         float& outMaxX, float& outMaxY) {
    const auto& isl = myiui::config::GetUserSettingsConst().island;
    const float top = S(18.f) + isl.offset_y;
    outDrawX = (viewportW - currentW) * 0.5f + isl.offset_x;
    outDrawY = top;
    const float introScale = 0.5f + 0.5f * introVal;
    outCx = outDrawX + currentW * 0.5f;
    outCy = outDrawY + currentH * 0.5f;
    outScaledW = currentW * introScale;
    outScaledH = currentH * introScale;
    outMinX = outCx - outScaledW * 0.5f;
    outMinY = outCy - outScaledH * 0.5f;
    outMaxX = outCx + outScaledW * 0.5f;
    outMaxY = outCy + outScaledH * 0.5f;
}

}  // namespace

void IslandRender(const ThemeConfig& theme, const ShmReader& shm,
                  float viewportW, float viewportH, float dt) {
    myiui::shared::IslandState island{};
    const bool hasIsland = myiui::bridge::NativeState::Instance().ReadIsland(island);

    myiui::shared::TabListState tab{};
    const bool hasTab = myiui::bridge::NativeState::Instance().ReadTabList(tab);
    const bool tabActive = hasTab && tab.tab_visible != 0;

    myiui::shared::MusicHudState music{};
    myiui::bridge::NativeState::Instance().ReadMusicHud(music);

    if (!hasIsland && !tabActive) {
        return;
    }

    if (!hasIsland) {
        island.valid = 1;
        std::snprintf(island.title, sizeof(island.title), "MyiUI");
    }

    float delta = dt > 0.f ? std::min(dt, 0.1f) : 0.016f;

    // ── 弹簧参数初始化 ──
    if (!g_initialized) {
        g_initialized = true;
        g_springIntro.stiffness = 250.f;  g_springIntro.damping = 15.f;
        g_springExpand.stiffness = 300.f; g_springExpand.damping = 14.f;
        g_springSwitch.stiffness = 350.f; g_springSwitch.damping = 22.f;
        g_springW.stiffness = 320.f;      g_springW.damping = 14.f;
        g_springH.stiffness = 320.f;      g_springH.damping = 14.f;
        g_springHover.stiffness = 450.f;  g_springHover.damping = 18.f;
        g_springContent.stiffness = 280.f; g_springContent.damping = 18.f;
        
        // 分离气泡专用的弹簧设置
        g_springBubbleCenter.stiffness = 250.f; g_springBubbleCenter.damping = 18.f;
        g_springBubbleScale.stiffness = 300.f;  g_springBubbleScale.damping = 15.f;

        g_springIntro.Snap(0.f);
        g_springExpand.Snap(0.f);
        g_springSwitch.Snap(0.f);
        g_springW.Snap(0.f);
        g_springH.Snap(0.f);
        g_springHover.Snap(0.f);
        g_springContent.Snap(0.f);
        g_springBubbleCenter.Snap(0.f);
        g_springBubbleScale.Snap(0.f);
    }

    // ── 状态推导 ──
    if (tabActive) {
        if (g_springIntro.pos < 0.99f) {
            g_springIntro.Snap(1.f);
        }
        g_springIntro.SetTarget(1.f);
    } else if (island.valid && g_springIntro.target == 0.f) {
        g_springIntro.SetTarget(1.f);
    }

    const bool isMusicMode = island.active_slot == 0 && island.title[0];

    ImFont* fP = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
    ImFont* fS = fP;
    const float sizeP = S(13.f);
    const float sizeS = S(11.f);
    const float idleH = S(kIslandIdleH);
    const float expandedH = S(kIslandExpandedH);
    const float radius = S(kIslandRadius);

    const char* idleTitle = BuildIdleTitle(island);
    g_idleW = CalculateIdleWidth(fP, fS, sizeP, sizeS, idleTitle);

    // 先推进入场动画，再用当前尺寸做悬停命中
    g_springIntro.Step(delta);
    const float introVal = g_springIntro.pos;
    const float introAlpha = std::clamp(introVal, 0.f, 1.f);
    if (introAlpha <= 0.001f) {
        return;
    }

    SyncOverlayMousePos();

    float hitMinX = 0.f;
    float hitMinY = 0.f;
    float hitMaxX = 0.f;
    float hitMaxY = 0.f;
    float unusedDrawX = 0.f;
    float unusedDrawY = 0.f;
    float unusedCx = 0.f;
    float unusedCy = 0.f;
    float unusedScaledW = 0.f;
    float unusedScaledH = 0.f;
    ComputeIslandLayout(viewportW, introVal, g_springW.pos, g_springH.pos,
                        unusedDrawX, unusedDrawY, unusedCx, unusedCy, unusedScaledW, unusedScaledH,
                        hitMinX, hitMinY, hitMaxX, hitMaxY);

    const bool hovered = IslandHoverHitTest(hitMinX, hitMinY, hitMaxX, hitMaxY);
    const bool active = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);

    const bool shouldExpandNotify = !tabActive && island.notify_count > 0;
    const bool shouldExpandTab = tabActive;
    const bool shouldExpandHover = hovered && !tabActive && !shouldExpandNotify;
    const bool shouldExpandMusic = isMusicMode && shouldExpandHover;
    const bool shouldExpandIdleDetail = !isMusicMode && shouldExpandHover;
    const bool shouldExpand = shouldExpandTab || shouldExpandNotify || shouldExpandMusic || shouldExpandIdleDetail;
    g_springExpand.SetTarget(shouldExpand ? 1.f : 0.f);
    g_springContent.SetTarget(shouldExpand ? 1.f : 0.f);

    if (island.island_seq != g_lastSeq) {
        g_lastSeq = island.island_seq;
        g_springSwitch.Snap(shouldExpandNotify ? 0.f : std::clamp(g_springSwitch.pos, 0.f, 1.f));
        g_springSwitch.SetTarget(shouldExpandNotify ? 1.f : 0.f);
    }

    float targetW = g_idleW;
    float targetH = idleH;

    if (shouldExpandTab) {
        UpdateTabLayout(tab, fS, sizeS, viewportW);
        targetW = (std::max)(g_idleW, g_tabLayout.targetW);
        targetH = g_tabLayout.targetH;
    } else if (shouldExpandMusic) {
        float titleW = TextW(fP, sizeP, island.title[0] ? island.title : "正在播放");
        const char* detail = island.lyrics_line[0] ? island.lyrics_line
                            : (island.subtitle[0] ? island.subtitle : "暂无歌词");
        float detailW = TextW(fS, sizeS, detail);
        targetW = S(52.f) + (std::max)(titleW, detailW) + S(16.f);
        targetH = expandedH + S(8.f);
    } else if (shouldExpandIdleDetail) {
        float titleW = TextW(fP, sizeP, island.title[0] ? island.title : "MyiUI");
        const char* detail = island.subtitle[0] ? island.subtitle : BuildIdleTitle(island);
        float detailW = TextW(fS, sizeS, detail);
        targetW = S(52.f) + (std::max)(titleW, detailW) + S(16.f);
        targetH = expandedH;
    } else if (shouldExpandNotify) {
        float titleW = TextW(fP, sizeP, island.title[0] ? island.title : "通知");
        float descW = TextW(fS, sizeS, island.subtitle[0] ? island.subtitle : "模块已切换");
        targetW = S(52.f) + (std::max)(titleW, descW) + S(16.f);
        targetH = expandedH;
    }

    if (g_springW.pos == 0.f) {
        g_springW.Snap(targetW);
        g_springH.Snap(targetH);
    } else {
        g_springW.SetTarget(targetW);
        g_springH.SetTarget(targetH);
    }

    // ── 分离气泡的逻辑推导 ──
    // 条件：音乐处于激活状态，但主岛并没有处于显示音乐的状态，且灵动岛没有被展开
    bool showBubble = music.valid && (music.playing || music.paused) && !isMusicMode && !shouldExpand;
    float maxBubbleSize = S(28.f);
    
    g_springBubbleScale.SetTarget(showBubble ? 1.0f : 0.0f);
    // 隐藏时，气泡中心缩回主岛内部 (偏移量为负); 显示时，向外弹出产生间距
    g_springBubbleCenter.SetTarget(showBubble ? (maxBubbleSize * 0.5f + S(8.f)) : (-maxBubbleSize * 0.5f));

    g_springExpand.Step(delta);
    g_springSwitch.Step(delta);
    g_springW.Step(delta);
    g_springH.Step(delta);
    g_springHover.SetTarget(active ? -1.f : (hovered ? 1.f : 0.f));
    g_springHover.Step(delta);
    g_springContent.Step(delta);
    g_springBubbleScale.Step(delta);
    g_springBubbleCenter.Step(delta);

    const float expandVal = g_springExpand.pos;
    const float currentW = g_springW.pos;
    const float currentH = g_springH.pos;

    float drawX = 0.f, drawY = 0.f, cx = 0.f, cy = 0.f;
    float scaledW = 0.f, scaledH = 0.f, sMinX = 0.f, sMinY = 0.f, sMaxX = 0.f, sMaxY = 0.f;
    ComputeIslandLayout(viewportW, introVal, currentW, currentH,
                        drawX, drawY, cx, cy, scaledW, scaledH, sMinX, sMinY, sMaxX, sMaxY);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    // 1. 计算交互带来的形变缩放 (悬停放大，按下缩小)
    const float interactScale = 1.f + g_springHover.pos * 0.04f;

    // 2. 【升级版】果冻动能反馈 (Jelly Click) + 体积守恒
    // 结合宽高的物理速度，并叠加鼠标点击瞬间产生的物理波(vel)，制造 Q 弹震感
    float clickJelly = g_springHover.vel * 0.005f; 
    float squishX = 1.f + (g_springW.vel * 0.00015f) - (g_springH.vel * 0.0001f) + clickJelly;
    float squishY = 1.f + (g_springH.vel * 0.00015f) - (g_springW.vel * 0.0001f) - clickJelly;
    
    squishX = std::clamp(squishX, 0.85f, 1.15f);
    squishY = std::clamp(squishY, 0.85f, 1.15f);

    const float finalW = scaledW * interactScale * squishX;
    const float finalH = scaledH * interactScale * squishY;
    
    const float fMinX = cx - finalW * 0.5f;
    const float fMinY = cy - finalH * 0.5f;
    const float fMaxX = cx + finalW * 0.5f;
    const float fMaxY = cy + finalH * 0.5f;

    const float bgAlpha = DynOpacity() * introAlpha;
    const ImU32 bgCol = IM_COL32(0, 0, 0, static_cast<int>(255 * bgAlpha));
    const ImU32 borderCol = IM_COL32(255, 255, 255, static_cast<int>(25 * introAlpha));

    // ── 【新增】液态分离气泡 (Fluid Split Bubble) ──
    // 为了实现 Metaball (流体元球) 般的粘连效果，我们先于主岛绘制气泡及“连接桥”
    float bScale = g_springBubbleScale.pos;
    if (bScale > 0.01f) {
        float bSize = maxBubbleSize * bScale;
        float bCenterX = fMaxX + g_springBubbleCenter.pos;
        float bCenterY = cy;
        
        float bMinX = bCenterX - bSize * 0.5f;
        float bMaxX = bCenterX + bSize * 0.5f;
        float bMinY = bCenterY - bSize * 0.5f;
        float bMaxY = bCenterY + bSize * 0.5f;

        // 绘制“粘连桥”：当气泡尚未完全脱离主岛时，用纯黑矩形填补缝隙，产生类似液滴断开前的拉丝感
        if (bMinX < fMaxX) {
            float bridgeTop = bCenterY - bSize * 0.35f; 
            float bridgeBottom = bCenterY + bSize * 0.35f;
            dl->AddRectFilled(ImVec2(fMaxX - S(5.f), bridgeTop), ImVec2(bCenterX, bridgeBottom), bgCol);
        }

        // 绘制气泡本体
        dl->AddRectFilled(ImVec2(bMinX, bMinY), ImVec2(bMaxX, bMaxY), bgCol, bSize * 0.5f);
        dl->AddRect(ImVec2(bMinX, bMinY), ImVec2(bMaxX, bMaxY), borderCol, bSize * 0.5f, 0, S(1.f));

        // 绘制气泡内的小型音乐频谱
        if (music.valid && bScale > 0.5f) {
            float waveW = bSize * 0.55f;
            float waveH = bSize * 0.5f;
            float waveX = bCenterX - waveW * 0.5f;
            float waveY = bCenterY - waveH * 0.5f;
            
            dl->PushClipRect(ImVec2(bMinX, bMinY), ImVec2(bMaxX, bMaxY), true);
            myiui::ui::hud::RenderMiniWaveform(dl, ImVec2(waveX, waveY), ImVec2(waveX + waveW, waveY + waveH),
                                               music.waveform, 8, theme.accent, introAlpha * bScale, music.playing);
            dl->PopClipRect();
        }
    }

    // 绘制主岛背景与边框
    dl->AddRectFilled(ImVec2(fMinX, fMinY), ImVec2(fMaxX, fMaxY), bgCol, radius);
    dl->AddRect(ImVec2(fMinX, fMinY), ImVec2(fMaxX, fMaxY), borderCol, radius, 0, S(1.f));

    // 开启主岛内容裁剪
    dl->PushClipRect(ImVec2(fMinX, fMinY), ImVec2(fMaxX, fMaxY), true);

    // ── 内容惯性视差 (Inertia Parallax) ──
    float inertiaOffsetX = -g_springW.vel * 0.002f;
    float inertiaOffsetY = -g_springH.vel * 0.003f;

    float idleAlpha = std::clamp((1.f - expandVal) * introAlpha, 0.f, 1.f);
    float expAlpha = std::clamp(expandVal * introAlpha, 0.f, 1.f);
    float tabAlpha = std::clamp(g_springContent.pos * introAlpha, 0.f, 1.f);

    if (idleAlpha > 0.01f && !tabActive) {
        float offsetY = (1.f - expandVal) * S(8.f) - S(8.f);
        RenderIdle(dl, fP, fS, sizeP, sizeS, island,
                   fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY,
                   finalW, finalH, idleAlpha, theme);
    }

    if (expAlpha > 0.01f && shouldExpandNotify) {
        float offsetY = (1.f - expandVal) * -S(8.f);
        RenderExpanded(dl, fP, fS, sizeP, sizeS, island,
                       fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY,
                       finalW, finalH, expAlpha, theme);
    }

    if (expAlpha > 0.01f && shouldExpandMusic) {
        float offsetY = (1.f - expandVal) * -S(8.f);
        RenderLyricsExpanded(dl, fP, fS, sizeP, sizeS, island, music,
                             fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY,
                             finalW, finalH, expAlpha, theme);
    }

    if (expAlpha > 0.01f && shouldExpandIdleDetail) {
        float offsetY = (1.f - expandVal) * -S(8.f);
        RenderHoverDetail(dl, fP, fS, sizeP, sizeS, island,
                          fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY,
                          finalW, finalH, expAlpha, theme);
    }

    if (tabAlpha > 0.01f && shouldExpandTab && hasTab) {
        float offsetY = (1.f - expandVal) * -S(6.f);
        RenderTabList(dl, fP, fS, sizeP, sizeS, tab,
                      fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY,
                      finalW, finalH, tabAlpha, expandVal,
                      inertiaOffsetX, 0.f, theme);
    }

    dl->PopClipRect();
}

ImVec4 CalcIslandIdleBounds(float viewportW, float viewportH) {
    (void)viewportH;
    const float scale = DynScale();
    const float w = g_idleW > 0.f ? g_idleW : 200.f * scale;
    const float h = kIslandIdleH * scale;
    const auto& isl = myiui::config::GetUserSettingsConst().island;
    const float top = 18.f * scale + isl.offset_y;
    const float x = (viewportW - w) * 0.5f + isl.offset_x;
    return ImVec4(x, top, x + w, top + h);
}

}  // namespace myiui::ui::island
