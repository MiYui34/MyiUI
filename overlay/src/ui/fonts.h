#pragma once

#include "config/config_loader.h"

#include "imgui.h"

struct UiFonts {
    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    ImFont* brand = nullptr;
    ImFont* nav = nullptr;
    ImFont* profileName = nullptr;
    ImFont* profileSub = nullptr;
    ImFont* caption = nullptr;
};

bool InitUiFonts(const ThemeConfig& theme, float uiScale);
const UiFonts& GetUiFonts();
void InvalidateUiFonts();
