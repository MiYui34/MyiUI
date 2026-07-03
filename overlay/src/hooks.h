#pragma once

#include <windows.h>

void HooksInit(HMODULE module);
void OverlayRequestConfigReload();
void OverlayInvalidateBackgroundTexture();
void OverlayEnterGameScreenMode();
