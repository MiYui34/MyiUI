#include "ui/hud/hud_glass.h"

namespace myiui::ui::hud {

void DrawGlassPanel(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius, const int accent[4], float alpha) {
    if (!dl) return;
    const ImU32 bg = IM_COL32(12, 12, 16, static_cast<int>(120 * alpha));
    const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>(8 * alpha));
    const ImU32 border = IM_COL32(accent[0], accent[1], accent[2], static_cast<int>(80 * alpha));
    dl->AddRectFilled(min, max, bg, radius);
    dl->AddRectFilled(min, max, tint, radius);
    dl->AddRect(min, max, border, radius, 0, 1.f);
}

}  // namespace myiui::ui::hud
