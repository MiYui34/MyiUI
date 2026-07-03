#include "ui/music/music_panel.h"

#include "ui/music/cover_loader.h"
#include "ui/fonts.h"
#include "ipc/pipe_client.h"

#include "imgui.h"

#define NOMINMAX
#include <windows.h>
#include <gl/GL.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace myiui::ui::music {

namespace {

constexpr int kAccR = 64, kAccG = 194, kAccB = 255;
constexpr int kGlassBgBase[4]   = { 12, 12, 16, 140 }; // 加深一点背景让文字更清晰
constexpr int kGlassBorder[4]   = { 255, 255, 255, 30 };
constexpr int kGlassBorderAcc[4]= { 64, 194, 255, 100 };
constexpr int kTextPrimary[4]   = { 250, 250, 250, 255 };
constexpr int kTextSecondary[4] = { 180, 180, 185, 255 };
constexpr int kTextDim[4]       = { 110, 110, 115, 255 };
constexpr float kRadiusMd = 24.f;
constexpr float kRadiusSm = 12.f; // 稍微收敛倒角，显得更精致

// 与 ClickGui 同步 2× 缩放
constexpr float kScale = 2.f;
inline float S(float v) { return v * kScale; }

ImU32 RGBA(const int c[4], float aMul = 1.f) {
    return IM_COL32(c[0], c[1], c[2], static_cast<int>(std::min(255.f, c[3] * aMul)));
}
float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

ImFont* GetFont() { return GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont(); }
float TextW(ImFont* f, float sz, const char* t) { return f ? f->CalcTextSizeA(sz, FLT_MAX, 0.f, t).x : 0.f; }

void DrawGlass(ImDrawList* dl, ImVec2 min, ImVec2 max, float radius, float alpha, bool accent = false) {
    dl->AddRectFilled(min, max, RGBA(kGlassBgBase, alpha), radius);
    const int tintAlpha = accent ? 35 : 12;
    dl->AddRectFilled(min, max, IM_COL32(255, 255, 255, static_cast<int>(tintAlpha * alpha)), radius);
    const int* bc = accent ? kGlassBorderAcc : kGlassBorder;
    dl->AddRect(min, max, RGBA(bc, alpha), radius, 0, S(1.0f));
}

// ── 矢量图标绘制辅助 ──
void DrawPlayIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 col) {
    float s = size * 0.45f;
    ImVec2 p1(center.x - s * 0.4f, center.y - s * 0.866f);
    ImVec2 p2(center.x - s * 0.4f, center.y + s * 0.866f);
    ImVec2 p3(center.x + s * 0.9f, center.y);
    dl->AddTriangleFilled(p1, p2, p3, col);
}

void DrawPauseIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 col) {
    float s = size * 0.35f;
    float w = size * 0.22f;
    dl->AddRectFilled(ImVec2(center.x - s, center.y - s * 1.1f), ImVec2(center.x - s + w, center.y + s * 1.1f), col, S(1.f));
    dl->AddRectFilled(ImVec2(center.x + s - w, center.y - s * 1.1f), ImVec2(center.x + s, center.y + s * 1.1f), col, S(1.f));
}

void DrawSkipIcon(ImDrawList* dl, ImVec2 center, float size, ImU32 col, bool reverse) {
    float s = size * 0.35f;
    float dir = reverse ? -1.f : 1.f;
    ImVec2 p1(center.x - s * dir, center.y - s * 0.866f);
    ImVec2 p2(center.x - s * dir, center.y + s * 0.866f);
    ImVec2 p3(center.x + s * 0.6f * dir, center.y);
    dl->AddTriangleFilled(p1, p2, p3, col);
    dl->AddRectFilled(ImVec2(center.x + s * 0.6f * dir, center.y - s * 0.866f), 
                      ImVec2(center.x + s * 0.9f * dir, center.y + s * 0.866f), col, S(1.f));
}

// ── 数据模型 ──
struct SongItem { long long id = 0; std::string name, artist, album, cover; long long durationMs = 0; };
struct PlaylistItem { long long id = 0; std::string name, cover; int trackCount = 0; std::string creator; };
struct CommentItem { long long id = 0; std::string user, content; int likedCount = 0; };

struct MusicState {
    std::mutex mutex;
    bool loggedIn = false; std::string nickname, avatar; long long userId = 0;
    bool qrPending = false; int qrStatus = 0; std::string qrImageB64;
    std::string lastError;
    bool playing = false, paused = false; long long positionMs = 0, durationMs = 0; int volume = 80;
    std::string modeName = "顺序"; long long currentSongId = 0;
    std::string currentName, currentArtist, currentCover;
    std::string lyricCurrent, lyricNext; bool hasLyrics = false;
    int activeTab = 0;
    std::vector<SongItem> searchResults; std::vector<PlaylistItem> playlists;
    std::vector<SongItem> playlistTracks; long long selectedPlaylistId = 0; std::string selectedPlaylistName;
    std::vector<SongItem> dailySongs, historySongs; int historyType = 1;
    char searchBuf[256] = {};
    float statusPollTimer = 0.f, loginPollTimer = 0.f;
    std::string signinMessage;
    float signinMsgTimer = 0.f;
    bool apiUnreachable = false;
    float apiCheckTimer = 0.f;
    bool needsInitialFetch = true;
    int apiFailCount = 0;
};

MusicState g_state;
CoverTexture g_qrCover;
std::atomic<bool> g_qrDirty{false};

struct JsonView {
    const std::string& raw;

    static std::string DecodeJsonString(std::string s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] != '\\' || i + 1 >= s.size()) {
                out += s[i];
                continue;
            }
            const char c = s[++i];
            switch (c) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (i + 4 >= s.size()) {
                        out += 'u';
                        break;
                    }
                    unsigned int cp = 0;
                    bool ok = true;
                    for (int j = 0; j < 4; ++j) {
                        const char h = s[i + 1 + j];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else { ok = false; break; }
                    }
                    if (!ok) {
                        out += 'u';
                        break;
                    }
                    i += 4;
                    if (cp < 0x80) {
                        out += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: out += c; break;
            }
        }
        return out;
    }

    std::string str(const std::string& key) const {
        std::string needle = "\"" + key + "\"";
        size_t p = raw.find(needle);
        if (p == std::string::npos) return "";
        p = raw.find(':', p + needle.size());
        if (p == std::string::npos) return "";
        p = raw.find('"', p + 1);
        if (p == std::string::npos) return "";
        size_t end = raw.find('"', p + 1);
        if (end == std::string::npos) return "";
        return DecodeJsonString(raw.substr(p + 1, end - p - 1));
    }
    long long num(const std::string& key, long long def = 0) const {
        std::string needle = "\"" + key + "\"";
        size_t p = raw.find(needle);
        if (p == std::string::npos) return def;
        p = raw.find(':', p + needle.size());
        if (p == std::string::npos) return def;
        p++;
        while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) p++;
        try { return std::stoll(raw.substr(p)); } catch (...) { return def; }
    }
    bool boolean(const std::string& key, bool def = false) const {
        std::string needle = "\"" + key + "\"";
        size_t p = raw.find(needle);
        if (p == std::string::npos) return def;
        p = raw.find(':', p + needle.size());
        if (p == std::string::npos) return def;
        p++;
        while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) p++;
        return raw.compare(p, 4, "true") == 0;
    }
};

std::vector<SongItem> parseSongsArray(const std::string& json, const std::string& arrayKey) {
    std::vector<SongItem> out;
    std::string needle = "\"" + arrayKey + "\":[";
    size_t start = json.find(needle);
    if (start == std::string::npos) return out;
    start += needle.size();
    size_t depth = 1, objStart = std::string::npos;
    for (size_t i = start; i < json.size() && depth > 0; ++i) {
        char c = json[i];
        if (c == '{') { if (depth == 1) objStart = i; depth++; }
        else if (c == '}') {
            depth--;
            if (depth == 1 && objStart != std::string::npos) {
                JsonView v{json.substr(objStart, i - objStart + 1)};
                out.push_back({v.num("id"), v.str("name"), v.str("artist"), v.str("album"), v.str("cover"), v.num("duration_ms")});
                objStart = std::string::npos;
            }
        } else if (c == '[') depth++; else if (c == ']') { depth--; if (depth == 0) break; }
    }
    return out;
}

std::vector<PlaylistItem> parsePlaylistsArray(const std::string& json) {
    std::vector<PlaylistItem> out;
    std::string needle = "\"playlists\":[";
    size_t start = json.find(needle);
    if (start == std::string::npos) return out;
    start += needle.size();
    size_t depth = 1, objStart = std::string::npos;
    for (size_t i = start; i < json.size() && depth > 0; ++i) {
        char c = json[i];
        if (c == '{') { if (depth == 1) objStart = i; depth++; }
        else if (c == '}') {
            depth--;
            if (depth == 1 && objStart != std::string::npos) {
                JsonView v{json.substr(objStart, i - objStart + 1)};
                out.push_back({v.num("id"), v.str("name"), v.str("cover"), (int)v.num("track_count"), v.str("creator")});
                objStart = std::string::npos;
            }
        } else if (c == '[') depth++; else if (c == ']') { depth--; if (depth == 0) break; }
    }
    return out;
}

std::string songToJson(const SongItem& s) {
    std::string j = "{\"id\":" + std::to_string(s.id) + ",\"name\":\"";
    for (char c : s.name) { if (c == '"') j += "\\\""; else if (c == '\\') j += "\\\\"; else j += c; }
    j += "\",\"ar\":[{\"name\":\"";
    for (char c : s.artist) { if (c == '"') j += "\\\""; else if (c == '\\') j += "\\\\"; else j += c; }
    j += "\"}],\"al\":{\"name\":\"";
    for (char c : s.album) { if (c == '"') j += "\\\""; else if (c == '\\') j += "\\\\"; else j += c; }
    j += "\",\"picUrl\":\"" + s.cover + "\"},\"dt\":" + std::to_string(s.durationMs) + "}";
    return j;
}

// 播放队列用的精简 JSON：保留 picUrl（播放条封面需要），去掉冗余字段
std::string songToJsonForPlayback(const SongItem& s) {
    std::string j = "{\"id\":" + std::to_string(s.id) + ",\"name\":\"";
    for (char c : s.name) { if (c == '"') j += "\\\""; else if (c == '\\') j += "\\\\"; else j += c; }
    j += "\",\"ar\":[{\"name\":\"";
    for (char c : s.artist) { if (c == '"') j += "\\\""; else if (c == '\\') j += "\\\\"; else j += c; }
    j += "\"}],\"al\":{\"name\":\"";
    for (char c : s.album) { if (c == '"') j += "\\\""; else if (c == '\\') j += "\\\\"; else j += c; }
    j += "\",\"picUrl\":\"" + s.cover + "\"},\"dt\":" + std::to_string(s.durationMs) + "}";
    return j;
}

std::string buildPlayQueueJson(const std::vector<SongItem>& songs, size_t startIdx) {
    // 限制队列最多 50 首，避免 JSON 过大导致渲染线程卡顿
    const size_t maxQueue = 50;
    size_t begin = 0, end = songs.size();
    if (songs.size() > maxQueue) {
        begin = startIdx > 25 ? startIdx - 25 : 0;
        end = std::min(songs.size(), begin + maxQueue);
        startIdx -= begin;
    }
    std::string j = "{\"songs\":[";
    for (size_t i = begin; i < end; ++i) {
        if (i > begin) j += ",";
        j += songToJsonForPlayback(songs[i]);
    }
    j += "],\"start\":" + std::to_string(startIdx) + "}";
    return j;
}

std::vector<SongItem> getContextSongList() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    switch (g_state.activeTab) {
        case 0: return g_state.searchResults;
        case 1: return g_state.selectedPlaylistId != 0 ? g_state.playlistTracks : std::vector<SongItem>{};
        case 2: return g_state.dailySongs;
        case 3: return g_state.historySongs;
        default: return {};
    }
}

void playQueueAt(const std::vector<SongItem>& songs, size_t idx) {
    if (songs.empty() || idx >= songs.size()) return;
    PipeSendCommandAsync("NE_PLAY_QUEUE:" + buildPlayQueueJson(songs, idx));
}

void contextStepSong(int direction) {
    const std::vector<SongItem> songs = getContextSongList();
    if (songs.empty()) {
        PipeSendCommandAsync(direction > 0 ? "NE_PLAY_NEXT" : "NE_PLAY_PREV");
        return;
    }
    long long curId = 0;
    { std::lock_guard<std::mutex> lock(g_state.mutex); curId = g_state.currentSongId; }
    int idx = -1;
    for (size_t i = 0; i < songs.size(); ++i) {
        if (songs[i].id == curId) { idx = static_cast<int>(i); break; }
    }
    if (idx < 0) {
        playQueueAt(songs, direction > 0 ? 0 : songs.size() - 1);
        return;
    }
    const int count = static_cast<int>(songs.size());
    const int nextIdx = (idx + direction + count) % count;
    if (count == 1) {
        PipeSendCommandAsync(direction > 0 ? "NE_PLAY_NEXT" : "NE_PLAY_PREV");
        return;
    }
    playQueueAt(songs, static_cast<size_t>(nextIdx));
}

bool IsConnectionError(const std::string& err) {
    std::string errLower = err;
    std::transform(errLower.begin(), errLower.end(), errLower.begin(), ::tolower);
    return errLower.find("connection refused") != std::string::npos ||
           errLower.find("connect") != std::string::npos ||
           errLower.find("unreachable") != std::string::npos;
}

std::string FriendlyError(const std::string& err) {
    if (IsConnectionError(err)) return "API 服务未连接";
    if (err.rfind("signin:", 0) == 0) return "签到失败：" + err.substr(7);
    if (err.rfind("ERR ", 0) == 0) return err.substr(4);
    return err;
}

void asyncQuery(const std::string& cmd, std::function<void(const std::string&)> handler) {
    std::thread([cmd, handler]() {
        PipeQueryResult r = PipeQueryJson(cmd, 6000);
        if (r.ok) {
            if (handler) handler(r.body);
            { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.apiUnreachable = false; g_state.apiFailCount = 0; }
        } else {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.lastError = r.error.empty() ? "请求失败" : r.error;
            // 仅对连接拒绝类错误计为 API 不可达；超时等瞬时错误用计数器容忍
            if (IsConnectionError(r.error)) {
                g_state.apiFailCount++;
                if (g_state.apiFailCount >= 2) g_state.apiUnreachable = true;
            }
        }
    }).detach();
}

void asyncQueryErr(const std::string& cmd, std::function<void(const std::string&)> onOk,
                   std::function<void(const std::string&)> onErr = nullptr) {
    std::thread([cmd, onOk, onErr]() {
        PipeQueryResult r = PipeQueryJson(cmd, 6000);
        if (r.ok) {
            if (onOk) onOk(r.body);
            { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.apiUnreachable = false; g_state.apiFailCount = 0; }
        } else {
            std::string err = r.error.empty() ? "请求失败" : r.error;
            {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                g_state.lastError = err;
                if (IsConnectionError(err)) {
                    g_state.apiFailCount++;
                    if (g_state.apiFailCount >= 2) g_state.apiUnreachable = true;
                }
            }
            if (onErr) onErr(err);
        }
    }).detach();
}

void DrawApiUnreachableBanner(ImVec2 min, ImVec2 max, float alpha) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    DrawGlass(dl, min, max, kRadiusMd, alpha, false);
    dl->AddText(font, S(15.f), ImVec2(min.x + S(20.f), min.y + S(20.f)),
                IM_COL32(239, 68, 68, static_cast<int>(255 * alpha)), "无法连接 API 服务");
    dl->AddText(font, S(12.f), ImVec2(min.x + S(20.f), min.y + S(48.f)),
                RGBA(kTextDim, alpha), "请确保 api-enhanced 已启动，或点击下方重试");

    ImVec2 btnMin(min.x + S(20.f), min.y + S(78.f));
    ImVec2 btnMax(btnMin.x + S(120.f), btnMin.y + S(32.f));
    ImGui::SetCursorScreenPos(btnMin);
    bool hov = ImGui::IsMouseHoveringRect(btnMin, btnMax);
    DrawGlass(dl, btnMin, btnMax, kRadiusSm, alpha * (hov ? 1.3f : 1.f), true);
    const char* label = "重试启动";
    float lw = TextW(font, S(13.f), label);
    dl->AddText(font, S(13.f), ImVec2(btnMin.x + (S(120.f) - lw) * 0.5f, btnMin.y + S(9.f)),
                RGBA(kTextPrimary, alpha), label);
    if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.apiUnreachable = false; g_state.lastError.clear(); }
        asyncQueryErr("NE_API_START", [](const std::string&) {
            asyncQueryErr("NE_MY_PLAYLISTS", [](const std::string& body) {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                g_state.playlists = parsePlaylistsArray(body);
            });
        });
    }
}

void DrawEmptyHint(ImVec2 min, ImVec2 max, float alpha, const char* text) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    float tw = TextW(font, S(13.f), text);
    ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    dl->AddText(font, S(13.f), ImVec2(center.x - tw * 0.5f, center.y - S(8.f)),
                RGBA(kTextDim, alpha), text);
}

void refreshLoginStatus() {
    asyncQuery("NE_LOGIN_STATUS", [](const std::string& body) {
        JsonView v{body};
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.loggedIn = v.boolean("logged_in"); g_state.nickname = v.str("nickname");
        g_state.avatar = v.str("avatar"); g_state.userId = v.num("user_id");
        g_state.qrPending = v.boolean("qr_pending"); g_state.qrStatus = static_cast<int>(v.num("qr_status"));
    });
}

void refreshPlayStatus() {
    asyncQuery("NE_PLAY_STATUS", [](const std::string& body) {
        JsonView v{body};
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.playing = v.boolean("playing"); g_state.paused = v.boolean("paused");
        g_state.positionMs = v.num("position_ms"); g_state.durationMs = v.num("duration_ms");
        g_state.volume = static_cast<int>(v.num("volume")); g_state.modeName = v.str("mode");
        long long sid = v.num("song_id");
        g_state.currentSongId = sid;
        if (sid != 0) {
            g_state.currentName = v.str("song_name");
            g_state.currentArtist = v.str("song_artist");
            g_state.currentCover = v.str("song_cover");
        }
    });
}

void refreshLyrics() {
    asyncQuery("NE_LYRICS", [](const std::string& body) {
        JsonView v{body};
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.lyricCurrent = v.str("current"); g_state.lyricNext = v.str("next"); g_state.hasLyrics = v.boolean("has_lyrics");
    });
}

void DrawTextEllipsis(ImDrawList* dl, ImFont* f, float sz, ImVec2 pos, float maxW, ImU32 col, const char* text) {
    if (!text || !text[0]) return;
    std::string s = text;
    if (TextW(f, sz, s.c_str()) <= maxW) { dl->AddText(f, sz, pos, col, s.c_str()); return; }
    while (!s.empty() && TextW(f, sz, (s + "...").c_str()) > maxW) s.pop_back();
    dl->AddText(f, sz, pos, col, (s + "...").c_str());
}

std::string formatTime(long long ms) {
    ms = std::max(0LL, ms); int total = static_cast<int>(ms / 1000);
    char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

void DrawLoginSection(ImVec2 min, ImVec2 max, float alpha, float dt) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    dl->AddText(font, S(18.f), ImVec2(min.x + S(20.f), min.y + S(16.f)), RGBA(kTextPrimary, alpha), "登录网易云账号");

    // 二维码轮询：当 QR 已显示且未过期/未成功时，定期调用 NE_QR_POLL 检测扫码状态
    {
        std::string qrB64; int qrStatus = 0;
        { std::lock_guard<std::mutex> lock(g_state.mutex); qrB64 = g_state.qrImageB64; qrStatus = g_state.qrStatus; }
        if (!qrB64.empty() && (qrStatus == 801 || qrStatus == 802 || qrStatus == 0)) {
            float pollTimer = 0.f;
            { std::lock_guard<std::mutex> lock(g_state.mutex); pollTimer = (g_state.loginPollTimer += dt); }
            if (pollTimer > 2.f) {
                { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.loginPollTimer = 0.f; }
                asyncQueryErr("NE_QR_POLL", [](const std::string& body) {
                    JsonView v{body};
                    int status = static_cast<int>(v.num("status"));
                    bool loggedIn = v.boolean("logged_in");
                    {
                        std::lock_guard<std::mutex> lock(g_state.mutex);
                        g_state.qrStatus = status;
                        if (loggedIn) {
                            g_state.loggedIn = true;
                            g_state.qrImageB64.clear();
                            g_state.qrPending = false;
                            g_state.needsInitialFetch = true;
                            g_qrDirty.store(true);
                        } else if (status == 800) {
                            g_state.qrImageB64.clear();
                            g_state.qrPending = false;
                            g_qrDirty.store(true);
                        }
                    }
                    if (loggedIn) refreshLoginStatus();
                });
            }
        }
    }

    ImVec2 cardMin(min.x + S(20.f), min.y + S(50.f));
    ImVec2 cardMax(max.x - S(20.f), std::min(max.y - S(20.f), min.y + S(220.f)));
    DrawGlass(dl, cardMin, cardMax, kRadiusMd, alpha, false);

    ImVec2 qrMin(cardMin.x + S(16.f), cardMin.y + S(16.f));
    ImVec2 qrMax(qrMin.x + S(120.f), qrMin.y + S(120.f));
    dl->AddRectFilled(qrMin, qrMax, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), kRadiusSm);

    std::string qrB64; int qrStatus = 0; bool qrPending = false; std::string lastError;
    { std::lock_guard<std::mutex> lock(g_state.mutex); qrB64 = g_state.qrImageB64; qrStatus = g_state.qrStatus; qrPending = g_state.qrPending; lastError = g_state.lastError; }

    if (!qrB64.empty()) {
        if (g_qrDirty.exchange(false) && g_qrCover.tex != 0) { glDeleteTextures(1, &g_qrCover.tex); g_qrCover = CoverTexture{}; }
        if (!g_qrCover.valid()) g_qrCover = CoverFromBase64Png(qrB64);
        if (g_qrCover.valid()) dl->AddImage((ImTextureID)(intptr_t)g_qrCover.tex, qrMin, qrMax);
    } else {
        if (g_qrCover.tex != 0) { glDeleteTextures(1, &g_qrCover.tex); g_qrCover = CoverTexture{}; }
        dl->AddText(font, S(13.f), ImVec2(qrMin.x + S(20.f), qrMin.y + S(50.f)), RGBA(kTextDim, alpha), "二维码未生成");
    }

    ImVec2 btnAreaMin(qrMax.x + S(24.f), cardMin.y + S(16.f));
    ImU32 statusCol = RGBA(kTextSecondary, alpha); const char* statusText = "点击下方按钮生成二维码";
    if (qrStatus == 801) { statusText = "等待扫码..."; statusCol = RGBA(kGlassBorderAcc, alpha); }
    else if (qrStatus == 802) { statusText = "已扫码，请确认登录"; statusCol = RGBA(kGlassBorderAcc, alpha); }
    else if (qrStatus == 803) { statusText = "登录成功！"; statusCol = IM_COL32(16, 185, 129, static_cast<int>(255 * alpha)); }
    else if (qrStatus == 800) { statusText = "二维码已过期，请重新生成"; statusCol = IM_COL32(239, 68, 68, static_cast<int>(255 * alpha)); }
    dl->AddText(font, S(14.f), btnAreaMin, statusCol, statusText);

    // QR 按钮默认位置（有错误时下移）
    ImVec2 btnMin(btnAreaMin.x, btnAreaMin.y + S(32.f));
    ImVec2 btnMax(btnMin.x + S(140.f), btnMin.y + S(32.f));

    if (!lastError.empty()) {
        const char* friendly = lastError.c_str();
        bool isConnErr = IsConnectionError(lastError);
        if (isConnErr) friendly = "无法连接 API 服务\n请确保 api-enhanced 已安装并启动";

        float errY = btnAreaMin.y + S(24.f);
        const char* lineStart = friendly;
        while (*lineStart) {
            const char* nl = strchr(lineStart, '\n');
            std::string line = nl ? std::string(lineStart, nl) : std::string(lineStart);
            dl->AddText(font, S(12.f), ImVec2(btnAreaMin.x, errY), IM_COL32(239, 68, 68, static_cast<int>(255 * alpha)), line.c_str());
            errY += S(16.f);
            if (!nl) break;
            lineStart = nl + 1;
        }
        if (isConnErr) {
            ImVec2 retryMin(btnAreaMin.x, errY + S(8.f));
            ImVec2 retryMax(retryMin.x + S(100.f), retryMin.y + S(26.f));
            bool retryHov = ImGui::IsMouseHoveringRect(retryMin, retryMax);
            DrawGlass(dl, retryMin, retryMax, kRadiusSm, alpha * (retryHov ? 1.2f : 1.f), true);
            const char* retryLabel = "重试启动";
            float retryW = TextW(font, S(13.f), retryLabel);
            dl->AddText(font, S(13.f), ImVec2(retryMin.x + (S(100.f) - retryW) * 0.5f, retryMin.y + S(6.f)), RGBA(kTextPrimary, alpha), retryLabel);
            if (retryHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.lastError.clear(); g_state.apiUnreachable = false; g_state.apiFailCount = 0; }
                asyncQueryErr("NE_API_START", [](const std::string& body) {
                    JsonView v{body};
                    if (v.boolean("ok")) {
                        asyncQueryErr("NE_QR_START", [](const std::string&) {
                            asyncQueryErr("NE_QR_IMAGE", [](const std::string& imgBody) {
                                std::lock_guard<std::mutex> lock(g_state.mutex);
                                g_state.qrImageB64 = JsonView{imgBody}.str("image_b64");
                                g_qrDirty.store(true);
                            });
                        });
                    } else { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.lastError = v.str("error"); }
                });
            }
            errY = retryMax.y;
        }
        btnMin.y = errY + S(16.f);
        btnMax.y = btnMin.y + S(32.f);
    }

    const char* btnLabel = qrB64.empty() ? "生成二维码" : "刷新二维码";
    float btnTextW = TextW(font, S(14.f), btnLabel);
    bool btnHov = ImGui::IsMouseHoveringRect(btnMin, btnMax);
    DrawGlass(dl, btnMin, btnMax, kRadiusSm, alpha * (btnHov ? 1.3f : 1.f), true);
    dl->AddText(font, S(14.f), ImVec2(btnMin.x + (S(140.f) - btnTextW) * 0.5f, btnMin.y + S(9.f)), RGBA(kTextPrimary, alpha), btnLabel);
    if (btnHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (g_qrCover.tex != 0) { glDeleteTextures(1, &g_qrCover.tex); g_qrCover = CoverTexture{}; }
        { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.qrImageB64.clear(); g_state.lastError.clear(); }
        asyncQueryErr("NE_QR_START", [](const std::string& body) {
            asyncQueryErr("NE_QR_IMAGE", [](const std::string& imgBody) {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                g_state.qrImageB64 = JsonView{imgBody}.str("image_b64");
                g_qrDirty.store(true);
            });
        }, [](const std::string& err) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.qrStatus = 0; });
    }
}

void DrawPlayerBar(ImVec2 min, ImVec2 max, float alpha, float dt) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    float barH = max.y - min.y;
    DrawGlass(dl, min, max, kRadiusMd, alpha, false);

    std::string coverUrl, songName, artist, modeName; long long posMs = 0, durMs = 0; bool playing = false, paused = false; int volume = 80;
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        coverUrl = g_state.currentCover; songName = g_state.currentName; artist = g_state.currentArtist;
        posMs = g_state.positionMs; durMs = g_state.durationMs; playing = g_state.playing;
        paused = g_state.paused; volume = g_state.volume; modeName = g_state.modeName;
    }

    static bool s_draggingSeek = false;
    static long long s_dragSeekMs = 0;
    long long displayPosMs = posMs;
    if (s_draggingSeek) displayPosMs = s_dragSeekMs;

    ImVec2 coverMin(min.x + S(16.f), min.y + (barH - S(48.f)) * 0.5f);
    if (!coverUrl.empty()) CoverRequest(coverUrl);
    CoverTexture cover = CoverGet(coverUrl);
    if (cover.valid()) {
        dl->AddImage((ImTextureID)(intptr_t)cover.tex, coverMin,
                     ImVec2(coverMin.x + S(48.f), coverMin.y + S(48.f)));
    } else {
        dl->AddRectFilled(coverMin, ImVec2(coverMin.x + S(48.f), coverMin.y + S(48.f)),
                          IM_COL32(255, 255, 255, (int)(20 * alpha)), kRadiusSm);
    }

    float btnSize = S(32.f), btnGap = S(12.f);
    float controlsTotalW = (btnSize * 4) + (btnGap * 4) + S(100.f);
    float textX = coverMin.x + S(48.f) + S(16.f);
    float textW = max.x - textX - controlsTotalW - S(20.f);
    if (textW < S(50.f)) textW = S(50.f);

    DrawTextEllipsis(dl, font, S(15.f), ImVec2(textX, coverMin.y + S(4.f)), textW, RGBA(kTextPrimary, alpha), songName.c_str());
    DrawTextEllipsis(dl, font, S(12.f), ImVec2(textX, coverMin.y + S(24.f)), textW, RGBA(kTextDim, alpha), artist.c_str());

    // ── 进度条（GlassSlider 风格：圆角轨道 + 圆形抓手） ──
    float pBarY = coverMin.y + S(44.f);
    float pRatio = durMs > 0 ? ClampF((float)displayPosMs / durMs, 0.f, 1.f) : 0.f;
    ImVec2 seekMin(textX, pBarY - S(8.f)), seekMax(textX + textW, pBarY + S(8.f));
    bool seekHov = ImGui::IsMouseHoveringRect(seekMin, seekMax);

    const float pBarH = S(5.f);
    const float pBarCY = pBarY + S(2.f);
    const ImU32 seekFillCol = IM_COL32(kAccR, kAccG, kAccB, static_cast<int>(230 * alpha));
    const ImU32 seekTrackCol = IM_COL32(255, 255, 255, static_cast<int>(30 * alpha));
    dl->AddRectFilled(ImVec2(textX, pBarCY - pBarH * 0.5f), ImVec2(textX + textW, pBarCY + pBarH * 0.5f),
                      seekTrackCol, pBarH * 0.5f);
    if (textX + textW * pRatio > textX) {
        dl->AddRectFilled(ImVec2(textX, pBarCY - pBarH * 0.5f), ImVec2(textX + textW * pRatio, pBarCY + pBarH * 0.5f),
                          seekFillCol, pBarH * 0.5f);
    }
    if (seekHov || s_draggingSeek) {
        const float knobR = S(7.f);
        dl->AddCircleFilled(ImVec2(textX + textW * pRatio, pBarCY), knobR, IM_COL32(255, 255, 255, static_cast<int>(245 * alpha)));
        dl->AddCircle(ImVec2(textX + textW * pRatio, pBarCY), knobR, seekFillCol, 0, S(2.f));
    }
    dl->AddText(font, S(11.f), ImVec2(textX + textW + S(10.f), pBarY - S(4.f)), RGBA(kTextDim, alpha),
                (formatTime(displayPosMs) + " / " + formatTime(durMs)).c_str());

    if (durMs > 0 && ((seekHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
        (s_draggingSeek && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
        s_draggingSeek = true;
        s_dragSeekMs = static_cast<long long>(ClampF((ImGui::GetMousePos().x - textX) / textW, 0.f, 1.f) * durMs);
    }
    if (s_draggingSeek && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        PipeSendCommandAsync("NE_PLAY_SEEK:" + std::to_string(s_dragSeekMs));
        s_draggingSeek = false;
    }

    // ── 控制按钮（IsMouseHoveringRect 方式，兼容全屏窗口） ──
    float btnY = min.y + (barH - btnSize) * 0.5f;
    float curX = max.x - S(16.f) - btnSize;

    auto doBtnLogic = [&](ImVec2 bMin, ImVec2 bMax, float alphaMul = 1.0f) -> bool {
        bool hov = ImGui::IsMouseHoveringRect(bMin, bMax);
        DrawGlass(dl, bMin, bMax, btnSize * 0.5f, alpha * (hov ? 1.2f : 0.8f) * alphaMul, hov);
        return hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    };

    // 模式切换文字按钮
    ImVec2 modeMin(curX, btnY); ImVec2 modeMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(modeMin, modeMax)) PipeSendCommandAsync("NE_PLAY_TOGGLE_MODE");
    dl->AddText(font, S(11.f), ImVec2(curX + (btnSize - TextW(font, S(11.f), modeName.c_str())) * 0.5f, btnY + S(9.f)), RGBA(kTextPrimary, alpha), modeName.c_str());
    curX -= btnSize + btnGap;

    // 下一首
    ImVec2 nxtMin(curX, btnY); ImVec2 nxtMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(nxtMin, nxtMax)) contextStepSong(1);
    DrawSkipIcon(dl, ImVec2(curX + btnSize*0.5f, btnY + btnSize*0.5f), S(12.f), RGBA(kTextPrimary, alpha), false);
    curX -= btnSize + btnGap;

    // 播放/暂停
    ImVec2 pMin(curX, btnY); ImVec2 pMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(pMin, pMax, 1.4f)) PipeSendCommandAsync(paused ? "NE_PLAY_RESUME" : (playing ? "NE_PLAY_PAUSE" : ""));
    if (playing && !paused) {
        DrawPauseIcon(dl, ImVec2(curX + btnSize*0.5f, btnY + btnSize*0.5f), S(14.f), RGBA(kTextPrimary, alpha));
    } else {
        DrawPlayIcon(dl, ImVec2(curX + btnSize*0.54f, btnY + btnSize*0.5f), S(14.f), RGBA(kTextPrimary, alpha));
    }
    curX -= btnSize + btnGap;

    // 上一首
    ImVec2 prvMin(curX, btnY); ImVec2 prvMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(prvMin, prvMax)) contextStepSong(-1);
    DrawSkipIcon(dl, ImVec2(curX + btnSize*0.5f, btnY + btnSize*0.5f), S(12.f), RGBA(kTextPrimary, alpha), true);

    // ── 音量控制（GlassSlider 风格） ──
    curX -= S(90.f) + btnGap;
    dl->AddText(font, S(12.f), ImVec2(curX, btnY + S(8.f)), RGBA(kTextDim, alpha), "VOL");
    float volX = curX + S(30.f);
    float volW = S(60.f);
    float volCY = btnY + S(16.f);
    ImVec2 volMin(volX - S(4.f), btnY), volMax(volX + volW + S(4.f), btnY + btnSize);
    bool volHov = ImGui::IsMouseHoveringRect(volMin, volMax);

    const float volBarH = S(5.f);
    const ImU32 volFillCol = IM_COL32(kAccR, kAccG, kAccB, static_cast<int>(230 * alpha));
    const ImU32 volTrackCol = IM_COL32(255, 255, 255, static_cast<int>(30 * alpha));
    dl->AddRectFilled(ImVec2(volX, volCY - volBarH * 0.5f), ImVec2(volX + volW, volCY + volBarH * 0.5f),
                      volTrackCol, volBarH * 0.5f);
    if (volX + volW * volume / 100.f > volX) {
        dl->AddRectFilled(ImVec2(volX, volCY - volBarH * 0.5f), ImVec2(volX + volW * volume / 100.f, volCY + volBarH * 0.5f),
                          volFillCol, volBarH * 0.5f);
    }
    if (volHov) {
        const float knobR = S(7.f);
        dl->AddCircleFilled(ImVec2(volX + volW * volume / 100.f, volCY), knobR, IM_COL32(255, 255, 255, static_cast<int>(245 * alpha)));
        dl->AddCircle(ImVec2(volX + volW * volume / 100.f, volCY), knobR, volFillCol, 0, S(2.f));
    }

    if ((volHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && volHov)) {
        static float s_lastVolSend = 0.f;
        const float now = ImGui::GetTime();
        if (now - s_lastVolSend > 0.08f) {
            s_lastVolSend = now;
            PipeSendCommandAsync("NE_SET_VOLUME:" + std::to_string((int)(ClampF((ImGui::GetMousePos().x - volX) / volW, 0.f, 1.f) * 100)));
        }
    }
}

// ── 核心改造：原生支持滚动的交互式歌单 ──
void DrawSongList(const std::vector<SongItem>& songs, ImVec2 min, ImVec2 max, float alpha, float dt) {
    const float childH = std::max(1.f, max.y - min.y);
    ImGui::SetCursorScreenPos(min);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##song_list_child", ImVec2(max.x - min.x, childH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = GetFont();
    long long currentId = 0;
    { std::lock_guard<std::mutex> lock(g_state.mutex); currentId = g_state.currentSongId; }

    for (size_t idx = 0; idx < songs.size(); ++idx) {
        const SongItem& s = songs[idx];

        ImVec2 itemMin = ImGui::GetCursorScreenPos();
        ImVec2 itemMax = ImVec2(itemMin.x + (max.x - min.x), itemMin.y + S(54.f));

        ImGui::Dummy(ImVec2(itemMax.x - itemMin.x, S(54.f)));
        bool hov = ImGui::IsItemHovered();
        bool isCur = (s.id == currentId);

        // 视觉绘制
        if (hov || isCur) {
            dl->AddRectFilled(ImVec2(itemMin.x, itemMin.y + S(2.f)),
                              ImVec2(itemMax.x, itemMax.y - S(2.f)),
                              IM_COL32(kAccR, kAccG, kAccB, (int)((isCur ? 45 : 18) * alpha)), kRadiusSm);
        }

        char idxBuf[8]; snprintf(idxBuf, sizeof(idxBuf), "%02zu", idx + 1);
        dl->AddText(font, S(12.f), ImVec2(itemMin.x + S(10.f), itemMin.y + S(19.f)), RGBA(kTextDim, alpha), idxBuf);

        // 封面缩略图
        const float coverSize = S(38.f);
        const ImVec2 coverMin(itemMin.x + S(34.f), itemMin.y + S(8.f));
        const ImVec2 coverMax(coverMin.x + coverSize, coverMin.y + coverSize);
        if (!s.cover.empty()) CoverRequest(s.cover);
        CoverTexture cover = CoverGet(s.cover);
        if (cover.valid()) {
            dl->AddImage((ImTextureID)(intptr_t)cover.tex, coverMin, coverMax);
        } else {
            dl->AddRectFilled(coverMin, coverMax, IM_COL32(255, 255, 255, (int)(20 * alpha)), kRadiusSm);
        }

        // 歌名/歌手（右移给封面腾出空间）
        const float textX = itemMin.x + S(82.f);
        const float textMaxW = (max.x - min.x) - S(82.f) - S(60.f);
        DrawTextEllipsis(dl, font, S(14.f), ImVec2(textX, itemMin.y + S(10.f)), textMaxW, isCur ? RGBA(kGlassBorderAcc, alpha) : RGBA(kTextPrimary, alpha), s.name.c_str());
        DrawTextEllipsis(dl, font, S(11.f), ImVec2(textX, itemMin.y + S(30.f)), textMaxW, RGBA(kTextDim, alpha), s.artist.c_str());
        dl->AddText(font, S(12.f), ImVec2(itemMax.x - TextW(font, S(12.f), formatTime(s.durationMs).c_str()) - S(14.f), itemMin.y + S(19.f)), RGBA(kTextDim, alpha), formatTime(s.durationMs).c_str());

        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) playQueueAt(songs, idx);
        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) PipeSendCommandAsync("NE_LIKE:" + std::to_string(s.id) + ":true");
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void DrawPlaylistList(const std::vector<PlaylistItem>& pls, ImVec2 min, ImVec2 max, float alpha) {
    const float childH = std::max(1.f, max.y - min.y);
    ImGui::SetCursorScreenPos(min);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##pl_list_child", ImVec2(max.x - min.x, childH), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = GetFont();
    const float rowW = max.x - min.x;

    for (size_t i = 0; i < pls.size(); ++i) {
        const float rowH = S(54.f);
        ImVec2 rMin = ImGui::GetCursorScreenPos();
        ImVec2 rMax(rMin.x + rowW, rMin.y + rowH);

        ImGui::Dummy(ImVec2(rowW, rowH));
        bool plHov = ImGui::IsItemHovered();

        if (plHov) {
            dl->AddRectFilled(ImVec2(rMin.x, rMin.y + S(2.f)), ImVec2(rMax.x, rMax.y - S(2.f)),
                              IM_COL32(255, 255, 255, (int)(15 * alpha)), kRadiusSm);
        }

        if (!pls[i].cover.empty()) CoverRequest(pls[i].cover);
        CoverTexture ct = CoverGet(pls[i].cover);
        const ImVec2 coverMin(rMin.x + S(8.f), rMin.y + S(6.f));
        const ImVec2 coverMax(rMin.x + S(50.f), rMin.y + S(48.f));
        if (ct.valid()) {
            dl->AddImage((ImTextureID)(intptr_t)ct.tex, coverMin, coverMax);
        } else {
            dl->AddRectFilled(coverMin, coverMax, IM_COL32(255, 255, 255, (int)(20 * alpha)), kRadiusSm);
        }

        DrawTextEllipsis(dl, font, S(14.f), ImVec2(rMin.x + S(64.f), rMin.y + S(10.f)), S(300.f),
                         RGBA(kTextPrimary, alpha), pls[i].name.c_str());
        DrawTextEllipsis(dl, font, S(11.f), ImVec2(rMin.x + S(64.f), rMin.y + S(30.f)), S(300.f),
                         RGBA(kTextDim, alpha),
                         (pls[i].creator + " · " + std::to_string(pls[i].trackCount) + " 首").c_str());

        if (plHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const long long pid = pls[i].id;
            const std::string pname = pls[i].name;
            {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                g_state.selectedPlaylistId = pid;
                g_state.selectedPlaylistName = pname;
            }
            asyncQuery("NE_PLAYLIST_TRACKS:" + std::to_string(pid), [pid](const std::string& body) {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                if (g_state.selectedPlaylistId == pid) {
                    g_state.playlistTracks = parseSongsArray(body, "songs");
                }
            });
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void DrawTabs(ImVec2 min, ImVec2 max, float alpha) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    const char* tabs[] = {"搜索", "我的歌单", "每日推荐", "播放记录"};
    float tabW = (max.x - min.x) / 4;
    int active = 0; { std::lock_guard<std::mutex> lock(g_state.mutex); active = g_state.activeTab; }
    for (int i = 0; i < 4; ++i) {
        ImVec2 tMin(min.x + i * tabW, min.y), tMax(tMin.x + tabW, max.y);
        bool act = (active == i);
        bool hov = ImGui::IsMouseHoveringRect(tMin, tMax);
        if (act) dl->AddRectFilled(tMin, tMax, IM_COL32(kAccR, kAccG, kAccB, (int)(40 * alpha)), kRadiusSm);
        else if (hov) dl->AddRectFilled(tMin, tMax, IM_COL32(255, 255, 255, (int)(15 * alpha)), kRadiusSm);
        dl->AddText(font, S(13.f), ImVec2(tMin.x + (tabW - TextW(font, S(13.f), tabs[i])) * 0.5f, tMin.y + S(10.f)), act ? RGBA(kTextPrimary, alpha) : RGBA(kTextSecondary, alpha), tabs[i]);
        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            std::lock_guard<std::mutex> lock(g_state.mutex); g_state.activeTab = i;
            if (i == 1) asyncQuery("NE_MY_PLAYLISTS", [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.playlists = parsePlaylistsArray(body); });
            else if (i == 2) asyncQuery("NE_DAILY_RECOMMEND", [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.dailySongs = parseSongsArray(body, "songs"); });
            else if (i == 3) asyncQuery("NE_HISTORY:" + std::to_string(g_state.historyType), [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.historySongs = parseSongsArray(body, "songs"); });
        }
    }
}

}  // namespace

void MusicPanelRender(ImVec2 contentMin, ImVec2 contentMax, float alpha, float dt) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    float pollTimer; { std::lock_guard<std::mutex> lock(g_state.mutex); pollTimer = (g_state.statusPollTimer += dt); }
    if (pollTimer > 2.f) { { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.statusPollTimer = 0.f; } refreshPlayStatus(); refreshLyrics(); refreshLoginStatus(); }

    bool loggedIn = false; { std::lock_guard<std::mutex> lock(g_state.mutex); loggedIn = g_state.loggedIn; }
    static bool s_firstLoginCheck = false; if (!s_firstLoginCheck) { s_firstLoginCheck = true; refreshLoginStatus(); }

    // 首次进入已登录面板时自动拉取歌单，避免「我的歌单」空白
    bool needsFetch = false;
    { std::lock_guard<std::mutex> lock(g_state.mutex); needsFetch = g_state.needsInitialFetch; }
    if (loggedIn && needsFetch) {
        { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.needsInitialFetch = false; }
        asyncQuery("NE_MY_PLAYLISTS", [](const std::string& body) {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.playlists = parsePlaylistsArray(body);
        });
    }

    dl->AddText(font, S(20.f), ImVec2(contentMin.x + S(24.f), contentMin.y + S(22.f)), RGBA(kTextPrimary, alpha), "音乐");

    if (!loggedIn) {
        DrawLoginSection(ImVec2(contentMin.x + S(24.f), contentMin.y + S(76.f)), ImVec2(contentMax.x - S(24.f), contentMax.y - S(24.f)), alpha, dt);
        return;
    }

    std::string nick; std::string signinMsg; float signinTimer = 0.f;
    { std::lock_guard<std::mutex> lock(g_state.mutex);
      nick = g_state.nickname;
      signinMsg = g_state.signinMessage;
      signinTimer = g_state.signinMsgTimer;
      if (signinTimer > 0.f) g_state.signinMsgTimer = std::max(0.f, g_state.signinMsgTimer - dt);
    }
    if (!nick.empty()) {
        // 退出登录按钮（最右侧）
        const float logoutBtnW = S(56.f);
        const float signinBtnW = S(70.f);
        const float topBtnGap = S(8.f);
        ImVec2 logoutMin(contentMax.x - S(24.f) - logoutBtnW, contentMin.y + S(18.f));
        ImVec2 logoutMax(logoutMin.x + logoutBtnW, logoutMin.y + S(30.f));
        bool logoutHov = ImGui::IsMouseHoveringRect(logoutMin, logoutMax);
        DrawGlass(dl, logoutMin, logoutMax, kRadiusSm,
                  alpha * (logoutHov ? 1.3f : 1.f), logoutHov);
        const char* logoutLabel = "退出";
        float logoutW = TextW(font, S(12.f), logoutLabel);
        dl->AddText(font, S(12.f),
                    ImVec2(logoutMin.x + (logoutBtnW - logoutW) * 0.5f, logoutMin.y + S(8.f)),
                    IM_COL32(239, 68, 68, static_cast<int>(255 * alpha)), logoutLabel);
        if (logoutHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            // 立即清除本地状态，不等 API 响应（NE_LOGOUT 是本地操作）
            {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                g_state.loggedIn = false;
                g_state.nickname.clear();
                g_state.avatar.clear();
                g_state.userId = 0;
                g_state.playlists.clear();
                g_state.playlistTracks.clear();
                g_state.dailySongs.clear();
                g_state.historySongs.clear();
                g_state.searchResults.clear();
                g_state.selectedPlaylistId = 0;
                g_state.selectedPlaylistName.clear();
                g_state.currentSongId = 0;
                g_state.currentName.clear();
                g_state.currentArtist.clear();
                g_state.currentCover.clear();
                g_state.lyricCurrent.clear();
                g_state.lyricNext.clear();
                g_state.hasLyrics = false;
                g_state.signinMessage.clear();
                g_state.signinMsgTimer = 0.f;
                g_state.apiUnreachable = false;
                g_state.needsInitialFetch = true;
                g_state.activeTab = 0;
                g_state.searchBuf[0] = '\0';
            }
            PipeSendCommandAsync("NE_LOGOUT");
            PipeSendCommandAsync("NE_PLAY_STOP");
        }

        // 每日签到按钮（退出按钮左侧）
        ImVec2 sMin(logoutMin.x - topBtnGap - signinBtnW, contentMin.y + S(18.f));
        ImVec2 sMax(sMin.x + signinBtnW, sMin.y + S(30.f));
        std::string greet = "Hi, " + nick;
        float greetW = TextW(font, S(13.f), greet.c_str());
        dl->AddText(font, S(13.f),
                    ImVec2(sMin.x - topBtnGap - greetW, contentMin.y + S(24.f)),
                    RGBA(kTextSecondary, alpha), greet.c_str());
        const char* signinLabel = "每日签到";
        float signinW = TextW(font, S(12.f), signinLabel);
        bool signinHov = ImGui::IsMouseHoveringRect(sMin, sMax);
        DrawGlass(dl, sMin, sMax, kRadiusSm, alpha * (signinHov ? 1.3f : 1.f), true);
        dl->AddText(font, S(12.f), ImVec2(sMin.x + (signinBtnW - signinW) * 0.5f, sMin.y + S(8.f)), RGBA(kTextPrimary, alpha), signinLabel);
        if (signinHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            { std::lock_guard<std::mutex> lock(g_state.mutex);
              g_state.signinMessage.clear();
              g_state.signinMsgTimer = 0.f;
            }
            asyncQueryErr("NE_SIGNIN",
                [](const std::string&) {
                    std::lock_guard<std::mutex> lock(g_state.mutex);
                    g_state.signinMessage = "签到成功";
                    g_state.signinMsgTimer = 4.f;
                },
                [](const std::string& err) {
                    std::lock_guard<std::mutex> lock(g_state.mutex);
                    g_state.signinMessage = FriendlyError(err);
                    g_state.signinMsgTimer = 4.f;
                });
        }
        if (signinTimer > 0.f && !signinMsg.empty()) {
            bool isErr = signinMsg.find("失败") != std::string::npos ||
                         signinMsg.find("ERR") != std::string::npos ||
                         signinMsg.find("未连接") != std::string::npos;
            ImU32 msgCol = isErr
                ? IM_COL32(239, 68, 68, static_cast<int>(255 * alpha))
                : IM_COL32(16, 185, 129, static_cast<int>(255 * alpha));
            float msgW = TextW(font, S(11.f), signinMsg.c_str());
            dl->AddText(font, S(11.f), ImVec2(sMin.x + (S(70.f) - msgW) * 0.5f, sMax.y + S(4.f)), msgCol, signinMsg.c_str());
        }
    }

    dl->AddLine(ImVec2(contentMin.x + S(24.f), contentMin.y + S(64.f)), ImVec2(contentMax.x - S(24.f), contentMin.y + S(64.f)), RGBA(kGlassBorder, alpha * 0.2f), 1.f);

    ImVec2 tabsMin(contentMin.x + S(24.f), contentMin.y + S(76.f));
    ImVec2 tabsMax(contentMax.x - S(24.f), contentMin.y + S(112.f));
    DrawTabs(tabsMin, tabsMax, alpha);

    ImVec2 listMin(contentMin.x + S(24.f), tabsMax.y + S(16.f)); 
    
    std::string lcur, lnext; bool hasLrc = false;
    { std::lock_guard<std::mutex> lock(g_state.mutex); lcur = g_state.lyricCurrent; lnext = g_state.lyricNext; hasLrc = g_state.hasLyrics; }

    const float playerBarTop = contentMax.y - S(76.f);
    float listBottom = playerBarTop - S(12.f);
    if (hasLrc && !lcur.empty()) {
        listBottom = std::min(listBottom, contentMax.y - S(116.f) - S(12.f));
    }
    ImVec2 listMax(contentMax.x - S(24.f), listBottom);

    int activeTab = 0; bool apiDown = false;
    { std::lock_guard<std::mutex> lock(g_state.mutex);
      activeTab = g_state.activeTab;
      apiDown = g_state.apiUnreachable;
      g_state.apiCheckTimer += dt;
    }
    // API 断开时每 5 秒自动重试启动
    if (apiDown) {
        float apiCheckTimer = 0.f;
        { std::lock_guard<std::mutex> lock(g_state.mutex); apiCheckTimer = g_state.apiCheckTimer; }
        if (apiCheckTimer > 5.f) {
            { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.apiCheckTimer = 0.f; }
            asyncQueryErr("NE_API_START", [](const std::string&) {
                std::lock_guard<std::mutex> lock(g_state.mutex);
                g_state.apiUnreachable = false;
            });
        }
    }

    const ImVec2 playerMin(contentMin.x + S(24.f), contentMax.y - S(76.f));
    const ImVec2 playerMax(contentMax.x - S(24.f), contentMax.y - S(16.f));

    if (apiDown) {
        DrawApiUnreachableBanner(listMin, listMax, alpha);
    } else if (activeTab == 0) {
        ImVec2 sMin(listMin.x, listMin.y); ImVec2 sMax(listMax.x, sMin.y + S(36.f));
        DrawGlass(dl, sMin, sMax, kRadiusSm, alpha, false);
        ImGui::SetCursorScreenPos(ImVec2(sMin.x + S(10.f), sMin.y + S(5.f))); ImGui::PushItemWidth(sMax.x - sMin.x - S(20.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.97f, 0.98f, alpha));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.43f, 0.43f, 0.45f, alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(6.f), S(8.f))); ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        ImGui::InputTextWithHint("##ne_search", "在此键入关键词搜索歌曲...", g_state.searchBuf, sizeof(g_state.searchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(3); ImGui::PopItemWidth();
        if (ImGui::IsItemDeactivatedAfterEdit() && g_state.searchBuf[0]) {
            asyncQuery("NE_SEARCH:" + std::string(g_state.searchBuf), [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.searchResults = parseSongsArray(body, "songs"); });
        }
        listMin.y = sMax.y + S(16.f);
        std::vector<SongItem> songs; { std::lock_guard<std::mutex> lock(g_state.mutex); songs = g_state.searchResults; }
        if (songs.empty() && !g_state.searchBuf[0]) {
            DrawEmptyHint(listMin, listMax, alpha, "输入关键词后回车搜索");
        } else if (songs.empty()) {
            DrawEmptyHint(listMin, listMax, alpha, "未找到匹配歌曲");
        } else {
            DrawSongList(songs, listMin, listMax, alpha, dt);
        }
    } else if (activeTab == 1) {
        std::vector<PlaylistItem> pls; std::vector<SongItem> tracks; long long selId = 0; std::string selName;
        { std::lock_guard<std::mutex> lock(g_state.mutex); pls = g_state.playlists; tracks = g_state.playlistTracks; selId = g_state.selectedPlaylistId; selName = g_state.selectedPlaylistName; }
        if (selId == 0) {
            if (pls.empty()) {
                DrawEmptyHint(listMin, listMax, alpha, "暂无歌单，点击上方标签刷新");
            } else {
                DrawPlaylistList(pls, listMin, listMax, alpha);
            }
        } else {
            ImVec2 backMax(listMin.x + S(74.f), listMin.y + S(30.f));
            bool backHov = ImGui::IsMouseHoveringRect(listMin, backMax);
            DrawGlass(dl, listMin, backMax, kRadiusSm, alpha * (backHov ? 1.3f : 1.f), true);
            dl->AddText(font, S(12.f), ImVec2(listMin.x + S(14.f), listMin.y + S(8.f)), RGBA(kTextPrimary, alpha), "< 返回");
            if (backHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.selectedPlaylistId = 0; g_state.playlistTracks.clear(); }
            dl->AddText(font, S(15.f), ImVec2(listMin.x + S(88.f), listMin.y + S(6.f)), RGBA(kTextPrimary, alpha), selName.c_str());
            if (tracks.empty()) {
                DrawEmptyHint(ImVec2(listMin.x, listMin.y + S(42.f)), listMax, alpha, "歌单为空或加载中");
            } else {
                DrawSongList(tracks, ImVec2(listMin.x, listMin.y + S(42.f)), listMax, alpha, dt);
            }
        }
    } else if (activeTab == 2) {
        std::vector<SongItem> songs; { std::lock_guard<std::mutex> lock(g_state.mutex); songs = g_state.dailySongs; }
        if (songs.empty()) {
            DrawEmptyHint(listMin, listMax, alpha, "点击上方标签获取每日推荐");
        } else {
            DrawSongList(songs, listMin, listMax, alpha, dt);
        }
    } else if (activeTab == 3) {
        ImVec2 weekMax(listMin.x + S(80.f), listMin.y + S(28.f));
        bool weekHov = ImGui::IsMouseHoveringRect(listMin, weekMax);
        DrawGlass(dl, listMin, weekMax, kRadiusSm, alpha * (weekHov ? 1.2f : 1.f), g_state.historyType == 1);
        dl->AddText(font, S(12.f), ImVec2(listMin.x + (S(80.f) - TextW(font, S(12.f), "本周")) * 0.5f, listMin.y + S(8.f)), RGBA(kTextPrimary, alpha), "本周");
        if (weekHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && g_state.historyType != 1) {
            { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.historyType = 1; g_state.historySongs.clear(); }
            asyncQuery("NE_HISTORY:1", [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.historySongs = parseSongsArray(body, "songs"); });
        }
        
        ImVec2 t2Min(listMin.x + S(90.f), listMin.y); ImVec2 t2Max(t2Min.x + S(80.f), t2Min.y + S(28.f));
        bool allHov = ImGui::IsMouseHoveringRect(t2Min, t2Max);
        DrawGlass(dl, t2Min, t2Max, kRadiusSm, alpha * (allHov ? 1.2f : 1.f), g_state.historyType == 0);
        dl->AddText(font, S(12.f), ImVec2(t2Min.x + (S(80.f) - TextW(font, S(12.f), "全部")) * 0.5f, t2Min.y + S(8.f)), RGBA(kTextPrimary, alpha), "全部");
        if (allHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && g_state.historyType != 0) {
            { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.historyType = 0; g_state.historySongs.clear(); }
            asyncQuery("NE_HISTORY:0", [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.historySongs = parseSongsArray(body, "songs"); });
        }
        
        std::vector<SongItem> songs; { std::lock_guard<std::mutex> lock(g_state.mutex); songs = g_state.historySongs; }
        if (songs.empty()) {
            DrawEmptyHint(ImVec2(listMin.x, listMin.y + S(40.f)), listMax, alpha, "暂无播放记录");
        } else {
            DrawSongList(songs, ImVec2(listMin.x, listMin.y + S(40.f)), listMax, alpha, dt);
        }
    }

    if (hasLrc && !lcur.empty()) {
        float lY = contentMax.y - S(116.f);
        DrawTextEllipsis(dl, font, S(14.f), ImVec2(contentMin.x + S(24.f), lY), contentMax.x - contentMin.x - S(48.f), RGBA(kTextPrimary, alpha), lcur.c_str());
        DrawTextEllipsis(dl, font, S(12.f), ImVec2(contentMin.x + S(24.f), lY + S(20.f)), contentMax.x - contentMin.x - S(48.f), RGBA(kTextDim, alpha), lnext.c_str());
    }

    // 列表交互优先于底部播放条（listBottom 已保证不重叠）
    DrawPlayerBar(playerMin, playerMax, alpha, dt);
}

void MusicPanelTick() { CoverProcessPending(); }

}  // namespace myiui::ui::music