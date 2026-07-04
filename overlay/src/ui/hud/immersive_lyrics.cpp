#include "ui/hud/immersive_lyrics.h"

#include "ui/fonts.h"
#include "ui/hud/now_playing_layout.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace myiui::ui::hud {

namespace {

void DrawKaraokeLine(ImDrawList* dl, ImFont* font, float fontSize, ImVec2 pos, const char* text, float progress,
                     const int accent[4]) {
    if (!font || !text || !text[0]) return;
    progress = std::clamp(progress, 0.f, 1.f);

    const ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
    const ImU32 dimCol = IM_COL32(110, 114, 128, 220);
    const ImU32 brightCol = IM_COL32(250, 252, 255, 255);
    const ImU32 accentCol = IM_COL32(accent[0], accent[1], accent[2], 255);

    dl->AddText(font, fontSize, pos, dimCol, text);

    const float clipW = ts.x * progress;
    if (clipW > 0.5f) {
        dl->PushClipRect(pos, ImVec2(pos.x + clipW, pos.y + ts.y + 4.f), true);
        dl->AddText(font, fontSize, pos, brightCol, text);
        dl->PopClipRect();

        const float glowW = std::min(20.f, ts.x * 0.08f);
        const float glowStart = std::max(pos.x, pos.x + clipW - glowW);
        dl->PushClipRect(ImVec2(glowStart, pos.y), ImVec2(pos.x + clipW, pos.y + ts.y + 4.f), true);
        dl->AddText(font, fontSize, pos, accentCol, text);
        dl->PopClipRect();
    }
}

float LyricAnchorTop(const myiui::config::UserSettings& settings, float viewportW, float viewportH) {
    return ComputeLyricsAnchorTop(settings, viewportW, viewportH);
}

}  // namespace

void RenderImmersiveLyrics(const AppConfig& cfg, const myiui::shared::MusicHudState& music,
                           const myiui::shared::IslandState& island, const myiui::config::UserSettings& settings,
                           float viewportW, float viewportH) {
    if (!settings.now_playing.immersive_lyrics) return;
    if (!music.valid || (!music.playing && !music.paused)) return;

    const char* line = music.lyric_current[0] ? music.lyric_current : island.lyrics_line;
    const char* next = music.lyric_next[0] ? music.lyric_next : "";
    if (!line || !line[0]) return;

    float progress = music.lyric_progress;
    if (!music.lyric_current[0] && music.duration_ms > 0) {
        progress = std::clamp(static_cast<float>(music.position_ms) / static_cast<float>(music.duration_ms), 0.f, 1.f);
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* lyricFont = GetUiFonts().semibold ? GetUiFonts().semibold : ImGui::GetFont();

    const float anchorTop = LyricAnchorTop(settings, viewportW, viewportH);
    const float lineFs = std::clamp(viewportW * 0.028f, 24.f, 42.f);
    const float nextFs = lineFs * 0.55f;
    const ImVec2 lineSz =
        lyricFont ? lyricFont->CalcTextSizeA(lineFs, FLT_MAX, 0.f, line) : ImGui::CalcTextSize(line);
    const float nextH = next[0] && lyricFont ? lyricFont->CalcTextSizeA(nextFs, FLT_MAX, 0.f, next).y + 8.f : 0.f;
    const float blockH = lineSz.y + nextH;
    const float gapAboveHud = 18.f;
    const float blockBottom = std::max(blockH + 8.f, anchorTop - gapAboveHud);
    const float blockTop = blockBottom - blockH;
    const float padY = 12.f;

    dl->AddRectFilled(ImVec2(0.f, std::max(0.f, blockTop - padY)), ImVec2(viewportW, blockBottom + padY * 0.5f),
                      IM_COL32(0, 0, 0, 88), 0.f);

    const ImVec2 linePos((viewportW - lineSz.x) * 0.5f, blockBottom - blockH);
    DrawKaraokeLine(dl, lyricFont, lineFs, linePos, line, progress, cfg.theme.accent);

    if (next[0] && lyricFont) {
        const ImVec2 nextSz = lyricFont->CalcTextSizeA(nextFs, FLT_MAX, 0.f, next);
        dl->AddText(lyricFont, nextFs, ImVec2((viewportW - nextSz.x) * 0.5f, linePos.y + lineSz.y + 8.f),
                    IM_COL32(130, 136, 148, 180), next);
    }
}

}  // namespace myiui::ui::hud
