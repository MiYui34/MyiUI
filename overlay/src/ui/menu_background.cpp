#include "ui/menu_background.h"

#include <algorithm>

#include "ui/easing.h"
#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/logo_assets.h"
#include "ui/menu_icons.h"
#include "ui/profile_card.h"
#include "ui/ui_manager.h"
#include "ui/ui_scale.h"

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassRect;
using myiui::ui::DrawGlassSurface;

static constexpr float kSafeMargin = 12.f;

static ImVec2 ClampPos(const ImVec2& pos, const ImVec2& size, const ImVec2& display) {
    ImVec2 out = pos;
    const float maxX = (std::max)(kSafeMargin, display.x - size.x - kSafeMargin);
    const float maxY = (std::max)(kSafeMargin, display.y - size.y - kSafeMargin);
    out.x = ClampF(out.x, kSafeMargin, maxX);
    out.y = ClampF(out.y, kSafeMargin, maxY);
    return out;
}

void DrawMenuBackground(MenuRenderContext& ctx) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    auto* bg = ImGui::GetBackgroundDrawList();
    if (ctx.hasBg && ctx.bgTexture && ctx.bgTexW > 0 && ctx.bgTexH > 0) {
        const float coverScale =
            (std::max)(display.x / static_cast<float>(ctx.bgTexW), display.y / static_cast<float>(ctx.bgTexH));
        const ImVec2 drawSize(ctx.bgTexW * coverScale, ctx.bgTexH * coverScale);
        const ImVec2 p0((display.x - drawSize.x) * 0.5f, (display.y - drawSize.y) * 0.5f);
        bg->AddImage(ctx.bgTexture, p0, ImVec2(p0.x + drawSize.x, p0.y + drawSize.y));
    } else if (ctx.hasBg && ctx.bgTexture) {
        bg->AddImage(ctx.bgTexture, ImVec2(0, 0), display);
    } else {
        bg->AddRectFilled(ImVec2(0, 0), display, ColorFromRGBA(ctx.cfg.background.fallback_color, 1.f));
    }

    bg->AddRectFilled(ImVec2(0, 0), display,
                      IM_COL32(8, 12, 20, static_cast<int>(140 * ctx.cfg.background.vignette_strength / 0.55f)));
    // Uniform scrim so glass panels stay readable on bright vanilla backgrounds.
    bg->AddRectFilled(ImVec2(0, 0), display, IM_COL32(10, 14, 22, 72));
    const float vignette = ctx.cfg.background.vignette_strength;
    bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, display.y * 0.25f),
                                IM_COL32(0, 0, 0, static_cast<int>(120 * vignette)),
                                IM_COL32(0, 0, 0, static_cast<int>(120 * vignette)), IM_COL32(0, 0, 0, 0),
                                IM_COL32(0, 0, 0, 0));
    bg->AddRectFilledMultiColor(ImVec2(0, display.y * 0.75f), display, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                IM_COL32(0, 0, 0, static_cast<int>(160 * vignette)),
                                IM_COL32(0, 0, 0, static_cast<int>(160 * vignette)));
}

namespace {

constexpr int kHoverManager = 0;
constexpr int kHoverProfile = 8;

}  // namespace

void DrawMenuTopBar(MenuRenderContext& ctx, bool inputsEnabled) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    auto* dl = ImGui::GetWindowDrawList();
    const float scale = ctx.scale;
    const float sideMargin = Px(48.f, scale);
    const float topMargin = Px(32.f, scale);
    const UiFonts& fonts = GetUiFonts();

    const float markSize = Px(36.f, scale);
    const ImVec2 logoMarkMin(sideMargin, topMargin);
    const ImVec2 logoMarkMax(logoMarkMin.x + markSize, logoMarkMin.y + markSize);
    const LogoSet& logos = GetLogos();
    if (logos.mark.valid()) {
        DrawLogoFit(dl, logos.mark, logoMarkMin, logoMarkMax, 1.f);
    } else {
        DrawGlassRect(dl, logoMarkMin, logoMarkMax, ctx.cfg.theme, 1.f);
        DrawTextStyled(dl, fonts.brand, ImVec2(logoMarkMin.x + Px(11.f, scale), logoMarkMin.y + Px(8.f, scale)),
                                  ColorFromRGBA(ctx.cfg.theme.text_primary, 1.f), "M");
    }

    const char* brandText = ctx.cfg.layout.brand_label.c_str();
    const ImVec2 brandSize = fonts.brand ? fonts.brand->CalcTextSizeA(fonts.brand->FontSize, FLT_MAX, 0.f, brandText)
                                         : ImGui::CalcTextSize(brandText);
    DrawTextStyled(dl, fonts.brand,
                   ImVec2(logoMarkMax.x + Px(10.f, scale), logoMarkMin.y + (markSize - brandSize.y) * 0.5f),
                   ColorFromRGBA(ctx.cfg.theme.text_primary, 1.f), brandText);

    if (inputsEnabled) {
        const ImVec2 mgrSize(Px(168.f, scale), Px(44.f, scale));
        ImVec2 mgrPos(display.x - sideMargin - mgrSize.x, topMargin);
        mgrPos = ClampPos(mgrPos, mgrSize, display);
        float& mgrHover = ctx.state.hover_anim[kHoverManager];
        ImGui::SetCursorScreenPos(mgrPos);
        ImGui::InvisibleButton("##mgr", mgrSize);
        const bool mgrHovered = ImGui::IsItemHovered();
        const bool mgrClicked = ImGui::IsItemClicked();
        const bool mgrPressed = ImGui::IsItemActive();
        const float mgrTarget = mgrHovered || mgrPressed ? 1.f : 0.f;
        mgrHover = myiui::easing::Lerp(mgrHover, mgrTarget, 0.18f);
        const ImVec2 mgrMin(mgrPos.x, mgrPos.y);
        const ImVec2 mgrMax(mgrPos.x + mgrSize.x, mgrPos.y + mgrSize.y);
        const ImVec2 mgrCenter(mgrPos.x + mgrSize.x * 0.5f, mgrPos.y + mgrSize.y * 0.5f);
        const int* mgrFill = mgrHovered ? ctx.cfg.theme.accent_hover_bg : ctx.cfg.theme.glass_tint;
        const int* mgrBorder = mgrHovered ? ctx.cfg.theme.border_accent : ctx.cfg.theme.border_color;
        DrawGlassSurface(dl, mgrMin, mgrMax, mgrFill, mgrBorder, Px(ctx.cfg.theme.corner_radius, scale),
                         ctx.cfg.theme.border_width, 1.f);
        const float iconSize = Px(20.f, scale);
        const ImVec2 iconCenter(mgrMin.x + Px(18.f, scale) + iconSize * 0.5f, mgrCenter.y);
        DrawMenuIcon(dl, MenuIcon::Manager, iconCenter, iconSize,
                     mgrHovered ? ColorFromRGBA(ctx.cfg.theme.accent, 1.f)
                                : ColorFromRGBA(ctx.cfg.theme.text_secondary, 1.f),
                     2.f);
        const char* mgrLabel = ctx.cfg.layout.manager_label.c_str();
        const ImVec2 mgrLabelSize = fonts.regular
                                        ? fonts.regular->CalcTextSizeA(fonts.regular->FontSize, FLT_MAX, 0.f, mgrLabel)
                                        : ImGui::CalcTextSize(mgrLabel);
        DrawTextStyled(dl, fonts.regular,
                       ImVec2(iconCenter.x + iconSize * 0.5f + Px(8.f, scale),
                                mgrCenter.y - mgrLabelSize.y * 0.5f),
                       ColorFromRGBA(ctx.cfg.theme.text_primary, 1.f), mgrLabel);
        if (mgrClicked) {
            ctx.state.show_manager = true;
            ctx.state.manager.library.needs_refresh = true;
        }
    }
}

void DrawMenuProfile(MenuRenderContext& ctx, float clusterTop, float clusterH) {
    if (!ctx.state.manager.settings.show_profile) return;

    const float deltaMs = ImGui::GetIO().DeltaTime * 1000.f;
    ProfileCardEnsureLoaded(ctx.state, deltaMs);
    ProfileCardUpdateAvatar(ctx.state);

    const float scale = ctx.scale;
    const float sideMargin = Px(48.f, scale);
    const float profileW = Px(ctx.cfg.layout.profile_card_w, scale);
    const float profileH = Px(168.f, scale);
    const ImVec2 profilePos(sideMargin, clusterTop + (clusterH - profileH) * 0.5f);
    DrawProfileCard(ImGui::GetWindowDrawList(), profilePos, ImVec2(profileW, profileH), ctx.cfg, GetUiFonts(), scale,
                    ctx.state.hover_anim[kHoverProfile], ctx.state.data.profile);
}
