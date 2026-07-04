#pragma once

class ShmReader;
struct AppConfig;

namespace myiui::ui::hud {

void HudRender(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH, float dt,
               bool layoutPreview = false);

/** 在所有 HUD 层之后绘制，避免被灵动岛等覆盖。 */
void HudRenderImmersiveLyrics(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH);

}  // namespace myiui::ui::hud
