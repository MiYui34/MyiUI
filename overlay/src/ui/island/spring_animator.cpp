#include "spring_animator.h"
#include <cmath>

namespace myiui::ui::island {

void Spring1D::Step(float dt) {
    // 限制单帧最大时间跨度，防止挂机/卡顿切回时物理引擎爆炸
    float clamped_dt = std::min(dt, 0.1f); 
    
    // 引入子步长 (Sub-stepping) 以确保高刚度弹簧的数值稳定性
    const float step_size = 0.005f; // 200Hz 内部更新率
    int steps = static_cast<int>(std::ceil(clamped_dt / step_size));
    float sub_dt = clamped_dt / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        float force = -stiffness * (pos - target);
        float damp = -damping * vel;
        // 半隐式欧拉 (Semi-implicit Euler): 先更新速度，再用新速度更新位置
        vel += (force + damp) * sub_dt;
        pos += vel * sub_dt;
    }
}

}  // namespace myiui::ui::island