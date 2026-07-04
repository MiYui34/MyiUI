#include "ui/theme/material_you.h"

#include "config/config_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace myiui::ui::theme {

namespace {

float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Lab {
    float l, a, b;
};

Lab RgbToLab(int r, int g, int b) {
    auto srgb = [](int c) {
        float v = c / 255.f;
        return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
    };
    float R = srgb(r), G = srgb(g), B = srgb(b);
    float X = R * 0.4124564f + G * 0.3575761f + B * 0.1804375f;
    float Y = R * 0.2126729f + G * 0.7151522f + B * 0.0721750f;
    float Z = R * 0.0193339f + G * 0.1191920f + B * 0.9503041f;
    auto f = [](float t) { return t > 0.008856f ? std::cbrt(t) : (7.787f * t + 16.f / 116.f); };
    X /= 0.95047f;
    Z /= 1.08883f;
    float fx = f(X), fy = f(Y), fz = f(Z);
    return {116.f * fy - 16.f, 500.f * (fx - fy), 200.f * (fy - fz)};
}

RgbColor LabToRgb(float L, float A, float B) {
    float fy = (L + 16.f) / 116.f;
    float fx = A / 500.f + fy;
    float fz = fy - B / 200.f;
    auto inv = [](float t) {
        float t3 = t * t * t;
        return t3 > 0.008856f ? t3 : (t - 16.f / 116.f) / 7.787f;
    };
    float X = 0.95047f * inv(fx);
    float Y = inv(fy);
    float Z = 1.08883f * inv(fz);
    float r = X * 3.2404542f + Y * -1.5371385f + Z * -0.4985314f;
    float g = X * -0.9692660f + Y * 1.8760108f + Z * 0.0415560f;
    float b = X * 0.0556434f + Y * -0.2040259f + Z * 1.0572252f;
    auto lin = [](float c) {
        c = ClampF(c, 0.f, 1.f);
        return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.f / 2.4f) - 0.055f;
    };
    return {static_cast<int>(lin(r) * 255.f), static_cast<int>(lin(g) * 255.f), static_cast<int>(lin(b) * 255.f)};
}

}  // namespace

RgbColor QuantizeDominantFromRgba(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return {90, 200, 250};
    long long rSum = 0, gSum = 0, bSum = 0;
    int count = 0;
    const int stepX = std::max(1, w / 32);
    const int stepY = std::max(1, h / 32);
    for (int y = 0; y < h; y += stepY) {
        for (int x = 0; x < w; x += stepX) {
            const int i = (y * w + x) * 4;
            const uint8_t a = rgba[i + 3];
            if (a < 32) continue;
            const int lum = (rgba[i] * 299 + rgba[i + 1] * 587 + rgba[i + 2] * 114) / 1000;
            if (lum < 24 || lum > 232) continue;
            rSum += rgba[i];
            gSum += rgba[i + 1];
            bSum += rgba[i + 2];
            ++count;
        }
    }
    if (count == 0) return {90, 200, 250};
    RgbColor avg{static_cast<int>(rSum / count), static_cast<int>(gSum / count), static_cast<int>(bSum / count)};
    Lab lab = RgbToLab(avg.r, avg.g, avg.b);
    lab.l = ClampF(lab.l, 35.f, 75.f);
    lab.a = ClampF(lab.a, -40.f, 40.f);
    lab.b = ClampF(lab.b, -40.f, 40.f);
    return LabToRgb(lab.l + 8.f, lab.a * 1.1f, lab.b * 1.1f);
}

MaterialYouPalette BuildPaletteFromSeed(const RgbColor& seed) {
    MaterialYouPalette p{};
    p.accent = seed;
    p.accent_fill = {seed.r, seed.g, seed.b};
    p.accent_hover = {std::min(255, seed.r + 20), std::min(255, seed.g + 20), std::min(255, seed.b + 20)};
    p.surface = {18, 20, 28};
    p.on_surface = {245, 247, 250};
    p.border_accent = {seed.r, seed.g, seed.b};
    return p;
}

void ApplyPaletteToTheme(ThemeConfig& theme, const MaterialYouPalette& palette, float alphaMul) {
    theme.accent[0] = palette.accent.r;
    theme.accent[1] = palette.accent.g;
    theme.accent[2] = palette.accent.b;
    theme.accent[3] = 255;
    theme.accent_fill[0] = palette.accent_fill.r;
    theme.accent_fill[1] = palette.accent_fill.g;
    theme.accent_fill[2] = palette.accent_fill.b;
    theme.accent_fill[3] = static_cast<int>(184 * alphaMul);
    theme.accent_hover_bg[0] = palette.accent_hover.r;
    theme.accent_hover_bg[1] = palette.accent_hover.g;
    theme.accent_hover_bg[2] = palette.accent_hover.b;
    theme.accent_hover_bg[3] = static_cast<int>(31 * alphaMul);
    theme.border_accent[0] = palette.border_accent.r;
    theme.border_accent[1] = palette.border_accent.g;
    theme.border_accent[2] = palette.border_accent.b;
    theme.border_accent[3] = static_cast<int>(115 * alphaMul);
}

}  // namespace myiui::ui::theme
