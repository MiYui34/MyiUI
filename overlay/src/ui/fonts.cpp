#include "ui/fonts.h"

#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include "overlay_runtime.h"

#include <windows.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

UiFonts g_fonts{};
bool g_ready = false;
float g_lastScale = 0.f;

bool FileExists(const std::wstring& path) {
    return !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::wstring GetLocalAppDataMyiui() {
    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) == 0) {
        return L"";
    }
    return std::wstring(localAppData) + L"\\MyiUI";
}

std::vector<std::wstring> FontSearchDirs() {
    std::vector<std::wstring> dirs;
    const std::wstring root = myiui::overlay::ResolveProjectRoot();
    if (!root.empty()) {
        dirs.push_back(root + L"\\assets\\fonts");
    }
    const std::wstring local = GetLocalAppDataMyiui();
    if (!local.empty()) {
        dirs.push_back(local + L"\\runtime\\assets\\fonts");
    }
    wchar_t winDir[MAX_PATH]{};
    if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
        dirs.push_back(std::wstring(winDir) + L"\\Fonts");
    }
    return dirs;
}

std::vector<std::wstring> RegularFontCandidates() {
    return {
        L"AlibabaPuHuiTi-2-55-Regular.ttf",
        L"AlibabaPuHuiTi-3-55-Regular.ttf",
        L"AlibabaPuHuiTi-2-45-Light.ttf",
        L"Alibaba-PuHuiTi-Regular.ttf",
        L"AlibabaPuHuiTi-Regular.ttf",
    };
}

std::vector<std::wstring> BoldFontCandidates() {
    return {
        L"AlibabaPuHuiTi-3-75-SemiBold.ttf",
        L"AlibabaPuHuiTi-2-75-SemiBold.ttf",
        L"AlibabaPuHuiTi-3-65-Medium.ttf",
        L"AlibabaPuHuiTi-2-65-Medium.ttf",
        L"AlibabaPuHuiTi-2-85-Bold.ttf",
        L"Alibaba-PuHuiTi-Medium.ttf",
        L"AlibabaPuHuiTi-Medium.ttf",
    };
}

std::vector<std::wstring> CjkFontCandidates() {
    return {
        L"msyh.ttc",
        L"msyhbd.ttc",
        L"msyhl.ttc",
        L"simhei.ttf",
        L"simsun.ttc",
        L"msjh.ttc",
        L"msjhbd.ttc",
    };
}

bool IsAlibabaPuHuiTiFace(const std::wstring& face) {
    if (face.empty()) return false;
    const std::wstring lower = [&] {
        std::wstring s = face;
        for (wchar_t& ch : s) {
            if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
        return s;
    }();
    return lower.find(L"alibaba") != std::wstring::npos || lower.find(L"puhuiti") != std::wstring::npos ||
           face.find(L"普惠") != std::wstring::npos;
}

std::wstring FindFirstExisting(const std::vector<std::wstring>& dirs, const std::vector<std::wstring>& files) {
    for (const auto& dir : dirs) {
        for (const auto& file : files) {
            const std::wstring path = dir + L"\\" + file;
            if (FileExists(path)) {
                return path;
            }
        }
    }
    return L"";
}

std::wstring ResolveFontPath(const std::wstring& faceName, bool bold) {
    const auto dirs = FontSearchDirs();
    if (IsAlibabaPuHuiTiFace(faceName) || faceName.empty()) {
        const std::wstring path = FindFirstExisting(dirs, bold ? BoldFontCandidates() : RegularFontCandidates());
        if (!path.empty()) return path;
        if (!bold) {
            const std::wstring alt = FindFirstExisting(dirs, BoldFontCandidates());
            if (!alt.empty()) return alt;
        }
        const std::wstring cjk = FindFirstExisting(dirs, CjkFontCandidates());
        if (!cjk.empty()) return cjk;
    }

    wchar_t winDir[MAX_PATH]{};
    if (GetWindowsDirectoryW(winDir, MAX_PATH) == 0) {
        return L"";
    }
    const std::wstring fontsDir = std::wstring(winDir) + L"\\Fonts\\";
    const struct {
        const wchar_t* face;
        const wchar_t* file;
    } kWindowsFallback[] = {
        {L"Segoe UI Semibold", L"seguisb.ttf"},
        {L"Segoe UI", L"segoeui.ttf"},
    };
    for (const auto& entry : kWindowsFallback) {
        if (_wcsicmp(faceName.c_str(), entry.face) != 0) continue;
        const std::wstring path = fontsDir + entry.file;
        if (FileExists(path)) return path;
    }
    const std::wstring cjk = FindFirstExisting(dirs, CjkFontCandidates());
    if (!cjk.empty()) return cjk;
    return FindFirstExisting(dirs, RegularFontCandidates());
}

ImFont* LoadFont(const std::wstring& path, float sizePx, int fontNo = 0) {
    if (!FileExists(path)) return nullptr;
    char utf8[MAX_PATH * 3]{};
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8, sizeof(utf8), nullptr, nullptr);

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;
    cfg.FontNo = fontNo;

    ImFontGlyphRangesBuilder rangeBuilder;
    rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesChineseFull());
    rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesJapanese());
    rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesKorean());
    rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
    rangeBuilder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesDefault());
    static ImVector<ImWchar> s_ranges;
    s_ranges.clear();
    rangeBuilder.BuildRanges(&s_ranges);
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(utf8, sizePx, &cfg, s_ranges.Data);
}

}  // namespace

bool InitUiFonts(const ThemeConfig& theme, float uiScale) {
    ImGuiContext* imguiCtx = ImGui::GetCurrentContext();
    if (imguiCtx && imguiCtx->WithinFrameScope) {
        // 帧内禁止重建字体图集，否则会释放 NewFrame 仍引用的 ImFont* 并导致崩溃
        return g_fonts.regular != nullptr;
    }

    if (g_ready && std::abs(g_lastScale - uiScale) < 0.01f) {
        return g_fonts.regular != nullptr;
    }

    InvalidateUiFonts();

    const std::wstring regularPath = ResolveFontPath(theme.font_regular, false);
    const std::wstring boldPath = ResolveFontPath(theme.font_bold, true);
    if (regularPath.empty()) {
        myiui::overlay::OverlayLog(L"Font missing: no CJK font found");
    } else if (regularPath.find(L"msyh") != std::wstring::npos ||
               regularPath.find(L"simhei") != std::wstring::npos ||
               regularPath.find(L"simsun") != std::wstring::npos) {
        myiui::overlay::OverlayLog(L"Font loaded (system CJK fallback).");
    } else {
        myiui::overlay::OverlayLog(L"Font loaded.");
    }

    g_fonts.regular = LoadFont(regularPath, theme.nav_size * uiScale);
    if (!g_fonts.regular) {
        g_fonts.regular = ImGui::GetIO().Fonts->AddFontDefault();
    }
    g_fonts.semibold = LoadFont(boldPath, theme.profile_name_size * uiScale);
    if (!g_fonts.semibold) {
        g_fonts.semibold = LoadFont(regularPath, theme.profile_name_size * uiScale);
    }
    g_fonts.brand = LoadFont(boldPath, theme.brand_size * uiScale);
    if (!g_fonts.brand) {
        g_fonts.brand = LoadFont(regularPath, theme.brand_size * uiScale);
    }
    g_fonts.nav = LoadFont(regularPath, theme.nav_size * uiScale);
    if (!g_fonts.nav) {
        g_fonts.nav = g_fonts.regular;
    }
    g_fonts.profileName = g_fonts.semibold ? g_fonts.semibold : g_fonts.regular;
    g_fonts.profileSub = LoadFont(regularPath, theme.profile_sub_size * uiScale);
    if (!g_fonts.profileSub) {
        g_fonts.profileSub = g_fonts.regular;
    }
    g_fonts.caption = LoadFont(regularPath, theme.caption_size * uiScale);
    if (!g_fonts.caption) {
        g_fonts.caption = g_fonts.regular;
    }

    ImGui::GetIO().Fonts->Build();
    if (g_fonts.regular) {
        ImGui::GetIO().FontDefault = g_fonts.regular;
    }
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();

    g_lastScale = uiScale;
    g_ready = g_fonts.regular != nullptr;
    return g_ready;
}

const UiFonts& GetUiFonts() {
    return g_fonts;
}

void InvalidateUiFonts() {
    ImGui::GetIO().Fonts->Clear();
    g_fonts = {};
    g_ready = false;
    g_lastScale = 0.f;
}
