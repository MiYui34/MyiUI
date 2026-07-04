#pragma once

namespace myiui::ui::clickgui {

void Toggle();
bool IsOpen();
void Render(float viewportW, float viewportH, float dt);
bool HandleKey(int key, int scancode, int action);

// Settings getters — consumed by island/chat/hud renderers
bool IslandVisible();
float IslandScale();
bool IslandBlur();
float IslandOpacity();
bool ShowFps();
bool HudVisible();
bool ChatVisible();

void SyncTheme(const int accent[4]);

// ESC keyup 抑制：ClickGui 用 ESC 关闭后，吞掉随后的 ESC up，避免游戏弹出暂停菜单
void RequestSuppressEscUp();
bool ConsumeSuppressEscUp();

}  // namespace myiui::ui::clickgui
