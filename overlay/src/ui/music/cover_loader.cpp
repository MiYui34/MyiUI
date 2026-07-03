#include "ui/music/cover_loader.h"

#include "logo_wic.h"

#include <windows.h>
#include <winhttp.h>

#include <gl/GL.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace myiui::ui::music {

namespace {

struct CoverEntry {
    GLuint tex = 0;
    int w = 0;
    int h = 0;
    long long lastUsed = 0;  // 帧计数，用于 LRU 淘汰
    bool valid() const { return tex != 0 && w > 0 && h > 0; }
};

struct PendingCover {
    std::string url;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
};

std::mutex g_cacheMutex;
std::unordered_map<std::string, CoverEntry> g_cache;
constexpr size_t kMaxCacheEntries = 256;
static long long g_frameCounter = 0;

// 下载队列 + 线程池
std::mutex g_queueMutex;
std::deque<std::string> g_downloadQueue;
std::unordered_set<std::string> g_queuedUrls;  // 去重
std::condition_variable g_queueCv;
std::atomic<bool> g_shutdown{false};
std::vector<std::thread> g_workers;
std::atomic<int> g_activeWorkers{0};
constexpr int kMaxConcurrentDownloads = 4;

// 已下载完成、待上传纹理的队列
std::mutex g_pendingMutex;
std::deque<PendingCover> g_pendingUploads;

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
    // 缩短超时，封面是小文件不需要长等待
    DWORD timeout = 8000;
    WinHttpSetOption(session, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(session, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

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
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    outTex = newTex;
}

void WorkerLoop() {
    while (true) {
        std::string url;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait(lock, [] { return g_shutdown.load() || !g_downloadQueue.empty(); });
            if (g_shutdown.load() && g_downloadQueue.empty()) return;
            if (g_downloadQueue.empty()) continue;
            url = g_downloadQueue.front();
            g_downloadQueue.pop_front();
            g_activeWorkers.fetch_add(1);
        }

        std::vector<uint8_t> bytes;
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        bool ok = false;
        if (DownloadUrlBytes(url, bytes)) {
            ok = LoadImageRgbaFromBytes(bytes.data(), bytes.size(), rgba, w, h);
        }

        if (ok) {
            std::lock_guard<std::mutex> lock(g_pendingMutex);
            g_pendingUploads.push_back({url, std::move(rgba), w, h});
        }

        {
            std::lock_guard<std::mutex> lock(g_queueMutex);
            g_queuedUrls.erase(url);
            g_activeWorkers.fetch_sub(1);
        }
    }
}

void EnsureWorkers() {
    if (!g_workers.empty()) return;
    for (int i = 0; i < kMaxConcurrentDownloads; ++i) {
        g_workers.emplace_back(WorkerLoop);
    }
}

}  // namespace

void CoverRequest(const std::string& url) {
    if (url.empty()) return;
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cache.count(url) && g_cache[url].valid()) return;
    }
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_queuedUrls.count(url)) return;  // Already queued
        g_queuedUrls.insert(url);
        g_downloadQueue.push_back(url);
    }
    EnsureWorkers();
    g_queueCv.notify_one();
}

void CoverProcessPending() {
    g_frameCounter++;
    std::deque<PendingCover> uploads;
    {
        std::lock_guard<std::mutex> lock(g_pendingMutex);
        uploads.swap(g_pendingUploads);
    }
    if (uploads.empty()) return;

    std::lock_guard<std::mutex> lock(g_cacheMutex);
    for (auto& pc : uploads) {
        // LRU 淘汰：优先淘汰 lastUsed 最小的（最久未使用的）
        while (g_cache.size() >= kMaxCacheEntries) {
            auto oldest = g_cache.begin();
            for (auto it = g_cache.begin(); it != g_cache.end(); ++it) {
                if (it->second.lastUsed < oldest->second.lastUsed) {
                    oldest = it;
                }
            }
            if (oldest->second.tex != 0) glDeleteTextures(1, &oldest->second.tex);
            g_cache.erase(oldest);
        }
        auto& entry = g_cache[pc.url];
        if (entry.tex != 0) glDeleteTextures(1, &entry.tex);
        UploadTexture(pc.rgba, pc.w, pc.h, entry.tex);
        entry.w = pc.w;
        entry.h = pc.h;
        entry.lastUsed = g_frameCounter;
    }
}

CoverTexture CoverGet(const std::string& url) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto it = g_cache.find(url);
    if (it == g_cache.end()) return CoverTexture{};
    it->second.lastUsed = g_frameCounter;  // 标记为最近使用
    return CoverTexture{it->second.tex, it->second.w, it->second.h};
}

CoverTexture CoverFromBase64Png(const std::string& b64) {
    if (b64.empty()) return CoverTexture{};

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
    g_shutdown.store(true);
    g_queueCv.notify_all();
    for (auto& t : g_workers) {
        if (t.joinable()) t.join();
    }
    g_workers.clear();

    std::lock_guard<std::mutex> lock(g_cacheMutex);
    for (auto& kv : g_cache) {
        if (kv.second.tex != 0) glDeleteTextures(1, &kv.second.tex);
    }
    g_cache.clear();
}

}  // namespace myiui::ui::music
