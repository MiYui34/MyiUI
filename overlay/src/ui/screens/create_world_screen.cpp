#include "ui/screens/menu_screens.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"
#include "ui/widgets/glass_button.h"
#include "ui/widgets/screen_shell.h"
#include "ui/widgets/yc_segment_selector.h"

using myiui::ui::ColorFromRGBA;

static void DrawFormField(ImDrawList* dl, const UiFonts& fonts, const AppConfig& cfg, float scale, float alpha,
                          const ImVec2& pos, float rowW, float rowH, const char* inputId, const char* label,
                          char* buf, size_t bufSize, const char* hint) {
    const ImVec2 rectMax(pos.x + rowW, pos.y + rowH);
    myiui::ui::DrawGlassSurface(dl, pos, rectMax, cfg.theme.glass_tint, cfg.theme.border_color, Px(10.f, scale),
                                cfg.theme.border_width, alpha);
    DrawTextStyled(dl, fonts.regular, ImVec2(pos.x + Px(14.f, scale), pos.y + Px(10.f, scale)),
                   ColorFromRGBA(cfg.theme.text_dim, alpha), label);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + Px(12.f, scale), pos.y + Px(30.f, scale)));
    ImGui::PushFont(fonts.regular);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 8.f));
    ImGui::SetNextItemWidth(rowW - Px(28.f, scale));
    ImGui::InputTextWithHint(inputId, hint, buf, bufSize, ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopFont();
}

void RenderCreateWorldScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;

    const float scale = ctx.scale;
    const float alpha = ctx.state.transition.ContentAlpha();
    const ScreenShellLayout layout = CalcScreenShellLayout(ctx.cfg, scale);
    auto* dl = ImGui::GetWindowDrawList();
    DrawScreenShellBackground(dl, layout, ctx.cfg, scale, alpha);

    const UiFonts& fonts = GetUiFonts();
    if (ScreenShellHeader("create_w", layout, ctx.cfg, fonts, scale, "\xe5\x88\x9b\xe5\xbb\xba\xe6\x96\xb0\xe4\xb8\x96\xe7\x95\x8c",
                           "\xe8\xae\xbe\xe7\xbd\xae\xe4\xb8\x96\xe7\x95\x8c\xe5\x90\x8d\xe7\xa7\xb0\xe4\xb8\x8e\xe6\xb8\xb8\xe6\x88\x8f\xe6\xa8\xa1\xe5\xbc\x8f",
                           !router.IsTransitioning(), ctx.state.back_hover)) {
        router.Pop();
        return;
    }

    const float rowW = layout.content_size.x;
    const float fieldH = Px(72.f, scale);
    const float gap = Px(14.f, scale);
    const float footerH = Px(56.f, scale);

    if (!BeginScreenContentScroll("cw_scroll", layout, footerH)) {
        return;
    }
    auto* contentDl = ImGui::GetWindowDrawList();
    float y = 0.f;

    DrawTextStyled(contentDl, fonts.regular, ImGui::GetCursorScreenPos(), ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                   "\xe5\x9f\xba\xe6\x9c\xac\xe4\xbf\xa1\xe6\x81\xaf");
    y += Px(26.f, scale);

    ImGui::PushID("create_world");
    ImGui::SetCursorPos(ImVec2(0.f, y));
    DrawFormField(contentDl, fonts, ctx.cfg, scale, alpha, ImGui::GetCursorScreenPos(), rowW, fieldH,
                  "##create_world_name_field",
                  "\xe4\xb8\x96\xe7\x95\x8c\xe5\x90\x8d\xe7\xa7\xb0", ctx.state.create_world_name,
                  sizeof(ctx.state.create_world_name), "\xe8\xbe\x93\xe5\x85\xa5\xe4\xb8\x96\xe7\x95\x8c\xe5\x90\x8d\xe7\xa7\xb0");
    y += fieldH + gap;

    {
        ImGui::SetCursorPos(ImVec2(0.f, y));
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(rowW, fieldH);
        const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
        myiui::ui::DrawGlassSurface(contentDl, pos, rectMax, ctx.cfg.theme.glass_tint, ctx.cfg.theme.border_color, Px(10.f, scale),
                                    ctx.cfg.theme.border_width, alpha);
        DrawTextStyled(contentDl, fonts.regular, ImVec2(pos.x + Px(14.f, scale), pos.y + Px(10.f, scale)),
                       ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), "\xe6\xb8\xb8\xe6\x88\x8f\xe6\xa8\xa1\xe5\xbc\x8f");
        const char* modes[] = {"\xe7\x94\x9f\xe5\xad\x98", "\xe5\x88\x9b\xe9\x80\xa0", "\xe6\x9e\x81\xe9\x99\x90"};
        const float segW = rowW - Px(28.f, scale);
        const float segH = Px(36.f, scale);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + Px(14.f, scale), pos.y + Px(30.f, scale)));
        const auto segStyle =
            myiui::ui::YcSegmentSelector::StyleFromTheme(ctx.cfg.theme.accent, ctx.cfg.theme.glass_tint, alpha);
        myiui::ui::YcSegmentSelector::Draw("##game_mode_seg", &ctx.state.create_world_mode, modes, 3, segW / scale,
                                             scale, segStyle);
        y += fieldH + gap;
    }

    ImGui::SetCursorPos(ImVec2(0.f, y));
    DrawFormField(contentDl, fonts, ctx.cfg, scale, alpha, ImGui::GetCursorScreenPos(), rowW, fieldH,
                  "##create_world_seed_field",
                  "\xe4\xb8\x96\xe7\x95\x8c\xe7\xa7\x8d\xe5\xad\x90", ctx.state.create_world_seed,
                  sizeof(ctx.state.create_world_seed), "\xe7\x95\x99\xe7\xa9\xba\xe5\x88\x99\xe9\x9a\x8f\xe6\x9c\xba\xe7\x94\x9f\xe6\x88\x90");
    ImGui::PopID();
    EndScreenContentScroll();

    const ImVec2 btnSize(Px(120.f, scale), Px(40.f, scale));
    const float bottomY = layout.panel_pos.y + layout.panel_size.y - Px(56.f, scale);
    const float rightX = layout.content_pos.x + rowW;

    float hoverCancel = ctx.state.hover_anim[26];
    bool pressed = false;
    if (GlassButton("cw_cancel", ImVec2(rightX - btnSize.x * 2.f - Px(12.f, scale), bottomY), btnSize,
                    "\xe5\x8f\x96\xe6\xb6\x88", ctx.cfg, hoverCancel, pressed, fonts.regular, GlassButtonStyle::Default,
                    MenuIcon::None, scale, !router.IsTransitioning())) {
        router.Pop();
    }
    ctx.state.hover_anim[26] = hoverCancel;

    float hoverCreate = ctx.state.hover_anim[27];
    const bool canCreate = !router.IsTransitioning() && !ctx.state.create_world_pending;
    if (GlassButton("cw_create", ImVec2(rightX - btnSize.x, bottomY), btnSize,
                    "\xe5\x88\x9b\xe5\xbb\xba\xe4\xb8\x96\xe7\x95\x8c", ctx.cfg, hoverCreate, pressed, fonts.regular,
                    GlassButtonStyle::Primary, MenuIcon::Singleplayer, scale, canCreate)) {
        const char* modeNames[] = {"survival", "creative", "hardcore"};
        const int modeIdx = (std::max)(0, (std::min)(2, ctx.state.create_world_mode));
        std::string cmd = std::string("CREATE_WORLD_SUBMIT:") + ctx.state.create_world_name + ":" + modeNames[modeIdx];
        if (ctx.state.create_world_seed[0] != '\0') {
            cmd += ":";
            cmd += ctx.state.create_world_seed;
        }
        ctx.state.create_world_pending = true;
        MenuAppRunPipeAction(ctx.state, cmd);
        ctx.state.data.worlds_fetch_done = false;
        router.Pop();
    }
    ctx.state.hover_anim[27] = hoverCreate;
}
