#pragma once

#include <cstdint>

struct ThemeConfig;

namespace myiui::ui::theme {

struct RgbColor {
    int r = 90;
    int g = 200;
    int b = 250;
};

struct MaterialYouPalette {
    RgbColor accent;
    RgbColor accent_fill;
    RgbColor accent_hover;
    RgbColor surface;
    RgbColor on_surface;
    RgbColor border_accent;
};

RgbColor QuantizeDominantFromRgba(const uint8_t* rgba, int w, int h);
MaterialYouPalette BuildPaletteFromSeed(const RgbColor& seed);
void ApplyPaletteToTheme(ThemeConfig& theme, const MaterialYouPalette& palette, float alphaMul = 1.f);

}  // namespace myiui::ui::theme
