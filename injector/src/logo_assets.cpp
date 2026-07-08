#include "logo_assets.h"

#include "logo_wic.h"

#include <windows.h>

#include <algorithm>
#include <vector>

namespace {

InjectorLogoTexture g_mark{};
InjectorLogoTexture g_background{};

bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.ends_with(L"\\") || a.ends_with(L"/")) return a + b;
    return a + L"\\" + b;
}

std::wstring GetExeDirectory() {
    wchar_t exePath[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return {};
    std::wstring path(exePath);
    const auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    return path;
}

std::wstring ResolveFile(const std::wstring& projectRoot, const wchar_t** candidates, size_t numCandidates) {
    const std::wstring exeDir = GetExeDirectory();
    const std::wstring dirs[] = {
        exeDir,
        JoinPath(exeDir, L"assets\\logos\\png"),
        JoinPath(projectRoot, L"assets\\logos\\png"),
        JoinPath(projectRoot, L"..\\MyUI\\logos\\png"),
    };

    wchar_t localAppData[MAX_PATH * 2]{};
    std::vector<std::wstring> searchDirs;
    for (const auto& dir : dirs) {
        if (!dir.empty()) searchDirs.push_back(dir);
    }
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) > 0) {
        searchDirs.push_back(std::wstring(localAppData) + L"\\MyiUI\\runtime\\assets\\logos\\png");
    }

    for (const auto& dir : searchDirs) {
        for (size_t i = 0; i < numCandidates; ++i) {
            const std::wstring path = JoinPath(dir, candidates[i]);
            if (FileExists(path)) return path;
        }
    }
    return {};
}

std::wstring ResolveLogoFile(const std::wstring& projectRoot) {
    static const wchar_t* kCandidates[] = {L"logo-glass-m-64.png", L"logo-glass-m-128.png", L"logo-glass-m-256.png"};
    return ResolveFile(projectRoot, kCandidates, 3);
}

std::wstring ResolveBackgroundFile(const std::wstring& projectRoot) {
    static const wchar_t* kCandidates[] = {L"background.png", L"background.jpg", L"bg.png", L"bg.jpg"};
    return ResolveFile(projectRoot, kCandidates, 4);
}

bool UploadLogoTexture(ID3D11Device* device, const std::wstring& path, InjectorLogoTexture& out) {
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    if (!LoadPngRgba(path, rgba, w, h) || w <= 0 || h <= 0) return false;

    if (out.srv) {
        out.srv->Release();
        out.srv = nullptr;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(w);
    desc.Height = static_cast<UINT>(h);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub{};
    sub.pSysMem = rgba.data();
    sub.SysMemPitch = static_cast<UINT>(w) * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &sub, &tex)) || !tex) return false;

    ID3D11ShaderResourceView* srv = nullptr;
    const HRESULT hr = device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    if (FAILED(hr) || !srv) return false;

    out.srv = srv;
    out.w = w;
    out.h = h;
    return true;
}

}  // namespace

void InitInjectorLogos(ID3D11Device* device, const std::wstring& projectRoot) {
    if (!device) return;
    const std::wstring logoPath = ResolveLogoFile(projectRoot);
    if (!logoPath.empty()) {
        UploadLogoTexture(device, logoPath, g_mark);
    }
    const std::wstring bgPath = ResolveBackgroundFile(projectRoot);
    if (!bgPath.empty()) {
        UploadLogoTexture(device, bgPath, g_background);
    }
}

void ShutdownInjectorLogos() {
    if (g_mark.srv) {
        g_mark.srv->Release();
        g_mark = {};
    }
    if (g_background.srv) {
        g_background.srv->Release();
        g_background = {};
    }
}

const InjectorLogoTexture& GetInjectorMarkLogo() {
    return g_mark;
}

const InjectorLogoTexture& GetInjectorBackground() {
    return g_background;
}

void DrawInjectorLogoFit(ImDrawList* dl, const InjectorLogoTexture& logo, const ImVec2& min, const ImVec2& max,
                         float alpha) {
    if (!dl || !logo.valid()) return;

    const float boxW = max.x - min.x;
    const float boxH = max.y - min.y;
    if (boxW <= 0.f || boxH <= 0.f) return;

    const float scale = (std::min)(boxW / static_cast<float>(logo.w), boxH / static_cast<float>(logo.h));
    const float drawW = static_cast<float>(logo.w) * scale;
    const float drawH = static_cast<float>(logo.h) * scale;
    const ImVec2 p0(min.x + (boxW - drawW) * 0.5f, min.y + (boxH - drawH) * 0.5f);
    const ImVec2 p1(p0.x + drawW, p0.y + drawH);
    const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>((std::min)(1.f, (std::max)(0.f, alpha)) * 255.f));
    dl->AddImage((ImTextureID)logo.srv, p0, p1, ImVec2(0, 0), ImVec2(1, 1), tint);
}

void DrawInjectorBackground(ImDrawList* dl, const InjectorLogoTexture& bg, const ImVec2& min, const ImVec2& max,
                            float alpha) {
    if (!dl || !bg.valid()) return;

    const float boxW = max.x - min.x;
    const float boxH = max.y - min.y;
    if (boxW <= 0.f || boxH <= 0.f) return;

    // Fill cover
    const float scaleX = boxW / static_cast<float>(bg.w);
    const float scaleY = boxH / static_cast<float>(bg.h);
    const float scale = (std::max)(scaleX, scaleY);
    
    const float drawW = static_cast<float>(bg.w) * scale;
    const float drawH = static_cast<float>(bg.h) * scale;
    const ImVec2 p0(min.x + (boxW - drawW) * 0.5f, min.y + (boxH - drawH) * 0.5f);
    const ImVec2 p1(p0.x + drawW, p0.y + drawH);
    
    const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>((std::min)(1.f, (std::max)(0.f, alpha)) * 255.f));
    dl->AddImage((ImTextureID)bg.srv, p0, p1, ImVec2(0, 0), ImVec2(1, 1), tint);
}
