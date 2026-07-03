#pragma once

#include "config/config_loader.h"
#include "ui/ui_manager.h"

#include "imgui.h"

#include <windows.h>

struct MainMenuState {
    bool show_manager = false;
    float hover_anim[8]{};
    UiManagerState manager;
};

void MainMenuInit(const AppConfig& config);
void MainMenuSetWindowHandle(HWND hwnd);
void MainMenuRender(AppConfig& cfg, MainMenuState& state, ImTextureID bgTexture, bool hasBg,
                    int bgTexW = 0, int bgTexH = 0);
