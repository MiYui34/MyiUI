#pragma once

#include "imgui.h"

#include <string>

struct LogoTexture {
    unsigned int tex = 0;
    int w = 0;
    int h = 0;

    bool valid() const { return tex != 0 && w > 0 && h > 0; }
};

struct LogoSet {
    LogoTexture mark;
    LogoTexture emblem;
};

void InitLogos(const std::wstring& projectRoot);
void EnsureLogos(const std::wstring& projectRoot);
void ShutdownLogos();
const LogoSet& GetLogos();

void DrawLogoFit(ImDrawList* dl, const LogoTexture& logo, const ImVec2& min, const ImVec2& max, float alpha = 1.f);
