#include "ui/hud/hud_renderer.h"

#include "bridge/native_state.h"
#include "config/user_settings.h"
#include "ipc/shm_reader.h"
#include "ui/clickgui/clickgui.h"
#include "ui/hud/immersive_lyrics.h"
#include "ui/hud/now_playing.h"
#include "ui/hud/now_playing_layout.h"

namespace myiui::ui::hud {

void HudRender(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH, float dt, bool layoutPreview) {
    if (!myiui::config::GetUserSettingsConst().hud_visible) return;
    if (!myiui::ui::clickgui::HudVisible()) return;
    if (myiui::ui::clickgui::IsOpen() && !layoutPreview) return;
    if (shm.GetScreenKind() != myiui::shared::ScreenKind::InGame) return;

    myiui::shared::MusicHudState music{};
    shm.ReadMusicHudState(music);

    const auto& settings = myiui::config::GetUserSettingsConst();
    RenderNowPlaying(cfg, music, settings.now_playing, viewportW, viewportH, dt);
}

void HudRenderImmersiveLyrics(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH) {
    if (!myiui::config::GetUserSettingsConst().hud_visible) return;
    if (!myiui::ui::clickgui::HudVisible()) return;
    if (myiui::ui::clickgui::IsOpen()) return;
    if (shm.GetScreenKind() != myiui::shared::ScreenKind::InGame) return;
    if (!myiui::config::GetUserSettingsConst().now_playing.immersive_lyrics) return;

    myiui::shared::MusicHudState music{};
    shm.ReadMusicHudState(music);
    myiui::shared::IslandState island{};
    myiui::bridge::NativeState::Instance().ReadIsland(island);
    RenderImmersiveLyrics(cfg, music, island, myiui::config::GetUserSettingsConst(), viewportW, viewportH);
}

}  // namespace myiui::ui::hud
