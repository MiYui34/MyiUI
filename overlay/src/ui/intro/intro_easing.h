#pragma once

#include <algorithm>
#include <cmath>

namespace myiui::intro {

inline float Clamp01(float v) {
    return std::clamp(v, 0.f, 1.f);
}

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float CubicBezier(float t, float x1, float y1, float x2, float y2) {
    t = Clamp01(t);
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    const float cx = 3.f * x1;
    const float bx = 3.f * (x2 - x1) - cx;
    const float ax = 1.f - cx - bx;
    const float cy = 3.f * y1;
    const float by = 3.f * (y2 - y1) - cy;
    const float ay = 1.f - cy - by;
    auto sampleX = [&](float u) { return ((ax * u + bx) * u + cx) * u; };
    auto sampleY = [&](float u) { return ((ay * u + by) * u + cy) * u; };
    float x = t;
    for (int i = 0; i < 8; ++i) {
        const float curX = sampleX(x) - t;
        if (std::fabs(curX) < 1e-5f) break;
        const float dX = (3.f * ax * x + 2.f * bx) * x + cx;
        if (std::fabs(dX) < 1e-6f) break;
        x -= curX / dX;
    }
    return sampleY(x);
}

inline float EasePremium(float t) { return CubicBezier(t, 0.16f, 1.f, 0.3f, 1.f); }
inline float EaseCinema(float t) { return CubicBezier(t, 0.77f, 0.f, 0.175f, 1.f); }

inline float TrackProgress(float elapsedMs, float startMs, float durationMs) {
    if (elapsedMs <= startMs) return 0.f;
    if (elapsedMs >= startMs + durationMs) return 1.f;
    return (elapsedMs - startMs) / durationMs;
}

inline float LerpKeyframes(float normalizedT, const float* times, const float* values, int count) {
    if (count <= 0) return 0.f;
    if (normalizedT <= times[0]) return values[0];
    for (int i = 1; i < count; ++i) {
        if (normalizedT <= times[i]) {
            const float localT = (normalizedT - times[i - 1]) / (times[i] - times[i - 1]);
            return Lerp(values[i - 1], values[i], localT);
        }
    }
    return values[count - 1];
}

}  // namespace myiui::intro
