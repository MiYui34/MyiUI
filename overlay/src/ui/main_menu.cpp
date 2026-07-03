#include "ui/main_menu.h"

#include "ipc/pipe_client.h"
#include "ui/easing.h"
#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/logo_assets.h"
#include "ui/media_library.h"
#include "ui/ui_manager.h"
#include "ui/strings_zh.h"
#include "ui/menu_icons.h"

#include <windows.h>

#undef min
#undef max

#include <algorithm>
#include <cstring>
#include <unordered_map>

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassRect;
using myiui::ui::DrawGlassSurface;

static std::unordered_map<std::string, float> g_pressAnim;

static constexpr float kRefW = 1920.f;
static constexpr float kRefH = 1080.f;
static constexpr float kSafeMargin = 12.f;

enum class TextAlign { Center, Left };
enum class NavButtonStyle { Default, Primary, Danger };

static float UiScale(const ImVec2& display) {
    return (std::min)(display.x / kRefW, display.y / kRefH);
}

static float Px(float value, float scale) {
    return value * scale;
}

static float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static ImVec2 ClampPos(const ImVec2& pos, const ImVec2& size, const ImVec2& display) {
    ImVec2 out = pos;
    const float maxX = (std::max)(kSafeMargin, display.x - size.x - kSafeMargin);
    const float maxY = (std::max)(kSafeMargin, display.y - size.y - kSafeMargin);
    out.x = ClampF(out.x, kSafeMargin, maxX);
    out.y = ClampF(out.y, kSafeMargin, maxY);
    return out;
}

static void DrawTextStyled(ImDrawList* dl, ImFont* font, const ImVec2& pos, ImU32 color, const char* text) {
    if (font) {
        dl->AddText(font, font->FontSize, pos, color, text);
    } else {
        dl->AddText(pos, color, text);
    }
}

static NavButtonStyle ResolveNavStyle(const NavItem& item) {
    if (item.style == "primary") return NavButtonStyle::Primary;
    if (item.style == "danger") return NavButtonStyle::Danger;
    if (item.id == "single") return NavButtonStyle::Primary;
    if (item.id == "quit") return NavButtonStyle::Danger;
    return NavButtonStyle::Default;
}

static MenuIcon NavIconForItem(const NavItem& item) {
    if (item.id == "single") return MenuIcon::Singleplayer;
    if (item.id == "multi") return MenuIcon::Multiplayer;
    if (item.id == "options") return MenuIcon::Options;
    if (item.id == "quit") return MenuIcon::Quit;
    return MenuIcon::Options;
}

static bool MenuGlassButton(const char* id, const ImVec2& pos, const ImVec2& size, const char* label,
                            const AppConfig& cfg, float& hoverT, bool& pressed, ImFont* font, NavButtonStyle style,
                            MenuIcon icon) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    pressed = ImGui::IsItemActive();

    const float target = hovered || pressed ? 1.f : 0.f;
    hoverT = myiui::easing::Lerp(hoverT, target, 0.18f);
    const float ease = myiui::easing::EaseOutCubic(hoverT);
    const float alphaMul = 1.f + (cfg.theme.hover_alpha_mul - 1.f) * ease;
    const float scale = myiui::easing::Lerp(1.f, cfg.motion.hover_scale, ease);
    if (pressed) {
        float& p = g_pressAnim[id];
        p = myiui::easing::Lerp(p, 1.f, 0.35f);
    } else {
        g_pressAnim[id] = myiui::easing::Lerp(g_pressAnim[id], 0.f, 0.25f);
    }
    const float pressScale = myiui::easing::Lerp(1.f, cfg.motion.press_scale, g_pressAnim[id]);

    const ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
    const ImVec2 scaledSize = ImVec2(size.x * scale * pressScale, size.y * scale * pressScale);
    const ImVec2 min = ImVec2(center.x - scaledSize.x * 0.5f, center.y - scaledSize.y * 0.5f);
    const ImVec2 max = ImVec2(center.x + scaledSize.x * 0.5f, center.y + scaledSize.y * 0.5f);

    auto* dl = ImGui::GetWindowDrawList();
    const float round = Px(cfg.theme.corner_radius, ImGui::GetIO().DisplaySize.x / kRefW);

    const int* fillTint = cfg.theme.glass_tint;
    const int* borderTint = cfg.theme.border_color;
    if (style == NavButtonStyle::Primary) {
        fillTint = cfg.theme.accent_fill;
        borderTint = cfg.theme.border_accent;
    } else if (hovered || pressed) {
        fillTint = cfg.theme.accent_hover_bg;
        borderTint = cfg.theme.border_accent;
    }

    DrawGlassSurface(dl, min, max, fillTint, borderTint, round, cfg.theme.border_width, alphaMul);

    const bool primary = style == NavButtonStyle::Primary;
    const bool danger = style == NavButtonStyle::Danger;
    const ImU32 textCol = primary ? IM_COL32(255, 255, 255, 255) : ColorFromRGBA(cfg.theme.text_primary, alphaMul);
    const ImU32 iconCol = primary ? IM_COL32(255, 255, 255, 255)
                                  : danger && hovered
                                        ? ColorFromRGBA(cfg.theme.danger, alphaMul)
                                        : ColorFromRGBA(cfg.theme.text_secondary, alphaMul);

    const float iconSize = Px(24.f, ImGui::GetIO().DisplaySize.x / kRefW);
    const float iconPad = Px(18.f, ImGui::GetIO().DisplaySize.x / kRefW);
    const ImVec2 iconCenter(min.x + iconPad + iconSize * 0.5f, center.y);
    DrawMenuIcon(dl, icon, iconCenter, iconSize, iconCol, 2.f);

    const ImVec2 textSize = font ? font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, label)
                                 : ImGui::CalcTextSize(label);
    const float textX = min.x + iconPad + iconSize + Px(14.f, ImGui::GetIO().DisplaySize.x / kRefW);
    DrawTextStyled(dl, font, ImVec2(textX, center.y - textSize.y * 0.5f), textCol, label);
    return clicked;
}

static void DrawProfileCard(ImDrawList* dl, const ImVec2& pos, const ImVec2& size, const AppConfig& cfg,
                            const UiFonts& fonts, float scale, float hoverT) {
    const float alphaMul = 1.f + (cfg.theme.hover_alpha_mul - 1.f) * hoverT * 0.35f;
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    const float round = Px(cfg.theme.profile_corner_radius, scale);
    DrawGlassSurface(dl, pos, max, cfg.theme.glass_tint_strong, cfg.theme.border_color, round,
                     cfg.theme.border_width, alphaMul);

    const float pad = Px(20.f, scale);
    const float avatarSize = Px(48.f, scale);
    const float avatarRound = Px(12.f, scale);
    const ImVec2 avatarMin(pos.x + pad, pos.y + pad);
    const ImVec2 avatarMax(avatarMin.x + avatarSize, avatarMin.y + avatarSize);
    DrawGlassSurface(dl, avatarMin, avatarMax, cfg.theme.glass_tint, cfg.theme.border_color, avatarRound,
                     cfg.theme.border_width, alphaMul);
    DrawTextStyled(dl, fonts.semibold,
                   ImVec2(avatarMin.x + avatarSize * 0.5f - Px(10.f, scale),
                          avatarMin.y + avatarSize * 0.5f - Px(8.f, scale)),
                   ColorFromRGBA(cfg.theme.text_secondary, alphaMul), "PL");

    const float textX = avatarMax.x + Px(14.f, scale);
    const float headerMidY = pos.y + pad + avatarSize * 0.5f;
    DrawTextStyled(dl, fonts.profileName, ImVec2(textX, headerMidY - Px(16.f, scale)),
                   ColorFromRGBA(cfg.theme.text_primary, alphaMul), myiui::strings::kProfileName);
    DrawTextStyled(dl, fonts.caption, ImVec2(textX, headerMidY + Px(2.f, scale)),
                   ColorFromRGBA(cfg.theme.text_dim, alphaMul), myiui::strings::kProfileUuid);

    const float statusY = avatarMax.y + Px(16.f, scale);
    const ImVec2 statusMin(pos.x + pad, statusY);
    const ImVec2 statusMax(pos.x + size.x - pad, statusY + Px(36.f, scale));
    DrawGlassSurface(dl, statusMin, statusMax, cfg.theme.glass_tint, cfg.theme.border_color, Px(10.f, scale), 2.f,
                     alphaMul * 0.85f);
    const float dotR = Px(4.f, scale);
    const ImVec2 dotCenter(statusMin.x + Px(12.f, scale), (statusMin.y + statusMax.y) * 0.5f);
    dl->AddCircleFilled(dotCenter, dotR, ColorFromRGBA(cfg.theme.accent, alphaMul));
    DrawTextStyled(dl, fonts.profileSub, ImVec2(dotCenter.x + Px(14.f, scale), statusMin.y + Px(10.f, scale)),
                   ColorFromRGBA(cfg.theme.text_secondary, alphaMul), myiui::strings::kProfileStatus);

    const float metaY = statusMax.y + Px(14.f, scale);
    DrawTextStyled(dl, fonts.caption, ImVec2(pos.x + pad, metaY), ColorFromRGBA(cfg.theme.text_dim, alphaMul),
                   myiui::strings::kProfileMetaLeft);
    const char* metaRight = myiui::strings::kProfileMetaRight;
    const ImVec2 metaRightSize = fonts.caption ? fonts.caption->CalcTextSizeA(fonts.caption->FontSize, FLT_MAX, 0.f, metaRight)
                                               : ImGui::CalcTextSize(metaRight);
    DrawTextStyled(dl, fonts.caption, ImVec2(max.x - pad - metaRightSize.x, metaY),
                   ColorFromRGBA(cfg.theme.text_dim, alphaMul), metaRight);
}

void MainMenuInit(const AppConfig& cfg) {
    MediaLibraryLimits limits;
    limits.max_image_bytes = cfg.background.max_image_bytes;
    limits.max_video_bytes = cfg.background.max_video_bytes;
    MediaLibrarySetLimits(limits);
}

void MainMenuSetWindowHandle(HWND hwnd) {
    MediaLibrarySetWindowHandle(hwnd);
}

void MainMenuRender(AppConfig& cfg, MainMenuState& state, ImTextureID bgTexture, bool hasBg, int bgTexW,
                    int bgTexH) {
    UiManagerApplyAccentPreset(cfg, state.manager.settings.accent_preset);
    cfg.theme.blur_radius = static_cast<float>(state.manager.settings.blur_strength);
    if (state.manager.settings.hover_scale_enabled) {
        cfg.motion.hover_scale = state.manager.settings.hover_scale;
    }
    cfg.background.vignette_strength = state.manager.settings.vignette_strength;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float scale = UiScale(display);
    const UiFonts& fonts = GetUiFonts();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display);
    ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    if (state.show_manager) {
        mainFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    }
    ImGui::Begin("##MyiUI_MainMenu", nullptr, mainFlags);

    auto* bg = ImGui::GetBackgroundDrawList();
    if (hasBg && bgTexture && bgTexW > 0 && bgTexH > 0) {
        const float coverScale = (std::max)(display.x / static_cast<float>(bgTexW),
                                            display.y / static_cast<float>(bgTexH));
        const ImVec2 drawSize(bgTexW * coverScale, bgTexH * coverScale);
        const ImVec2 p0((display.x - drawSize.x) * 0.5f, (display.y - drawSize.y) * 0.5f);
        bg->AddImage(bgTexture, p0, ImVec2(p0.x + drawSize.x, p0.y + drawSize.y));
    } else if (hasBg && bgTexture) {
        bg->AddImage(bgTexture, ImVec2(0, 0), display);
    } else {
        bg->AddRectFilled(ImVec2(0, 0), display, ColorFromRGBA(cfg.background.fallback_color, 1.f));
    }

    bg->AddRectFilled(ImVec2(0, 0), display,
                      IM_COL32(8, 12, 20, static_cast<int>(140 * cfg.background.vignette_strength / 0.55f)));

    const float vignette = cfg.background.vignette_strength;
    bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, display.y * 0.25f),
                                IM_COL32(0, 0, 0, static_cast<int>(120 * vignette)),
                                IM_COL32(0, 0, 0, static_cast<int>(120 * vignette)), IM_COL32(0, 0, 0, 0),
                                IM_COL32(0, 0, 0, 0));
    bg->AddRectFilledMultiColor(ImVec2(0, display.y * 0.75f), display, IM_COL32(0, 0, 0, 0),
                                IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, static_cast<int>(160 * vignette)),
                                IM_COL32(0, 0, 0, static_cast<int>(160 * vignette)));

    auto* dl = ImGui::GetWindowDrawList();
    const float sideMargin = Px(48.f, scale);
    const float topMargin = Px(32.f, scale);

    // Top bar — brand + manager
    const float markSize = Px(36.f, scale);
    const ImVec2 logoMarkMin(sideMargin, topMargin);
    const ImVec2 logoMarkMax(logoMarkMin.x + markSize, logoMarkMin.y + markSize);
    const LogoSet& logos = GetLogos();
    if (logos.mark.valid()) {
        DrawLogoFit(dl, logos.mark, logoMarkMin, logoMarkMax, 1.f);
    } else {
        DrawGlassRect(dl, logoMarkMin, logoMarkMax, cfg.theme, 1.f);
        DrawTextStyled(dl, fonts.brand, ImVec2(logoMarkMin.x + Px(11.f, scale), logoMarkMin.y + Px(8.f, scale)),
                       ColorFromRGBA(cfg.theme.text_primary, 1.f), "M");
    }

    const char* brandText = cfg.layout.brand_label.c_str();
    const ImVec2 brandSize = fonts.brand ? fonts.brand->CalcTextSizeA(fonts.brand->FontSize, FLT_MAX, 0.f, brandText)
                                         : ImGui::CalcTextSize(brandText);
    const float brandX = logoMarkMax.x + Px(10.f, scale);
    DrawTextStyled(dl, fonts.brand, ImVec2(brandX, logoMarkMin.y + (markSize - brandSize.y) * 0.5f),
                   ColorFromRGBA(cfg.theme.text_primary, 1.f), brandText);

    const char* versionText = cfg.layout.version_label.c_str();
    const ImVec2 versionSize =
        fonts.caption ? fonts.caption->CalcTextSizeA(fonts.caption->FontSize, FLT_MAX, 0.f, versionText)
                      : ImGui::CalcTextSize(versionText);
    DrawTextStyled(dl, fonts.caption, ImVec2(brandX + brandSize.x + Px(12.f, scale),
                                             logoMarkMin.y + (markSize - versionSize.y) * 0.5f),
                   ColorFromRGBA(cfg.theme.text_secondary, 1.f), versionText);

    if (!state.show_manager) {
        const ImVec2 mgrSize(Px(168.f, scale), Px(44.f, scale));
        ImVec2 mgrPos(display.x - sideMargin - mgrSize.x, topMargin);
        mgrPos = ClampPos(mgrPos, mgrSize, display);
        float mgrHover = state.hover_anim[0];
        bool mgrPressed = false;
        ImGui::SetCursorScreenPos(mgrPos);
        ImGui::InvisibleButton("##mgr", mgrSize);
        const bool mgrHovered = ImGui::IsItemHovered();
        const bool mgrClicked = ImGui::IsItemClicked();
        mgrPressed = ImGui::IsItemActive();
        const float mgrTarget = mgrHovered || mgrPressed ? 1.f : 0.f;
        mgrHover = myiui::easing::Lerp(mgrHover, mgrTarget, 0.18f);
        const ImVec2 mgrCenter(mgrPos.x + mgrSize.x * 0.5f, mgrPos.y + mgrSize.y * 0.5f);
        const ImVec2 mgrMin(mgrPos.x, mgrPos.y);
        const ImVec2 mgrMax(mgrPos.x + mgrSize.x, mgrPos.y + mgrSize.y);
        const int* mgrFill = mgrHovered ? cfg.theme.accent_hover_bg : cfg.theme.glass_tint;
        const int* mgrBorder = mgrHovered ? cfg.theme.border_accent : cfg.theme.border_color;
        DrawGlassSurface(dl, mgrMin, mgrMax, mgrFill, mgrBorder, Px(cfg.theme.corner_radius, scale),
                         cfg.theme.border_width, 1.f);
        const float iconSize = Px(20.f, scale);
        const ImVec2 iconCenter(mgrMin.x + Px(18.f, scale) + iconSize * 0.5f, mgrCenter.y);
        DrawMenuIcon(dl, MenuIcon::Manager, iconCenter, iconSize,
                     mgrHovered ? ColorFromRGBA(cfg.theme.accent, 1.f) : ColorFromRGBA(cfg.theme.text_secondary, 1.f),
                     2.f);
        const char* mgrLabel = cfg.layout.manager_label.c_str();
        const ImVec2 mgrLabelSize = fonts.regular
                                        ? fonts.regular->CalcTextSizeA(fonts.regular->FontSize, FLT_MAX, 0.f, mgrLabel)
                                        : ImGui::CalcTextSize(mgrLabel);
        DrawTextStyled(dl, fonts.regular,
                       ImVec2(iconCenter.x + iconSize * 0.5f + Px(8.f, scale), mgrCenter.y - mgrLabelSize.y * 0.5f),
                       ColorFromRGBA(cfg.theme.text_primary, 1.f), mgrLabel);
        if (mgrClicked) {
            state.show_manager = true;
            state.manager.library.needs_refresh = true;
        }
        state.hover_anim[0] = mgrHover;
    }

    // Profile — far left; Nav — far right
    const float profileW = Px(cfg.layout.profile_card_w, scale);
    const float profileH = Px(168.f, scale);
    const ImVec2 btnSize(Px(cfg.layout.nav_button_w, scale), Px(cfg.layout.nav_button_h, scale));
    const float gap = Px(static_cast<float>(cfg.layout.nav_gap), scale);
    const size_t navCount = (std::min)(cfg.layout.nav_items.size(), size_t{7});
    const float menuH =
        static_cast<float>(navCount) * btnSize.y + static_cast<float>(navCount > 0 ? navCount - 1 : 0) * gap;
    const float clusterH = (std::max)(profileH, menuH);
    const float clusterTop = cfg.layout.main_center_y * display.y - clusterH * 0.5f;

    if (state.manager.settings.show_profile) {
        const ImVec2 profilePos(sideMargin, clusterTop + (clusterH - profileH) * 0.5f);
        const ImVec2 profileSize(profileW, profileH);
        DrawProfileCard(dl, profilePos, profileSize, cfg, fonts, scale, state.hover_anim[0]);
    }

    const float menuLeft = display.x - sideMargin - btnSize.x;
    float menuTop = clusterTop + (clusterH - menuH) * 0.5f;

    if (!state.show_manager) {
        for (size_t i = 0; i < navCount; ++i) {
            const auto& item = cfg.layout.nav_items[i];
            const ImVec2 pos(menuLeft, menuTop);
            float hover = state.hover_anim[i + 1];
            bool pressed = false;
            if (MenuGlassButton(item.id.c_str(), pos, btnSize, item.label.c_str(), cfg, hover, pressed, fonts.nav,
                                ResolveNavStyle(item), NavIconForItem(item))) {
                PipeSendCommandAsync(item.pipe);
            }
            state.hover_anim[i + 1] = hover;
            menuTop += btnSize.y + gap;
        }
    }

    if (state.show_manager) {
        bg->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, 140));
    }

    ImGui::End();

    if (state.show_manager) {
        UiManagerRenderPanel(cfg, state.manager, scale, &state.show_manager);
    }
}
