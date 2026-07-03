#include "ui/profile_avatar.h"

#include "logo_wic.h"

#include <windows.h>
#include <winhttp.h>

#include <gl/GL.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace {

struct PendingAvatar {
    std::mutex mutex;
    bool ready = false;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
};

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}

ProfileAvatarTexture g_avatar{};
PendingAvatar g_pending;
std::string g_requestedSource;
std::string g_loadedSource;
std::atomic<bool> g_downloading{false};

bool DownloadUrlBytes(const std::string& url, std::vector<uint8_t>& outBytes) {
    if (url.empty()) return false;
    outBytes.clear();

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
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    WinHttpAddRequestHeaders(request, L"User-Agent: MyiUI/1.0\r\n", static_cast<DWORD>(-1L),
                             WINHTTP_ADDREQ_FLAG_ADD);

    bool ok = false;
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr)) {
        DWORD available = 0;
        while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
            const size_t offset = outBytes.size();
            outBytes.resize(offset + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, outBytes.data() + offset, available, &read) || read == 0) break;
            outBytes.resize(offset + read);
        }
        ok = !outBytes.empty();
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

void UploadAvatarTexture(const std::vector<uint8_t>& rgba, int w, int h) {
    if (rgba.empty() || w <= 0 || h <= 0) return;
    GLuint newTex = 0;
    glGenTextures(1, &newTex);
    glBindTexture(GL_TEXTURE_2D, newTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const GLuint oldTex = g_avatar.tex;
    g_avatar.tex = newTex;
    g_avatar.w = w;
    g_avatar.h = h;
    if (oldTex != 0) {
        glDeleteTextures(1, &oldTex);
    }
}

bool LocalFileExists(const std::string& path) {
    if (path.empty()) return false;
    const std::wstring wide = Utf8ToWide(path);
    if (wide.empty()) return false;
    const DWORD attrs = GetFileAttributesW(wide.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

}  // namespace

void ProfileAvatarInit() {}

void ProfileAvatarShutdown() {
    if (g_avatar.tex != 0) {
        glDeleteTextures(1, &g_avatar.tex);
        g_avatar.tex = 0;
    }
    g_avatar.w = g_avatar.h = 0;
    g_requestedSource.clear();
    g_loadedSource.clear();
    g_downloading.store(false);
}

void ProfileAvatarInvalidate() {
    g_requestedSource.clear();
    g_loadedSource.clear();
}

bool ProfileAvatarIsBusy() {
    return g_downloading.load();
}

void ProfileAvatarRequest(const std::string& url) {
    if (url.empty() || url == g_requestedSource) return;
    if (url == g_loadedSource && g_avatar.valid()) return;

    g_requestedSource = url;
    g_downloading.store(true);

    std::thread([url]() {
        PendingAvatar local;
        std::vector<uint8_t> bytes;
        if (DownloadUrlBytes(url, bytes)) {
            int w = 0, h = 0;
            if (LoadImageRgbaFromBytes(bytes.data(), bytes.size(), local.rgba, w, h)) {
                local.w = w;
                local.h = h;
                local.ready = true;
            }
        }
        std::lock_guard<std::mutex> lock(g_pending.mutex);
        g_pending.ready = local.ready;
        if (local.ready) {
            g_pending.rgba = std::move(local.rgba);
            g_pending.w = local.w;
            g_pending.h = local.h;
        } else if (g_requestedSource == url) {
            g_requestedSource.clear();
        }
        g_downloading.store(false);
    }).detach();
}

void ProfileAvatarRequestLocalFile(const std::string& path) {
    if (path.empty() || path == g_requestedSource) return;
    if (path == g_loadedSource && g_avatar.valid()) return;
    if (!LocalFileExists(path)) return;

    g_requestedSource = path;
    g_downloading.store(true);

    std::thread([path]() {
        PendingAvatar local;
        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        const std::wstring widePath = Utf8ToWide(path);
        if (!widePath.empty() && LoadPngRgba(widePath, rgba, w, h)) {
            local.rgba = std::move(rgba);
            local.w = w;
            local.h = h;
            local.ready = true;
        }
        std::lock_guard<std::mutex> lock(g_pending.mutex);
        g_pending.ready = local.ready;
        if (local.ready) {
            g_pending.rgba = std::move(local.rgba);
            g_pending.w = local.w;
            g_pending.h = local.h;
        } else if (g_requestedSource == path) {
            g_requestedSource.clear();
        }
        g_downloading.store(false);
    }).detach();
}

void ProfileAvatarUpdate() {
    PendingAvatar copy;
    {
        std::lock_guard<std::mutex> lock(g_pending.mutex);
        if (!g_pending.ready) return;
        copy.ready = true;
        copy.rgba = std::move(g_pending.rgba);
        copy.w = g_pending.w;
        copy.h = g_pending.h;
        g_pending.ready = false;
    }
    if (copy.ready) {
        UploadAvatarTexture(copy.rgba, copy.w, copy.h);
        g_loadedSource = g_requestedSource;
    }
}

const ProfileAvatarTexture& ProfileAvatarGet() {
    return g_avatar;
}

ImTextureID ProfileAvatarImGuiId() {
    return (ImTextureID)(intptr_t)g_avatar.tex;
}
