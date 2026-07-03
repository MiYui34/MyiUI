#pragma once

#include "ui/island/island_tokens.h"  // EaseExpoOut, EaseOutBack, TimeAnim 已在此定义

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace myiui::ui::island {

// ── SmoothFollow (仅此处定义) ──

inline float SmoothFollow(float dt, float current, float target, float speed = 14.f) {
    return current + (target - current) * std::min(1.f, dt * speed);
}

// ── Spring physics ──

struct Spring1D {
    float pos = 0.f;
    float vel = 0.f;
    float target = 0.f;
    float stiffness = 280.f;
    float damping = 22.f;

    void SetTarget(float t) { target = t; }
    void Snap(float v) { pos = v; vel = 0.f; target = v; }
    bool Settled(float epsilon = 0.35f) const {
        return std::abs(pos - target) < epsilon && std::abs(vel) < epsilon;
    }
    void Step(float dt);
};

}  // namespace myiui::ui::island
