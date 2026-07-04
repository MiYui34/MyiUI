// overlay/src/ui/widgets/audio_visualizer.cpp
#include "audio_visualizer.h"
#include <imgui_internal.h>
#include <cmath>
#include <vector>

namespace MyiUI {
namespace Widgets {

    void DrawAudioVisualizer(const ImVec2& size, bool is_playing, ImU32 primary_color) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return;

        ImDrawList* draw_list = window->DrawList;
        ImVec2 p_min = window->DC.CursorPos;
        ImVec2 p_max = ImVec2(p_min.x + size.x, p_min.y + size.y);

        // 频谱柱的参数配置
        const int num_bars = 20;            // 柱子数量
        const float bar_padding = 2.0f;     // 柱子间距
        const float bar_width = (size.x - (num_bars - 1) * bar_padding) / num_bars;
        
        // 获取动画时间
        float time = (float)ImGui::GetTime();
        
        // 我们需要一个静态数组来平滑过渡动画（让暂停时柱子慢慢降下来，而不是瞬间消失）
        static std::vector<float> smoothed_heights(num_bars, 0.1f);

        for (int i = 0; i < num_bars; i++) {
            float target_height = 0.1f; // 默认最小高度（暂停时的大小）

            if (is_playing) {
                // 使用三个不同频率和相位的正弦波叠加，制造随机但连续的跳动感
                float t = time * 4.0f + (float)i * 0.3f;
                float wave1 = std::sin(t);
                float wave2 = std::sin(t * 1.5f + 1.2f);
                float wave3 = std::sin(t * 0.7f + 2.5f);
                
                // 将波形映射到 0.1 ~ 1.0 的范围
                float combined_wave = (wave1 + wave2 + wave3) / 3.0f; 
                target_height = 0.2f + std::abs(combined_wave) * 0.8f;
                
                // 让中间的柱子跳得更高，两边的较平缓 (高斯分布模拟)
                float distance_from_center = std::abs((float)i - (num_bars / 2.0f)) / (num_bars / 2.0f);
                float bell_curve = 1.0f - (distance_from_center * distance_from_center);
                target_height *= (0.4f + 0.6f * bell_curve);
            }

            // 平滑动画 (Lerp 插值)
            float delta_time = ImGui::GetIO().DeltaTime;
            // 播放时反应快(15.0f)，暂停时降落慢(5.0f)
            float lerp_speed = is_playing ? 15.0f : 5.0f; 
            smoothed_heights[i] = ImLerp(smoothed_heights[i], target_height, ImClamp(delta_time * lerp_speed, 0.0f, 1.0f));

            // 计算单个柱子的坐标
            float bar_height = size.y * smoothed_heights[i];
            ImVec2 bar_min = ImVec2(p_min.x + i * (bar_width + bar_padding), p_max.y - bar_height);
            ImVec2 bar_max = ImVec2(bar_min.x + bar_width, p_max.y);

            // 绘制圆角矩形
            float rounding = bar_width * 0.5f; 
            draw_list->AddRectFilled(bar_min, bar_max, primary_color, rounding);
        }

        // 占位，让 ImGui 的光标移到组件下方
        ImGui::Dummy(size);
    }

} // namespace Widgets
} // namespace MyiUI