#include "ui/hud/hud_renderer.h"

#include "ipc/shm_reader.h"
#include "config/config_loader.h"

namespace myiui::ui::hud {

void HudRender(const ThemeConfig&, const ShmReader&, float, float) {
    // Status HUD reverted to vanilla — overlay does not draw in-game bars.
}

} // namespace myiui::ui::hud
