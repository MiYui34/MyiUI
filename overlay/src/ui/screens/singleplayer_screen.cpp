#include "ui/screens/menu_screens.h"

#include <string>

#include "ui/async_pipe.h"
#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/menu_data.h"
#include "ui/menu_filter.h"
#include "ui/ui_scale.h"
#include "ui/widgets/glass_button.h"
#include "ui/widgets/screen_shell.h"
#include "ui/widgets/yc_search_box.h"

using myiui::ui::ColorFromRGBA;

static void EnsureWorldsLoaded(MenuAppState& state, float deltaMs) {
    if (state.data.worlds_fetch_done) return;

    if (state.data.worlds_loading) {
        state.data.worlds_load_ms += deltaMs;
        if (state.data.worlds_load_ms > 6500.f) {
            state.data.worlds_loading = false;
            state.data.worlds_fetch_done = true;
            LoadWorldsFromDisk(state.data.worlds);
        }
        return;
    }

    state.data.worlds_loading = true;
    state.data.worlds_load_ms = 0.f;
    MenuAppStartPipeLoad(state, PipeLoadKind::Worlds, "GET_WORLDS");
}

void RenderSingleplayerScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;
    const float deltaMs = ImGui::GetIO().DeltaTime * 1000.f;
    EnsureWorldsLoaded(ctx.state, deltaMs);

    const float scale = ctx.scale;
    const float alpha = ctx.state.transition.ContentAlpha();
    const ScreenShellLayout layout = CalcScreenShellLayout(ctx.cfg, scale);
    auto* dl = ImGui::GetWindowDrawList();
    DrawScreenShellBackground(dl, layout, ctx.cfg, scale, alpha);

    const UiFonts& fonts = GetUiFonts();
    if (ScreenShellHeader("sp", layout, ctx.cfg, fonts, scale, "\xe5\x8d\x95\xe4\xba\xba\xe6\xb8\xb8\xe6\x88\x8f",
                           "\xe9\x80\x89\xe6\x8b\xa9\xe6\x88\x96\xe5\x88\x9b\xe5\xbb\xba\xe4\xb8\x96\xe7\x95\x8c",
                           !router.IsTransitioning(), ctx.state.back_hover)) {
        router.Pop();
        return;
    }

    const float rowH = Px(ctx.cfg.components.world_card_h, scale);
    const float gap = Px(10.f, scale);
    const float rowW = layout.content_size.x;
    const float footerH = Px(52.f, scale);

    if (!BeginScreenContentScroll("sp_scroll", layout, footerH)) {
        return;
    }
    auto* contentDl = ImGui::GetWindowDrawList();
    float y = 0.f;

    {
        const float searchH = Px(56.f, scale);
        ImGui::SetCursorPos(ImVec2(0.f, y));
        auto searchStyle = myiui::ui::YcSearchBox::StyleFromTheme(ctx.cfg.theme.accent, ctx.cfg.theme.text_primary,
                                                                  ctx.cfg.theme.text_dim, alpha);
        searchStyle.width = rowW / scale;
        myiui::ui::YcSearchBox::Draw("##world_search", "\xe6\x90\x9c\xe7\xb4\xa2\xe4\xb8\x96\xe7\x95\x8c", ctx.state.world_search,
                                     sizeof(ctx.state.world_search), scale, searchStyle);
        y += searchH + gap;
    }

    if (ctx.state.data.worlds_loading) {
        DrawTextStyled(contentDl, fonts.regular, ImGui::GetCursorScreenPos(),
                       ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), "\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad...");
        EndScreenContentScroll();
        return;
    }

    if (ctx.state.data.worlds.empty()) {
        DrawTextStyled(contentDl, fonts.regular, ImGui::GetCursorScreenPos(),
                       ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                       "\xe6\x9a\x82\xe6\x97\xa0\xe4\xb8\x96\xe7\x95\x8c\xef\xbc\x8c\xe5\x8f\xaf\xe7\x82\xb9\xe5\x87\xbb\xe4\xb8\x8b\xe6\x96\xb9\xe5\x88\x9b\xe5\xbb\xba");
    }

    int rowIdx = 0;
    bool anyVisible = false;
    for (const auto& world : ctx.state.data.worlds) {
        if (!TextContainsInsensitive(world.name, ctx.state.world_search) &&
            !TextContainsInsensitive(world.mode, ctx.state.world_search)) {
            continue;
        }
        anyVisible = true;
        ImGui::PushID(rowIdx);
        ImGui::SetCursorPos(ImVec2(0.f, y));
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(rowW, rowH);
        const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
        const int* fill = ctx.cfg.theme.glass_tint;
        myiui::ui::DrawGlassSurface(contentDl, pos, rectMax, fill, ctx.cfg.theme.border_color, Px(10.f, scale),
                                    ctx.cfg.theme.border_width, alpha);

        DrawTextStyled(contentDl, fonts.regular, ImVec2(pos.x + Px(14.f, scale), pos.y + Px(10.f, scale)),
                       ColorFromRGBA(ctx.cfg.theme.text_primary, alpha), world.name.c_str());
        DrawTextStyled(contentDl, fonts.caption, ImVec2(pos.x + Px(14.f, scale), pos.y + Px(32.f, scale)),
                       ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), world.mode.c_str());

        const ImVec2 playSize(Px(84.f, scale), Px(32.f, scale));
        const ImVec2 playPos(rectMax.x - Px(86.f, scale), pos.y + (rowH - playSize.y) * 0.5f);
        const ImVec2 rowClickSize(rowW - playSize.x - Px(12.f, scale), rowH);
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##row_join", rowClickSize);
        if (ImGui::IsItemClicked() && !router.IsTransitioning()) {
            MenuAppRunPipeAction(ctx.state, "JOIN_WORLD:" + world.name);
        }

        float hover = ctx.state.hover_anim[10 + rowIdx];
        bool pressed = false;
        if (GlassButton("##play", playPos, playSize,
                        "\xe6\xb8\xb8\xe7\x8e\xa9", ctx.cfg, hover, pressed, fonts.regular, GlassButtonStyle::Primary,
                        MenuIcon::Singleplayer, scale, !router.IsTransitioning())) {
            MenuAppRunPipeAction(ctx.state, "JOIN_WORLD:" + world.name);
        }
        ctx.state.hover_anim[10 + rowIdx] = hover;
        y += rowH + gap;
        ++rowIdx;
        ImGui::PopID();
    }
    if (!ctx.state.data.worlds.empty() && !anyVisible) {
        DrawTextStyled(contentDl, fonts.regular, ImVec2(0.f, y), ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                       "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0\xe5\x8c\xb9\xe9\x85\x8d\xe7\x9a\x84\xe4\xb8\x96\xe7\x95\x8c");
    }
    EndScreenContentScroll();

    const ImVec2 btnSize(Px(140.f, scale), Px(40.f, scale));
    float hoverCreate = ctx.state.hover_anim[20];
    bool pressed = false;
    if (GlassButton("create_world", ImVec2(layout.content_pos.x, layout.panel_pos.y + layout.panel_size.y - Px(56.f, scale)),
                    btnSize, "\xe5\x88\x9b\xe5\xbb\xba\xe6\x96\xb0\xe4\xb8\x96\xe7\x95\x8c", ctx.cfg, hoverCreate, pressed,
                    fonts.regular, GlassButtonStyle::Primary, MenuIcon::Singleplayer, scale, !router.IsTransitioning())) {
        router.Push(ScreenId::CreateWorld);
    }
    ctx.state.hover_anim[20] = hoverCreate;
}
