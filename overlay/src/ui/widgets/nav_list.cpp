#include "ui/widgets/nav_list.h"

#include "ui/glass_panel.h"
#include "ui/ui_scale.h"

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassSurface;

bool NavListRow(const char* id, const ImVec2& pos, const ImVec2& size, const char* label, const AppConfig& cfg,
                float& hoverT, float scale) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const float target = hovered ? 1.f : 0.f;
    hoverT = target + (hoverT - target) * 0.82f;

    auto* dl = ImGui::GetWindowDrawList();
    const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
    const int* fill = hovered ? cfg.theme.accent_hover_bg : cfg.theme.glass_tint;
    const int* border = hovered ? cfg.theme.border_accent : cfg.theme.border_color;
    DrawGlassSurface(dl, pos, rectMax, fill, border, Px(10.f, scale), cfg.theme.border_width, 1.f);

    const ImVec2 textSize = ImGui::CalcTextSize(label);
    DrawTextStyled(dl, ImGui::GetFont(), ImVec2(pos.x + Px(16.f, scale), pos.y + (size.y - textSize.y) * 0.5f),
                   ColorFromRGBA(cfg.theme.text_primary, 1.f), label);
    DrawTextStyled(dl, ImGui::GetFont(),
                   ImVec2(rectMax.x - Px(24.f, scale), pos.y + (size.y - textSize.y) * 0.5f),
                   ColorFromRGBA(cfg.theme.text_dim, 1.f), ">");
    return clicked;
}
