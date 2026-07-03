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
struct SongItem { long id = 0; std::string name, artist, album, cover; long durationMs = 0; };
struct PlaylistItem { long id = 0; std::string name, cover; int trackCount = 0; std::string creator; };
struct CommentItem { long id = 0; std::string user, content; int likedCount = 0; };

struct MusicState {
    std::mutex mutex;
    bool loggedIn = false; std::string nickname, avatar; long userId = 0;
    bool qrPending = false; int qrStatus = 0; std::string qrImageB64;
    std::string lastError;
    bool playing = false, paused = false; long positionMs = 0, durationMs = 0; int volume = 80;
    std::string modeName = "顺序"; long currentSongId = 0;
    std::string currentName, currentArtist, currentCover;
    std::string lyricCurrent, lyricNext; bool hasLyrics = false;
    int activeTab = 0; 
    std::vector<SongItem> searchResults; std::vector<PlaylistItem> playlists;
    std::vector<SongItem> playlistTracks; long selectedPlaylistId = 0; std::string selectedPlaylistName;
    std::vector<SongItem> dailySongs, historySongs; int historyType = 1;
    char searchBuf[256] = {};
    float statusPollTimer = 0.f, loginPollTimer = 0.f;
};

MusicState g_state;
CoverTexture g_qrCover;
std::atomic<bool> g_qrDirty{false};

struct JsonView {
    const std::string& raw;
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
        return raw.substr(p + 1, end - p - 1);
    }
    long num(const std::string& key, long def = 0) const {
        std::string needle = "\"" + key + "\"";
        size_t p = raw.find(needle);
        if (p == std::string::npos) return def;
        p = raw.find(':', p + needle.size());
        if (p == std::string::npos) return def;
        p++;
        while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) p++;
        try { return std::stol(raw.substr(p)); } catch (...) { return def; }
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

void asyncQuery(const std::string& cmd, std::function<void(const std::string&)> handler) {
    std::thread([cmd, handler]() {
        PipeQueryResult r = PipeQueryJson(cmd, 6000);
        if (r.ok) {
            if (handler) handler(r.body);
        } else {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.lastError = r.error.empty() ? "请求失败" : r.error;
        }
    }).detach();
}

void asyncQueryErr(const std::string& cmd, std::function<void(const std::string&)> onOk,
                   std::function<void(const std::string&)> onErr = nullptr) {
    std::thread([cmd, onOk, onErr]() {
        PipeQueryResult r = PipeQueryJson(cmd, 6000);
        if (r.ok) { if (onOk) onOk(r.body); }
        else {
            std::string err = r.error.empty() ? "请求失败" : r.error;
            { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.lastError = err; }
            if (onErr) onErr(err);
        }
    }).detach();
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
        long sid = v.num("song_id");
        if (sid != g_state.currentSongId) {
            g_state.currentSongId = sid; g_state.currentName = v.str("song_name");
            g_state.currentArtist = v.str("song_artist"); g_state.currentCover = v.str("song_cover");
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

std::string formatTime(long ms) {
    ms = std::max(0L, ms); int total = static_cast<int>(ms / 1000);
    char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", total / 60, total % 60);
    return buf;
}

void DrawLoginSection(ImVec2 min, ImVec2 max, float alpha, float dt) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImFont* font = GetFont();
    dl->AddText(font, S(18.f), ImVec2(min.x + S(20.f), min.y + S(16.f)), RGBA(kTextPrimary, alpha), "登录网易云账号");

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

    if (!lastError.empty()) {
        const char* friendly = lastError.c_str();
        std::string errLower = lastError;
        std::transform(errLower.begin(), errLower.end(), errLower.begin(), ::tolower);
        bool isConnErr = errLower.find("connection refused") != std::string::npos || errLower.find("connect") != std::string::npos;
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
                { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.lastError.clear(); }
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
        }
    }

    ImVec2 btnMin(btnAreaMin.x, btnAreaMin.y + S(32.f));
    ImVec2 btnMax(btnMin.x + S(140.f), btnMin.y + S(32.f));
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

    std::string coverUrl, songName, artist, modeName; long posMs = 0, durMs = 0; bool playing = false, paused = false; int volume = 80;
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        coverUrl = g_state.currentCover; songName = g_state.currentName; artist = g_state.currentArtist;
        posMs = g_state.positionMs; durMs = g_state.durationMs; playing = g_state.playing;
        paused = g_state.paused; volume = g_state.volume; modeName = g_state.modeName;
    }

    ImVec2 coverMin(min.x + S(16.f), min.y + (barH - S(48.f)) * 0.5f);
    if (!coverUrl.empty()) CoverRequest(coverUrl);
    CoverTexture cover = CoverGet(coverUrl);
    if (cover.valid()) { ImGui::SetCursorScreenPos(coverMin); ImGui::Image((ImTextureID)(intptr_t)cover.tex, ImVec2(S(48.f), S(48.f))); }
    else dl->AddRectFilled(coverMin, ImVec2(coverMin.x + S(48.f), coverMin.y + S(48.f)), IM_COL32(255, 255, 255, (int)(20 * alpha)), kRadiusSm);

    float btnSize = S(32.f), btnGap = S(12.f);
    float controlsTotalW = (btnSize * 4) + (btnGap * 4) + S(100.f);
    float textX = coverMin.x + S(48.f) + S(16.f);
    float textW = max.x - textX - controlsTotalW - S(20.f);
    if (textW < S(50.f)) textW = S(50.f);

    DrawTextEllipsis(dl, font, S(15.f), ImVec2(textX, coverMin.y + S(4.f)), textW, RGBA(kTextPrimary, alpha), songName.c_str());
    DrawTextEllipsis(dl, font, S(12.f), ImVec2(textX, coverMin.y + S(24.f)), textW, RGBA(kTextDim, alpha), artist.c_str());

    // ── 进度条优化（带交互抓手） ──
    float pBarY = coverMin.y + S(44.f);
    float pRatio = durMs > 0 ? ClampF((float)posMs / durMs, 0.f, 1.f) : 0.f;
    ImVec2 seekMin(textX, pBarY - S(6.f)), seekMax(textX + textW, pBarY + S(6.f));
    bool seekHov = ImGui::IsMouseHoveringRect(seekMin, seekMax);
    
    dl->AddRectFilled(ImVec2(textX, pBarY), ImVec2(textX + textW, pBarY + S(4.f)), IM_COL32(255, 255, 255, (int)(25 * alpha)), S(2.f));
    dl->AddRectFilled(ImVec2(textX, pBarY), ImVec2(textX + textW * pRatio, pBarY + S(4.f)), IM_COL32(kAccR, kAccG, kAccB, (int)(230 * alpha)), S(2.f));
    
    // 如果悬停，显示圆点小抓手
    if (seekHov) {
        dl->AddCircleFilled(ImVec2(textX + textW * pRatio, pBarY + S(2.f)), S(5.f), IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
    }
    
    dl->AddText(font, S(11.f), ImVec2(textX + textW + S(10.f), pBarY - S(4.f)), RGBA(kTextDim, alpha), (formatTime(posMs) + " / " + formatTime(durMs)).c_str());
    
    if ((seekHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && durMs > 0) ||
        (ImGui::IsMouseDown(ImGuiMouseButton_Left) && seekHov && durMs > 0))
        PipeSendCommandAsync("NE_PLAY_SEEK:" + std::to_string((long)(ClampF((ImGui::GetMousePos().x - textX) / textW, 0.f, 1.f) * durMs)));

    // ── 矢量级控制按钮 ──
    float btnY = min.y + (barH - btnSize) * 0.5f;
    float curX = max.x - S(16.f) - btnSize;
    
    auto doBtnLogic = [&](ImVec2 bMin, ImVec2 bMax, float alphaMul = 1.0f) -> bool {
        bool hov = ImGui::IsMouseHoveringRect(bMin, bMax);
        DrawGlass(dl, bMin, bMax, btnSize*0.5f, alpha * (hov ? 1.2f : 0.8f) * alphaMul, hov);
        return hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    };

    // 模式切换文字按钮
    ImVec2 modeMin(curX, btnY); ImVec2 modeMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(modeMin, modeMax)) PipeSendCommandAsync("NE_PLAY_TOGGLE_MODE");
    dl->AddText(font, S(11.f), ImVec2(curX + (btnSize - TextW(font, S(11.f), modeName.c_str())) * 0.5f, btnY + S(9.f)), RGBA(kTextPrimary, alpha), modeName.c_str());
    curX -= btnSize + btnGap;

    // 下一首
    ImVec2 nxtMin(curX, btnY); ImVec2 nxtMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(nxtMin, nxtMax)) PipeSendCommandAsync("NE_PLAY_NEXT");
    DrawSkipIcon(dl, ImVec2(curX + btnSize*0.5f, btnY + btnSize*0.5f), S(12.f), RGBA(kTextPrimary, alpha), false);
    curX -= btnSize + btnGap;

    // 播放/暂停 (突出重点)
    ImVec2 pMin(curX, btnY); ImVec2 pMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(pMin, pMax, 1.4f)) PipeSendCommandAsync(paused ? "NE_PLAY_RESUME" : (playing ? "NE_PLAY_PAUSE" : ""));
    if (playing && !paused) {
        DrawPauseIcon(dl, ImVec2(curX + btnSize*0.5f, btnY + btnSize*0.5f), S(14.f), RGBA(kTextPrimary, alpha));
    } else {
        // 微调居中产生视觉平衡
        DrawPlayIcon(dl, ImVec2(curX + btnSize*0.54f, btnY + btnSize*0.5f), S(14.f), RGBA(kTextPrimary, alpha));
    }
    curX -= btnSize + btnGap;

    // 上一首
    ImVec2 prvMin(curX, btnY); ImVec2 prvMax(curX + btnSize, btnY + btnSize);
    if (doBtnLogic(prvMin, prvMax)) PipeSendCommandAsync("NE_PLAY_PREV");
    DrawSkipIcon(dl, ImVec2(curX + btnSize*0.5f, btnY + btnSize*0.5f), S(12.f), RGBA(kTextPrimary, alpha), true);
    
    // ── 音量控制优化 ──
    curX -= S(90.f) + btnGap;
    dl->AddText(font, S(12.f), ImVec2(curX, btnY + S(8.f)), RGBA(kTextDim, alpha), "VOL");
    float volX = curX + S(30.f);
    float volW = S(60.f);
    float volY = btnY + S(14.f);
    ImVec2 volMin(volX - S(4.f), btnY), volMax(volX + volW + S(4.f), btnY + btnSize);
    bool volHov = ImGui::IsMouseHoveringRect(volMin, volMax);
    
    dl->AddRectFilled(ImVec2(volX, volY), ImVec2(volX + volW, volY + S(4.f)), IM_COL32(255, 255, 255, (int)(25 * alpha)), S(2.f));
    dl->AddRectFilled(ImVec2(volX, volY), ImVec2(volX + volW * volume / 100.f, volY + S(4.f)), IM_COL32(kAccR, kAccG, kAccB, (int)(230 * alpha)), S(2.f));
    
    if (volHov) {
        dl->AddCircleFilled(ImVec2(volX + volW * volume / 100.f, volY + S(2.f)), S(5.f), IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
    }

    if ((volHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || 
        (ImGui::IsMouseDown(ImGuiMouseButton_Left) && volHov) ||
        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::GetMousePos().x >= volMin.x && ImGui::GetMousePos().x <= volMax.x && ImGui::GetMousePos().y >= volMin.y && ImGui::GetMousePos().y <= volMax.y)) {
        PipeSendCommandAsync("NE_SET_VOLUME:" + std::to_string((int)(ClampF((ImGui::GetMousePos().x - volX) / volW, 0.f, 1.f) * 100)));
    }
}

// ── 核心改造：原生支持滚动的交互式歌单 ──
void DrawSongList(const std::vector<SongItem>& songs, ImVec2 min, ImVec2 max, float alpha, float dt) {
    ImGui::SetCursorScreenPos(min);
    // 去除内边距和背景，建立一个透明的滚动容器
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    // 开启原生 Child，隐藏丑陋的滚动条，保留滚轮功能
    ImGui::BeginChild("##song_list_child", ImVec2(max.x - min.x, max.y - min.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    
    ImDrawList* dl = ImGui::GetWindowDrawList(); 
    ImFont* font = GetFont();
    long currentId = 0; 
    { std::lock_guard<std::mutex> lock(g_state.mutex); currentId = g_state.currentSongId; }
    
    for (size_t idx = 0; idx < songs.size(); ++idx) {
        const SongItem& s = songs[idx];
        
        ImVec2 itemMin = ImGui::GetCursorScreenPos();
        ImVec2 itemMax = ImVec2(itemMin.x + (max.x - min.x), itemMin.y + S(38.f));
        
        // 关键逻辑：使用 Dummy 占据排版空间，让 ImGui 知道可滚动区域的总高度
        ImGui::Dummy(ImVec2(itemMax.x - itemMin.x, S(38.f)));
        
        // 基于 Dummy 产生的 Rect 进行状态判定，完美兼容滚动裁剪
        bool hov = ImGui::IsItemHovered();
        bool isCur = (s.id == currentId);
        
        // 视觉绘制
        if (hov || isCur) {
            dl->AddRectFilled(ImVec2(itemMin.x, itemMin.y + S(2.f)), 
                              ImVec2(itemMax.x, itemMax.y - S(2.f)), 
                              IM_COL32(kAccR, kAccG, kAccB, (int)((isCur ? 45 : 18) * alpha)), kRadiusSm);
        }
        
        char idxBuf[8]; snprintf(idxBuf, sizeof(idxBuf), "%02zu", idx + 1);
        dl->AddText(font, S(12.f), ImVec2(itemMin.x + S(12.f), itemMin.y + S(11.f)), RGBA(kTextDim, alpha), idxBuf);
        
        DrawTextEllipsis(dl, font, S(14.f), ImVec2(itemMin.x + S(40.f), itemMin.y + S(4.f)), S(260.f), isCur ? RGBA(kGlassBorderAcc, alpha) : RGBA(kTextPrimary, alpha), s.name.c_str());
        DrawTextEllipsis(dl, font, S(11.f), ImVec2(itemMin.x + S(40.f), itemMin.y + S(21.f)), S(260.f), RGBA(kTextDim, alpha), s.artist.c_str());
        dl->AddText(font, S(12.f), ImVec2(itemMax.x - TextW(font, S(12.f), formatTime(s.durationMs).c_str()) - S(14.f), itemMin.y + S(11.f)), RGBA(kTextDim, alpha), formatTime(s.durationMs).c_str());
        
        // 点击事件绑定
        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) PipeSendCommandAsync("NE_PLAY_SONG:" + songToJson(s));
        if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) PipeSendCommandAsync("NE_LIKE:" + std::to_string(s.id) + ":true");
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
        bool act = (active == i), hov = ImGui::IsMouseHoveringRect(tMin, tMax);
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
    if (pollTimer > 0.6f) { { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.statusPollTimer = 0.f; } refreshPlayStatus(); refreshLyrics(); refreshLoginStatus(); }

    bool loggedIn = false; { std::lock_guard<std::mutex> lock(g_state.mutex); loggedIn = g_state.loggedIn; }
    static bool s_firstLoginCheck = false; if (!s_firstLoginCheck) { s_firstLoginCheck = true; refreshLoginStatus(); }

    dl->AddText(font, S(20.f), ImVec2(contentMin.x + S(24.f), contentMin.y + S(22.f)), RGBA(kTextPrimary, alpha), "音乐");

    if (!loggedIn) {
        DrawLoginSection(ImVec2(contentMin.x + S(24.f), contentMin.y + S(76.f)), ImVec2(contentMax.x - S(24.f), contentMax.y - S(24.f)), alpha, dt);
        return;
    }

    std::string nick; { std::lock_guard<std::mutex> lock(g_state.mutex); nick = g_state.nickname; }
    if (!nick.empty()) {
        std::string greet = "Hi, " + nick;
        float greetW = TextW(font, S(13.f), greet.c_str());
        dl->AddText(font, S(13.f), ImVec2(contentMax.x - S(24.f) - greetW - S(80.f), contentMin.y + S(24.f)), RGBA(kTextSecondary, alpha), greet.c_str());
        ImVec2 sMin(contentMax.x - S(94.f), contentMin.y + S(18.f));
        ImVec2 sMax(sMin.x + S(70.f), sMin.y + S(30.f));
        const char* signinLabel = "每日签到";
        float signinW = TextW(font, S(12.f), signinLabel);
        bool signinHov = ImGui::IsMouseHoveringRect(sMin, sMax);
        DrawGlass(dl, sMin, sMax, kRadiusSm, alpha * (signinHov ? 1.3f : 1.f), true);
        dl->AddText(font, S(12.f), ImVec2(sMin.x + (S(70.f) - signinW) * 0.5f, sMin.y + S(8.f)), RGBA(kTextPrimary, alpha), signinLabel);
        if (signinHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) PipeSendCommandAsync("NE_SIGNIN");
    }

    dl->AddLine(ImVec2(contentMin.x + S(24.f), contentMin.y + S(64.f)), ImVec2(contentMax.x - S(24.f), contentMin.y + S(64.f)), RGBA(kGlassBorder, alpha * 0.2f), 1.f);

    ImVec2 tabsMin(contentMin.x + S(24.f), contentMin.y + S(76.f));
    ImVec2 tabsMax(contentMax.x - S(24.f), contentMin.y + S(112.f));
    DrawTabs(tabsMin, tabsMax, alpha);

    ImVec2 listMin(contentMin.x + S(24.f), tabsMax.y + S(16.f)); 
    
    std::string lcur, lnext; bool hasLrc = false;
    { std::lock_guard<std::mutex> lock(g_state.mutex); lcur = g_state.lyricCurrent; lnext = g_state.lyricNext; hasLrc = g_state.hasLyrics; }

    float bottomReserved = S(90.f);
    if (hasLrc && !lcur.empty()) bottomReserved += S(45.f);
    ImVec2 listMax(contentMax.x - S(24.f), contentMax.y - bottomReserved);

    int activeTab = 0; { std::lock_guard<std::mutex> lock(g_state.mutex); activeTab = g_state.activeTab; }

    if (activeTab == 0) {
        ImVec2 sMin(listMin.x, listMin.y); ImVec2 sMax(listMax.x, sMin.y + S(36.f));
        DrawGlass(dl, sMin, sMax, kRadiusSm, alpha, false);
        dl->AddText(font, S(13.f), ImVec2(sMin.x + S(12.f), sMin.y + S(11.f)), RGBA(kTextDim, alpha), g_state.searchBuf[0] ? "" : "在此键入关键词搜索歌曲...");
        ImGui::SetCursorScreenPos(ImVec2(sMin.x + S(10.f), sMin.y + S(5.f))); ImGui::PushItemWidth(sMax.x - sMin.x - S(20.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0)); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.97f, 0.98f, alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(6.f), S(8.f))); ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        ImGui::InputText("##ne_search", g_state.searchBuf, sizeof(g_state.searchBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(2); ImGui::PopItemWidth();
        if (ImGui::IsItemDeactivatedAfterEdit() && g_state.searchBuf[0]) {
            asyncQuery("NE_SEARCH:" + std::string(g_state.searchBuf), [](const std::string& body) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.searchResults = parseSongsArray(body, "songs"); });
        }
        listMin.y = sMax.y + S(16.f); 
        std::vector<SongItem> songs; { std::lock_guard<std::mutex> lock(g_state.mutex); songs = g_state.searchResults; }
        DrawSongList(songs, listMin, listMax, alpha, dt);
    } else if (activeTab == 1) {
        std::vector<PlaylistItem> pls; std::vector<SongItem> tracks; long selId = 0; std::string selName;
        { std::lock_guard<std::mutex> lock(g_state.mutex); pls = g_state.playlists; tracks = g_state.playlistTracks; selId = g_state.selectedPlaylistId; selName = g_state.selectedPlaylistName; }
        if (selId == 0) {
            // 将播放列表也改造为真正的滚屏区域
            ImGui::SetCursorScreenPos(listMin);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::BeginChild("##pl_list_child", ImVec2(listMax.x - listMin.x, listMax.y - listMin.y), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
            
            for (size_t i = 0; i < pls.size(); ++i) {
                ImVec2 rMin = ImGui::GetCursorScreenPos();
                ImVec2 rMax = ImVec2(rMin.x + (listMax.x - listMin.x), rMin.y + S(54.f));
                ImGui::Dummy(ImVec2(rMax.x - rMin.x, S(54.f)));
                
                bool plHov = ImGui::IsItemHovered();
                if (plHov) dl->AddRectFilled(ImVec2(rMin.x, rMin.y + S(2.f)), ImVec2(rMax.x, rMax.y - S(2.f)), IM_COL32(255, 255, 255, (int)(15 * alpha)), kRadiusSm);
                
                if (!pls[i].cover.empty()) CoverRequest(pls[i].cover); CoverTexture ct = CoverGet(pls[i].cover);
                if (ct.valid()) { ImGui::SetCursorScreenPos(ImVec2(rMin.x + S(8.f), rMin.y + S(6.f))); ImGui::Image((ImTextureID)(intptr_t)ct.tex, ImVec2(S(42.f), S(42.f))); }
                else dl->AddRectFilled(ImVec2(rMin.x + S(8.f), rMin.y + S(6.f)), ImVec2(rMin.x + S(50.f), rMin.y + S(48.f)), IM_COL32(255, 255, 255, (int)(20 * alpha)), kRadiusSm);
                
                DrawTextEllipsis(dl, font, S(14.f), ImVec2(rMin.x + S(64.f), rMin.y + S(10.f)), S(300.f), RGBA(kTextPrimary, alpha), pls[i].name.c_str());
                DrawTextEllipsis(dl, font, S(11.f), ImVec2(rMin.x + S(64.f), rMin.y + S(30.f)), S(300.f), RGBA(kTextDim, alpha), (pls[i].creator + " · " + std::to_string(pls[i].trackCount) + " 首").c_str());
                
                if (plHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    std::lock_guard<std::mutex> lock(g_state.mutex); g_state.selectedPlaylistId = pls[i].id; g_state.selectedPlaylistName = pls[i].name;
                    asyncQuery("NE_PLAYLIST_TRACKS:" + std::to_string(pls[i].id), [pid = pls[i].id](const std::string& body) {
                        std::lock_guard<std::mutex> lock(g_state.mutex); if (g_state.selectedPlaylistId == pid) g_state.playlistTracks = parseSongsArray(body, "songs");
                    });
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        } else {
            ImVec2 backMax(listMin.x + S(74.f), listMin.y + S(30.f));
            bool backHov = ImGui::IsMouseHoveringRect(listMin, backMax);
            DrawGlass(dl, listMin, backMax, kRadiusSm, alpha * (backHov ? 1.3f : 1.f), true);
            dl->AddText(font, S(12.f), ImVec2(listMin.x + S(14.f), listMin.y + S(8.f)), RGBA(kTextPrimary, alpha), "< 返回");
            if (backHov && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { std::lock_guard<std::mutex> lock(g_state.mutex); g_state.selectedPlaylistId = 0; g_state.playlistTracks.clear(); }
            dl->AddText(font, S(15.f), ImVec2(listMin.x + S(88.f), listMin.y + S(6.f)), RGBA(kTextPrimary, alpha), selName.c_str());
            DrawSongList(tracks, ImVec2(listMin.x, listMin.y + S(42.f)), listMax, alpha, dt);
        }
    } else if (activeTab == 2) {
        std::vector<SongItem> songs; { std::lock_guard<std::mutex> lock(g_state.mutex); songs = g_state.dailySongs; }
        DrawSongList(songs, listMin, listMax, alpha, dt);
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
        DrawSongList(songs, ImVec2(listMin.x, listMin.y + S(40.f)), listMax, alpha, dt);
    }

    if (hasLrc && !lcur.empty()) {
        float lY = contentMax.y - S(116.f);
        DrawTextEllipsis(dl, font, S(14.f), ImVec2(contentMin.x + S(24.f), lY), contentMax.x - contentMin.x - S(48.f), RGBA(kTextPrimary, alpha), lcur.c_str());
        DrawTextEllipsis(dl, font, S(12.f), ImVec2(contentMin.x + S(24.f), lY + S(20.f)), contentMax.x - contentMin.x - S(48.f), RGBA(kTextDim, alpha), lnext.c_str());
    }

    DrawPlayerBar(ImVec2(contentMin.x + S(24.f), contentMax.y - S(76.f)), ImVec2(contentMax.x - S(24.f), contentMax.y - S(16.f)), alpha, dt);
}

void MusicPanelTick() { CoverProcessPending(); }

}  // namespace myiui::ui::music