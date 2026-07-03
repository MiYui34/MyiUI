#pragma once

#include <cmath>

namespace myiui::ui::island {

// ── Easing functions (from MakiseClient Easings.java) ──

inline float EaseExpoOut(float t) {
    return t >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * t);
}

inline float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
}

inline float LerpF(float delta, float a, float b) {
    return a + delta * (b - a);
}

inline float ClampF(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ── Time-based animation (from MakiseClient Animation.java) ──

struct TimeAnim {
    float start = 0.f;
    float target = 0.f;
    float startTimeMs = 0.f;
    float durationMs = 300.f;
    bool running = false;

    void SetValue(float v) {
        start = v;
        target = v;
        running = false;
    }

    void Animate(float tgt, float nowMs) {
        if (tgt == target) return;
        start = GetValue(nowMs);
        target = tgt;
        startTimeMs = nowMs;
        running = true;
    }

    float GetValue(float nowMs) const {
        if (!running) return target;
        float t = (nowMs - startTimeMs) / durationMs;
        if (t >= 1.f) return target;
        return start + (target - start) * EaseExpoOut(t);
    }
};

// ── Island layout tokens (matching MakiseClient style) ──

inline constexpr float kIslandRadius = 12.f;
inline constexpr float kIslandIdleH = 26.f;
inline constexpr float kIslandExpandedH = 40.f;
inline constexpr float kIslandPad = 14.f;
inline constexpr float kIslandHoldMs = 2000.f;
inline constexpr float kDotSepWidth = 14.f;  // "  •  "

}  // namespace myiui::ui::island
