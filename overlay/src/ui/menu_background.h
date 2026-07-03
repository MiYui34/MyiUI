#pragma once

#include "config/config_loader.h"
#include "ui/fonts.h"
#include "ui/menu_app.h"

#include "imgui.h"

void DrawMenuBackground(MenuRenderContext& ctx);
void DrawMenuTopBar(MenuRenderContext& ctx, bool inputsEnabled);
void DrawMenuProfile(MenuRenderContext& ctx, float clusterTop, float clusterH);
