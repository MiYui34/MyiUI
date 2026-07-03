#include "ui/screens/menu_screens.h"

#include <string>

#include "ui/fonts.h"
#include "ui/screen_id.h"
#include "ui/ui_scale.h"
#include "ui/widgets/nav_list.h"
#include "ui/widgets/screen_shell.h"

void RenderOptionsHubScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;

    const float scale = ctx.scale;
    const float alpha = ctx.state.transition.ContentAlpha();
    const ScreenShellLayout layout = CalcScreenShellLayout(ctx.cfg, scale);
    auto* dl = ImGui::GetWindowDrawList();
    DrawScreenShellBackground(dl, layout, ctx.cfg, scale, alpha);

    const UiFonts& fonts = GetUiFonts();
    if (ScreenShellHeader("opt_hub", layout, ctx.cfg, fonts, scale, "\xe9\x80\x89\xe9\xa1\xb9",
                           "\xe6\xb8\xb8\xe6\x88\x8f\xe8\xae\xbe\xe7\xbd\xae", !router.IsTransitioning(),
                           ctx.state.back_hover)) {
        router.Pop();
        return;
    }

    const float rowH = Px(ctx.cfg.components.nav_row_h, scale);
    const float gap = Px(8.f, scale);
    const float rowW = layout.content_size.x;

    if (!BeginScreenContentScroll("opt_hub_scroll", layout)) {
        return;
    }
    float y = 0.f;
    int idx = 0;
    for (const auto& item : ctx.state.options_nav) {
        ImGui::SetCursorPos(ImVec2(0.f, y));
        const ImVec2 rowPos = ImGui::GetCursorScreenPos();
        float hover = ctx.state.hover_anim[12 + idx];
        if (NavListRow((std::string("nav_") + item.id).c_str(), rowPos, ImVec2(rowW, rowH), item.label.c_str(),
                       ctx.cfg, hover, scale) &&
            !router.IsTransitioning()) {
            if (item.id == "video" || item.screen == ScreenId::OptionsVideo) {
                MenuAppRunPipeAction(ctx.state, "OPEN_VIDEO_OPTIONS", false);
            } else {
                router.Push(item.screen);
            }
        }
        ctx.state.hover_anim[12 + idx] = hover;
        y += rowH + gap;
        ++idx;
    }
    EndScreenContentScroll();
}
