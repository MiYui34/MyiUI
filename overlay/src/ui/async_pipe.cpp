#include "ui/async_pipe.h"

#include "ui/menu_app.h"
#include "ui/menu_data.h"
#include "ui/profile_avatar.h"
#include "ui/widgets/toast.h"

#include <thread>

void MenuAppFlushPendingLoads(MenuAppState& state) {
  PipeLoadKind kind = PipeLoadKind::None;
  PipeQueryResult result;
  std::string optionsJsonName;
  {
    std::lock_guard<std::mutex> lock(state.pending_load.mutex);
    if (!state.pending_load.ready) return;
    kind = state.pending_load.kind;
    result = state.pending_load.result;
    optionsJsonName = state.pending_load.options_json_name;
    state.pending_load.ready = false;
    state.pending_load.kind = PipeLoadKind::None;
  }

  switch (kind) {
    case PipeLoadKind::Worlds:
      state.data.worlds_loading = false;
      state.data.worlds_fetch_done = true;
      if (result.ok) {
        ParseWorldsJson(result.body, state.data.worlds);
      } else if (state.data.worlds.empty()) {
        LoadWorldsFromDisk(state.data.worlds);
      }
      if (!result.ok && state.data.worlds.empty()) {
        ToastShow(state.toast, result.error.empty() ? "pipe unreachable" : result.error.c_str(), 2800.f, true);
      }
      break;
    case PipeLoadKind::Servers:
      state.data.servers_loading = false;
      state.data.servers_fetch_done = true;
      if (result.ok) {
        ParseServersJson(result.body, state.data.servers);
      } else if (state.data.servers.empty()) {
        LoadServersFromDisk(state.data.servers);
      }
      if (!result.ok && state.data.servers.empty()) {
        ToastShow(state.toast, result.error.empty() ? "pipe unreachable" : result.error.c_str(), 2800.f, true);
      }
      break;
    case PipeLoadKind::Profile: {
      const std::string prevSkinPath = state.data.profile.skin_path;
      const std::string prevSkinUrl = state.data.profile.skin_url;
      state.data.profile_loading = false;
      state.data.profile_fetch_done = true;
      if (result.ok) {
        ParsePlayerJson(result.body, state.data.profile);
      } else if (state.data.profile.name.empty() || state.data.profile.name == "Player") {
        LoadProfileFromDisk(state.data.profile);
      }
      state.data.profile_avatar_url.clear();
      if (state.data.profile.skin_path != prevSkinPath || state.data.profile.skin_url != prevSkinUrl) {
        ProfileAvatarInvalidate();
      }
      break;
    }
    case PipeLoadKind::Options:
      state.data.options_loading = false;
      state.data.options_fetch_done = true;
      if (result.ok) {
        std::string merged = result.body;
        for (const auto& entry : state.slider_drafts) {
          SetOptionValueInJson(merged, entry.first, std::to_string(static_cast<int>(entry.second)));
        }
        state.data.options_json = std::move(merged);
        state.data.options_baseline_json = state.data.options_json;
        state.pending_option_changes.clear();
        state.slider_drafts.clear();
        if (optionsJsonName == "options_controls") {
          ParseKeybindsJson(result.body, state.data.keybinds);
        } else if (optionsJsonName == "options_resource_packs") {
          ParsePacksJson(result.body, state.data.packs);
        }
      }
      if (!result.ok && state.data.options_json.empty() && state.options_spec.rows.empty()) {
        ToastShow(state.toast, result.error.empty() ? "pipe unreachable" : result.error.c_str(), 2800.f, true);
      }
      break;
    default:
      break;
  }
}

void MenuAppStartPipeLoad(MenuAppState& state, PipeLoadKind kind, const std::string& command,
                          const std::string& optionsJsonName) {
  MenuAppState* statePtr = &state;
  std::thread([statePtr, kind, command, optionsJsonName]() {
    const PipeQueryResult result = PipeQueryJson(command, 5000);
    std::lock_guard<std::mutex> lock(statePtr->pending_load.mutex);
    statePtr->pending_load.ready = true;
    statePtr->pending_load.kind = kind;
    statePtr->pending_load.result = result;
    statePtr->pending_load.options_json_name = optionsJsonName;
  }).detach();
}
