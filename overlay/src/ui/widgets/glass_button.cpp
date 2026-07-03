#include "ui/widgets/glass_button.h"

#include "ui/easing.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

#include <unordered_map>

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassSurface;

static std::unordered_map<std::string, float> g_glassPressAnim;

bool GlassButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, const AppConfig& cfg,
                 float& hoverT, bool& pressed, ImFont* font, GlassButtonStyle style, MenuIcon icon, float scale,
                 bool enabled) {
    auto* dl = ImGui::GetWindowDrawList();
    const float round = Px(cfg.theme.corner_radius, scale);

    if (!enabled) {
        hoverT = myiui::easing::Lerp(hoverT, 0.f, 0.25f);
        pressed = false;

        const ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        const ImVec2 min = ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        const ImVec2 max = ImVec2(center.x + size.x * 0.5f, center.y + size.y * 0.5f);
        constexpr float kDisabledAlpha = 0.42f;
        DrawGlassSurface(dl, min, max, cfg.theme.glass_tint, cfg.theme.border_color, round, cfg.theme.border_width,
                         kDisabledAlpha);

        const ImVec2 textSize = font ? font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, label)
                                     : ImGui::CalcTextSize(label);
        const float textX = min.x + (size.x - textSize.x) * 0.5f;
        DrawTextStyled(dl, font, ImVec2(textX, center.y - textSize.y * 0.5f),
                       ColorFromRGBA(cfg.theme.text_dim, kDisabledAlpha), label);
        return false;
    }

    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    pressed = ImGui::IsItemActive();

    const float target = hovered || pressed ? 1.f : 0.f;
    hoverT = myiui::easing::Lerp(hoverT, target, 0.18f);
    const float ease = myiui::easing::EaseOutCubic(hoverT);
    const float alphaMul = 1.f + (cfg.theme.hover_alpha_mul - 1.f) * ease;
    const float hoverScale = myiui::easing::Lerp(1.f, cfg.motion.hover_scale, ease);
    if (pressed) {
        float& p = g_glassPressAnim[id];
        p = myiui::easing::Lerp(p, 1.f, 0.35f);
    } else {
        g_glassPressAnim[id] = myiui::easing::Lerp(g_glassPressAnim[id], 0.f, 0.25f);
    }
    const float pressScale = myiui::easing::Lerp(1.f, cfg.motion.press_scale, g_glassPressAnim[id]);

    const ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    const ImVec2 scaledSize = ImVec2(size.x * hoverScale * pressScale, size.y * hoverScale * pressScale);
    const ImVec2 min = ImVec2(center.x - scaledSize.x * 0.5f, center.y - scaledSize.y * 0.5f);
    const ImVec2 max = ImVec2(center.x + scaledSize.x * 0.5f, center.y + scaledSize.y * 0.5f);

    const int* fillTint = cfg.theme.glass_tint;
    const int* borderTint = cfg.theme.border_color;
    if (style == GlassButtonStyle::Primary) {
        fillTint = cfg.theme.accent_fill;
        borderTint = cfg.theme.border_accent;
    } else if (style == GlassButtonStyle::Danger && (hovered || pressed)) {
        fillTint = cfg.theme.accent_hover_bg;
        borderTint = cfg.theme.border_accent;
    } else if (hovered || pressed) {
        fillTint = cfg.theme.accent_hover_bg;
        borderTint = cfg.theme.border_accent;
    }

    DrawGlassSurface(dl, min, max, fillTint, borderTint, round, cfg.theme.border_width, alphaMul);

    const bool primary = style == GlassButtonStyle::Primary;
    const bool danger = style == GlassButtonStyle::Danger;
    const ImU32 textCol = primary ? IM_COL32(255, 255, 255, 255) : ColorFromRGBA(cfg.theme.text_primary, alphaMul);
    const ImU32 iconCol = primary ? IM_COL32(255, 255, 255, 255)
                                  : danger && hovered ? ColorFromRGBA(cfg.theme.danger, alphaMul)
                                                      : ColorFromRGBA(cfg.theme.text_secondary, alphaMul);

    const ImVec2 textSize = font ? font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, label)
                                 : ImGui::CalcTextSize(label);
    if (icon != MenuIcon::None) {
        const float iconSize = Px(24.f, scale);
        const float gap = Px(6.f, scale);
        const float groupW = iconSize + gap + textSize.x;
        const float groupStart = center.x - groupW * 0.5f;
        const ImVec2 iconCenter(groupStart + iconSize * 0.5f, center.y);
        DrawMenuIcon(dl, icon, iconCenter, iconSize, iconCol, 2.f);
        DrawTextStyled(dl, font, ImVec2(groupStart + iconSize + gap, center.y - textSize.y * 0.5f), textCol, label);
    } else {
        const float textX = min.x + (size.x - textSize.x) * 0.5f;
        DrawTextStyled(dl, font, ImVec2(textX, center.y - textSize.y * 0.5f), textCol, label);
    }
    return clicked;
}
