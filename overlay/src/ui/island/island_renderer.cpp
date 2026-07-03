#include "ui/island/island_renderer.h"
#include "bridge/ui_state_types.h"
#include "ui/clickgui/clickgui.h"
#include "ui/fonts.h"
#include "ui/island/island_tokens.h"
#include "spring_animator.h" // 引入弹簧动画

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>

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

float g_idleW = 200.f;
bool g_initialized = false;
float g_lastFrameMs = 0.f;

uint16_t g_lastSeq = 0;
bool g_lastNotifyActive = false;

float NowMs() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return static_cast<float>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
}

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

    const char* title = (!myiui::ui::clickgui::ShowFps() || !island.title[0]) ? "Ready" : island.title;
    DrawOutlinedText(dl, fS, sizeS, ImVec2(curX, cy - FontCapH(fS, sizeS)),
                     gray, title, alpha);
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

}  // namespace

void IslandRender(const ThemeConfig& theme, const ShmReader& shm,
                  float viewportW, float viewportH, float dt) {
    myiui::shared::IslandState island{};
    if (!shm.ReadIslandState(island)) return;

    const float now = NowMs();
    float delta = std::min((now - g_lastFrameMs) / 1000.f, 0.1f);
    if (g_lastFrameMs == 0.f) delta = 0.016f;
    g_lastFrameMs = now;

    // ── 弹簧参数初始化 ──
    if (!g_initialized) {
        g_initialized = true;
        g_springIntro.stiffness = 250.f;  g_springIntro.damping = 15.f;
        g_springExpand.stiffness = 300.f; g_springExpand.damping = 18.f;
        g_springSwitch.stiffness = 350.f; g_springSwitch.damping = 22.f; 
        g_springW.stiffness = 320.f;      g_springW.damping = 16.f; // 略微调低阻尼，让宽高伸缩更 Q 弹
        g_springH.stiffness = 320.f;      g_springH.damping = 16.f;
        
        // 交互弹簧刚度要高，让按压反馈立刻生效
        g_springHover.stiffness = 450.f;  g_springHover.damping = 18.f; 

        g_springIntro.Snap(0.f);
        g_springExpand.Snap(0.f);
        g_springSwitch.Snap(0.f);
        g_springW.Snap(0.f);
        g_springH.Snap(0.f);
        g_springHover.Snap(0.f);
    }

    // ── 状态推导 ──
    if (island.valid && g_springIntro.target == 0.f) {
        g_springIntro.SetTarget(1.f);
    }

    bool shouldExpand = island.notify_count > 0;
    g_springExpand.SetTarget(shouldExpand ? 1.f : 0.f);

    if (island.island_seq != g_lastSeq) {
        g_lastSeq = island.island_seq;
        g_springSwitch.Snap(shouldExpand ? 0.f : std::clamp(g_springSwitch.pos, 0.f, 1.f));
        g_springSwitch.SetTarget(shouldExpand ? 1.f : 0.f);
    }

    // ── 计算目标尺寸 ──
    ImFont* fP = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
    ImFont* fS = fP;
    const float sizeP = S(13.f);
    const float sizeS = S(11.f);
    const float idleH = S(kIslandIdleH);
    const float expandedH = S(kIslandExpandedH);
    const float radius = S(kIslandRadius);

    const char* idleTitle = (!myiui::ui::clickgui::ShowFps() || !island.title[0]) ? "Ready" : island.title;
    g_idleW = CalculateIdleWidth(fP, fS, sizeP, sizeS, idleTitle);

    float targetW = g_idleW;
    float targetH = idleH;

    if (shouldExpand) {
        float titleW = TextW(fP, sizeP, island.title[0] ? island.title : "通知");
        float descW = TextW(fS, sizeS, island.subtitle[0] ? island.subtitle : "模块已切换");
        targetW = S(52.f) + std::max(titleW, descW) + S(16.f);
        targetH = expandedH;
    }

    if (g_springW.pos == 0.f) {
        g_springW.Snap(targetW);
        g_springH.Snap(targetH);
    } else {
        g_springW.SetTarget(targetW);
        g_springH.SetTarget(targetH);
    }

    // ── 更新所有物理弹簧 ──
    g_springIntro.Step(delta);
    g_springExpand.Step(delta);
    g_springSwitch.Step(delta);
    g_springW.Step(delta);
    g_springH.Step(delta);
    g_springHover.Step(delta);

    // ── 渲染前的数据准备 ──
    float introVal = g_springIntro.pos;
    float expandVal = g_springExpand.pos;
    float introAlpha = std::clamp(introVal, 0.f, 1.f);
    float expandAlpha = std::clamp(expandVal, 0.f, 1.f);
    
    if (introAlpha <= 0.001f) return;

    float currentW = g_springW.pos;
    float currentH = g_springH.pos;

    const float top = S(18.f);
    const float drawX = (viewportW - currentW) * 0.5f;
    const float drawY = top;

    float introScale = 0.5f + 0.5f * introVal; 

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) return;

    const float cx = drawX + currentW * 0.5f;
    const float cy = drawY + currentH * 0.5f;
    const float scaledW = currentW * introScale;
    const float scaledH = currentH * introScale;
    const float sMinX = cx - scaledW * 0.5f;
    const float sMinY = cy - scaledH * 0.5f;
    const float sMaxX = cx + scaledW * 0.5f;
    const float sMaxY = cy + scaledH * 0.5f;

    // ── 【新增】交互检测与按压下陷反馈 ──
    bool hovered = ImGui::IsMouseHoveringRect(ImVec2(sMinX, sMinY), ImVec2(sMaxX, sMaxY));
    bool active = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    // 按下时 target 为 -1.0 (产生缩小效果)，悬停为 1.0 (放大)，默认 0.0
    g_springHover.SetTarget(active ? -1.f : (hovered ? 1.f : 0.f));

    // 计算交互带来的形变缩放 (悬停+4%，按下-5%)
    const float interactScale = 1.f + g_springHover.pos * 0.04f;

    // ── 【新增】基于速度的体积守恒形变 (Squash & Stretch) ──
    // 利用自身以及相反轴的变化速度来压缩/拉伸当前轴，营造真实的胶体质感
    float squishX = 1.f + (g_springW.vel * 0.00015f) - (g_springH.vel * 0.0001f);
    float squishY = 1.f + (g_springH.vel * 0.00015f) - (g_springW.vel * 0.0001f);
    
    // 限制极致形变防止画面穿模崩坏
    squishX = std::clamp(squishX, 0.85f, 1.15f);
    squishY = std::clamp(squishY, 0.85f, 1.15f);

    const float finalW = scaledW * interactScale * squishX;
    const float finalH = scaledH * interactScale * squishY;
    
    const float fMinX = cx - finalW * 0.5f;
    const float fMinY = cy - finalH * 0.5f;
    const float fMaxX = cx + finalW * 0.5f;
    const float fMaxY = cy + finalH * 0.5f;

    // 绘制背景与边框
    const float bgAlpha = DynOpacity() * introAlpha;
    const ImU32 bgCol = IM_COL32(0, 0, 0, static_cast<int>(255 * bgAlpha));
    dl->AddRectFilled(ImVec2(fMinX, fMinY), ImVec2(fMaxX, fMaxY), bgCol, radius);

    const ImU32 borderCol = IM_COL32(255, 255, 255, static_cast<int>(25 * introAlpha));
    dl->AddRect(ImVec2(fMinX, fMinY), ImVec2(fMaxX, fMaxY), borderCol, radius, 0, S(1.f));

    dl->PushClipRect(ImVec2(fMinX, fMinY), ImVec2(fMaxX, fMaxY), true);

    // ── 【新增】内容惯性视差 (Inertia Parallax) ──
    // 当外框剧烈改变尺寸时，里面的文字会有细微的物理滞后，而不是死死粘在原位
    float inertiaOffsetX = -g_springW.vel * 0.002f;
    float inertiaOffsetY = -g_springH.vel * 0.003f;

    float idleAlpha = std::clamp((1.f - expandVal) * introAlpha, 0.f, 1.f);
    float expAlpha = std::clamp(expandVal * introAlpha, 0.f, 1.f);

    if (idleAlpha > 0.01f) {
        // 合并由于状态切换的 Y 轴偏移 和 物理惯性偏移
        float offsetY = (1.f - expandVal) * S(8.f) - S(8.f); 
        RenderIdle(dl, fP, fS, sizeP, sizeS, island,
                   fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY, 
                   finalW, finalH, idleAlpha, theme);
    }

    if (expAlpha > 0.01f && shouldExpand) {
        float offsetY = (1.f - expandVal) * -S(8.f);
        RenderExpanded(dl, fP, fS, sizeP, sizeS, island,
                       fMinX + inertiaOffsetX, fMinY + offsetY + inertiaOffsetY, 
                       finalW, finalH, expAlpha, theme);
    }

    dl->PopClipRect();
}

}  // namespace myiui::ui::island