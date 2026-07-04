#include "ui/widgets/parallax_card.h"

#include "ui/fonts.h"
#include "ui/island/spring_animator.h"

#include <imgui_internal.h>

#include <algorithm>
#include <map>

namespace myiui::ui::widgets {

namespace {

struct ParallaxState {
    myiui::ui::island::Spring1D spring_x;
    myiui::ui::island::Spring1D spring_y;
    myiui::ui::island::Spring1D spring_hover;
    bool initialized = false;
};

void InitSprings(ParallaxState& state) {
    if (state.initialized) {
        return;
    }
    state.spring_x.stiffness = 340.f;
    state.spring_x.damping = 20.f;
    state.spring_y.stiffness = 340.f;
    state.spring_y.damping = 20.f;
    state.spring_hover.stiffness = 420.f;
    state.spring_hover.damping = 22.f;
    state.initialized = true;
}

}  // namespace

float ParallaxCardHoverMargin(const ImVec2& size) {
    const float layoutScale = std::max(0.75f, size.y / 100.f);
    const float scalePad = (size.x + size.y) * 0.02f;
    const float parallaxPad = 13.f * layoutScale;
    return scalePad + parallaxPad + 2.f;
}

ParallaxCardClipGuard::ParallaxCardClipGuard() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!dl || !window) {
        return;
    }
    dl->PushClipRect(window->InnerClipRect.Min, window->InnerClipRect.Max, true);
    active = true;
}

ParallaxCardClipGuard::ParallaxCardClipGuard(const ImVec2& clip_min, const ImVec2& clip_max) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) {
        return;
    }
    dl->PushClipRect(clip_min, clip_max, true);
    active = true;
}

ParallaxCardClipGuard::~ParallaxCardClipGuard() {
    if (!active) {
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (dl) {
        dl->PopClipRect();
    }
}

bool DrawParallaxCard(const char* str_id, const ImVec2& size, const ImVec2& pos, const ThemeConfig& theme,
                      const char* title, const char* desc, float reservedRightPx) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }

    const ImGuiID id = window->GetID(str_id);
    const ImVec2 p_min = pos;
    const ImVec2 p_max(p_min.x + size.x, p_min.y + size.y);
    const ImRect bb(p_min, p_max);

    ImRect interact_bb = bb;
    if (reservedRightPx > 0.f) {
        interact_bb.Max.x = std::max(interact_bb.Min.x + 8.f, interact_bb.Max.x - reservedRightPx);
    }

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) {
        return false;
    }

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(interact_bb, id, &hovered, &held);

    static std::map<ImGuiID, ParallaxState> states;
    ParallaxState& state = states[id];
    InitSprings(state);

    const float dt = ImGui::GetIO().DeltaTime;
    const float layoutScale = std::max(0.75f, size.y / 100.f);
    const ImVec2 center(p_min.x + size.x * 0.5f, p_min.y + size.y * 0.5f);

    float target_x = 0.f;
    float target_y = 0.f;
    float target_hover = hovered ? 1.f : 0.f;

    if (hovered) {
        const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        target_x = (mouse_pos.x - center.x) / (size.x * 0.5f);
        target_y = (mouse_pos.y - center.y) / (size.y * 0.5f);
        target_x = std::clamp(target_x, -1.f, 1.f);
        target_y = std::clamp(target_y, -1.f, 1.f);
    }

    if (held) {
        target_hover = 0.85f;
    }

    state.spring_x.SetTarget(target_x);
    state.spring_y.SetTarget(target_y);
    state.spring_hover.SetTarget(target_hover);
    state.spring_x.Step(dt);
    state.spring_y.Step(dt);
    state.spring_hover.Step(dt);

    const float tx = state.spring_x.pos;
    const float ty = state.spring_y.pos;
    const float hover_val = state.spring_hover.pos;

    // ForegroundDrawList 绘制，配合 ParallaxCardClipGuard 在滚动区上下裁切
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl) {
        return pressed;
    }

    // [Layer 1] 底板 — 与鼠标反方向位移 + 悬停放大
    const float bg_parallax = 5.f * layoutScale;
    const float scale = 1.f + hover_val * 0.04f;

    ImVec2 card_min(p_min.x - tx * bg_parallax, p_min.y - ty * bg_parallax);
    ImVec2 card_max(p_max.x - tx * bg_parallax, p_max.y - ty * bg_parallax);
    card_min.x = center.x + (card_min.x - center.x) * scale;
    card_min.y = center.y + (card_min.y - center.y) * scale;
    card_max.x = center.x + (card_max.x - center.x) * scale;
    card_max.y = center.y + (card_max.y - center.y) * scale;

    myiui::ui::DrawGlassRect(dl, card_min, card_max, theme, 1.f);

    // [Layer 2] 动态环境光泽 — 四角渐变 Shimmer
    if (hover_val > 0.01f) {
        const float max_alpha = 0.12f * hover_val;
        const ImU32 tl_col =
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, std::max(0.f, -tx - ty) * max_alpha));
        const ImU32 tr_col =
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, std::max(0.f, tx - ty) * max_alpha));
        const ImU32 br_col =
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, std::max(0.f, tx + ty) * max_alpha));
        const ImU32 bl_col =
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, std::max(0.f, -tx + ty) * max_alpha));

        dl->PushClipRect(card_min, card_max, true);
        dl->AddRectFilledMultiColor(card_min, card_max, tl_col, tr_col, br_col, bl_col);
        dl->PopClipRect();
    }

    // [Layer 3] 前景文字 — 与鼠标同方向偏移，产生出屏悬浮感
    const UiFonts& fonts = GetUiFonts();
    ImFont* titleFont = fonts.semibold ? fonts.semibold : (fonts.regular ? fonts.regular : ImGui::GetFont());
    ImFont* descFont = fonts.regular ? fonts.regular : ImGui::GetFont();
    const float titleSize = (titleFont ? titleFont->FontSize : ImGui::GetFontSize()) * 1.1f * scale;
    const float descSize = (descFont ? descFont->FontSize : ImGui::GetFontSize()) * 0.9f * scale;

    const float padX = 20.f * layoutScale;
    const float titleY = 20.f * layoutScale;
    const float descY = 45.f * layoutScale;
    const float fg_parallax = 8.f * layoutScale * hover_val;
    const ImVec2 content_offset(tx * fg_parallax, ty * fg_parallax);

    const ImVec2 title_pos(card_min.x + padX + content_offset.x, card_min.y + titleY + content_offset.y);
    const ImVec2 desc_pos(card_min.x + padX + content_offset.x, card_min.y + descY + content_offset.y);

    if (titleFont && title && title[0]) {
        dl->AddText(titleFont, titleSize, title_pos, IM_COL32(255, 255, 255, 255), title);
    }
    if (descFont && desc && desc[0]) {
        dl->AddText(descFont, descSize, desc_pos, IM_COL32(180, 185, 195, 255), desc);
    }

    return pressed;
}

}  // namespace myiui::ui::widgets
