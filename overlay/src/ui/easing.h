#pragma once

#include <cmath>

namespace myiui::easing {

inline float Clamp01(float v) {
    return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

inline float EaseOutCubic(float t) {
    t = Clamp01(t);
    const float u = 1.f - t;
    return 1.f - u * u * u;
}

inline float EaseOutQuad(float t) {
    t = Clamp01(t);
    const float u = 1.f - t;
    return 1.f - u * u;
}

inline float EaseOutBack(float t, float overshoot = 1.70158f) {
    t = Clamp01(t);
    const float u = t - 1.f;
    return 1.f + (overshoot + 1.f) * u * u * u + overshoot * u * u;
}

inline float EaseOutElastic(float t) {
    t = Clamp01(t);
    if (t == 0.f || t == 1.f) {
        return t;
    }
    const float p = 0.3f;
    return std::pow(2.f, -10.f * t) * std::sin((t - p / 4.f) * (2.f * 3.14159265f) / p) + 1.f;
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace myiui::easing
