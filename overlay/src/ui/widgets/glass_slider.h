#pragma once

#include "imgui.h"
#include "ui/ui_scale.h"

#undef min
#undef max

#include <algorithm>
#include <cmath>

// 1:1 跟手的玻璃风格滑条（拖拽时线性映射，无缓动延迟）
inline bool GlassSliderFloat(const char* id, float* value, float minV, float maxV, float scale, ImU32 fillColor,
                             float trackW = 160.f) {
    ImGui::PushID(id);
    const float tw = Px(trackW, scale);
    const float hitH = Px(28.f, scale);
    ImGui::InvisibleButton("##slider", ImVec2(tw, hitH));
    const ImVec2 barMin = ImGui::GetItemRectMin();
    const bool dragging = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool changed = false;
    if (dragging || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
        const float t = (ImGui::GetIO().MousePos.x - barMin.x) / tw;
        const float clamped = (std::max)(0.f, (std::min)(1.f, t));
        const float next = minV + clamped * (maxV - minV);
        if (next != *value) {
            *value = next;
            changed = true;
        }
    }

    const float range = maxV - minV;
    const float norm = range > 0.f ? ((*value - minV) / range) : 0.f;
    const float cy = barMin.y + hitH * 0.5f;
    const float barH = Px(6.f, scale);
    const ImVec2 bar0(barMin.x, cy - barH * 0.5f);
    const ImVec2 bar1(barMin.x + tw, cy + barH * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bar0, bar1, IM_COL32(255, 255, 255, 30), Px(3.f, scale));
    const ImVec2 fill1(barMin.x + tw * norm, bar1.y);
    if (fill1.x > bar0.x) {
        dl->AddRectFilled(bar0, fill1, fillColor, Px(3.f, scale));
    }
    const float knobR = Px(7.f, scale);
    const ImVec2 knob(barMin.x + tw * norm, cy);
    dl->AddCircleFilled(knob, knobR, IM_COL32(255, 255, 255, 245));
    dl->AddCircle(knob, knobR, fillColor, 0, 2.f);
    ImGui::PopID();
    return changed;
}
