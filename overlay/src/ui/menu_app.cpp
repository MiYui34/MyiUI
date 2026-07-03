#include "ui/menu_app.h"

#include "ipc/pipe_client.h"
#include "ui/fonts.h"
#include "ui/media_library.h"
#include "ui/menu_background.h"
#include "ui/profile_avatar.h"
#include "ui/screens/menu_screens.h"
#include "ui/strings_zh.h"
#include "ui/ui_manager.h"
#include "ui/ui_scale.h"
#include "ui/widgets/toast.h"

#include "ui/glass_panel.h"
#include "ui/intro/intro_screen.h"

#include <windows.h>

#undef min
#undef max

#include <algorithm>
#include <cstring>
#include <thread>

using myiui::ui::ColorFromRGBA;

static void ResetScreenDataOnPush(MenuAppState& state, ScreenId id) {
    switch (id) {
        case ScreenId::Singleplayer:
            state.data.worlds.clear();
            state.data.worlds_loading = false;
            state.data.worlds_load_ms = 0.f;
            state.data.worlds_fetch_done = false;
            break;
        case ScreenId::Multiplayer:
            state.data.servers.clear();
            state.data.servers_loading = false;
            state.data.servers_load_ms = 0.f;
            state.data.servers_fetch_done = false;
            break;
        case ScreenId::CreateWorld:
            std::memset(state.create_world_name, 0, sizeof(state.create_world_name));
            std::strncpy(state.create_world_name, "\xe6\x96\xb0\xe4\xb8\x96\xe7\x95\x8c", sizeof(state.create_world_name) - 1);
            std::memset(state.create_world_seed, 0, sizeof(state.create_world_seed));
            state.create_world_mode = 0;
            break;
        case ScreenId::AddServer:
            if (!state.add_server_edit_mode) {
                std::memset(state.add_server_name, 0, sizeof(state.add_server_name));
                std::memset(state.add_server_address, 0, sizeof(state.add_server_address));
                std::memset(state.editing_server_id, 0, sizeof(state.editing_server_id));
            }
            break;
        default:
            break;
    }
    if (IsOptionsDetailScreen(id)) {
        state.data.options_json.clear();
        state.data.options_baseline_json.clear();
        state.data.keybinds.clear();
        state.data.packs.clear();
        state.data.category.clear();
        state.data.options_loading = false;
        state.data.options_load_ms = 0.f;
        state.data.options_fetch_done = false;
        state.pending_option_changes.clear();
        state.slider_drafts.clear();
    }
}

ScreenId ScreenRouter::Current() const {
    if (!state || state->stack.empty()) return ScreenId::Home;
    return state->stack.back();
}

void ScreenRouter::Push(ScreenId id) {
    if (!state || !cfg || state->transition.IsActive()) return;
    if (Current() == id) return;
    ResetScreenDataOnPush(*state, id);
    state->transition.Configure(cfg->page_transition, state->reduce_motion);
    state->stack.push_back(id);
    state->transition.Begin(true);
}

void ScreenRouter::Pop() {
    if (!state || !cfg || state->transition.IsActive()) return;
    if (state->stack.size() <= 1) return;
    state->transition.Configure(cfg->page_transition, state->reduce_motion);
    state->pending_pop = true;
    state->transition.Begin(false);
}

void ScreenRouter::Replace(ScreenId id) {
    if (!state || !cfg || state->transition.IsActive()) return;
    if (!state->stack.empty()) state->stack.pop_back();
    Push(id);
}

bool ScreenRouter::IsTransitioning() const {
    return state && state->transition.IsActive();
}

void ScreenRouter::UpdateTransition(float deltaMs) {
    if (!state || !cfg) return;
    const bool wasActive = state->transition.IsActive();
    state->transition.Update(deltaMs, state->reduce_motion);
    if (wasActive && !state->transition.IsActive()) {
        if (state->pending_pop) {
            if (state->stack.size() > 1) state->stack.pop_back();
            state->pending_pop = false;
            state->transition.alpha = 1.f;
        }
    }
}

void MenuAppInit(const AppConfig& config) {
    MediaLibraryLimits limits;
    limits.max_image_bytes = config.background.max_image_bytes;
    limits.max_video_bytes = config.background.max_video_bytes;
    MediaLibrarySetLimits(limits);
    ProfileAvatarInit();
}

void MenuAppRunPipeAction(MenuAppState& state, const std::string& command, bool showSuccessToast) {
    MenuAppState* statePtr = &state;
    const bool refreshPacksOnSuccess =
        command.rfind("SET_PACK_ORDER:", 0) == 0 || command.rfind("SET_PACK_TOGGLE:", 0) == 0;
    std::thread([statePtr, command, showSuccessToast, refreshPacksOnSuccess]() {
        const PipeQueryResult result = PipeQueryJson(command, 8000);
        std::lock_guard<std::mutex> lock(statePtr->action_mutex);
        if (result.ok) {
            if (command.rfind("CREATE_WORLD_SUBMIT:", 0) == 0) {
                statePtr->create_world_pending = false;
            }
            if (refreshPacksOnSuccess) {
                statePtr->data.options_loading = false;
                statePtr->data.options_fetch_done = false;
            }
            if (showSuccessToast) {
                statePtr->pending_toast = "\xe6\x93\x8d\xe4\xbd\x9c\xe5\xb7\xb2\xe6\x89\xa7\xe8\xa1\x8c";
                statePtr->pending_toast_error = false;
                statePtr->pending_toast_ms = 2200.f;
            }
        } else {
            if (command.rfind("CREATE_WORLD_SUBMIT:", 0) == 0) {
                statePtr->create_world_pending = false;
            }
            std::string msg = result.error.empty() ? "pipe unreachable" : result.error;
            if (msg == "option") {
                msg = "\xe8\xae\xbe\xe7\xbd\xae\xe9\xa1\xb9\xe6\x9a\x82\xe4\xb8\x8d\xe6\x94\xaf\xe6\x8c\x81";
            } else if (msg == "quit") {
                msg = "\xe6\x97\xa0\xe6\xb3\x95\xe9\x80\x80\xe5\x87\xba\xe6\xb8\xb8\xe6\x88\x8f";
            } else if (msg == "create") {
                msg = "\xe5\x88\x9b\xe5\xbb\xba\xe4\xb8\x96\xe7\x95\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8c\xe8\xaf\xb7\xe6\x9f\xa5\xe7\x9c\x8b\xe6\x97\xa5\xe5\xbf\x97";
            } else             if (msg == "pack") {
                msg = "\xe8\xb5\x84\xe6\xba\x90\xe5\x8c\x85\xe6\x93\x8d\xe4\xbd\x9c\xe5\xa4\xb1\xe8\xb4\xa5";
            } else if (msg == "folder") {
                msg = "\xe6\x97\xa0\xe6\xb3\x95\xe6\x89\x93\xe5\xbc\x80\xe8\xb5\x84\xe6\xba\x90\xe5\x8c\x85\xe6\x96\x87\xe4\xbb\xb6\xe5\xa4\xb9";
            } else if (msg == "connect") {
                msg = "\xe8\xbf\x9e\xe6\x8e\xa5\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe5\xa4\xb1\xe8\xb4\xa5";
            } else if (msg == "join") {
                msg = "\xe8\xbf\x9b\xe5\x85\xa5\xe4\xb8\x96\xe7\x95\x8c\xe5\xa4\xb1\xe8\xb4\xa5";
            }
            statePtr->pending_toast = msg;
            statePtr->pending_toast_error = true;
            statePtr->pending_toast_ms = 3200.f;
        }
    }).detach();
}

void MenuAppSetWindowHandle(HWND hwnd) {
    MediaLibrarySetWindowHandle(hwnd);
}

void MenuAppOnMenuResumed(MenuAppState& state) {
    state.stack = {ScreenId::Home};
    state.pending_pop = false;
    state.show_manager = false;
    state.create_world_pending = false;
    state.transition.active = false;
    state.transition.t = 0.f;
    state.transition.alpha = 1.f;

    MenuAppState* statePtr = &state;
    std::thread([statePtr]() {
        const PipeQueryResult result = PipeQueryJson("GET_DISCONNECT_REASON", 800);
        if (!result.ok || result.body.empty()) return;
        std::lock_guard<std::mutex> lock(statePtr->action_mutex);
        statePtr->pending_toast = result.body;
        statePtr->pending_toast_error = true;
        statePtr->pending_toast_ms = 4200.f;
    }).detach();

    state.data.worlds.clear();
    state.data.worlds_fetch_done = false;
    state.data.worlds_loading = false;
    state.data.worlds_load_ms = 0.f;

    state.data.servers.clear();
    state.data.servers_fetch_done = false;
    state.data.servers_loading = false;
    state.data.servers_load_ms = 0.f;

    {
        std::lock_guard<std::mutex> lock(state.pending_load.mutex);
        state.pending_load.ready = false;
        state.pending_load.kind = PipeLoadKind::None;
    }
}

void MenuAppHandleEsc(MenuAppState& state, AppConfig& cfg) {
    if (state.show_manager) {
        state.show_manager = false;
        return;
    }
    if (state.transition.IsActive()) return;
    ScreenRouter router{&state, &cfg};
    if (router.Current() != ScreenId::Home) {
        router.Pop();
    }
}

void MenuAppRender(MenuRenderContext& ctx) {
    auto& state = ctx.state;
    auto& cfg = ctx.cfg;

    UiManagerApplyAccentPreset(cfg, state.manager.settings.accent_preset);
    cfg.theme.blur_radius = static_cast<float>(state.manager.settings.blur_strength);
    if (state.manager.settings.hover_scale_enabled) {
        cfg.motion.hover_scale = state.manager.settings.hover_scale;
    }
    cfg.background.vignette_strength = state.manager.settings.vignette_strength;
    state.reduce_motion = !state.manager.settings.hover_scale_enabled;

    if (state.options_nav.empty()) {
        LoadOptionsHubNav(cfg.root_path, state.options_nav);
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float deltaMs = ImGui::GetIO().DeltaTime * 1000.f;
    myiui::intro::IntroScreenOnMenuActive(state.intro);

    if (myiui::intro::IntroScreenIsBlocking(state.intro)) {
        ctx.scale = UiScale(display);
        myiui::intro::IntroScreenRender(ctx, state.intro);
        return;
    }

    ScreenRouter router{&state, &cfg};
    MenuAppFlushPendingLoads(state);
    router.UpdateTransition(deltaMs);

    {
        std::lock_guard<std::mutex> lock(state.action_mutex);
        if (!state.pending_toast.empty()) {
            ToastShow(state.toast, state.pending_toast, state.pending_toast_ms, state.pending_toast_error);
            state.pending_toast.clear();
        }
    }

    ToastUpdate(state.toast, deltaMs);

    ctx.scale = UiScale(display);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display);
    ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    if (state.show_manager) {
        mainFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    }
    ImGui::Begin("##MyiUI_MenuApp", nullptr, mainFlags);

    DrawMenuBackground(ctx);
    const bool homeInputs = !state.show_manager && router.Current() == ScreenId::Home;
    DrawMenuTopBar(ctx, homeInputs);

    const float sideMargin = Px(48.f, ctx.scale);
    const ImVec2 btnSize(Px(cfg.layout.nav_button_w, ctx.scale), Px(cfg.layout.nav_button_h, ctx.scale));
    const float gap = Px(static_cast<float>(cfg.layout.nav_gap), ctx.scale);
    const size_t navCount = (std::min)(cfg.layout.nav_items.size(), size_t{7});
    const float profileH = Px(168.f, ctx.scale);
    const float menuH =
        static_cast<float>(navCount) * btnSize.y + static_cast<float>(navCount > 0 ? navCount - 1 : 0) * gap;
    const float clusterH = (std::max)(profileH, menuH);
    const float clusterTop = cfg.layout.main_center_y * display.y - clusterH * 0.5f;
    if (!state.show_manager) {
        DrawMenuProfile(ctx, clusterTop, clusterH);
    }

    const ScreenId current = router.Current();
    if (current == ScreenId::Home) {
        RenderHomeScreen(ctx, router);
    } else {
        if (current == ScreenId::Singleplayer) {
            RenderSingleplayerScreen(ctx, router);
        } else if (current == ScreenId::CreateWorld) {
            RenderCreateWorldScreen(ctx, router);
        } else if (current == ScreenId::Multiplayer) {
            RenderMultiplayerScreen(ctx, router);
        } else if (current == ScreenId::AddServer) {
            RenderAddServerScreen(ctx, router);
        } else if (current == ScreenId::OptionsHub) {
            RenderOptionsHubScreen(ctx, router);
        } else if (IsOptionsDetailScreen(current)) {
            RenderOptionsDetailScreen(ctx, router);
        }
    }

    if (state.show_manager) {
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, 140));
    }

    ImGui::End();

    if (state.show_manager) {
        UiManagerRenderPanel(cfg, state.manager, ctx.scale, &state.show_manager);
    }

    ToastRender(state.toast, ctx.scale);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        MenuAppHandleEsc(state, cfg);
    }
}
