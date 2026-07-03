#include "ui/music/cover_loader.h"

#include "logo_wic.h"

#include <windows.h>
#include <winhttp.h>

#include <gl/GL.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace myiui::ui::music {

namespace {

struct CoverEntry {
    GLuint tex = 0;
    int w = 0;
    int h = 0;
    bool valid() const { return tex != 0 && w > 0 && h > 0; }
};

struct PendingCover {
    std::mutex mutex;
    std::string url;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
    bool ready = false;
};

std::mutex g_cacheMutex;
std::unordered_map<std::string, CoverEntry> g_cache;
PendingCover g_pending;
std::atomic<bool> g_downloading{false};
std::string g_requestedUrl;
constexpr size_t kMaxCacheEntries = 24;

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}

bool DownloadUrlBytes(const std::string& url, std::vector<uint8_t>& out) {
    if (url.empty()) return false;
    out.clear();

    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256]{};
    wchar_t path[2048]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;

    const std::wstring wideUrl = Utf8ToWide(url);
    if (wideUrl.empty()) return false;
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) return false;

    INTERNET_PORT port = parts.nPort;
    if (port == 0) {
        port = parts.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }

    HINTERNET session = WinHttpOpen(L"MyiUI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(session, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    HINTERNET connect = WinHttpConnect(session, host, port, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }

    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false; }

    bool ok = false;
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
            const size_t offset = out.size();
            out.resize(offset + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, out.data() + offset, available, &read) || read == 0) break;
            out.resize(offset + read);
        }
        ok = !out.empty();
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

void UploadTexture(const std::vector<uint8_t>& rgba, int w, int h, GLuint& outTex) {
    if (rgba.empty() || w <= 0 || h <= 0) return;
    GLuint newTex = 0;
    glGenTextures(1, &newTex);
    glBindTexture(GL_TEXTURE_2D, newTex);
    // Minecraft / ImGui 可能修改 unpack 状态，上传前必须重置否则出现斜纹
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    outTex = newTex;
}

}  // namespace

void CoverRequest(const std::string& url) {
    if (url.empty()) return;
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cache.count(url) && g_cache[url].valid()) return;
    }
    if (url == g_requestedUrl && g_downloading.load()) return;
    g_requestedUrl = url;
    g_downloading.store(true);

    std::thread([url]() {
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        bool ok = false;
        if (DownloadUrlBytes(url, bytes)) {
            ok = LoadImageRgbaFromBytes(bytes.data(), bytes.size(), rgba, w, h);
        }
        std::lock_guard<std::mutex> lock(g_pending.mutex);
        g_pending.ready = ok;
        if (ok) {
            g_pending.url = url;
            g_pending.rgba = std::move(rgba);
            g_pending.w = w;
            g_pending.h = h;
        } else if (g_requestedUrl == url) {
            g_requestedUrl.clear();
        }
        g_downloading.store(false);
    }).detach();
}

void CoverProcessPending() {
    PendingCover copy;
    {
        std::lock_guard<std::mutex> lock(g_pending.mutex);
        if (!g_pending.ready) return;
        copy.ready = true;
        copy.url = g_pending.url;
        copy.rgba = std::move(g_pending.rgba);
        copy.w = g_pending.w;
        copy.h = g_pending.h;
        g_pending.ready = false;
        g_pending.url.clear();
    }
    if (!copy.ready) return;
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    while (g_cache.size() >= kMaxCacheEntries) {
        auto it = g_cache.begin();
        if (it->second.tex != 0) glDeleteTextures(1, &it->second.tex);
        g_cache.erase(it);
    }
    auto& entry = g_cache[copy.url];
    if (entry.tex != 0) glDeleteTextures(1, &entry.tex);
    UploadTexture(copy.rgba, copy.w, copy.h, entry.tex);
    entry.w = copy.w;
    entry.h = copy.h;
}

CoverTexture CoverGet(const std::string& url) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_cache.find(url);
    if (it == g_cache.end()) return CoverTexture{};
    return CoverTexture{it->second.tex, it->second.w, it->second.h};
}

CoverTexture CoverFromBase64Png(const std::string& b64) {
    if (b64.empty()) return CoverTexture{};
    
    // 剥除 data:image/png;base64, 前缀，防止解码乱码破坏 PNG 头文件
    std::string data = b64;
    size_t pos = data.find("base64,");
    if (pos != std::string::npos) {
        data = data.substr(pos + 7);
    }

    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    
    std::vector<uint8_t> bytes;
    bytes.reserve(data.size() * 3 / 4);
    int accum = 0, bits = 0;
    
    for (char c : data) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int v = val(c);
        if (v < 0) continue;
        accum = (accum << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<uint8_t>((accum >> bits) & 0xFF));
        }
    }
    
    if (bytes.empty()) return CoverTexture{};
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    if (!LoadImageRgbaFromBytes(bytes.data(), bytes.size(), rgba, w, h)) return CoverTexture{};
    CoverTexture out;
    UploadTexture(rgba, w, h, out.tex);
    out.w = w;
    out.h = h;
    return out;
}

void CoverShutdown() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    for (auto& kv : g_cache) {
        if (kv.second.tex != 0) glDeleteTextures(1, &kv.second.tex);
    }
    g_cache.clear();
}

}  // namespace myiui::ui::music