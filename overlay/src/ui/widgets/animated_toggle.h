#pragma once
// UI 管理器同款开关：圆角轨道 + 圆形滑块 + 缓动动画

#include "imgui.h"
#include "ui/easing.h"

#include <algorithm>
#include <cmath>

namespace myiui::ui::AnimatedToggle {

inline float ScalePx(float value, float ui_scale) {
    return value * ui_scale;
}

inline float AnimStep(float dt, float speed = 14.f) {
    return (std::min)(1.f, dt * speed);
}

inline ImU32 ScaleAlpha(ImU32 col, float alphaMul) {
    if (alphaMul >= 0.999f) return col;
    const int a = static_cast<int>(((col >> IM_COL32_A_SHIFT) & 0xFF) * alphaMul);
    return (col & ~IM_COL32_A_MASK) | (IM_COL32_A_MASK & (a << IM_COL32_A_SHIFT));
}

inline bool Draw(const char* id, bool* value, float ui_scale, const int accent[4], float* anim = nullptr,
                 float alphaMul = 1.f) {
    if (!value) return false;

    ImGui::PushID(id);
    const ImVec2 size(ScalePx(48.f, ui_scale), ScalePx(28.f, ui_scale));
    ImGui::InvisibleButton("##toggle", size);
    bool changed = false;
    if (ImGui::IsItemClicked()) {
        *value = !*value;
        changed = true;
    }

    const float dt = ImGui::GetIO().DeltaTime;
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID animId = ImGui::GetID("##anim");
    float animNorm = anim ? *anim : st->GetFloat(animId, *value ? 1.f : 0.f);
    animNorm = myiui::easing::Lerp(animNorm, *value ? 1.f : 0.f, AnimStep(dt));
    if (anim) {
        *anim = animNorm;
    } else {
        st->SetFloat(animId, animNorm);
    }
    const float eased = myiui::easing::EaseOutCubic(animNorm);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 offTrack = ScaleAlpha(IM_COL32(255, 255, 255, 25), alphaMul);
    const ImU32 onTrack = ScaleAlpha(IM_COL32(accent[0], accent[1], accent[2], 60), alphaMul);
    const ImU32 track = eased > 0.5f ? onTrack : offTrack;
    const float rounding = ScalePx(14.f, ui_scale);
    dl->AddRectFilled(min, max, track, rounding);
    dl->AddRect(min, max, ScaleAlpha(IM_COL32(255, 255, 255, 56), alphaMul), rounding, 0, 2.f);

    const float knobR = ScalePx(10.f, ui_scale);
    const float knobX =
        myiui::easing::Lerp(min.x + ScalePx(14.f, ui_scale), max.x - ScalePx(14.f, ui_scale), eased);
    dl->AddCircleFilled(
        ImVec2(knobX, (min.y + max.y) * 0.5f), knobR,
        ScaleAlpha(IM_COL32(static_cast<int>(myiui::easing::Lerp(178.f, static_cast<float>(accent[0]), eased)),
                              static_cast<int>(myiui::easing::Lerp(184.f, static_cast<float>(accent[1]), eased)),
                              static_cast<int>(myiui::easing::Lerp(196.f, static_cast<float>(accent[2]), eased)), 255),
                      alphaMul));

    ImGui::PopID();
    return changed;
}

} // namespace myiui::ui::AnimatedToggle
