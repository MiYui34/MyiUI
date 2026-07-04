#pragma once

class ShmReader;
struct AppConfig;

namespace myiui::ui::hud {

void HudRender(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH, float dt);

}  // namespace myiui::ui::hud
