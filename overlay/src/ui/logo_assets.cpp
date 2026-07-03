#include <windows.h>

#include <gl/GL.h>

#include "ui/logo_assets.h"

#include "logo_wic.h"

#include <algorithm>
#include <vector>

namespace {

LogoSet g_logos{};

bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.ends_with(L"\\") || a.ends_with(L"/")) return a + b;
    return a + L"\\" + b;
}

std::wstring GetDllDirectory() {
    wchar_t dllPath[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&GetDllDirectory), &self);
    if (!self || GetModuleFileNameW(self, dllPath, MAX_PATH) == 0) return {};

    std::wstring path(dllPath);
    const auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    return path;
}

std::wstring GetModuleProjectRoot() {
    wchar_t dllPath[MAX_PATH]{};
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&GetModuleProjectRoot), &self);
    if (!self || GetModuleFileNameW(self, dllPath, MAX_PATH) == 0) return {};

    std::wstring path(dllPath);
    const auto pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) path.resize(pos);
    if (path.ends_with(L"Release") || path.ends_with(L"Debug")) {
        path = path.substr(0, path.find_last_of(L"\\/"));
    }
    if (path.ends_with(L"overlay")) {
        path = path.substr(0, path.find_last_of(L"\\/"));
    }
    if (path.ends_with(L"build")) {
        path = path.substr(0, path.find_last_of(L"\\/"));
    }
    return path;
}

std::wstring ResolveLogoFile(const std::wstring& projectRoot, const wchar_t* const* candidates, size_t count) {
    const std::wstring moduleRoot = GetModuleProjectRoot();
    const std::wstring dllDir = GetDllDirectory();
    const std::wstring dirs[] = {
        JoinPath(dllDir, L"assets\\logos\\png"),
        JoinPath(projectRoot, L"assets\\logos\\png"),
        JoinPath(moduleRoot, L"assets\\logos\\png"),
        JoinPath(moduleRoot, L"..\\MyUI\\logos\\png"),
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
        for (size_t i = 0; i < count; ++i) {
            const std::wstring path = JoinPath(dir, candidates[i]);
            if (FileExists(path)) return path;
        }
    }
    return {};
}

bool UploadLogoTexture(const std::wstring& path, LogoTexture& out) {
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    if (!LoadPngRgba(path, rgba, w, h) || w <= 0 || h <= 0) return false;

    if (out.tex != 0) {
        glDeleteTextures(1, &out.tex);
        out.tex = 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    out.tex = tex;
    out.w = w;
    out.h = h;
    return true;
}

}  // namespace

void InitLogos(const std::wstring& projectRoot) {
    static const wchar_t* kMarkCandidates[] = {L"logo-glass-m-128.png", L"logo-glass-m-64.png",
                                               L"logo-glass-m-256.png"};
    static const wchar_t* kEmblemCandidates[] = {L"logo-glass-m-256.png", L"logo-glass-m-512.png",
                                                 L"logo-glass-m-128.png"};

    if (!g_logos.mark.valid()) {
        const std::wstring markPath =
            ResolveLogoFile(projectRoot, kMarkCandidates, sizeof(kMarkCandidates) / sizeof(kMarkCandidates[0]));
        if (!markPath.empty()) {
            UploadLogoTexture(markPath, g_logos.mark);
        }
    }

    if (!g_logos.emblem.valid()) {
        const std::wstring emblemPath =
            ResolveLogoFile(projectRoot, kEmblemCandidates, sizeof(kEmblemCandidates) / sizeof(kEmblemCandidates[0]));
        if (!emblemPath.empty()) {
            UploadLogoTexture(emblemPath, g_logos.emblem);
        }
    }
}

void EnsureLogos(const std::wstring& projectRoot) {
    if (g_logos.mark.valid() && g_logos.emblem.valid()) return;
    InitLogos(projectRoot);
}

void ShutdownLogos() {
    if (g_logos.mark.tex != 0) {
        glDeleteTextures(1, &g_logos.mark.tex);
        g_logos.mark = {};
    }
    if (g_logos.emblem.tex != 0) {
        glDeleteTextures(1, &g_logos.emblem.tex);
        g_logos.emblem = {};
    }
}

const LogoSet& GetLogos() {
    return g_logos;
}

void DrawLogoFit(ImDrawList* dl, const LogoTexture& logo, const ImVec2& min, const ImVec2& max, float alpha) {
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
    dl->AddImage((ImTextureID)(intptr_t)logo.tex, p0, p1, ImVec2(0, 0), ImVec2(1, 1), tint);
}
