#pragma once

namespace myiui::ui::chat {

inline constexpr float kPanelWidth = 480.f;
inline constexpr float kPanelMinHeight = 240.f;
inline constexpr float kPanelMaxHeight = 420.f;
inline constexpr float kPanelOffsetX = 16.f;
inline constexpr float kPanelOffsetY = 16.f;
inline constexpr float kPanelRadius = 14.f;
inline constexpr float kPanelPadX = 18.f;
inline constexpr float kPanelPadY = 16.f;
inline constexpr float kMessageGap = 6.f;
inline constexpr float kBaseFontPx = 16.f;
inline constexpr float kBaselineWidth = 1920.f;

inline float ChatUiScale(float viewportW, float viewportH) {
    const float sx = viewportW / kBaselineWidth;
    const float sy = viewportH / 1080.f;
    return sx < sy ? sx : sy;
}

} // namespace myiui::ui::chat
