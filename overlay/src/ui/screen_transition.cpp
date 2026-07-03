#include "ui/screen_transition.h"

#include "ui/easing.h"

void ScreenTransition::Configure(const PageTransitionConfig& cfg, bool reduceMotion) {
    duration_ms = reduceMotion ? cfg.reduce_motion_ms : cfg.duration_ms;
    reduce_motion_ms = cfg.reduce_motion_ms;
    slide_px = reduceMotion ? 0.f : cfg.slide_px;
}

void ScreenTransition::Begin(bool fadingIn) {
    active = true;
    fade_in = fadingIn;
    t = 0.f;
    alpha = fadingIn ? 0.f : 1.f;
}

void ScreenTransition::Update(float deltaMs, bool reduceMotion) {
    if (!active) {
        alpha = 1.f;
        return;
    }
    if (deltaMs <= 0.f) {
        deltaMs = 16.f;
    }
    const float dur = reduceMotion ? reduce_motion_ms : duration_ms;
    if (dur <= 0.f) {
        active = false;
        alpha = fade_in ? 1.f : 0.f;
        return;
    }
    t += deltaMs / dur;
    if (t >= 1.f || t > 2.f) {
        t = 1.f;
        active = false;
        alpha = fade_in ? 1.f : 0.f;
        return;
    }
    const float eased = myiui::easing::EaseOutQuad(t);
    alpha = fade_in ? eased : (1.f - eased);
}
