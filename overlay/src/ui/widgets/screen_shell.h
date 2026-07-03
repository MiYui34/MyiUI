#pragma once

#include "config/config_loader.h"
#include "ui/fonts.h"

#include "imgui.h"

struct ScreenShellLayout {
    ImVec2 panel_pos;
    ImVec2 panel_size;
    ImVec2 content_pos;
    ImVec2 content_size;
};

ScreenShellLayout CalcScreenShellLayout(const AppConfig& cfg, float scale);

void DrawScreenShellBackground(ImDrawList* dl, const ScreenShellLayout& layout, const AppConfig& cfg, float scale,
                               float contentAlpha);

bool ScreenShellHeader(const char* id, const ScreenShellLayout& layout, const AppConfig& cfg, const UiFonts& fonts,
                       float scale, const char* title, const char* subtitle, bool backEnabled, float& backHover);

bool ScreenShellApplyButton(const char* id, const ScreenShellLayout& layout, const AppConfig& cfg,
                            const UiFonts& fonts, float scale, bool hasPending, float& hover);

ImVec2 ScreenContentMin(const ScreenShellLayout& layout);
ImVec2 ScreenContentMax(const ScreenShellLayout& layout);

struct ScreenContentClipGuard {
    ImDrawList* dl = nullptr;

    void Begin(ImDrawList* drawList, const ScreenShellLayout& layout);
    void End();
};

bool BeginScreenContentScroll(const char* id, const ImVec2& pos, const ImVec2& size);
void EndScreenContentScroll();
bool BeginScreenContentScroll(const char* id, const ScreenShellLayout& layout, float footerReserve = 0.f);
