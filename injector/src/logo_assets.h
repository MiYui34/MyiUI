#pragma once

#include <d3d11.h>
#include <imgui.h>

#include <string>

struct InjectorLogoTexture {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 0;
    int h = 0;

    bool valid() const { return srv != nullptr && w > 0 && h > 0; }
};

void InitInjectorLogos(ID3D11Device* device, const std::wstring& projectRoot);
void ShutdownInjectorLogos();
const InjectorLogoTexture& GetInjectorMarkLogo();

void DrawInjectorLogoFit(ImDrawList* dl, const InjectorLogoTexture& logo, const ImVec2& min, const ImVec2& max,
                         float alpha = 1.f);
