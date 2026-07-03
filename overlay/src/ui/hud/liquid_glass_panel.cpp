#include "ui/hud/liquid_glass_panel.h"

#include "render/liquid_glass_shader.h"
#include "ui/glass_panel.h"

#include <algorithm>

namespace myiui::ui::hud {

namespace {
bool g_inGameShader = false;
}

void SetInGameLiquidGlassShader(bool enabled) {
    g_inGameShader = enabled;
}

bool IsInGameLiquidGlassShader() {
    return g_inGameShader;
}

void DrawLiquidGlassPanel(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ThemeConfig& theme,
                          LiquidGlassMode mode, float alphaMul, bool selectedBoost, float cornerRadiusOverride) {
    const float rounding =
        cornerRadiusOverride > 0.f ? cornerRadiusOverride
                                   : (theme.corner_radius > 0.f ? theme.corner_radius : 16.f);
    if (g_inGameShader && mode != LiquidGlassMode::Flat && myiui::render::LiquidGlassReady()) {
        myiui::render::LiquidGlassDrawPanel(min, max, theme, alphaMul, selectedBoost, rounding);
        return;
    }

    if (mode == LiquidGlassMode::Flat) {
        DrawGlassRect(dl, min, max, theme, alphaMul);
        return;
    }

    const int tint[4]{90, 200, 250, mode == LiquidGlassMode::Tinted ? 28 : 22};
    const int border[4]{90, 200, 250, selectedBoost ? 140 : 90};
    DrawGlassSurface(dl, min, max, tint, border, rounding, theme.border_width, alphaMul);

    const float edge = selectedBoost ? 0.22f : 0.14f;
    const ImU32 fresnel = IM_COL32(90, 200, 250, static_cast<int>(255 * edge * alphaMul));
    dl->AddRect(min, max, fresnel, rounding, 0, 1.5f);

    const float panelH = max.y - min.y;
    if (panelH > 50.f) {
        const ImVec2 innerMin(min.x + 2.f, min.y + 1.f);
        const ImVec2 innerMax(max.x - 2.f, min.y + 4.f);
        dl->AddRectFilledMultiColor(innerMin, innerMax, IM_COL32(255, 255, 255, 25), IM_COL32(255, 255, 255, 25),
                                    IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    }

    const float chroma = selectedBoost ? 1.2f : 0.8f;
    dl->AddRect(ImVec2(min.x - chroma, min.y), ImVec2(max.x - chroma, max.y), IM_COL32(255, 80, 80, 18), rounding);
    dl->AddRect(ImVec2(min.x + chroma, min.y), ImVec2(max.x + chroma, max.y), IM_COL32(80, 140, 255, 18), rounding);
}

} // namespace myiui::ui::hud
