#pragma once

#include "imgui.h"
#include "ui/glass_panel.h"

namespace myiui::ui::widgets {

// 悬停放大 + 视差偏移时，相对布局尺寸向外扩展的安全边距
float ParallaxCardHoverMargin(const ImVec2& size);

// 将 ForegroundDrawList 上的视差卡片裁切到当前滚动子窗口可视区域（上下边界）
struct ParallaxCardClipGuard {
    ParallaxCardClipGuard();
    explicit ParallaxCardClipGuard(const ImVec2& clip_min, const ImVec2& clip_max);
    ~ParallaxCardClipGuard();
    ParallaxCardClipGuard(const ParallaxCardClipGuard&) = delete;
    ParallaxCardClipGuard& operator=(const ParallaxCardClipGuard&) = delete;

    bool active = false;
};

// 绘制带有 3D 视差悬浮、动态光晕和物理回弹的玻璃交互卡片
// size: 卡片尺寸
// pos: 卡片绝对坐标
// theme: 全局主题配置 (传递给玻璃渲染器)
// title, desc: 卡片前景内容
// reservedRightPx: 右侧保留宽度（按钮区等），不参与卡片点击
bool DrawParallaxCard(const char* str_id, const ImVec2& size, const ImVec2& pos, const ThemeConfig& theme,
                      const char* title, const char* desc, float reservedRightPx = 0.f);

}  // namespace myiui::ui::widgets
