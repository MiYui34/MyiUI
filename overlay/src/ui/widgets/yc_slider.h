#pragma once
// 改编自 YCloud 自定义控件库 / 滑条控件.h — 扁平竖向胶囊滑块，跟手无缓动延迟

#include "imgui.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

#undef min
#undef max

#include <algorithm>
#include <cmath>

namespace myiui::ui::YcSlider {

inline float ClampFloat(float v, float mn, float mx) {
    return v < mn ? mn : (v > mx ? mx : v);
}

inline float SafeRatio(float v, float mn, float mx) {
    if (std::fabs(mx - mn) < 0.00001f) return 0.f;
    return ClampFloat((v - mn) / (mx - mn), 0.f, 1.f);
}

struct Style {
    ImU32 track_col = IM_COL32(58, 63, 74, 255);
    ImU32 fill_col = IM_COL32(30, 144, 255, 255);
    ImU32 grab_col = IM_COL32(30, 144, 255, 255);
    ImU32 grab_hover_col = IM_COL32(44, 162, 255, 255);
    ImU32 grab_active_col = IM_COL32(0, 153, 255, 255);
    ImU32 grab_border_col = IM_COL32(255, 255, 255, 255);
};

inline Style StyleFromTheme(const int accent[4], float alphaMul = 1.f) {
    Style s;
    s.fill_col = ColorFromRGBA(accent, alphaMul);
    s.grab_col = s.fill_col;
    s.grab_hover_col = ColorFromRGBA(accent, (std::min)(1.f, alphaMul * 1.08f));
    s.grab_active_col = ColorFromRGBA(accent, (std::min)(1.f, alphaMul * 0.92f));
    const int trackTint[4]{255, 255, 255, 30};
    s.track_col = ColorFromRGBA(trackTint, alphaMul);
    s.grab_border_col = IM_COL32(255, 255, 255, static_cast<int>(245.f * alphaMul));
    return s;
}

// 选项行内嵌滑条：无右上角数值，适合左侧标签 + 右侧数值的布局
inline bool Draw(const char* id, float* value, float min_value, float max_value, float track_width, float ui_scale,
                 const Style& style = Style()) {
    if (!value) return false;

    const float us = ui_scale;
    const float track_w = Px(track_width, us);
    const float track_h = Px(8.f, us);
    const float track_round = Px(4.f, us);
    const float grab_w = Px(10.f, us);
    const float grab_h = Px(22.f, us);
    const float grab_round = Px(5.f, us);
    const float hit_h = (std::max)(Px(26.f, us), grab_h + Px(2.f, us));

    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float grab_pad = grab_w * 0.5f;
    const ImVec2 widget_size(track_w + grab_w, hit_h);
    const ImVec2 track_min(start.x + grab_pad, start.y + (hit_h - track_h) * 0.5f);
    const ImVec2 track_max(track_min.x + track_w, track_min.y + track_h);
    const ImVec2 track_center(track_min.x, track_min.y + track_h * 0.5f);

    ImGui::InvisibleButton("##yc_slider", widget_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    bool changed = false;
    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float mouse_x = ImGui::GetIO().MousePos.x;
        const float t = ClampFloat((mouse_x - track_min.x) / track_w, 0.f, 1.f);
        const float new_value = min_value + (max_value - min_value) * t;
        if (std::fabs(new_value - *value) > 0.0001f) {
            *value = new_value;
            changed = true;
        }
    }

    const float ratio = SafeRatio(*value, min_value, max_value);
    *value = min_value + (max_value - min_value) * ratio;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(track_min, track_max, style.track_col, track_round);

    const float fill_x = track_min.x + track_w * ratio;
    if (fill_x > track_min.x + 0.5f) {
        const ImVec2 fill_max(fill_x, track_max.y);
        dl->PushClipRect(track_min, track_max, true);
        dl->AddRectFilled(track_min, fill_max, style.fill_col, track_round);
        dl->PopClipRect();
    }

    const float grab_center_x = track_min.x + track_w * ratio;
    const ImVec2 grab_center(grab_center_x, track_center.y);
    const ImVec2 grab_min(grab_center.x - grab_w * 0.5f, grab_center.y - grab_h * 0.5f);
    const ImVec2 grab_max(grab_center.x + grab_w * 0.5f, grab_center.y + grab_h * 0.5f);
    const ImVec2 clipped_grab_min((std::max)(grab_min.x, start.x), grab_min.y);
    const ImVec2 clipped_grab_max((std::min)(grab_max.x, start.x + widget_size.x), grab_max.y);

    const ImU32 grab_col =
        active ? style.grab_active_col : (hovered ? style.grab_hover_col : style.grab_col);
    dl->AddRectFilled(clipped_grab_min, clipped_grab_max, grab_col, grab_round);
    dl->AddRect(clipped_grab_min, clipped_grab_max, IM_COL32(255, 255, 255, 245), grab_round, 0, (std::max)(1.f, 1.5f * us));

    ImGui::PopID();
    return changed;
}

} // namespace myiui::ui::YcSlider
