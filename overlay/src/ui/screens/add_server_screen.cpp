#include "ui/screens/menu_screens.h"

#include <cstring>
#include <string>

#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/ui_scale.h"
#include "ui/widgets/glass_button.h"
#include "ui/widgets/screen_shell.h"

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

void RenderAddServerScreen(MenuRenderContext& ctx, ScreenRouter& router) {
    if (ctx.state.show_manager) return;

    const bool editMode = ctx.state.add_server_edit_mode;
    const float scale = ctx.scale;
    const float alpha = ctx.state.transition.ContentAlpha();
    const ScreenShellLayout layout = CalcScreenShellLayout(ctx.cfg, scale);
    auto* dl = ImGui::GetWindowDrawList();
    DrawScreenShellBackground(dl, layout, ctx.cfg, scale, alpha);

    const UiFonts& fonts = GetUiFonts();
    const char* title = editMode ? "\xe7\xbc\x96\xe8\xbe\x91\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8" : "\xe6\xb7\xbb\xe5\x8a\xa0\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8";
    const char* subtitle = editMode ? "\xe4\xbf\xae\xe6\x94\xb9\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x90\x8d\xe7\xa7\xb0\xe4\xb8\x8e\xe5\x9c\xb0\xe5\x9d\x80"
                                  : "\xe8\xbe\x93\xe5\x85\xa5\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x90\x8d\xe7\xa7\xb0\xe4\xb8\x8e\xe5\x9c\xb0\xe5\x9d\x80";
    if (ScreenShellHeader("add_srv", layout, ctx.cfg, fonts, scale, title, subtitle, !router.IsTransitioning(),
                           ctx.state.back_hover)) {
        router.Pop();
        return;
    }

    const float rowW = layout.content_size.x;
    const float fieldH = Px(72.f, scale);
    const float gap = Px(14.f, scale);
    const float footerH = Px(56.f, scale);

    if (!BeginScreenContentScroll("as_scroll", layout, footerH)) {
        return;
    }
    auto* contentDl = ImGui::GetWindowDrawList();
    float y = 0.f;

    DrawTextStyled(contentDl, fonts.regular, ImGui::GetCursorScreenPos(), ColorFromRGBA(ctx.cfg.theme.text_dim, alpha),
                   "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe4\xbf\xa1\xe6\x81\xaf");
    y += Px(26.f, scale);

    ImGui::PushID("add_server");
    ImGui::SetCursorPos(ImVec2(0.f, y));
    DrawFormField(contentDl, fonts, ctx.cfg, scale, alpha, ImGui::GetCursorScreenPos(), rowW, fieldH,
                  "##add_server_name_field",
                  "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x90\x8d\xe7\xa7\xb0", ctx.state.add_server_name,
                  sizeof(ctx.state.add_server_name), "\xe4\xbe\x8b\xe5\xa6\x82\xef\xbc\x9a\xe6\x88\x91\xe7\x9a\x84\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8");
    y += fieldH + gap;

    ImGui::SetCursorPos(ImVec2(0.f, y));
    DrawFormField(contentDl, fonts, ctx.cfg, scale, alpha, ImGui::GetCursorScreenPos(), rowW, fieldH,
                  "##add_server_address_field",
                  "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\x9c\xb0\xe5\x9d\x80", ctx.state.add_server_address,
                  sizeof(ctx.state.add_server_address), "host \xe6\x88\x96 host:\xe7\xab\xaf\xe5\x8f\xa3");
    ImGui::PopID();
    EndScreenContentScroll();

    const ImVec2 btnSize(Px(120.f, scale), Px(40.f, scale));
    const float bottomY = layout.panel_pos.y + layout.panel_size.y - Px(56.f, scale);
    const float rightX = layout.content_pos.x + rowW;

    float hoverCancel = ctx.state.hover_anim[28];
    bool pressed = false;
    if (GlassButton("as_cancel", ImVec2(rightX - btnSize.x * 2.f - Px(12.f, scale), bottomY), btnSize,
                    "\xe5\x8f\x96\xe6\xb6\x88", ctx.cfg, hoverCancel, pressed, fonts.regular, GlassButtonStyle::Default,
                    MenuIcon::None, scale, !router.IsTransitioning())) {
        ctx.state.add_server_edit_mode = false;
        router.Pop();
    }
    ctx.state.hover_anim[28] = hoverCancel;

    float hoverSave = ctx.state.hover_anim[29];
    const char* saveLabel = editMode ? "\xe4\xbf\x9d\xe5\xad\x98\xe4\xbf\xae\xe6\x94\xb9" : "\xe4\xbf\x9d\xe5\xad\x98";
    if (GlassButton("as_save", ImVec2(rightX - btnSize.x, bottomY), btnSize, saveLabel, ctx.cfg, hoverSave, pressed,
                    fonts.regular, GlassButtonStyle::Primary, MenuIcon::Multiplayer, scale, !router.IsTransitioning())) {
        if (ctx.state.add_server_name[0] != '\0' && ctx.state.add_server_address[0] != '\0') {
            std::string cmd;
            if (editMode && ctx.state.editing_server_id[0] != '\0') {
                cmd = std::string("EDIT_SERVER_SUBMIT:") + ctx.state.editing_server_id + ":" +
                      ctx.state.add_server_name + ":" + ctx.state.add_server_address;
            } else {
                cmd = std::string("ADD_SERVER_SUBMIT:") + ctx.state.add_server_name + ":" +
                      ctx.state.add_server_address;
            }
            MenuAppRunPipeAction(ctx.state, cmd);
            ctx.state.data.servers_fetch_done = false;
            ctx.state.add_server_edit_mode = false;
            router.Pop();
        }
    }
    ctx.state.hover_anim[29] = hoverSave;
}
