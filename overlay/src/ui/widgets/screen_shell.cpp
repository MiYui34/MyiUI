#include "ui/widgets/screen_shell.h"

#include <algorithm>
#include <string>

#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/strings_zh.h"
#include "ui/ui_scale.h"

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassSurface;

ScreenShellLayout CalcScreenShellLayout(const AppConfig& cfg, float scale) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float sideMargin = Px(48.f, scale);
    const float panelW = Px(cfg.components.content_panel_w, scale);
    const float panelH = Px(560.f, scale);
    const float panelX = display.x - sideMargin - panelW;
    const float panelY = display.y * 0.5f - panelH * 0.5f;
    const float pad = Px(cfg.components.content_padding, scale);
    const float headerH = Px((std::max)(cfg.components.shell_header_h, 100.f), scale);

    ScreenShellLayout layout{};
    layout.panel_pos = ImVec2(panelX, panelY);
    layout.panel_size = ImVec2(panelW, panelH);
    layout.content_pos = ImVec2(panelX + pad, panelY + headerH);
    const float bottomInset = Px((std::max)(cfg.theme.corner_radius, 8.f), scale);
    layout.content_size = ImVec2(panelW - pad * 2.f, panelH - headerH - pad - bottomInset);
    return layout;
}

void DrawScreenShellBackground(ImDrawList* dl, const ScreenShellLayout& layout, const AppConfig& cfg, float scale,
                               float contentAlpha) {
    const ImVec2 panelMax(layout.panel_pos.x + layout.panel_size.x, layout.panel_pos.y + layout.panel_size.y);
    DrawGlassSurface(dl, layout.panel_pos, panelMax, cfg.theme.glass_tint_strong, cfg.theme.border_color,
                     Px(cfg.theme.corner_radius, scale), cfg.theme.border_width, contentAlpha);
}

bool ScreenShellHeader(const char* id, const ScreenShellLayout& layout, const AppConfig& cfg, const UiFonts& fonts,
                       float scale, const char* title, const char* subtitle, bool backEnabled, float& backHover) {
    auto* dl = ImGui::GetWindowDrawList();
    const float pad = Px(cfg.components.content_padding, scale);
    const ImVec2 headerMin(layout.panel_pos.x + pad, layout.panel_pos.y + Px(12.f, scale));

    const ImVec2 backSize(Px(52.f, scale), Px(28.f, scale));
    const ImVec2 backPos(headerMin.x, headerMin.y);
    ImGui::SetCursorScreenPos(backPos);
    ImGui::InvisibleButton((std::string(id) + "_back").c_str(), backSize);
    const bool backHovered = ImGui::IsItemHovered();
    const bool backClicked = ImGui::IsItemClicked();
    backHover = backHovered ? 1.f : 0.f;
    const ImVec2 backMax(backPos.x + backSize.x, backPos.y + backSize.y);
    const int* fill = backHovered ? cfg.theme.accent_hover_bg : cfg.theme.glass_tint;
    DrawGlassSurface(dl, backPos, backMax, fill, cfg.theme.border_color, Px(8.f, scale), cfg.theme.border_width, 1.f);
    const ImVec2 textSize = fonts.regular->CalcTextSizeA(fonts.regular->FontSize, FLT_MAX, 0.f, myiui::strings::kBack);
    DrawTextStyled(dl, fonts.regular,
                   ImVec2(backPos.x + (backSize.x - textSize.x) * 0.5f, backPos.y + (backSize.y - textSize.y) * 0.5f),
                   ColorFromRGBA(cfg.theme.text_primary, 1.f), myiui::strings::kBack);

    const float titleY = backPos.y + backSize.y + Px(10.f, scale);
    DrawTextStyled(dl, fonts.semibold, ImVec2(headerMin.x, titleY), ColorFromRGBA(cfg.theme.text_primary, 1.f), title);
    if (subtitle && subtitle[0]) {
        DrawTextStyled(dl, fonts.regular, ImVec2(headerMin.x, titleY + Px(26.f, scale)),
                       ColorFromRGBA(cfg.theme.text_dim, 1.f), subtitle);
    }
    return backClicked && backEnabled;
}

bool ScreenShellApplyButton(const char* id, const ScreenShellLayout& layout, const AppConfig& cfg,
                            const UiFonts& fonts, float scale, bool hasPending, float& hover) {
    auto* dl = ImGui::GetWindowDrawList();
    const float pad = Px(cfg.components.content_padding, scale);
    const ImVec2 btnSize(Px(56.f, scale), Px(28.f, scale));
    const ImVec2 btnPos(layout.panel_pos.x + layout.panel_size.x - pad - btnSize.x, layout.panel_pos.y + Px(12.f, scale));
    ImGui::SetCursorScreenPos(btnPos);
    ImGui::InvisibleButton((std::string(id) + "_apply").c_str(), btnSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    hover = hovered ? 1.f : 0.f;

    const ImVec2 btnMax(btnPos.x + btnSize.x, btnPos.y + btnSize.y);
    const int* fill = hasPending || hovered ? cfg.theme.accent_hover_bg : cfg.theme.glass_tint;
    const int* border = hasPending ? cfg.theme.border_accent : cfg.theme.border_color;
    DrawGlassSurface(dl, btnPos, btnMax, fill, border, Px(8.f, scale), cfg.theme.border_width, 1.f);

    const ImU32 textCol =
        hasPending ? ColorFromRGBA(cfg.theme.accent, 1.f) : ColorFromRGBA(cfg.theme.text_primary, 1.f);
    const ImVec2 textSize =
        fonts.regular->CalcTextSizeA(fonts.regular->FontSize, FLT_MAX, 0.f, myiui::strings::kApply);
    DrawTextStyled(dl, fonts.regular,
                   ImVec2(btnPos.x + (btnSize.x - textSize.x) * 0.5f, btnPos.y + (btnSize.y - textSize.y) * 0.5f),
                   textCol, myiui::strings::kApply);
    return clicked;
}

ImVec2 ScreenContentMin(const ScreenShellLayout& layout) {
    return layout.content_pos;
}

ImVec2 ScreenContentMax(const ScreenShellLayout& layout) {
    return ImVec2(layout.content_pos.x + layout.content_size.x, layout.content_pos.y + layout.content_size.y);
}

void ScreenContentClipGuard::Begin(ImDrawList* drawList, const ScreenShellLayout& layout) {
    dl = drawList;
    const ImVec2 clipMin = ScreenContentMin(layout);
    const ImVec2 clipMax = ScreenContentMax(layout);
    dl->PushClipRect(clipMin, clipMax, true);
    ImGui::PushClipRect(clipMin, clipMax, true);
}

void ScreenContentClipGuard::End() {
    if (!dl) return;
    dl->PopClipRect();
    ImGui::PopClipRect();
    dl = nullptr;
}

bool BeginScreenContentScroll(const char* id, const ImVec2& pos, const ImVec2& size) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    return ImGui::BeginChild(id, size, ImGuiChildFlags_None,
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
}

void EndScreenContentScroll() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

bool BeginScreenContentScroll(const char* id, const ScreenShellLayout& layout, float footerReserve) {
    ImVec2 size(layout.content_size.x, layout.content_size.y - footerReserve);
    if (size.y < 0.f) size.y = 0.f;
    return BeginScreenContentScroll(id, layout.content_pos, size);
}
