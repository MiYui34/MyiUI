#include "ui/chat/chat_renderer.h"

#include "bridge/ui_state_types.h"
#include "ui/fonts.h"
#include "ui/hud/liquid_glass_panel.h"
#include "ui/chat/chat_tokens.h"
#include "ui/island/spring_animator.h"
#include "ui/easing.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace myiui::ui::chat {

namespace {

using myiui::ui::hud::DrawLiquidGlassPanel;
using myiui::ui::hud::LiquidGlassMode;
using myiui::ui::island::Spring1D;

struct ChatAnim {
    Spring1D alpha;        // panel fade 0→1
    Spring1D offsetY;      // slide from bottom
    Spring1D scale;        // jelly bounce
    Spring1D height;       // smooth panel height

    // per-message appear progress (0 = invisible, 1 = settled)
    float msgAlpha[myiui::shared::kChatMaxMessages]{};
    float msgOffsetX[myiui::shared::kChatMaxMessages]{};

    uint16_t lastSeq = 0;
    int lastMsgCount = 0;
    bool initialized = false;

    void Init() {
        alpha.Snap(0.f);
        offsetY.Snap(60.f);
        scale.Snap(0.90f);
        height.Snap(0.f);
        // Jelly: low damping for bounce
        scale.stiffness = 320.f;
        scale.damping = 14.f;
        alpha.stiffness = 260.f;
        alpha.damping = 20.f;
        offsetY.stiffness = 300.f;
        offsetY.damping = 18.f;
        height.stiffness = 260.f;
        height.damping = 20.f;
        initialized = true;
    }

    void Show(float targetH) {
        alpha.SetTarget(1.f);
        offsetY.SetTarget(0.f);
        scale.SetTarget(1.f);
        height.SetTarget(targetH);
    }

    void Hide() {
        alpha.SetTarget(0.f);
        offsetY.SetTarget(60.f);
        scale.SetTarget(0.92f);
        height.SetTarget(0.f);
    }

    void Update(float dt) {
        alpha.Step(dt);
        offsetY.Step(dt);
        scale.Step(dt);
        height.Step(dt);
        // message appear animation
        for (int i = 0; i < myiui::shared::kChatMaxMessages; ++i) {
            if (msgAlpha[i] < 1.f) {
                msgAlpha[i] = std::min(1.f, msgAlpha[i] + dt * 4.5f);
            }
            if (msgOffsetX[i] > 0.f) {
                msgOffsetX[i] = std::max(0.f, msgOffsetX[i] - dt * 120.f);
            }
        }
    }

    void OnNewMessages(int oldCount, int newCount) {
        // Animate newly appeared messages
        for (int i = oldCount; i < newCount && i < myiui::shared::kChatMaxMessages; ++i) {
            msgAlpha[i] = 0.f;
            msgOffsetX[i] = 24.f;
        }
        // If messages shifted (old ones removed), reset all
        if (newCount < oldCount) {
            for (int i = 0; i < newCount; ++i) {
                if (msgAlpha[i] < 0.01f) {
                    msgAlpha[i] = 0.f;
                    msgOffsetX[i] = 16.f;
                }
            }
        }
    }
};

ChatAnim g_anim{};

void DrawOutlinedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, float alpha, ImU32 color, const char* text) {
    if (!font || !text || !text[0] || alpha < 0.01f) {
        return;
    }
    const int a = static_cast<int>(200 * alpha);
    const int aMain = static_cast<int>(((color >> 24) & 0xFF) * alpha);
    const ImU32 outline = IM_COL32(0, 0, 0, a);
    const ImU32 main = (color & 0x00FFFFFF) | (aMain << 24);
    dl->AddText(font, size, ImVec2(pos.x - 1.f, pos.y), outline, text);
    dl->AddText(font, size, ImVec2(pos.x + 1.f, pos.y), outline, text);
    dl->AddText(font, size, ImVec2(pos.x, pos.y - 1.f), outline, text);
    dl->AddText(font, size, ImVec2(pos.x, pos.y + 1.f), outline, text);
    dl->AddText(font, size, pos, main, text);
}

float MeasureTextHeight(ImFont* font, float size) {
    if (font) {
        return font->CalcTextSizeA(size, FLT_MAX, 0.f, "Ay").y;
    }
    return size * 1.6f;
}

} // namespace

void ChatRender(const ThemeConfig& theme, const ShmReader& shm, float viewportW, float viewportH, float dt) {
    myiui::shared::ChatState chat{};
    if (!shm.ReadChatState(chat)) {
        return;
    }

    if (!g_anim.initialized) {
        g_anim.Init();
    }

    const bool visible = chat.visible != 0;
    const int msgCount = std::min(static_cast<int>(chat.msg_count), static_cast<int>(myiui::shared::kChatMaxMessages));

    // Detect new messages
    if (chat.chat_seq != g_anim.lastSeq) {
        g_anim.OnNewMessages(g_anim.lastMsgCount, msgCount);
        g_anim.lastSeq = chat.chat_seq;
        g_anim.lastMsgCount = msgCount;
    }

    const float scale = ChatUiScale(viewportW, viewportH);
    const float dtClamped = std::min(dt, 0.05f);

    // Calculate target height
    const ImFont* font = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
    ImFont* drawFont = const_cast<ImFont*>(font);
    const float fontSize = std::max(14.f, kBaseFontPx * scale);
    const float lineH = MeasureTextHeight(drawFont, fontSize) + kMessageGap * scale;
    const float panelW = kPanelWidth * scale;
    const float contentH = msgCount * lineH + kPanelPadY * 2.f * scale;
    const float targetH = visible ? std::min(std::max(contentH, kPanelMinHeight * scale), kPanelMaxHeight * scale) : 0.f;

    if (visible) {
        g_anim.Show(targetH);
    } else {
        g_anim.Hide();
    }

    g_anim.Update(dtClamped);

    const float panelAlpha = g_anim.alpha.pos;
    if (panelAlpha < 0.01f) {
        return;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) {
        return;
    }

    // Animated values
    const float animH = g_anim.height.pos;
    const float animOffsetY = g_anim.offsetY.pos;
    const float animScale = g_anim.scale.pos;

    // Jelly scale: expand from center-bottom
    const float scaledW = panelW * animScale;
    const float scaledH = animH * animScale;
    const float centerX = kPanelOffsetX * scale + panelW * 0.5f;
    const float x = centerX - scaledW * 0.5f;
    const float y = viewportH - scaledH - kPanelOffsetY * scale + animOffsetY;
    const ImVec2 min(x, y);
    const ImVec2 max(x + scaledW, y + scaledH);

    // Glass panel
    DrawLiquidGlassPanel(dl, min, max, theme, LiquidGlassMode::Tinted, 0.85f * panelAlpha, false, kPanelRadius * scale);

    // Left-bottom accent glow
    const int glowA = static_cast<int>(28 * panelAlpha);
    const ImU32 glowCol = IM_COL32(theme.accent[0], theme.accent[1], theme.accent[2], glowA);
    const ImU32 glowZero = IM_COL32(theme.accent[0], theme.accent[1], theme.accent[2], 0);
    dl->AddRectFilledMultiColor(
        ImVec2(min.x, max.y - scaledH * 0.5f),
        ImVec2(min.x + scaledW * 0.6f, max.y),
        glowZero, glowCol, glowCol, glowZero);

    // Border glow
    const int borderA = static_cast<int>(90 * panelAlpha * animScale);
    const ImU32 borderGlow = IM_COL32(theme.accent[0], theme.accent[1], theme.accent[2], borderA);
    dl->AddRect(ImVec2(min.x - 1.f, min.y - 1.f), ImVec2(max.x + 1.f, max.y + 1.f),
                borderGlow, kPanelRadius * scale, 0, 1.5f);

    // Messages
    const float padX = kPanelPadX * scale;
    const float padY = kPanelPadY * scale;
    float cursorY = min.y + padY;

    for (int i = 0; i < msgCount; ++i) {
        const auto& msg = chat.messages[i];
        if (!msg.user[0] && !msg.text[0]) {
            continue;
        }

        const float msgA = g_anim.msgAlpha[i];
        const float msgOff = g_anim.msgOffsetX[i];
        if (msgA < 0.01f) {
            cursorY += lineH;
            continue;
        }

        // Ease the message alpha (ease-out-cubic for smoothness)
        const float easedA = myiui::easing::EaseOutCubic(msgA);
        const float textX = min.x + padX + msgOff;

        if (msg.user[0]) {
            char userBuf[myiui::shared::kChatUserLen + 8];
            std::snprintf(userBuf, sizeof(userBuf), "%s >", msg.user);
            const ImVec2 userSize = drawFont ? drawFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, userBuf)
                                             : ImGui::CalcTextSize(userBuf);
            DrawOutlinedText(dl, drawFont, fontSize, ImVec2(textX, cursorY), easedA,
                             IM_COL32(theme.accent[0], theme.accent[1], theme.accent[2], 235), userBuf);

            if (msg.text[0]) {
                DrawOutlinedText(dl, drawFont, fontSize, ImVec2(textX + userSize.x + 4.f * scale, cursorY),
                                 easedA, IM_COL32(255, 255, 255, 220), msg.text);
            }
        } else if (msg.text[0]) {
            DrawOutlinedText(dl, drawFont, fontSize, ImVec2(textX, cursorY), easedA,
                             IM_COL32(255, 255, 255, 220), msg.text);
        }

        cursorY += lineH;
        if (cursorY > max.y - padY) {
            break;
        }
    }
}

} // namespace myiui::ui::chat
