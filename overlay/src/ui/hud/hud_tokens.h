#pragma once

namespace myiui::ui::hud {

// Vanilla MC HUD uses 182px hotbar width (9*20 + borders) and 81px per status cluster.
inline constexpr float kVanillaHotbarWidth = 182.f;
inline constexpr float kVanillaStatusWidth = 81.f;
inline constexpr float kVanillaHotbarOffsetY = 22.f;
inline constexpr float kVanillaXpOffsetY = 6.f;

inline float HudScaleForGui(int guiScale) {
    switch (guiScale) {
        case 1:
        case 2:
            return 1.f;
        case 4:
            return 1.1f;
        default:
            return 1.f;
    }
}

} // namespace myiui::ui::hud
