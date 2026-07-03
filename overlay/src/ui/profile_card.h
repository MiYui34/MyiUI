#pragma once

#include "config/config_loader.h"
#include "ui/fonts.h"
#include "ui/menu_data.h"

#include "imgui.h"

struct MenuAppState;

void ProfileCardEnsureLoaded(MenuAppState& state, float deltaMs);
void ProfileCardUpdateAvatar(MenuAppState& state);
void DrawProfileCard(ImDrawList* dl, const ImVec2& pos, const ImVec2& size, const AppConfig& cfg,
                     const UiFonts& fonts, float scale, float hoverT, const ProfileData& profile);
