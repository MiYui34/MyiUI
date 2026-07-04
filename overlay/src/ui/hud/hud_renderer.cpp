#include "ui/hud/hud_renderer.h"

#include "config/user_settings.h"
#include "ipc/shm_reader.h"
#include "ui/clickgui/clickgui.h"
#include "ui/hud/info_widgets.h"
#include "ui/hud/now_playing.h"

namespace myiui::ui::hud {

void HudRender(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH, float dt) {
    if (!myiui::config::GetUserSettingsConst().hud_visible) return;
    if (!myiui::ui::clickgui::HudVisible()) return;
    if (myiui::ui::clickgui::IsOpen()) return;
    if (shm.GetScreenKind() != myiui::shared::ScreenKind::InGame) return;

    myiui::shared::InfoHudState info{};
    myiui::shared::MusicHudState music{};
    shm.ReadInfoHudState(info);
    shm.ReadMusicHudState(music);

    const auto& settings = myiui::config::GetUserSettingsConst();
    RenderInfoWidgets(cfg, info, settings, viewportW, viewportH);
    RenderNowPlaying(cfg, music, settings.now_playing, viewportW, viewportH, dt);
}

}  // namespace myiui::ui::hud
