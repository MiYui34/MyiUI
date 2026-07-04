#include "ui/screens/menu_screens.h"

#include <cstring>
#include <string>

#include "ui/async_pipe.h"
#include "ui/fonts.h"
#include "ui/menu_data.h"
#include "ui/menu_filter.h"
#include "ui/ui_scale.h"
#include "ui/widgets/glass_button.h"
#include "ui/widgets/parallax_card.h"
#include "ui/widgets/screen_shell.h"
#include "ui/widgets/yc_search_box.h"

using myiui::ui::ColorFromRGBA;

static void EnsureServersLoaded(MenuAppState& state, float deltaMs) {
    if (state.data.servers_fetch_done) return;

    if (state.data.servers_loading) {
        state.data.servers_load_ms += deltaMs;
        if (state.data.servers_load_ms > 6500.f) {
            state.data.servers_loading = false;
            state.data.servers_fetch_done = true;
            LoadServersFromDisk(state.data.servers);
        }
        return;
    }

    state.data.servers_loading = true;
    state.data.servers_load_ms = 0.f;
    MenuAppStartPipeLoad(state, PipeLoadKind::Servers, "GET_SERVERS");
}

void RenderMultiplayerScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;
    const float deltaMs = ImGui::GetIO().DeltaTime * 1000.f;
    EnsureServersLoaded(ctx.state, deltaMs);

    const float scale = ctx.scale;
    const float alpha = ctx.state.transition.ContentAlpha();
    const ScreenShellLayout layout = CalcScreenShellLayout(ctx.cfg, scale);
    auto* dl = ImGui::GetWindowDrawList();
    DrawScreenShellBackground(dl, layout, ctx.cfg, scale, alpha);

    const UiFonts& fonts = GetUiFonts();
    if (ScreenShellHeader("mp", layout, ctx.cfg, fonts, scale, "\xe5\xa4\x9a\xe4\xba\xba\xe6\xb8\xb8\xe6\x88\x8f",
                           "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x88\x97\xe8\xa1\xa8", !router.IsTransitioning(),
                           ctx.state.back_hover)) {
        router.Pop();
        return;
    }

    const float cardH = Px(100.f, scale);
    const float gap = Px(15.f, scale);
    const float hoverMargin =
        myiui::ui::widgets::ParallaxCardHoverMargin(ImVec2(layout.content_size.x, cardH));
    const float rowW = layout.content_size.x - hoverMargin * 2.f;
    const float footerH = Px(52.f, scale);
    const ImVec2 btnSize(Px(68.f, scale), Px(30.f, scale));
    const float btnGap = Px(8.f, scale);
    const float btnRightPad = Px(12.f, scale);
    const float reservedRight = btnSize.x * 3.f + btnGap * 2.f + btnRightPad;

    if (!BeginScreenContentScroll("mp_scroll", layout, footerH)) {
        return;
    }
    {
        const myiui::ui::widgets::ParallaxCardClipGuard clipGuard;
        auto* contentDl = ImGui::GetWindowDrawList();
        float y = 0.f;

        {
            const float searchH = Px(56.f, scale);
            ImGui::SetCursorPos(ImVec2(hoverMargin, y));
            auto searchStyle = myiui::ui::YcSearchBox::StyleFromTheme(ctx.cfg.theme.accent, ctx.cfg.theme.text_primary,
                                                                      ctx.cfg.theme.text_dim, alpha);
            searchStyle.width = rowW / scale;
            myiui::ui::YcSearchBox::Draw("##server_search", "\xe6\x90\x9c\xe7\xb4\xa2\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8",
                                         ctx.state.server_search, sizeof(ctx.state.server_search), scale, searchStyle);
            y += searchH + gap;
        }

        if (ctx.state.data.servers_loading) {
            DrawTextStyled(contentDl, fonts.regular, ImGui::GetCursorScreenPos(),
                           ColorFromRGBA(ctx.cfg.theme.text_dim, alpha), "\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad...");
        } else {
            if (ctx.state.data.servers.empty()) {
                DrawTextStyled(contentDl, fonts.regular, ImGui::GetCursorScreenPos(),
                               ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                               "\xe6\x9a\x82\xe6\x97\xa0\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xef\xbc\x8c\xe5\x8f\xaf\xe7\x82\xb9\xe5\x87\xbb\xe4\xb8\x8b\xe6\x96\xb9\xe6\xb7\xbb\xe5\x8a\xa0");
            }

            int rowIdx = 0;
            bool anyVisible = false;
            for (const auto& server : ctx.state.data.servers) {
                if (!TextContainsInsensitive(server.name, ctx.state.server_search) &&
                    !TextContainsInsensitive(server.address, ctx.state.server_search)) {
                    continue;
                }
                anyVisible = true;
                ImGui::PushID(rowIdx);
                ImGui::SetCursorPos(ImVec2(hoverMargin, y));
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const ImVec2 cardSize(rowW, cardH);
                const ImVec2 rectMax(pos.x + cardSize.x, pos.y + cardSize.y);

                const std::string cardId = std::string("mp_srv_") + server.id;
                const bool cardPressed = myiui::ui::widgets::DrawParallaxCard(
                    cardId.c_str(), cardSize, pos, ctx.cfg.theme, server.name.c_str(), server.address.c_str(),
                    reservedRight);

                const float btnY = pos.y + (cardH - btnSize.y) * 0.5f;
                const float connX = rectMax.x - btnRightPad - btnSize.x;
                const float editX = connX - btnGap - btnSize.x;
                const float delX = editX - btnGap - btnSize.x;

                float hoverDel = ctx.state.hover_anim[30 + rowIdx * 3];
                bool pressed = false;
                if (GlassButton("##del", ImVec2(delX, btnY), btnSize,
                                "\xe5\x88\xa0\xe9\x99\xa4", ctx.cfg, hoverDel, pressed, fonts.caption,
                                GlassButtonStyle::Danger, MenuIcon::None, scale, !router.IsTransitioning())) {
                    MenuAppRunPipeAction(ctx.state, "DELETE_SERVER:" + server.id);
                    ctx.state.data.servers_fetch_done = false;
                }
                ctx.state.hover_anim[30 + rowIdx * 3] = hoverDel;

                float hoverEdit = ctx.state.hover_anim[31 + rowIdx * 3];
                if (GlassButton("##edit", ImVec2(editX, btnY), btnSize,
                                "\xe7\xbc\x96\xe8\xbe\x91", ctx.cfg, hoverEdit, pressed, fonts.caption,
                                GlassButtonStyle::Default, MenuIcon::None, scale, !router.IsTransitioning())) {
                    std::memset(ctx.state.add_server_name, 0, sizeof(ctx.state.add_server_name));
                    std::memset(ctx.state.add_server_address, 0, sizeof(ctx.state.add_server_address));
                    std::strncpy(ctx.state.add_server_name, server.name.c_str(), sizeof(ctx.state.add_server_name) - 1);
                    std::strncpy(ctx.state.add_server_address, server.address.c_str(),
                                 sizeof(ctx.state.add_server_address) - 1);
                    std::memset(ctx.state.editing_server_id, 0, sizeof(ctx.state.editing_server_id));
                    std::strncpy(ctx.state.editing_server_id, server.id.c_str(), sizeof(ctx.state.editing_server_id) - 1);
                    ctx.state.add_server_edit_mode = true;
                    router.Push(ScreenId::AddServer);
                }
                ctx.state.hover_anim[31 + rowIdx * 3] = hoverEdit;

                float hoverConn = ctx.state.hover_anim[32 + rowIdx * 3];
                if (GlassButton("##conn", ImVec2(connX, btnY), btnSize,
                                "\xe8\xbf\x9e\xe6\x8e\xa5", ctx.cfg, hoverConn, pressed, fonts.caption,
                                GlassButtonStyle::Primary, MenuIcon::None, scale, !router.IsTransitioning())) {
                    MenuAppRunPipeAction(ctx.state, "CONNECT_SERVER:" + server.id, false);
                } else if (cardPressed && !router.IsTransitioning()) {
                    MenuAppRunPipeAction(ctx.state, "CONNECT_SERVER:" + server.id, false);
                }
                ctx.state.hover_anim[32 + rowIdx * 3] = hoverConn;

                y += cardH + gap;
                ++rowIdx;
                ImGui::PopID();
            }
            if (!ctx.state.data.servers.empty() && !anyVisible) {
                DrawTextStyled(contentDl, fonts.regular, ImVec2(0.f, y), ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                               "\xe6\x9c\xaa\xe6\x89\xbe\xe5\x88\xb0\xe5\x8c\xb9\xe9\x85\x8d\xe7\x9a\x84\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8");
            }
            ImGui::Dummy(ImVec2(0.f, hoverMargin));
        }
    }
    EndScreenContentScroll();

    const ImVec2 addBtnSize(Px(140.f, scale), Px(40.f, scale));
    const float bottomY = layout.panel_pos.y + layout.panel_size.y - Px(56.f, scale);
    float hoverAdd = ctx.state.hover_anim[25];
    bool pressed = false;
    if (GlassButton("add_srv", ImVec2(layout.content_pos.x, bottomY), addBtnSize,
                    "\xe6\xb7\xbb\xe5\x8a\xa0\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8", ctx.cfg, hoverAdd, pressed,
                    fonts.regular, GlassButtonStyle::Primary, MenuIcon::Multiplayer, scale, !router.IsTransitioning())) {
        ctx.state.add_server_edit_mode = false;
        router.Push(ScreenId::AddServer);
    }
    ctx.state.hover_anim[25] = hoverAdd;
}
