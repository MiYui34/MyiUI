#pragma once

#include "ui/intro/intro_easing.h"
#include "ui/intro/intro_tokens.h"

namespace myiui::intro {

struct PanSharpenState {
    float blur_px = 48.f;
    float brightness = 0.7f;
    float saturate = 0.5f;
    float scale = 1.18f;
};

inline float GetIrisRadiusPct(float elapsedMs) {
    const float t = elapsedMs / kDurationMs;
    static const float times[] = {0.f, 0.42f, 0.58f, 0.72f, 0.88f, 1.f};
    static const float values[] = {0.f, 0.f, 28.f, 58.f, 92.f, 150.f};
    return LerpKeyframes(t, times, values, 6);
}

inline float GetVoidOpacity(float elapsedMs) {
    const float t = elapsedMs / kDurationMs;
    // 黑幕在 Logo 下层作暗底；与 timeline.json 一致
    static const float times[] = {0.f, 0.38f, 0.58f, 1.f};
    static const float values[] = {1.f, 1.f, 0.65f, 0.f};
    return LerpKeyframes(t, times, values, 4);
}

inline PanSharpenState GetPanSharpen(float elapsedMs) {
    const float t = elapsedMs / kDurationMs;
    PanSharpenState s;
    static const float times[] = {0.f, 0.42f, 0.58f, 0.74f, 1.f};
    static const float blur[] = {48.f, 48.f, 28.f, 10.f, 0.f};
    static const float bright[] = {0.7f, 0.7f, 0.85f, 0.95f, 1.f};
    static const float sat[] = {0.5f, 0.5f, 0.75f, 0.95f, 1.05f};
    static const float scale[] = {1.18f, 1.18f, 1.1f, 1.06f, 1.05f};
    s.blur_px = LerpKeyframes(t, times, blur, 5);
    s.brightness = LerpKeyframes(t, times, bright, 5);
    s.saturate = LerpKeyframes(t, times, sat, 5);
    s.scale = LerpKeyframes(t, times, scale, 5);
    return s;
}

inline float GetOverlayOpacity(float elapsedMs) {
    return EasePremium(TrackProgress(elapsedMs, 5200.f, 1800.f));
}

inline float GetLetterboxHeightNorm(float elapsedMs) {
    const float p = EasePremium(TrackProgress(elapsedMs, 4200.f, 1200.f));
    return Lerp(0.12f, 0.f, p);
}

inline float GetVignetteOpacity(float elapsedMs) {
    return 0.85f * EasePremium(TrackProgress(elapsedMs, 2800.f, 2400.f));
}

inline float GetEmblemOpacity(float elapsedMs) {
    if (elapsedMs < 1200.f) {
        return EasePremium(TrackProgress(elapsedMs, 1200.f, 1600.f));
    }
    if (elapsedMs >= 5200.f) {
        return 1.f - EasePremium(TrackProgress(elapsedMs, 5200.f, 1200.f));
    }
    return 1.f;
}

inline float GetEmblemScale(float elapsedMs) {
    if (elapsedMs < 1200.f) {
        return 0.78f;
    }
    if (elapsedMs < 2000.f) {
        const float p = EasePremium(TrackProgress(elapsedMs, 1200.f, 800.f));
        return Lerp(0.78f, 1.02f, p);
    }
    if (elapsedMs < 2800.f) {
        return Lerp(1.02f, 1.f, EasePremium(TrackProgress(elapsedMs, 2000.f, 800.f)));
    }
    if (elapsedMs >= 5200.f) {
        return Lerp(1.f, 0.96f, EasePremium(TrackProgress(elapsedMs, 5200.f, 1200.f)));
    }
    return 1.f;
}

inline float GetEmblemTranslateY(float elapsedMs) {
    if (elapsedMs < 1200.f) {
        return 12.f;
    }
    if (elapsedMs < 2000.f) {
        const float p = EasePremium(TrackProgress(elapsedMs, 1200.f, 800.f));
        return Lerp(12.f, -4.f, p);
    }
    if (elapsedMs < 2800.f) {
        return Lerp(-4.f, 0.f, EasePremium(TrackProgress(elapsedMs, 2000.f, 800.f)));
    }
    if (elapsedMs >= 5200.f) {
        return Lerp(0.f, -32.f, EasePremium(TrackProgress(elapsedMs, 5200.f, 1200.f)));
    }
    return 0.f;
}

inline float GetRingProgress(float elapsedMs, float startMs, float durationMs) {
    return EasePremium(TrackProgress(elapsedMs, startMs, durationMs));
}

inline float GetLetterReveal(float elapsedMs, int letterIndex) {
    const float start = 2000.f + static_cast<float>(letterIndex) * 140.f;
    return EasePremium(TrackProgress(elapsedMs, start, 900.f));
}

inline float GetBootHudOpacity(float elapsedMs) {
    if (elapsedMs < 3400.f) return 0.f;
    if (elapsedMs < 6400.f) return EasePremium(TrackProgress(elapsedMs, 3400.f, 900.f));
    return 1.f - EasePremium(TrackProgress(elapsedMs, 6400.f, 1000.f));
}

inline float GetBootLineOpacity(float elapsedMs, float lineStartMs) {
    return EasePremium(TrackProgress(elapsedMs, lineStartMs, 500.f));
}

inline float GetProgressRing(float elapsedMs) {
    return EasePremium(TrackProgress(elapsedMs, 0.f, kDurationMs));
}

inline float GetSuccessChipOpacity(float elapsedMs) {
    return EasePremium(TrackProgress(elapsedMs, 6600.f, 1400.f));
}

inline float GetExitFadeOpacity(float elapsedMs) {
    return EasePremium(TrackProgress(elapsedMs, 7400.f, 900.f));
}

inline float GetAmbientGlowOpacity(float elapsedMs) {
    const float p = TrackProgress(elapsedMs, 400.f, 3200.f);
    if (p <= 0.f) return 0.f;
    if (p >= 1.f) return 0.35f;
    if (p < 0.4f) return EasePremium(p / 0.4f);
    return Lerp(1.f, 0.35f, EasePremium((p - 0.4f) / 0.6f));
}

inline float GetLightSweepX(float elapsedMs) {
    return Lerp(-0.6f, 0.6f, EaseCinema(TrackProgress(elapsedMs, 300.f, 2200.f)));
}

inline float GetLightSweepOpacity(float elapsedMs) {
    const float p = TrackProgress(elapsedMs, 300.f, 2200.f);
    if (p <= 0.f || p >= 1.f) return 0.f;
    if (p < 0.15f) return p / 0.15f;
    return 1.f - (p - 0.15f) / 0.85f;
}

inline float GetScanLineY(float elapsedMs, float height) {
    const float p = EaseCinema(TrackProgress(elapsedMs, 500.f, 1400.f));
    return Lerp(-1.f, height + 1.f, p);
}

inline float GetScanLineOpacity(float elapsedMs) {
    const float p = TrackProgress(elapsedMs, 500.f, 1400.f);
    if (p <= 0.f || p >= 1.f) return 0.f;
    if (p < 0.1f) return p / 0.1f * 0.7f;
    if (p > 0.9f) return (1.f - p) / 0.1f * 0.4f;
    return 0.55f;
}

}  // namespace myiui::intro
