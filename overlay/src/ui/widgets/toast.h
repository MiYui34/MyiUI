#pragma once

#include <string>

struct ToastState {
    std::string title;
    std::string message;
    float remaining_ms = 0.f;
    float anim = 0.f;
    bool is_error = false;
};

void ToastShow(ToastState& toast, const std::string& message, float durationMs, bool isError = false);
void ToastUpdate(ToastState& toast, float deltaMs);
void ToastRender(const ToastState& toast, float scale);
