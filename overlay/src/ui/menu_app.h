#pragma once

#include "config/config_loader.h"
#include "ui/menu_data.h"
#include "ui/screen_id.h"
#include "ui/async_pipe.h"
#include "ui/intro/intro_screen.h"
#include "ui/async_pipe.h"
#include "ui/screen_transition.h"
#include "ui/ui_manager.h"
#include "ui/widgets/toast.h"

#include "imgui.h"

#include <windows.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct MenuAppState {
    std::vector<ScreenId> stack{ScreenId::Home};
    ScreenTransition transition;
    bool show_manager = false;
    float hover_anim[128]{};
    float back_hover = 0.f;
    float apply_hover = 0.f;
    UiManagerState manager;
    MenuDataCache data;
    ToastState toast;
    bool reduce_motion = false;
    bool pending_pop = false;
    int keybind_capture_index = -1;
    std::vector<OptionsHubNavItem> options_nav;
    OptionsScreenSpec options_spec;
    std::mutex action_mutex;
    std::string pending_toast;
    float pending_toast_ms = 2800.f;
    bool pending_toast_error = false;
    PendingPipeLoad pending_load;
    std::unordered_map<std::string, float> slider_drafts;
    std::unordered_map<std::string, std::string> pending_option_changes;
    char create_world_name[128] = "New World";
    char create_world_seed[64]{};
    int create_world_mode = 0;
    bool create_world_pending = false;
    char add_server_name[128]{};
    char add_server_address[256]{};
    bool add_server_edit_mode = false;
    char editing_server_id[256]{};
    char world_search[128]{};
    char server_search[128]{};
    myiui::intro::IntroScreenState intro;
};

struct MenuRenderContext {
    AppConfig& cfg;
    MenuAppState& state;
    ImTextureID bgTexture;
    bool hasBg;
    int bgTexW;
    int bgTexH;
    float scale;
};

void MenuAppInit(const AppConfig& config);
void MenuAppSetWindowHandle(HWND hwnd);
void MenuAppOnMenuResumed(MenuAppState& state);
void MenuAppRender(MenuRenderContext& ctx);
void MenuAppHandleEsc(MenuAppState& state, AppConfig& cfg);
void MenuAppRunPipeAction(MenuAppState& state, const std::string& command, bool showSuccessToast = true);

struct ScreenRouter {
    MenuAppState* state = nullptr;
    const AppConfig* cfg = nullptr;

    ScreenId Current() const;
    void Push(ScreenId id);
    void Pop();
    void Replace(ScreenId id);
    bool IsTransitioning() const;
    void UpdateTransition(float deltaMs);
};
