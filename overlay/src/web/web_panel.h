#pragma once

namespace myiui::web {

// ImGui chrome + WebView2 content host.
// Enabled via ClickGui → 视觉 → Web面板.
bool WebPanelWanted();
// True while the in-game browser should keep the overlay input path alive.
bool WebPanelActive();
void WebPanelTickAndRender();
void WebPanelShutdown();

}  // namespace myiui::web
