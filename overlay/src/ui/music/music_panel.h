#pragma once

#include "imgui.h"

namespace myiui::ui::music {

// ClickGui 音乐面板：登录、播放控制、搜索、歌单、每日推荐、播放记录。
// 在 ClickGui 渲染循环中由 clickgui.cpp 调用，绘制在内容区。
// contentMin/contentMax 为内容区矩形（与普通模块卡片相同坐标系）。
void MusicPanelRender(ImVec2 contentMin, ImVec2 contentMax, float alpha, float dt);

// 渲染线程每帧调用：处理待上传的封面纹理。
void MusicPanelTick();

}  // namespace myiui::ui::music
