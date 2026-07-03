#include "ui/media_library.h"

#include "hooks.h"
#include "ipc/pipe_client.h"
#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/strings_zh.h"

#include "imgui_internal.h"

#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <gl/GL.h>

#undef min
#undef max

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassRect;

namespace {

float Px(float value, float scale) {
    return value * scale;
}

MediaLibraryLimits g_limits{};
HWND g_ownerHwnd = nullptr;

struct ImportJob {
    std::atomic<bool> done{false};
    bool success = false;
    std::wstring dest;
    std::string message;
    std::mutex mtx;
};

ImportJob g_importJob;

struct BgSetJob {
    std::atomic<bool> done{false};
    bool success = false;
};

BgSetJob g_bgSetJob;

struct ThumbSlot {
    GLuint tex = 0;
    int w = 0;
    int h = 0;
};

struct PendingThumb {
    std::wstring key;
    std::vector<uint8_t> rgba;
    int w = 0;
    int h = 0;
};

std::unordered_map<std::wstring, ThumbSlot> g_thumbs;
std::mutex g_thumbMutex;
std::vector<PendingThumb> g_pendingThumbs;
std::mutex g_pendingMutex;

std::deque<std::wstring> g_thumbQueue;
std::unordered_set<std::wstring> g_thumbQueued;
std::mutex g_queueMutex;
std::condition_variable g_queueCv;
std::atomic<bool> g_thumbWorkerRunning{true};
std::once_flag g_thumbWorkerOnce;

void HashThumbColor(const std::wstring& key, bool isVideo, uint8_t& r, uint8_t& g, uint8_t& b);

ImU32 ThumbColorForItem(const MediaItem& item) {
    uint8_t r = 0, g = 0, b = 0;
    HashThumbColor(item.name, item.is_video, r, g, b);
    return IM_COL32(r, g, b, 255);
}

std::wstring LocalMyiuiDir() {
    wchar_t appData[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData);
    return std::wstring(appData) + L"\\MyiUI";
}

std::wstring LibraryDir() {
    return LocalMyiuiDir() + L"\\backgrounds";
}

std::wstring LibraryJsonPath() {
    return LibraryDir() + L"\\library.json";
}

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}

bool IsVideoName(const std::wstring& name) {
    const std::wstring lower = [&] {
        std::wstring s = name;
        for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
        return s;
    }();
    return lower.ends_with(L".mp4") || lower.ends_with(L".webm");
}

bool IsImageName(const std::wstring& name) {
    const std::wstring lower = [&] {
        std::wstring s = name;
        for (wchar_t& c : s) c = static_cast<wchar_t>(towlower(c));
        return s;
    }();
    return lower.ends_with(L".png") || lower.ends_with(L".jpg") || lower.ends_with(L".jpeg");
}

bool IsMediaName(const std::wstring& name) {
    return IsVideoName(name) || IsImageName(name);
}

void DrawTileLabelEllipsis(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float barH, float uiScale,
                           const char* text) {
    if (text == nullptr || *text == '\0') return;
    const float padX = Px(10.f, uiScale);
    const float textY = max.y - barH + Px(6.f, uiScale);
    const float fontSize = ImGui::GetFontSize();
    const ImVec2 textMin(min.x + padX, textY);
    const ImVec2 textMax(max.x - padX, textY + fontSize);
    const float clipX = textMax.x;
    dl->PushClipRect(ImVec2(min.x, max.y - barH), max, true);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 235.f / 255.f));
    ImGui::RenderTextEllipsis(dl, textMin, textMax, clipX, clipX, text, nullptr, nullptr);
    ImGui::PopStyleColor();
    dl->PopClipRect();
}

int64_t FileSizeBytes(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return -1;
    LARGE_INTEGER size;
    size.LowPart = fad.nFileSizeLow;
    size.HighPart = fad.nFileSizeHigh;
    return size.QuadPart;
}

std::wstring ParseJsonStringField(const std::string& json, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) return L"";
    const auto colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return L"";
    const auto q0 = json.find('"', colon + 1);
    if (q0 == std::string::npos) return L"";
    const auto q1 = json.find('"', q0 + 1);
    if (q1 == std::string::npos) return L"";
    return Utf8ToWide(json.substr(q0 + 1, q1 - q0 - 1));
}

void SaveLibrarySelection(const std::wstring& fileName) {
    const std::wstring dir = LibraryDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    const std::filesystem::path jsonPath = LibraryJsonPath();
    std::ofstream jsonOut(jsonPath);
    if (!jsonOut) return;
    jsonOut << "{\"selected\":\"" << WideToUtf8(fileName) << "\"}\n";
}

std::wstring LoadLibrarySelection() {
    const std::filesystem::path jsonPath = LibraryJsonPath();
    std::ifstream jsonIn(jsonPath);
    if (!jsonIn) return L"";
    std::string json((std::istreambuf_iterator<char>(jsonIn)), std::istreambuf_iterator<char>());
    return ParseJsonStringField(json, "selected");
}

std::wstring UniqueDestPath(const std::wstring& dir, const std::wstring& fileName) {
    const auto dot = fileName.find_last_of(L'.');
    const std::wstring stem = dot == std::wstring::npos ? fileName : fileName.substr(0, dot);
    const std::wstring ext = dot == std::wstring::npos ? L"" : fileName.substr(dot);
    std::wstring candidate = dir + L"\\" + fileName;
    if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) return candidate;
    for (int i = 1; i < 1000; ++i) {
        candidate = dir + L"\\" + stem + L"_" + std::to_wstring(i) + ext;
        if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) return candidate;
    }
    return dir + L"\\" + stem + L"_" + std::to_wstring(GetTickCount64()) + ext;
}

std::wstring SanitizeMediaFileName(const std::wstring& fileName) {
    const auto dot = fileName.find_last_of(L'.');
    std::wstring stem = dot == std::wstring::npos ? fileName : fileName.substr(0, dot);
    const std::wstring ext = dot == std::wstring::npos ? L"" : fileName.substr(dot);
    for (wchar_t& c : stem) {
        if (c == L' ' || c == L'\t') {
            c = L'_';
        } else if (c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'/' || c == L'\\' || c == L'|'
                   || c == L'?' || c == L'*') {
            c = L'_';
        }
    }
    while (!stem.empty() && stem.back() == L'.') {
        stem.pop_back();
    }
    if (stem.empty()) {
        stem = L"media";
    }
    return stem + ext;
}

bool RenameIfNeededForPlayback(const std::wstring& dir, std::wstring& pathInOut) {
    const wchar_t* filePart = PathFindFileNameW(pathInOut.c_str());
    if (filePart == nullptr || *filePart == L'\0') {
        return false;
    }
    const std::wstring currentName = filePart;
    const std::wstring sanitized = SanitizeMediaFileName(currentName);
    if (_wcsicmp(currentName.c_str(), sanitized.c_str()) == 0) {
        return false;
    }
    const std::wstring dest = UniqueDestPath(dir, sanitized);
    if (!MoveFileW(pathInOut.c_str(), dest.c_str())) {
        return false;
    }
    pathInOut = dest;
    return true;
}

void HashThumbColor(const std::wstring& key, bool isVideo, uint8_t& r, uint8_t& g, uint8_t& b) {
    uint32_t h = 2166136261u;
    for (wchar_t c : key) {
        h ^= static_cast<uint32_t>(c);
        h *= 16777619u;
    }
    r = static_cast<uint8_t>(52 + (h & 0x4F));
    g = static_cast<uint8_t>(58 + ((h >> 8) & 0x4F));
    b = static_cast<uint8_t>(72 + ((h >> 16) & 0x4F));
    if (isVideo) {
        r = static_cast<uint8_t>(r * 0.65f);
        g = static_cast<uint8_t>(g * 0.65f);
        b = static_cast<uint8_t>(b * 0.65f);
    }
}

bool HBitmapToRgba(HBITMAP hbmp, std::vector<uint8_t>& rgba, int& outW, int& outH) {
    BITMAP bm{};
    if (GetObject(hbmp, sizeof(bm), &bm) == 0) return false;
    outW = bm.bmWidth;
    outH = bm.bmHeight > 0 ? bm.bmHeight : -bm.bmHeight;
    if (outW <= 0 || outH <= 0) return false;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = outW;
    bi.bmiHeader.biHeight = -outH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    rgba.assign(static_cast<size_t>(outW) * outH * 4, 0);
    HDC hdc = GetDC(nullptr);
    if (!hdc) return false;
    const int lines = GetDIBits(hdc, hbmp, 0, static_cast<UINT>(outH), rgba.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (lines == 0) return false;

    for (size_t i = 0; i < rgba.size(); i += 4) {
        std::swap(rgba[i], rgba[i + 2]);
    }
    return true;
}

bool LoadShellThumbnail(const std::wstring& path, std::vector<uint8_t>& rgba, int& outW, int& outH) {
    IShellItem* item = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&item)))) return false;

    IShellItemImageFactory* factory = nullptr;
    const HRESULT qi = item->QueryInterface(IID_PPV_ARGS(&factory));
    item->Release();
    if (FAILED(qi) || !factory) return false;

    SIZE size{320, 180};
    HBITMAP hbmp = nullptr;
    const HRESULT hr =
        factory->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
    factory->Release();
    if (FAILED(hr) || !hbmp) return false;

    const bool ok = HBitmapToRgba(hbmp, rgba, outW, outH);
    DeleteObject(hbmp);
    return ok;
}

bool LoadWicThumbnail(const std::wstring& path, std::vector<uint8_t>& rgba, int& outW, int& outH) {
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) {
        factory->Release();
        return false;
    }

    UINT srcW = 0, srcH = 0;
    frame->GetSize(&srcW, &srcH);
    if (srcW == 0 || srcH == 0) {
        frame->Release();
        factory->Release();
        return false;
    }

    const UINT maxW = 320;
    const UINT maxH = 180;
    const double scale = (std::min)(static_cast<double>(maxW) / srcW, static_cast<double>(maxH) / srcH);
    const UINT dstW = static_cast<UINT>(std::max(1.0, std::floor(srcW * scale)));
    const UINT dstH = static_cast<UINT>(std::max(1.0, std::floor(srcH * scale)));

    IWICBitmapScaler* scaler = nullptr;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr) || !scaler) {
        frame->Release();
        factory->Release();
        return false;
    }
    scaler->Initialize(frame, dstW, dstH, WICBitmapInterpolationModeCubic);
    frame->Release();

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr) || !converter) {
        scaler->Release();
        factory->Release();
        return false;
    }
    hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f,
                               WICBitmapPaletteTypeCustom);
    scaler->Release();
    if (FAILED(hr)) {
        converter->Release();
        factory->Release();
        return false;
    }

    outW = static_cast<int>(dstW);
    outH = static_cast<int>(dstH);
    rgba.resize(static_cast<size_t>(outW) * outH * 4);
    hr = converter->CopyPixels(nullptr, static_cast<UINT>(outW) * 4, static_cast<UINT>(rgba.size()), rgba.data());
    converter->Release();
    factory->Release();
    return SUCCEEDED(hr);
}

bool LoadThumbnailPixels(const std::wstring& path, bool isVideo, std::vector<uint8_t>& rgba, int& outW, int& outH) {
    if (!isVideo && LoadWicThumbnail(path, rgba, outW, outH)) return true;
    return LoadShellThumbnail(path, rgba, outW, outH);
}

void QueueThumbLoad(const std::wstring& path) {
  {
    std::lock_guard lock(g_thumbMutex);
    if (g_thumbs.contains(path)) return;
  }
  {
    std::lock_guard lock(g_queueMutex);
    if (g_thumbQueued.contains(path)) return;
    g_thumbQueued.insert(path);
    g_thumbQueue.push_back(path);
  }
  g_queueCv.notify_one();
}

void ThumbWorkerLoop() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    while (g_thumbWorkerRunning.load()) {
        std::wstring path;
        {
            std::unique_lock lock(g_queueMutex);
            g_queueCv.wait(lock, [] {
                return !g_thumbWorkerRunning.load() || !g_thumbQueue.empty();
            });
            if (!g_thumbWorkerRunning.load()) break;
            path = std::move(g_thumbQueue.front());
            g_thumbQueue.pop_front();
        }

        std::vector<uint8_t> rgba;
        int w = 0, h = 0;
        const bool isVideo = IsVideoName(path);
        const bool ok = LoadThumbnailPixels(path, isVideo, rgba, w, h);

        {
            std::lock_guard lock(g_queueMutex);
            g_thumbQueued.erase(path);
        }

        if (!ok || rgba.empty() || w <= 0 || h <= 0) continue;

        PendingThumb pending;
        pending.key = path;
        pending.rgba = std::move(rgba);
        pending.w = w;
        pending.h = h;
        {
            std::lock_guard lock(g_pendingMutex);
            g_pendingThumbs.push_back(std::move(pending));
        }
    }
    CoUninitialize();
}

void EnsureThumbWorker() {
    std::call_once(g_thumbWorkerOnce, []() { std::thread(ThumbWorkerLoop).detach(); });
}

std::string FormatSizeLimitMb(int64_t bytes) {
    const int64_t mb = (bytes + (1024 * 1024 - 1)) / (1024 * 1024);
    return std::to_string(mb);
}

std::wstring BackgroundJsonPath() {
    wchar_t appData[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appData);
    return std::wstring(appData) + L"\\MyiUI\\runtime\\config\\menu\\background.json";
}

std::string JsonEscapePath(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

void PersistVideoPathConfig(const std::wstring& path) {
    const std::wstring cfgPath = BackgroundJsonPath();
    const auto parent = std::filesystem::path(cfgPath).parent_path();
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);

    std::string json;
    {
        std::ifstream in(cfgPath, std::ios::binary);
        if (in) {
            json.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
    }
    if (json.empty()) {
        json = "{\n  \"video_path\": \"\"\n}\n";
    }

    const std::string pathUtf8 = JsonEscapePath(WideToUtf8(path));
    {
        const std::regex field(R"("video_path"\s*:\s*"[^"]*")");
        if (std::regex_search(json, field)) {
            json = std::regex_replace(json, field, "\"video_path\": \"" + pathUtf8 + "\"");
        } else {
            const auto brace = json.rfind('}');
            if (brace != std::string::npos) {
                const std::string insert = "  \"video_path\": \"" + pathUtf8 + "\",\n";
                json.insert(brace, insert);
            }
        }
    }

    std::ofstream out(cfgPath, std::ios::binary | std::ios::trunc);
    if (out) out << json;
}

std::wstring LoadPersistedVideoPath() {
    const std::filesystem::path cfgPath = BackgroundJsonPath();
    std::ifstream in(cfgPath, std::ios::binary);
    if (!in) return L"";
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return ParseJsonStringField(json, "video_path");
}

bool PathsEqualInsensitive(const std::wstring& a, const std::wstring& b) {
    return !a.empty() && !b.empty() && _wcsicmp(a.c_str(), b.c_str()) == 0;
}

void StopBackgroundPlayback(MediaLibraryState& state) {
    state.selected_path.clear();
    SaveLibrarySelection(L"");
    PersistVideoPathConfig(L"");
    OverlayInvalidateBackgroundTexture();
    g_bgSetJob.done.store(false, std::memory_order_release);
    const bool ok = PipeSendCommandWaitMs("SET_BG_VIDEO:", 8000);
    g_bgSetJob.success = ok;
    g_bgSetJob.done.store(true, std::memory_order_release);
}

bool ValidateMediaSize(const std::wstring& path, bool isVideo, std::string& errorOut) {
    const int64_t size = FileSizeBytes(path);
    if (size < 0) {
        errorOut = myiui::strings::kErrReadSize;
        return false;
    }
    const int64_t limit = isVideo ? g_limits.max_video_bytes : g_limits.max_image_bytes;
    if (size > limit) {
        const int limitMb = static_cast<int>((limit + (1024 * 1024 - 1)) / (1024 * 1024));
        errorOut = isVideo ? myiui::strings::VideoSizeError(limitMb) : myiui::strings::ImageSizeError(limitMb);
        return false;
    }
    return true;
}

bool CopyMediaToLibrary(const std::wstring& src, std::wstring& destOut, std::string& errorOut) {
    const bool isVideo = IsVideoName(src);
    const bool isImage = IsImageName(src);
    if (!isVideo && !isImage) {
        errorOut = myiui::strings::kErrUnsupported;
        return false;
    }
    if (!ValidateMediaSize(src, isVideo, errorOut)) return false;

    const std::wstring dir = LibraryDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    const wchar_t* filePart = PathFindFileNameW(src.c_str());
    const std::wstring safeName = SanitizeMediaFileName(filePart);
    const std::wstring dest = UniqueDestPath(dir, safeName);
    if (!CopyFileW(src.c_str(), dest.c_str(), FALSE)) {
        errorOut = myiui::strings::kErrCopyFail;
        return false;
    }
    destOut = dest;
    return true;
}

void RemoveThumbCache(const std::wstring& path) {
    std::lock_guard lock(g_thumbMutex);
    const auto it = g_thumbs.find(path);
    if (it != g_thumbs.end()) {
        if (it->second.tex != 0) {
            glDeleteTextures(1, &it->second.tex);
        }
        g_thumbs.erase(it);
    }
}

void ClearBackgroundSelection(MediaLibraryState& state) {
    StopBackgroundPlayback(state);
}

bool IsPathUnderLibraryDir(const std::wstring& path) {
    const std::wstring libDir = LibraryDir();
    std::error_code ec;
    std::wstring normPath = std::filesystem::weakly_canonical(path, ec).wstring();
    if (ec) normPath = path;
    std::wstring normDir = std::filesystem::weakly_canonical(libDir, ec).wstring();
    if (ec) {
        std::filesystem::create_directories(libDir, ec);
        normDir = libDir;
    }
    for (wchar_t& c : normPath) {
        if (c == L'/') c = L'\\';
    }
    for (wchar_t& c : normDir) {
        if (c == L'/') c = L'\\';
    }
    if (!normDir.empty() && normDir.back() != L'\\') {
        normDir += L'\\';
    }
    if (normPath.size() < normDir.size()) return false;
    return _wcsnicmp(normPath.c_str(), normDir.c_str(), normDir.size()) == 0;
}

}  // namespace

bool MediaLibraryDeleteItem(const std::wstring& path, MediaLibraryState& state) {
    if (path.empty()) return false;
    if (!IsPathUnderLibraryDir(path)) {
        return false;
    }

    const bool wasSelected = PathsEqualInsensitive(state.selected_path, path);
    const bool isActiveBg = wasSelected || PathsEqualInsensitive(LoadPersistedVideoPath(), path);
    if (isActiveBg) {
        StopBackgroundPlayback(state);
    }

    if (!DeleteFileW(path.c_str())) {
        return false;
    }

    {
        std::lock_guard lock(g_thumbMutex);
        const auto it = g_thumbs.find(path);
        if (it != g_thumbs.end()) {
            if (it->second.tex != 0) {
                glDeleteTextures(1, &it->second.tex);
            }
            g_thumbs.erase(it);
        }
    }

    state.needs_refresh = true;
    return true;
}

void MediaLibrarySetLimits(const MediaLibraryLimits& limits) {
    g_limits = limits;
}

void MediaLibrarySetWindowHandle(HWND hwnd) {
    g_ownerHwnd = hwnd;
}

void MediaLibraryRefresh(MediaLibraryState& state) {
    const std::wstring dir = LibraryDir();
    CreateDirectoryW(dir.c_str(), nullptr);

    state.items.clear();
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            const std::wstring name = fd.cFileName;
            if (!IsMediaName(name)) continue;
            MediaItem item;
            item.name = name;
            item.path = dir + L"\\" + name;
            item.is_video = IsVideoName(name);
            LARGE_INTEGER size;
            size.LowPart = fd.nFileSizeLow;
            size.HighPart = fd.nFileSizeHigh;
            item.file_size = size.QuadPart;
            state.items.push_back(std::move(item));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    std::sort(state.items.begin(), state.items.end(),
              [](const MediaItem& a, const MediaItem& b) { return a.name < b.name; });

    const std::wstring selectedName = LoadLibrarySelection();
    state.selected_path.clear();
    if (!selectedName.empty()) {
        for (const auto& item : state.items) {
            if (_wcsicmp(item.name.c_str(), selectedName.c_str()) == 0) {
                state.selected_path = item.path;
                break;
            }
        }
    }

    EnsureThumbWorker();
    for (auto& item : state.items) {
        const std::wstring oldPath = item.path;
        if (RenameIfNeededForPlayback(dir, item.path)) {
            item.name = PathFindFileNameW(item.path.c_str());
            if (!state.selected_path.empty() && _wcsicmp(state.selected_path.c_str(), oldPath.c_str()) == 0) {
                state.selected_path = item.path;
                SaveLibrarySelection(item.name);
            }
        }
        QueueThumbLoad(item.path);
    }

    state.needs_refresh = false;

    if (state.auto_apply_on_load && !state.selected_path.empty()) {
        state.auto_apply_on_load = false;
        MediaLibraryApplyBackground(state.selected_path, state);
    }
}

void MediaLibraryApplyBackground(const std::wstring& path, MediaLibraryState& state) {
    if (path.empty()) return;

    std::wstring resolvedPath = path;
    const std::wstring dir = LibraryDir();
    if (RenameIfNeededForPlayback(dir, resolvedPath)) {
        const wchar_t* filePart = PathFindFileNameW(resolvedPath.c_str());
        if (!state.selected_path.empty() && _wcsicmp(state.selected_path.c_str(), path.c_str()) == 0) {
            state.selected_path = resolvedPath;
            if (filePart != nullptr) {
                SaveLibrarySelection(filePart);
            }
        }
        state.needs_refresh = true;
    }

    std::string sizeError;
    const bool isVideo = IsVideoName(resolvedPath);
    if (!ValidateMediaSize(resolvedPath, isVideo, sizeError)) {
        state.toast_message = sizeError;
        state.toast_timer = 4.f;
        return;
    }

    PersistVideoPathConfig(resolvedPath);
    OverlayInvalidateBackgroundTexture();

    g_bgSetJob.done.store(false, std::memory_order_release);
    const std::string cmd = std::string("SET_BG_VIDEO:") + WideToUtf8(resolvedPath);
    std::thread([cmd]() {
        const bool ok = PipeSendCommandWaitMs(cmd, 5000);
        g_bgSetJob.success = ok;
        g_bgSetJob.done.store(true, std::memory_order_release);
    }).detach();
}

void MediaLibrarySelect(const std::wstring& path, MediaLibraryState& state) {
    if (path.empty()) return;
    state.selected_path = path;
    const wchar_t* filePart = PathFindFileNameW(path.c_str());
    SaveLibrarySelection(filePart);
    MediaLibraryApplyBackground(path, state);
}

void PollImportJob(MediaLibraryState& state) {
    if (!g_importJob.done.load(std::memory_order_acquire)) return;

    bool success = false;
    std::wstring dest;
    std::string message;
    {
        std::lock_guard lock(g_importJob.mtx);
        success = g_importJob.success;
        dest = g_importJob.dest;
        message = g_importJob.message;
        g_importJob.done.store(false, std::memory_order_release);
    }

    state.picker_busy = false;

    if (success) {
        MediaLibrarySelect(dest, state);
        state.needs_refresh = true;
        state.toast_message = myiui::strings::kToastAdded;
        state.toast_timer = 3.f;
    } else if (!message.empty()) {
        state.toast_message = message;
        state.toast_timer = 4.f;
    }
}

static void StartCopyJob(const std::wstring& src) {
    std::thread([src]() {
        std::wstring dest;
        std::string error;
        const bool ok = CopyMediaToLibrary(src, dest, error);
        {
            std::lock_guard lock(g_importJob.mtx);
            g_importJob.success = ok;
            g_importJob.dest = std::move(dest);
            g_importJob.message = ok ? std::string{} : error;
        }
        g_importJob.done.store(true, std::memory_order_release);
    }).detach();
}

static void RunAddPickerOnMainThread(MediaLibraryState& state) {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_ownerHwnd;
    ofn.lpstrFilter = L"媒体文件\0*.mp4;*.webm;*.png;*.jpg;*.jpeg\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;

    state.picker_busy = true;
    StartCopyJob(file);
}

void MediaLibraryOpenAddPickerAsync(MediaLibraryState& state) {
    if (state.picker_busy) return;
    state.add_requested = true;
}

void MediaLibraryTick(MediaLibraryState& state) {
    MediaLibraryProcessPendingTextures();
    PollImportJob(state);

    if (g_bgSetJob.done.load(std::memory_order_acquire)) {
        const bool ok = g_bgSetJob.success;
        g_bgSetJob.done.store(false, std::memory_order_release);
        state.toast_message = ok ? myiui::strings::kToastBgOk : myiui::strings::kToastBgFail;
        state.toast_timer = ok ? 2.5f : 4.f;
        if (ok) {
            OverlayInvalidateBackgroundTexture();
        }
    }

    if (state.add_requested && !state.picker_busy) {
        state.add_requested = false;
        RunAddPickerOnMainThread(state);
    }

    if (state.needs_refresh) MediaLibraryRefresh(state);
}

void MediaLibraryRenderGrid(const AppConfig& cfg, MediaLibraryState& state, float uiScale, float gridHeight) {
    ImGui::BeginChild("##LibraryGrid", ImVec2(0, gridHeight), false);

    const float gap = Px(12.f, uiScale);
    const int columns = 3;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float tileW = (availW - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
    const float tileH = tileW * 9.f / 16.f;

    auto* dl = ImGui::GetWindowDrawList();

    auto drawTile = [&](const char* id, const ImVec2& size, auto&& drawContent) -> bool {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        const ImVec2 min = pos;
        const ImVec2 max(pos.x + size.x, pos.y + size.y);
        const float round = Px(10.f, uiScale);
        dl->AddRectFilled(min, max, IM_COL32(36, 40, 52, 255), round);
        drawContent(min, max);
        if (hovered) {
            dl->AddRect(min, max, IM_COL32(255, 255, 255, 60), round, 0, 2.f);
        }
        return clicked;
    };

    const size_t totalCells = state.items.size() + 1;
    const ImVec2 tileSize(tileW, tileH);

    for (size_t cell = 0; cell < totalCells; ++cell) {
        if (cell % static_cast<size_t>(columns) != 0) {
            ImGui::SameLine(0.f, gap);
        }

        if (cell == 0) {
            if (drawTile("##add_tile", tileSize, [&](const ImVec2& min, const ImVec2& max) {
                    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
                    const char* plusLine = "+";
                    const char* addLine = myiui::strings::kAdd;
                    const ImVec2 plusSize = ImGui::CalcTextSize(plusLine);
                    const ImVec2 addSize = ImGui::CalcTextSize(addLine);
                    const float lineGap = Px(2.f, uiScale);
                    const float blockH = plusSize.y + lineGap + addSize.y;
                    const float topY = center.y - blockH * 0.5f;
                    dl->AddText(ImVec2(center.x - plusSize.x * 0.5f, topY), IM_COL32(255, 255, 255, 220), plusLine);
                    dl->AddText(ImVec2(center.x - addSize.x * 0.5f, topY + plusSize.y + lineGap),
                                IM_COL32(200, 206, 220, 220), addLine);
                })) {
                if (!state.picker_busy) {
                    MediaLibraryOpenAddPickerAsync(state);
                }
            }
        } else {
            const MediaItem& item = state.items[cell - 1];
            ImGui::PushID(static_cast<int>(cell));
            const bool selected =
                !state.selected_path.empty() && _wcsicmp(state.selected_path.c_str(), item.path.c_str()) == 0;

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImGuiButtonFlags tileButtons =
                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight;
            ImGui::InvisibleButton("##tile", tileSize, tileButtons);
            const bool hovered = ImGui::IsItemHovered();
            const bool leftClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            const ImVec2 min = pos;
            const ImVec2 max(pos.x + tileSize.x, pos.y + tileSize.y);
            const float round = Px(10.f, uiScale);
            dl->AddRectFilled(min, max, IM_COL32(36, 40, 52, 255), round);
            {
                int tw = 0, th = 0;
                const ImTextureID thumb = MediaLibraryGetThumb(item.path, item.is_video, tw, th);
                if (thumb && tw > 0 && th > 0) {
                    dl->AddImageRounded(thumb, min, max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255),
                                        round);
                } else {
                    dl->AddRectFilled(min, max, ThumbColorForItem(item), round);
                }
                if (item.is_video) {
                    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
                    const char* playMark = ">";
                    const ImVec2 playSize = ImGui::CalcTextSize(playMark);
                    dl->AddText(ImVec2(center.x - playSize.x * 0.5f, center.y - playSize.y * 0.5f),
                                IM_COL32(255, 255, 255, 180), playMark);
                }
                const float barH = Px(28.f, uiScale);
                dl->AddRectFilled(ImVec2(min.x, max.y - barH), max, IM_COL32(0, 0, 0, 150), round,
                                  ImDrawFlags_RoundCornersBottom);
                const std::string nameUtf8 = WideToUtf8(item.name);
                DrawTileLabelEllipsis(dl, min, max, barH, uiScale, nameUtf8.c_str());
                if (selected) {
                    dl->AddRect(min, max, IM_COL32(255, 255, 255, 255), round, 0, Px(3.f, uiScale));
                }
            }
            if (hovered) {
                dl->AddRect(min, max, IM_COL32(255, 255, 255, 60), round, 0, 2.f);
            }
            if (leftClicked && !state.picker_busy) {
                MediaLibrarySelect(item.path, state);
            }
            if (selected && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
                !state.picker_busy) {
                if (MediaLibraryDeleteItem(item.path, state)) {
                    state.toast_message = myiui::strings::kToastDeleted;
                    state.toast_timer = 2.5f;
                } else {
                    state.toast_message = myiui::strings::kToastDeleteFail;
                    state.toast_timer = 3.f;
                }
            }
            ImGui::PopID();
        }

        if ((cell + 1) % static_cast<size_t>(columns) == 0 && cell + 1 < totalCells) {
            ImGui::Dummy(ImVec2(0.f, gap));
        }
    }

    ImGui::EndChild();
}

void MediaLibraryProcessPendingTextures() {
    std::vector<PendingThumb> batch;
    {
        std::lock_guard lock(g_pendingMutex);
        batch.swap(g_pendingThumbs);
    }
    for (auto& pending : batch) {
        if (pending.rgba.empty() || pending.w <= 0 || pending.h <= 0) continue;
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pending.w, pending.h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pending.rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::lock_guard lock(g_thumbMutex);
        auto& slot = g_thumbs[pending.key];
        if (slot.tex != 0) glDeleteTextures(1, &slot.tex);
        slot.tex = tex;
        slot.w = pending.w;
        slot.h = pending.h;
    }
}

ImTextureID MediaLibraryGetThumb(const std::wstring& path, bool is_video, int& outW, int& outH) {
    (void)is_video;
    std::lock_guard lock(g_thumbMutex);
    const auto it = g_thumbs.find(path);
    if (it == g_thumbs.end() || it->second.tex == 0) {
        outW = outH = 0;
        return (ImTextureID)(intptr_t)0;
    }
    outW = it->second.w;
    outH = it->second.h;
    return (ImTextureID)(intptr_t)it->second.tex;
}

void MediaLibraryRenderPanel(const AppConfig& cfg, MediaLibraryState& state, float uiScale, bool* open) {
    MediaLibraryTick(state);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && open && *open) {
        *open = false;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 panelSize(Px(920.f, uiScale), Px(640.f, uiScale));
    ImVec2 panelPos(display.x * 0.5f - panelSize.x * 0.5f, display.y * 0.5f - panelSize.y * 0.5f);

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
    ImGui::SetNextWindowFocus();

    const UiFonts& fonts = GetUiFonts();
    struct FontStackGuard {
        ImFont* font = nullptr;
        explicit FontStackGuard(ImFont* f) : font(f) {
            if (font) ImGui::PushFont(font);
        }
        ~FontStackGuard() {
            if (font) ImGui::PopFont();
        }
    } fontGuard(fonts.regular);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, 0.12f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cfg.theme.corner_radius);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(24.f, uiScale), Px(20.f, uiScale)));

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("##WallpaperLibrary", open, panelFlags)) {
        if (fonts.semibold) ImGui::PushFont(fonts.semibold);
        ImGui::TextUnformatted(myiui::strings::kWallpaperTitle);
        if (fonts.semibold) ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, Px(8.f, uiScale)));
        MediaLibraryRenderGrid(cfg, state, uiScale, Px(520.f, uiScale));

        if (state.toast_timer > 0.f) {
            state.toast_timer -= ImGui::GetIO().DeltaTime;
            ImGui::Dummy(ImVec2(0, Px(6.f, uiScale)));
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.45f, 1.f), "%s", state.toast_message.c_str());
        }
        if (state.picker_busy) {
            ImGui::SameLine();
            ImGui::TextDisabled(myiui::strings::kImporting);
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
