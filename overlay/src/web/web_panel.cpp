#include "web/web_panel.h"

#include "web/web_engine.h"
#include "web/web_view.h"

#include "bridge/native_state.h"
#include "bridge/ui_state_types.h"
#include "config/user_settings.h"
#include "overlay_runtime.h"
#include "ui/clickgui/clickgui.h"

#include "imgui.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace myiui::web {
namespace {

constexpr uint32_t kDefaultPanelWidth = 960;
constexpr uint32_t kDefaultPanelHeight = 640;
constexpr uint32_t kMinPanelSize = 64;

bool g_initialized = false;
bool g_home_loaded = false;
WebView g_view;
char g_urlBuffer[1024] = "";
bool g_urlBarFocused = false;
std::string g_lastSyncedUrl;
RECT g_lastBounds{0, 0, 0, 0};
bool g_lastVisible = false;

const char* kHomeHtml = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<style>
  html, body {
    margin: 0; height: 100%;
    background: #0c0c0e;
    color: #e8e8ea;
    font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
    display: flex; align-items: center; justify-content: center;
  }
  .box { text-align: center; max-width: 360px; padding: 24px; }
  h1 { margin: 0 0 10px; font-size: 20px; font-weight: 600; letter-spacing: 0.02em; }
  p { margin: 0; font-size: 13px; line-height: 1.6; color: #8b8b93; }
</style>
</head>
<body>
  <div class="box">
    <h1>MyiUI Browser</h1>
    <p>WebView2 已就绪。在上方输入网址或搜索。</p>
  </div>
</body>
</html>)HTML";

std::string TrimCopy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string UrlEncodeUtf8(const std::string& raw) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size() * 3);
    for (unsigned char c : raw) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

bool LooksLikeUrl(const std::string& s) {
    if (s.find(' ') != std::string::npos) {
        return false;
    }
    const auto dot = s.find('.');
    return dot != std::string::npos && dot + 1 < s.size();
}

std::string ResolveNavigationTarget(std::string input) {
    input = TrimCopy(std::move(input));
    if (input.empty()) {
        return {};
    }
    const bool hasScheme = input.rfind("http://", 0) == 0 || input.rfind("https://", 0) == 0 ||
                           input.rfind("file://", 0) == 0;
    if (hasScheme) {
        return input;
    }
    if (!LooksLikeUrl(input)) {
        return "https://www.baidu.com/s?wd=" + UrlEncodeUtf8(input);
    }
    return "https://" + input;
}

void SyncUrlBarFromView() {
    if (g_urlBarFocused) {
        return;
    }
    const std::string url = g_view.CurrentUrl();
    if (url.empty() || url == g_lastSyncedUrl) {
        return;
    }
    if (url.rfind("about:", 0) == 0 || url.rfind("data:", 0) == 0) {
        return;
    }
    g_lastSyncedUrl = url;
    std::snprintf(g_urlBuffer, sizeof(g_urlBuffer), "%s", url.c_str());
}

bool EnsureInitialized() {
    if (!WebEngineGetParentHwnd()) {
        return false;
    }

    if (!WebEngineIsReady()) {
        if (WebEngineHasFailed()) {
            return false;
        }
        WebEngineInit();
        return false;
    }

    if (!g_view.IsValid()) {
        g_view.Create(kDefaultPanelWidth, kDefaultPanelHeight);
    } else if (!g_view.IsReady()) {
        g_view.Create(kDefaultPanelWidth, kDefaultPanelHeight);
    }

    if (!g_view.IsReady()) {
        return false;
    }

    if (!g_home_loaded) {
        g_view.LoadHTML(kHomeHtml);
        g_home_loaded = true;
    }
    g_initialized = true;
    return true;
}

void ScreenToParentClient(HWND parent, const ImVec2& imguiPos, POINT* out) {
    // Overlay ImGui uses framebuffer/client space (DisplaySize = GL viewport), not Win32
    // screen coordinates. Do NOT ScreenToClient() or the WebView shifts upward over the chrome.
    RECT client{};
    GetClientRect(parent, &client);
    const ImGuiIO& io = ImGui::GetIO();
    const float dispW = (std::max)(io.DisplaySize.x, 1.f);
    const float dispH = (std::max)(io.DisplaySize.y, 1.f);
    const float scaleX = static_cast<float>(client.right - client.left) / dispW;
    const float scaleY = static_cast<float>(client.bottom - client.top) / dispH;
    out->x = static_cast<LONG>(imguiPos.x * scaleX + 0.5f);
    out->y = static_cast<LONG>(imguiPos.y * scaleY + 0.5f);
}

}  // namespace

bool WebPanelWanted() {
    if (!myiui::ui::clickgui::WebPanelVisible()) {
        return false;
    }
    return myiui::bridge::NativeState::Instance().GetScreenKind() == myiui::shared::ScreenKind::InGame;
}

bool WebPanelActive() {
    return WebPanelWanted();
}

void CloseWebPanel() {
    auto& settings = myiui::config::GetUserSettings();
    if (settings.web_panel_enabled) {
        settings.web_panel_enabled = false;
        myiui::config::UserSettingsRequestSave();
    }
    if (g_view.IsReady()) {
        g_view.SetVisible(false);
        g_view.Unfocus();
    }
    g_lastVisible = false;
}

void WebPanelTickAndRender() {
    if (!WebPanelWanted()) {
        if (g_initialized || g_view.IsValid()) {
            if (g_view.IsReady()) {
                g_view.SetVisible(false);
                g_view.Unfocus();
            }
            WebPanelShutdown();
        }
        return;
    }

    if (WebEngineHasFailed()) {
        bool open = true;
        ImGui::SetNextWindowSize(ImVec2(520.f, 220.f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("MyiUI Browser", &open)) {
            ImGui::TextWrapped("%ls", WebEngineErrorMessage().c_str());
            ImGui::Spacing();
            if (ImGui::Button("打开 WebView2 下载页")) {
                ShellExecuteW(nullptr, L"open",
                              L"https://developer.microsoft.com/microsoft-edge/webview2/", nullptr, nullptr,
                              SW_SHOWNORMAL);
            }
        }
        ImGui::End();
        if (!open) {
            CloseWebPanel();
        }
        return;
    }

    if (!EnsureInitialized()) {
        bool open = true;
        ImGui::SetNextWindowSize(
            ImVec2(static_cast<float>(kDefaultPanelWidth), static_cast<float>(kDefaultPanelHeight)),
            ImGuiCond_FirstUseEver);
        if (ImGui::Begin("MyiUI Browser", &open)) {
            ImGui::TextUnformatted("正在启动 WebView2…");
        }
        ImGui::End();
        if (!open) {
            CloseWebPanel();
        }
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kDefaultPanelWidth), static_cast<float>(kDefaultPanelHeight)),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.f, 240.f), ImVec2(4096.f, 4096.f));

    bool open = true;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin("MyiUI Browser", &open, flags)) {
        g_view.SetVisible(false);
        g_lastVisible = false;
        ImGui::End();
        if (!open) {
            CloseWebPanel();
        }
        return;
    }

    if (!open) {
        ImGui::End();
        CloseWebPanel();
        WebPanelShutdown();
        return;
    }

    SyncUrlBarFromView();

    bool triggerLoad = false;
    const bool canBack = g_view.CanGoBack();
    ImGui::BeginDisabled(!canBack);
    if (ImGui::Button("<")) {
        g_view.GoBack();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("刷新")) {
        g_view.Reload();
    }
    ImGui::SameLine();

    const float closeBtnW = ImGui::CalcTextSize("关闭").x + ImGui::GetStyle().FramePadding.x * 2.f;
    const float goBtnW = 110.f;
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - goBtnW - closeBtnW -
                         ImGui::GetStyle().ItemSpacing.x * 2.f);
    if (ImGui::InputTextWithHint("##URL", "输入网址 (例如: https://www.douyin.com) 或搜索内容...", g_urlBuffer,
                                 sizeof(g_urlBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        triggerLoad = true;
    }
    const bool urlActive = ImGui::IsItemActive();
    if (urlActive && !g_urlBarFocused) {
        g_view.Unfocus();
    }
    g_urlBarFocused = urlActive;
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("跳转 / 搜索", ImVec2(goBtnW, 0))) {
        triggerLoad = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("关闭")) {
        ImGui::End();
        CloseWebPanel();
        WebPanelShutdown();
        return;
    }

    if (g_view.IsLoading()) {
        ImGui::TextUnformatted("加载中…");
    }

    if (triggerLoad) {
        const std::string target = ResolveNavigationTarget(g_urlBuffer);
        if (!target.empty()) {
            g_view.LoadURL(target);
            g_lastSyncedUrl = target;
            std::snprintf(g_urlBuffer, sizeof(g_urlBuffer), "%s", target.c_str());
            g_view.Focus();
        }
    }

    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float contentW = (std::max)(avail.x, static_cast<float>(kMinPanelSize));
    const float contentH = (std::max)(avail.y, static_cast<float>(kMinPanelSize));

    ImGui::InvisibleButton("##webview_host", ImVec2(contentW, contentH));
    const bool contentHovered = ImGui::IsItemHovered();
    if (contentHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !g_urlBarFocused) {
        g_view.Focus();
    }

    // WebView2 HWND eats mouse hits. Only show it while the browser window is hovered
    // (or the page holds focus), so clicks outside pass through to the game.
    const bool browserHovered =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool wantNativeSurface = browserHovered || contentHovered;

    HWND parent = WebEngineGetParentHwnd();
    if (parent) {
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        POINT tl{};
        POINT br{};
        ScreenToParentClient(parent, rmin, &tl);
        ScreenToParentClient(parent, rmax, &br);
        const int bw = (std::max)(1, static_cast<int>(br.x - tl.x));
        const int bh = (std::max)(1, static_cast<int>(br.y - tl.y));
        const RECT bounds{tl.x, tl.y, tl.x + bw, tl.y + bh};
        if (memcmp(&bounds, &g_lastBounds, sizeof(RECT)) != 0) {
            g_view.SetBounds(tl.x, tl.y, bw, bh);
            g_lastBounds = bounds;
            g_view.NotifyParentMoved();
        }

        if (wantNativeSurface && bw > 8 && bh > 8) {
            if (!g_lastVisible) {
                g_view.SetVisible(true);
                g_lastVisible = true;
            }
        } else if (g_lastVisible) {
            g_view.SetVisible(false);
            g_view.Unfocus();
            g_lastVisible = false;
        }
    }

    ImGui::End();
}

void WebPanelShutdown() {
    if (g_view.IsValid()) {
        g_view.SetVisible(false);
        g_view.Unfocus();
        g_view.Destroy();
    }
    WebEngineShutdown();
    g_initialized = false;
    g_home_loaded = false;
    g_urlBarFocused = false;
    g_lastSyncedUrl.clear();
    g_urlBuffer[0] = '\0';
    g_lastBounds = {};
    g_lastVisible = false;
}

}  // namespace myiui::web
