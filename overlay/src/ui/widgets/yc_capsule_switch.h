#pragma once
// 改编自 YCloud 自定义控件库 / 胶囊开关控件.h — 胶囊轨道 + 白色滑块平移动画

#include "imgui.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

#include <cmath>

namespace myiui::ui::YcCapsuleSwitch {

inline float Clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

inline float SmoothTo(float current, float target, float speed) {
    const float dt = ImGui::GetIO().DeltaTime;
    const float t = 1.f - std::exp(-speed * dt);
    return current + (target - current) * t;
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

struct Style {
    float width = 52.f;
    float height = 28.f;
    float border = 3.f;
    float rounding = 18.f;
    float anim_speed = 12.f;
    ImU32 off_col = IM_COL32(90, 96, 110, 200);
    ImU32 on_col = IM_COL32(33, 150, 243, 255);
    ImU32 knob_col = IM_COL32(255, 255, 255, 255);
};

inline Style StyleFromTheme(const int accent[4], float alphaMul = 1.f) {
    Style s;
    s.on_col = ColorFromRGBA(accent, alphaMul);
    const int offTint[4]{90, 96, 110, 200};
    s.off_col = ColorFromRGBA(offTint, alphaMul);
    return s;
}

inline bool Draw(const char* id, bool* value, float ui_scale, const Style& style = Style()) {
    if (!value) return false;

    const float us = ui_scale;
    const float w = Px(style.width, us);
    const float h = Px(style.height, us);
    const float border = Px(style.border, us);
    const float rounding = Px(style.rounding, us);

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max(pos.x + w, pos.y + h);

    ImGui::InvisibleButton(id, ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool changed = false;

    if (clicked) {
        *value = !*value;
        changed = true;
    }

    const ImGuiID base_id = ImGui::GetID(id);
    ImGuiStorage* st = ImGui::GetStateStorage();

    const float target_on = *value ? 1.f : 0.f;
    float on_anim = st->GetFloat(base_id + 1, target_on);
    on_anim = SmoothTo(on_anim, target_on, style.anim_speed);
    if (std::fabs(on_anim - target_on) < 0.001f) on_anim = target_on;
    st->SetFloat(base_id + 1, on_anim);

    const float move = w * 0.5f;
    const float target_translate = held ? 0.f : (*value ? move : -move);
    float tx = st->GetFloat(base_id + 2, target_translate);
    tx = SmoothTo(tx, target_translate, style.anim_speed);
    if (std::fabs(tx - target_translate) < 0.01f) tx = target_translate;
    st->SetFloat(base_id + 2, tx);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg_col = LerpColor(style.off_col, style.on_col, on_anim);
    if (hovered) {
        bg_col = LerpColor(bg_col, IM_COL32(255, 255, 255, 255), 0.06f);
    }

    dl->AddRectFilled(pos, max, bg_col, rounding);

    dl->PushClipRect(pos, max, true);
    const float knob_x = pos.x + tx;
    const ImVec2 knob_min(knob_x, pos.y);
    const ImVec2 knob_max(knob_x + w, pos.y + h);
    dl->AddRectFilled(knob_min, knob_max, style.knob_col, rounding);
    dl->PopClipRect();

    dl->AddRect(pos, max, IM_COL32(255, 255, 255, hovered ? 35 : 18), rounding, 0, (std::max)(1.f, 1.f * us));

    return changed;
}

} // namespace myiui::ui::YcCapsuleSwitch
