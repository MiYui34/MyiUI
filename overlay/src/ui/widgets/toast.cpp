#include "ui/widgets/toast.h"

#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

#include <algorithm>
#include <cmath>

using myiui::ui::ColorFromRGBA;

void ToastShow(ToastState& toast, const std::string& message, float durationMs, bool isError) {
    toast.title = isError ? "\xe9\x94\x99\xe8\xaf\xaf" : "\xe6\x8f\x90\xe7\xa4\xba";
    toast.message = message;
    toast.remaining_ms = durationMs;
    toast.is_error = isError;
    toast.anim = 0.f;
}

void ToastUpdate(ToastState& toast, float deltaMs) {
    if (toast.message.empty()) return;
    const float dt = deltaMs * 0.001f;
    const float anim_speed = 6.f;
    toast.anim += (1.f - toast.anim) * (std::min)(dt * anim_speed, 1.f);
    if (toast.anim > 0.999f) toast.anim = 1.f;

    if (toast.remaining_ms > 0.f) {
        toast.remaining_ms -= deltaMs;
        if (toast.remaining_ms <= 0.f) {
            toast.message.clear();
            toast.title.clear();
            toast.anim = 0.f;
        }
    }
}

void ToastRender(const ToastState& toast, float scale) {
    if (toast.message.empty()) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float us = scale;
    const float card_w = Px(300.f, us);
    const float card_h = Px(64.f, us);
    const float margin = Px(20.f, us);
    const float fade = (std::min)(1.f, toast.remaining_ms / 300.f) * toast.anim;
    const float slide_x = (1.f - toast.anim) * Px(60.f, us);

    const float start_x = display.x - card_w - margin + slide_x;
    const float card_y = display.y - margin - card_h;
    const ImVec2 card_min(start_x, card_y);
    const ImVec2 card_max(start_x + card_w, card_y + card_h);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    dl->AddRectFilled(ImVec2(card_min.x + 2.f, card_min.y + 3.f), ImVec2(card_max.x + 2.f, card_max.y + 3.f),
                      IM_COL32(0, 0, 0, static_cast<int>(25.f * fade)), Px(14.f, us));

    dl->AddRectFilled(card_min, card_max, IM_COL32(255, 255, 255, static_cast<int>(240.f * fade)), Px(14.f, us));

    const float dot_r = Px(9.f, us);
    const ImVec2 dot_center(card_min.x + Px(26.f, us), card_min.y + card_h * 0.5f);
    const ImU32 dot_color = toast.is_error ? IM_COL32(244, 67, 54, static_cast<int>(255.f * fade))
                                           : IM_COL32(76, 175, 80, static_cast<int>(255.f * fade));
    dl->AddCircleFilled(dot_center, dot_r, dot_color, 32);

    const float text_x = card_min.x + Px(50.f, us);
    const float title_y = card_min.y + Px(10.f, us);
    const float body_y = card_min.y + Px(30.f, us);
    if (!toast.title.empty()) {
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.95f, ImVec2(text_x, title_y),
                    IM_COL32(30, 41, 59, static_cast<int>(255.f * fade)), toast.title.c_str());
    }
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(text_x, body_y),
                IM_COL32(71, 85, 105, static_cast<int>(230.f * fade)), toast.message.c_str());
}
