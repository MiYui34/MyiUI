#pragma once

#include "imgui.h"

enum class MenuIcon {
    Singleplayer,
    Multiplayer,
    Options,
    Quit,
    Manager,
    None,
};

void DrawMenuIcon(ImDrawList* dl, MenuIcon icon, const ImVec2& center, float size, ImU32 color, float strokeWidth);
