// overlay/src/ui/widgets/audio_visualizer.h
#pragma once
#include <imgui.h>

namespace MyiUI {
namespace Widgets {

    // 绘制一个动态的音频波形
    // size: 组件大小
    // is_playing: 当前是否正在播放音乐 (决定波形是否跳动)
    // primary_color: 波形的颜色 (建议传入 Material You 的强调色)
    void DrawAudioVisualizer(const ImVec2& size, bool is_playing, ImU32 primary_color);

} // namespace Widgets
} // namespace MyiUI