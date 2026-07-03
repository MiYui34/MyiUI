#pragma once

#include "imgui.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

inline constexpr float kRefW = 1920.f;
inline constexpr float kRefH = 1080.f;

inline float UiScale(const ImVec2& display) {
    const float sx = display.x / kRefW;
    const float sy = display.y / kRefH;
    return sx < sy ? sx : sy;
}

inline float Px(float value, float scale) {
    return value * scale;
}

inline float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

inline void DrawTextStyled(ImDrawList* dl, ImFont* font, const ImVec2& pos, ImU32 color, const char* text) {
    if (font) {
        dl->AddText(font, font->FontSize, pos, color, text);
    } else {
        dl->AddText(pos, color, text);
    }
}
