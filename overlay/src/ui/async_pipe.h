#pragma once

#include "ipc/pipe_client.h"
#include "ui/menu_data.h"

#include <mutex>
#include <string>

enum class PipeLoadKind { None, Worlds, Servers, Options, Profile };

struct PendingPipeLoad {
    std::mutex mutex;
    bool ready = false;
    PipeLoadKind kind = PipeLoadKind::None;
    PipeQueryResult result;
    std::string options_json_name;
};

void MenuAppFlushPendingLoads(struct MenuAppState& state);
void MenuAppStartPipeLoad(struct MenuAppState& state, PipeLoadKind kind, const std::string& command,
                          const std::string& optionsJsonName = {});
