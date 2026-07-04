#include "ui/hud/now_playing.h"

#include "ui/hud/hud_glass.h"
#include "ui/fonts.h"
#include "ui/music/cover_loader.h"
#include "ui/theme/theme_runtime.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace myiui::ui::hud {

namespace {

ImVec2 AnchorPos(myiui::config::InfoAnchor anchor, float ox, float oy, float w, float h, float vw, float vh) {
    switch (anchor) {
        case myiui::config::InfoAnchor::TopRight:
            return ImVec2(vw - w + ox, oy);
        case myiui::config::InfoAnchor::BottomLeft:
            return ImVec2(ox, vh - h + oy);
        case myiui::config::InfoAnchor::BottomRight:
            return ImVec2(vw - w + ox, vh - h + oy);
        default:
            return ImVec2(ox, oy);
    }
}

}  // namespace

void RenderMiniWaveform(ImDrawList* dl, ImVec2 min, ImVec2 max, const float* bins, int count, const int accent[4],
                        float alpha) {
    if (!dl || !bins || count <= 0) return;
    const float w = max.x - min.x;
    const float h = max.y - min.y;
    const float barW = w / static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        const float v = std::clamp(bins[i], 0.f, 1.f);
        const float bh = h * v;
        const ImVec2 bmin(min.x + i * barW + 1.f, max.y - bh);
        const ImVec2 bmax(min.x + (i + 1) * barW - 1.f, max.y);
        dl->AddRectFilled(bmin, bmax, IM_COL32(accent[0], accent[1], accent[2], static_cast<int>(220 * alpha)), 2.f);
    }
}

void RenderNowPlaying(const AppConfig& cfg, const myiui::shared::MusicHudState& music,
                      const myiui::config::NowPlayingSettings& settings, float viewportW, float viewportH, float dt) {
    (void)dt;
    if (!settings.enabled || !music.valid || (!music.playing && !music.paused)) return;

    if (music.cover_url[0]) {
        myiui::ui::theme::ThemeRuntimeSetCoverSeedUrl(music.cover_url);
        myiui::ui::music::CoverRequest(music.cover_url);
    }

    const float cardW = 280.f;
    const float cardH = settings.show_waveform ? 92.f : 64.f;
    const ImVec2 pos = AnchorPos(settings.anchor, settings.x, settings.y, cardW, cardH, viewportW, viewportH);
    const ImVec2 max(pos.x + cardW, pos.y + cardH);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    DrawGlassPanel(dl, pos, max, 12.f, cfg.theme.accent, 0.92f);

    const float cover = 48.f;
    ImVec2 coverMin(pos.x + 8.f, pos.y + 8.f);
    ImVec2 coverMax(coverMin.x + cover, coverMin.y + cover);
    if (music.cover_url[0]) {
        auto tex = myiui::ui::music::CoverGet(music.cover_url);
        if (tex.valid()) {
            dl->AddImageRounded((ImTextureID)(intptr_t)tex.tex, coverMin, coverMax, ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32(255, 255, 255, 255), 8.f);
        } else {
            dl->AddRectFilled(coverMin, coverMax, IM_COL32(40, 40, 48, 200), 8.f);
        }
    }

    ImFont* font = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
    ImFont* titleFont = GetUiFonts().semibold ? GetUiFonts().semibold : font;
    const char* title = music.title[0] ? music.title : "Unknown";
    const char* artist = music.artist[0] ? music.artist : "";
    if (titleFont) {
        dl->AddText(titleFont, 14.f, ImVec2(pos.x + 64.f, pos.y + 10.f), IM_COL32(245, 247, 250, 255), title);
    }
    if (font && artist[0]) {
        dl->AddText(font, 12.f, ImVec2(pos.x + 64.f, pos.y + 28.f), IM_COL32(178, 184, 196, 255), artist);
    }

    const float progY = pos.y + 46.f;
    const float progW = cardW - 16.f;
    const float pct = music.duration_ms > 0 ? static_cast<float>(music.position_ms) / music.duration_ms : 0.f;
    dl->AddRectFilled(ImVec2(pos.x + 8.f, progY), ImVec2(pos.x + 8.f + progW, progY + 4.f), IM_COL32(255, 255, 255, 30),
                      2.f);
    dl->AddRectFilled(ImVec2(pos.x + 8.f, progY), ImVec2(pos.x + 8.f + progW * pct, progY + 4.f),
                      IM_COL32(cfg.theme.accent[0], cfg.theme.accent[1], cfg.theme.accent[2], 230), 2.f);

    if (settings.show_waveform) {
        RenderMiniWaveform(dl, ImVec2(pos.x + 8.f, pos.y + 56.f), ImVec2(pos.x + cardW - 8.f, pos.y + cardH - 8.f),
                           music.waveform, myiui::shared::kMusicWaveformBins, cfg.theme.accent, 1.f);
    }
}

}  // namespace myiui::ui::hud
