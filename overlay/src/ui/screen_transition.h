#pragma once

#include "config/config_loader.h"

struct ScreenTransition {
    float duration_ms = 180.f;
    float reduce_motion_ms = 80.f;
    float slide_px = 0.f;
    bool active = false;
    bool fade_in = true;
    float t = 0.f;
    float alpha = 1.f;

    void Configure(const PageTransitionConfig& cfg, bool reduceMotion);
    void Begin(bool fadingIn);
    void Update(float deltaMs, bool reduceMotion);
    bool IsActive() const { return active; }
    float ContentAlpha() const { return alpha; }
};
