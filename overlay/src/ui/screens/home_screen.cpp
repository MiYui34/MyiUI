#include "ui/screens/menu_screens.h"

#include "ipc/pipe_client.h"
#include "ui/fonts.h"
#include "ui/menu_icons.h"
#include "ui/strings_zh.h"
#include "ui/ui_scale.h"
#include "ui/widgets/glass_button.h"
#include "ui/widgets/toast.h"

#undef min
#undef max

#include <algorithm>
#include <thread>

static GlassButtonStyle ToNavStyle(const NavItem& item) {
    if (item.style == "primary") return GlassButtonStyle::Primary;
    if (item.style == "danger") return GlassButtonStyle::Danger;
    if (item.id == "single") return GlassButtonStyle::Primary;
    if (item.id == "quit") return GlassButtonStyle::Danger;
    return GlassButtonStyle::Default;
}

static MenuIcon NavIconForItem(const NavItem& item) {
    if (item.id == "single") return MenuIcon::Singleplayer;
    if (item.id == "multi") return MenuIcon::Multiplayer;
    if (item.id == "options") return MenuIcon::Options;
    if (item.id == "quit") return MenuIcon::Quit;
    return MenuIcon::Options;
}

static ScreenId ScreenForNavItem(const NavItem& item) {
    if (item.id == "single") return ScreenId::Singleplayer;
    if (item.id == "multi") return ScreenId::Multiplayer;
    if (item.id == "options") return ScreenId::OptionsHub;
    return ScreenId::Home;
}

void RenderHomeScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float scale = ctx.scale;
    const float sideMargin = Px(48.f, scale);
    const ImVec2 btnSize(Px(ctx.cfg.layout.nav_button_w, scale), Px(ctx.cfg.layout.nav_button_h, scale));
    const float gap = Px(static_cast<float>(ctx.cfg.layout.nav_gap), scale);
    const size_t navCount = (std::min)(ctx.cfg.layout.nav_items.size(), size_t{7});
    const float profileH = Px(168.f, scale);
    const float menuH =
        static_cast<float>(navCount) * btnSize.y + static_cast<float>(navCount > 0 ? navCount - 1 : 0) * gap;
    const float clusterH = (std::max)(profileH, menuH);
    const float clusterTop = ctx.cfg.layout.main_center_y * display.y - clusterH * 0.5f;
    const float menuLeft = display.x - sideMargin - btnSize.x;
    float menuTop = clusterTop + (clusterH - menuH) * 0.5f;

    const UiFonts& fonts = GetUiFonts();
    const float alpha = ctx.state.transition.ContentAlpha();

    for (size_t i = 0; i < navCount; ++i) {
        const auto& item = ctx.cfg.layout.nav_items[i];
        const ImVec2 pos(menuLeft, menuTop);
        float hover = ctx.state.hover_anim[i + 1];
        bool pressed = false;
        GlassButtonStyle style = static_cast<GlassButtonStyle>(ToNavStyle(item));
        if (GlassButton(item.id.c_str(), pos, btnSize, item.label.c_str(), ctx.cfg, hover, pressed, fonts.nav, style,
                        NavIconForItem(item), scale, !router.IsTransitioning())) {
            if (item.id == "quit") {
                MenuAppRunPipeAction(ctx.state, "QUIT", false);
            } else {
                router.Push(ScreenForNavItem(item));
            }
        }
        ctx.state.hover_anim[i + 1] = hover;
        menuTop += btnSize.y + gap;
        (void)alpha;
    }
}
