#include "ui/glass_panel.h"

namespace myiui::ui {

ImU32 ColorFromRGBA(const int rgba[4], float alphaMul) {
    const int a = static_cast<int>(rgba[3] * alphaMul);
    return IM_COL32(rgba[0], rgba[1], rgba[2], a > 255 ? 255 : a);
}

void DrawGlassSurface(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const int tint[4], const int border[4],
                      float rounding, float borderWidth, float alphaMul) {
    const ImU32 bg = ColorFromRGBA(tint, alphaMul);
    const ImU32 borderCol = ColorFromRGBA(border, alphaMul);
    dl->AddRectFilled(min, max, bg, rounding);
    if (borderWidth > 0.f) {
        dl->AddRect(min, max, borderCol, rounding, 0, borderWidth);
    }
}

void DrawGlassRect(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ThemeConfig& theme, float alphaMul) {
    DrawGlassSurface(dl, min, max, theme.glass_tint, theme.border_color, theme.corner_radius, theme.border_width,
                     alphaMul);
}

} // namespace myiui::ui
