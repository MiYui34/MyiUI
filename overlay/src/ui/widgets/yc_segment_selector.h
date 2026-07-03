#pragma once
// 改编自 YCloud 自定义控件库 / 分段选择器.h

#include "imgui.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

#include <algorithm>
#include <cmath>

#undef min
#undef max

namespace myiui::ui::YcSegmentSelector {

inline int ClampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float ClampFloat(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float SmoothTo(float current, float target, float speed) {
    const float dt = ImGui::GetIO().DeltaTime;
    const float t = 1.f - std::exp(-speed * dt);
    return current + (target - current) * t;
}

inline ImU32 MulColor(ImU32 col, float k, float alpha_mul = 1.f) {
    int r = (col >> IM_COL32_R_SHIFT) & 0xFF;
    int g = (col >> IM_COL32_G_SHIFT) & 0xFF;
    int b = (col >> IM_COL32_B_SHIFT) & 0xFF;
    int a = (col >> IM_COL32_A_SHIFT) & 0xFF;
    r = ClampInt(static_cast<int>(r * k), 0, 255);
    g = ClampInt(static_cast<int>(g * k), 0, 255);
    b = ClampInt(static_cast<int>(b * k), 0, 255);
    a = ClampInt(static_cast<int>(a * alpha_mul), 0, 255);
    return IM_COL32(r, g, b, a);
}

struct Style {
    ImU32 bg_col = IM_COL32(31, 35, 45, 235);
    ImU32 border_col = IM_COL32(255, 255, 255, 32);
    ImU32 separator_col = IM_COL32(255, 255, 255, 24);
    ImU32 selected_col = IM_COL32(30, 144, 255, 255);
    ImU32 text_col = IM_COL32(190, 200, 215, 255);
    ImU32 text_sel_col = IM_COL32(255, 255, 255, 255);
    ImU32 hover_col = IM_COL32(255, 255, 255, 14);
    float height = 36.f;
    float rounding = 0.f;
    float padding = 3.f;
    float anim_speed = 14.f;
};

inline Style StyleFromTheme(const int accent[4], const int glass_tint[4], float alphaMul = 1.f) {
    Style s;
    s.bg_col = ColorFromRGBA(glass_tint, alphaMul);
    s.selected_col = ColorFromRGBA(accent, alphaMul);
    const int border[4]{255, 255, 255, 32};
    s.border_col = ColorFromRGBA(border, alphaMul);
    s.separator_col = ColorFromRGBA(border, alphaMul * 0.75f);
    const int textDim[4]{190, 200, 215, 255};
    s.text_col = ColorFromRGBA(textDim, alphaMul);
    s.text_sel_col = IM_COL32(255, 255, 255, static_cast<int>(255.f * alphaMul));
    s.hover_col = ColorFromRGBA(border, alphaMul * 0.45f);
    return s;
}

inline bool Draw(const char* id, int* current, const char* const* items, int count, float width, float ui_scale,
                 const Style& style = Style()) {
    if (!current || !items || count < 2) return false;
    count = ClampInt(count, 2, 8);
    *current = ClampInt(*current, 0, count - 1);

    const float us = ui_scale;
    const float h = Px(count > 4 ? 32.f : style.height, us);
    const float pad = Px(style.padding, us);
    const float rounding = style.rounding > 0.f ? Px(style.rounding, us) : h * 0.5f;
    const float w = Px(width, us);
    const float seg_w = w / static_cast<float>(count);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 total_size(w, h);

    ImGui::InvisibleButton(id, total_size);
    const bool hovered = ImGui::IsItemHovered();
    bool changed = false;

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float local_x = ImGui::GetIO().MousePos.x - pos.x;
        const int next = ClampInt(static_cast<int>(local_x / seg_w), 0, count - 1);
        if (next != *current) {
            *current = next;
            changed = true;
        }
    }

    const ImGuiID storage_id = ImGui::GetID(id);
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float anim_x = storage->GetFloat(storage_id, static_cast<float>(*current));
    anim_x = SmoothTo(anim_x, static_cast<float>(*current), style.anim_speed);
    if (std::fabs(anim_x - static_cast<float>(*current)) < 0.001f) anim_x = static_cast<float>(*current);
    storage->SetFloat(storage_id, anim_x);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + w, pos.y + h);

    const ImU32 bg = hovered ? MulColor(style.bg_col, 1.12f) : style.bg_col;
    dl->AddRectFilled(pos, max, bg, rounding);
    dl->AddRect(pos, max, style.border_col, rounding, 0, (std::max)(1.f, 1.f * us));

    for (int i = 1; i < count; ++i) {
        const float x = pos.x + seg_w * i;
        dl->AddLine(ImVec2(x, pos.y + 6.f * us), ImVec2(x, pos.y + h - 6.f * us), style.separator_col, 1.f * us);
    }

    const float sel_x = pos.x + seg_w * anim_x;
    const ImVec2 sel_min(sel_x + pad, pos.y + pad);
    const ImVec2 sel_max(sel_x + seg_w - pad, pos.y + h - pad);
    float sel_round = (std::max)(1.f, (h - pad * 2.f) * 0.5f);
    ImDrawFlags sel_flags = ImDrawFlags_RoundCornersNone;
    if (*current == 0) sel_flags |= ImDrawFlags_RoundCornersLeft;
    if (*current == count - 1) sel_flags |= ImDrawFlags_RoundCornersRight;
    if (*current > 0 && *current < count - 1) sel_round = (std::min)(sel_round, 10.f * us);

    dl->PushClipRect(pos, max, true);
    dl->AddRectFilled(sel_min, sel_max, style.selected_col, sel_round, sel_flags);
    dl->PopClipRect();

    if (hovered) {
        const float local_x = ImGui::GetIO().MousePos.x - pos.x;
        const int hover_idx = ClampInt(static_cast<int>(local_x / seg_w), 0, count - 1);
        if (hover_idx != *current) {
            const ImVec2 hm(pos.x + hover_idx * seg_w + pad, pos.y + pad);
            const ImVec2 hx(pos.x + (hover_idx + 1) * seg_w - pad, pos.y + h - pad);
            dl->AddRectFilled(hm, hx, style.hover_col, sel_round);
        }
    }

    for (int i = 0; i < count; ++i) {
        const char* text = items[i] ? items[i] : "";
        const float fontSize = ImGui::GetFontSize() * (count > 4 ? 0.82f : 1.f);
        const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        const ImVec2 text_pos(pos.x + seg_w * i + (seg_w - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f);
        const float dist = std::fabs(anim_x - static_cast<float>(i));
        const bool selected_now = dist < 0.5f;
        dl->AddText(ImGui::GetFont(), fontSize, text_pos, selected_now ? style.text_sel_col : style.text_col, text);
    }

    return changed;
}

} // namespace myiui::ui::YcSegmentSelector
