#include "ui/screens/menu_screens.h"

#include <string>

#include "ui/fonts.h"
#include "ui/screen_id.h"
#include "ui/ui_scale.h"
#include "ui/widgets/parallax_card.h"
#include "ui/widgets/screen_shell.h"

namespace {

const char* OptionsNavDesc(const std::string& id) {
    if (id == "options_video") return "分辨率、画质与性能";
    if (id == "options_sound") return "音量与音频设备";
    if (id == "options_controls") return "键位与鼠标设置";
    if (id == "options_language") return "界面语言";
    if (id == "options_chat") return "聊天与 HUD 显示";
    if (id == "options_skin") return "皮肤与模型";
    if (id == "options_accessibility") return "辅助功能";
    if (id == "options_credits") return "制作人员";
    return "进入设置";
}

}  // namespace

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

    const float cardH = Px(100.f, scale);
    const float gap = Px(15.f, scale);
    const float hoverMargin =
        myiui::ui::widgets::ParallaxCardHoverMargin(ImVec2(layout.content_size.x, cardH));
    const float rowW = layout.content_size.x - hoverMargin * 2.f;

    if (!BeginScreenContentScroll("opt_hub_scroll", layout)) {
        return;
    }
    {
        const myiui::ui::widgets::ParallaxCardClipGuard clipGuard;
        float y = hoverMargin;
        int idx = 0;
        for (const auto& item : ctx.state.options_nav) {
            ImGui::SetCursorPos(ImVec2(hoverMargin, y));
            const ImVec2 cardPos = ImGui::GetCursorScreenPos();
            const ImVec2 cardSize(rowW, cardH);
            const std::string cardId = std::string("opt_nav_") + item.id;

            if (myiui::ui::widgets::DrawParallaxCard(cardId.c_str(), cardSize, cardPos, ctx.cfg.theme,
                                                    item.label.c_str(), OptionsNavDesc(item.id)) &&
                !router.IsTransitioning()) {
                if (item.id == "options_video" || item.screen == ScreenId::OptionsVideo) {
                    MenuAppRunPipeAction(ctx.state, "OPEN_VIDEO_OPTIONS", false);
                } else {
                    router.Push(item.screen);
                }
            }

            y += cardSize.y + gap;
            ++idx;
            (void)idx;
        }
        ImGui::Dummy(ImVec2(0.f, hoverMargin));
    }
    EndScreenContentScroll();
}
