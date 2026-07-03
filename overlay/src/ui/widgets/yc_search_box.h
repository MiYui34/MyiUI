#pragma once
// 改编自 YCloud 自定义控件库 / 搜索框.h — 浮动标签 + 底线展开动画

#include "imgui.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

#include <cmath>
#include <cstring>

namespace myiui::ui::YcSearchBox {

inline float Clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

inline float SmoothTo(float current, float target, float speed) {
    const float dt = ImGui::GetIO().DeltaTime;
    const float k = 1.f - std::exp(-speed * dt);
    return current + (target - current) * k;
}

inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
    t = Clamp01(t);
    const int ar = (a >> IM_COL32_R_SHIFT) & 0xFF;
    const int ag = (a >> IM_COL32_G_SHIFT) & 0xFF;
    const int ab = (a >> IM_COL32_B_SHIFT) & 0xFF;
    const int aa = (a >> IM_COL32_A_SHIFT) & 0xFF;
    const int br = (b >> IM_COL32_R_SHIFT) & 0xFF;
    const int bg = (b >> IM_COL32_G_SHIFT) & 0xFF;
    const int bb = (b >> IM_COL32_B_SHIFT) & 0xFF;
    const int ba = (b >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(static_cast<int>(ar + (br - ar) * t), static_cast<int>(ag + (bg - ag) * t),
                    static_cast<int>(ab + (bb - ab) * t), static_cast<int>(aa + (ba - aa) * t));
}

inline ImU32 WithAlpha(ImU32 col, float alpha_mul) {
    alpha_mul = Clamp01(alpha_mul);
    const int r = (col >> IM_COL32_R_SHIFT) & 0xFF;
    const int g = (col >> IM_COL32_G_SHIFT) & 0xFF;
    const int b = (col >> IM_COL32_B_SHIFT) & 0xFF;
    const int a = (col >> IM_COL32_A_SHIFT) & 0xFF;
    return IM_COL32(r, g, b, static_cast<int>(a * alpha_mul));
}

struct Style {
    float width = 280.f;
    float height = 52.f;
    float active_bg_height = 40.f;
    float line_height = 2.f;
    float rounding = 4.f;
    float padding_x = 10.f;
    float input_top = 18.f;
    float label_move_x = -8.f;
    float label_move_y = -30.f;
    float normal_font_scale = 1.f;
    float active_font_scale = 0.78f;
    float anim_speed = 8.f;
    ImU32 text_color = IM_COL32(245, 247, 250, 255);
    ImU32 label_color = IM_COL32(148, 163, 184, 255);
    ImU32 active_color = IM_COL32(90, 200, 250, 255);
};

inline Style StyleFromTheme(const int accent[4], const int text_primary[4], const int text_dim[4],
                            float alphaMul = 1.f) {
    Style s;
    s.text_color = ColorFromRGBA(text_primary, alphaMul);
    s.label_color = ColorFromRGBA(text_dim, alphaMul);
    s.active_color = ColorFromRGBA(accent, alphaMul);
    return s;
}

inline bool Draw(const char* id, const char* hint, char* buffer, size_t buffer_size, float ui_scale,
                 const Style& style = Style()) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float us = ui_scale;
    const ImVec2 total_size(Px(style.width, us), Px(style.height, us));

    ImGui::PushID(id);
    const ImGuiID anim_id = ImGui::GetID("##SearchBoxAnim");
    const ImGuiID focus_id = ImGui::GetID("##SearchBoxFocus");
    ImGuiStorage* storage = ImGui::GetStateStorage();

    const bool has_text = buffer && buffer[0] != '\0';
    const ImVec2 input_pos(pos.x, pos.y + Px(style.input_top, us) - Px(4.f, us));
    const ImVec2 input_size(Px(style.width, us), Px(style.active_bg_height, us));
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool mouse_hover_rect =
        mouse.x >= input_pos.x && mouse.x <= input_pos.x + input_size.x && mouse.y >= input_pos.y &&
        mouse.y <= input_pos.y + input_size.y;
    const bool mouse_clicked = ImGui::GetIO().MouseClicked[0];
    const bool last_focused = storage->GetFloat(focus_id, 0.f) > 0.5f;
    const bool active_pre = has_text || last_focused || (mouse_hover_rect && mouse_clicked);

    float anim = storage->GetFloat(anim_id, active_pre ? 1.f : 0.f);
    anim = SmoothTo(anim, active_pre ? 1.f : 0.f, style.anim_speed);
    storage->SetFloat(anim_id, anim);

    const float hover_alpha = mouse_hover_rect || active_pre ? 1.f : 0.92f;
    const float bg_h = Px(style.line_height, us) + (Px(style.active_bg_height, us) - Px(style.line_height, us)) * anim;
    const ImVec2 line_min(pos.x, pos.y + total_size.y - bg_h);
    const ImVec2 line_max(pos.x + total_size.x, pos.y + total_size.y);
    draw->AddRectFilled(line_min, line_max, WithAlpha(style.active_color, hover_alpha), Px(style.rounding, us));

    const float label_x = pos.x + Px(style.padding_x, us) + Px(style.label_move_x, us) * anim;
    const float label_y = pos.y + Px(style.input_top, us) + Px(style.label_move_y, us) * anim;
    const float font_size =
        ImGui::GetFontSize() * (style.normal_font_scale + (style.active_font_scale - style.normal_font_scale) * anim);
    const ImU32 label_col = LerpColor(style.label_color, style.active_color, anim);
    draw->AddText(ImGui::GetFont(), font_size, ImVec2(label_x, label_y), label_col, hint);

    ImGui::SetCursorScreenPos(input_pos);
    ImGui::PushItemWidth(input_size.x);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, style.text_color);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Px(style.padding_x, us), 10.f * us));

    const bool changed = ImGui::InputText("##input", buffer, buffer_size);
    const bool focused = ImGui::IsItemActive() || ImGui::IsItemFocused();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
    ImGui::PopItemWidth();
    storage->SetFloat(focus_id, focused ? 1.f : 0.f);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + total_size.y + Px(4.f, us)));
    ImGui::PopID();
    return changed;
}

} // namespace myiui::ui::YcSearchBox
