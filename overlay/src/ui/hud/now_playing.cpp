#include "ui/hud/now_playing.h"

#include "ui/hud/hud_glass.h"
#include "ui/hud/now_playing_layout.h"
#include "ui/fonts.h"
#include "ui/music/cover_loader.h"
#include "ui/theme/theme_runtime.h"

#include "imgui.h"
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

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
                        float alpha, bool is_playing) {
    if (!dl || !bins || count <= 0) return;

    const float w = max.x - min.x;
    const float h = max.y - min.y;
    const float barW = w / static_cast<float>(count);
    const float padding = 2.0f * kNowPlayingScale;

    static std::vector<float> smoothed_bins;
    if (smoothed_bins.size() != static_cast<size_t>(count)) {
        smoothed_bins.assign(count, 0.05f);
    }

    const float delta_time = ImGui::GetIO().DeltaTime;
    const float lerp_speed = is_playing ? 25.0f : 12.0f;

    for (int i = 0; i < count; ++i) {
        const float target_v = is_playing ? std::clamp(bins[i], 0.05f, 1.f) : 0.05f;
        smoothed_bins[i] = ImLerp(smoothed_bins[i], target_v, ImClamp(delta_time * lerp_speed, 0.0f, 1.0f));

        const float bh = h * smoothed_bins[i];
        const float actual_bar_w = std::max(1.0f, barW - padding);
        const ImVec2 bmin(min.x + i * barW + (barW - actual_bar_w) * 0.5f, max.y - bh);
        const ImVec2 bmax(bmin.x + actual_bar_w, max.y);
        const float rounding = actual_bar_w * 0.5f;
        dl->AddRectFilled(bmin, bmax, IM_COL32(accent[0], accent[1], accent[2], static_cast<int>(255 * alpha)),
                          rounding);
    }
}

void RenderNowPlaying(const AppConfig& cfg, const myiui::shared::MusicHudState& music,
                      const myiui::config::NowPlayingSettings& settings, float viewportW, float viewportH, float dt) {
    (void)dt;
    if (!NowPlayingShouldShow(settings, music.valid != 0, music.playing != 0, music.paused != 0)) return;

    if (music.cover_url[0]) {
        myiui::ui::theme::ThemeRuntimeSetCoverSeedUrl(music.cover_url);
        myiui::ui::music::CoverRequest(music.cover_url);
    }

    const float cardW = NowPlayingCardWidth(settings);
    const float cardH = NowPlayingCardHeight(settings);
    const float uiScale = NowPlayingEffectiveScale(settings);
    const float pad = 8.f * uiScale;
    const float cover = 48.f * uiScale;
    const float radius = 12.f * uiScale;

    const ImVec2 pos = AnchorPos(settings.anchor, settings.x, settings.y, cardW, cardH, viewportW, viewportH);
    const ImVec2 max(pos.x + cardW, pos.y + cardH);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    DrawGlassPanel(dl, pos, max, radius, cfg.theme.accent, 0.92f);

    ImVec2 coverMin(pos.x + pad, pos.y + pad);
    ImVec2 coverMax(coverMin.x + cover, coverMin.y + cover);
    if (music.cover_url[0]) {
        auto tex = myiui::ui::music::CoverGet(music.cover_url);
        if (tex.valid()) {
            dl->AddImageRounded((ImTextureID)(intptr_t)tex.tex, coverMin, coverMax, ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32(255, 255, 255, 255), 8.f * uiScale);
        } else {
            dl->AddRectFilled(coverMin, coverMax, IM_COL32(40, 40, 48, 200), 8.f * uiScale);
        }
    }

    ImFont* font = GetUiFonts().regular ? GetUiFonts().regular : ImGui::GetFont();
    ImFont* titleFont = GetUiFonts().semibold ? GetUiFonts().semibold : font;
    const float titleFs = 14.f * uiScale;
    const float artistFs = 12.f * uiScale;
    const char* title = music.title[0] ? music.title : "Unknown";
    const char* artist = music.artist[0] ? music.artist : "";
    const float textX = pos.x + cover + pad * 2.f;
    if (titleFont) {
        dl->AddText(titleFont, titleFs, ImVec2(textX, pos.y + pad * 1.25f), IM_COL32(245, 247, 250, 255), title);
    }
    if (font && artist[0]) {
        dl->AddText(font, artistFs, ImVec2(textX, pos.y + pad * 1.25f + titleFs * 1.1f),
                    IM_COL32(178, 184, 196, 255), artist);
    }

    const float progY = pos.y + cover + pad;
    const float progW = cardW - pad * 2.f;
    const float progH = 4.f * uiScale;
    const float pct = music.duration_ms > 0 ? static_cast<float>(music.position_ms) / music.duration_ms : 0.f;
    dl->AddRectFilled(ImVec2(pos.x + pad, progY), ImVec2(pos.x + pad + progW, progY + progH), IM_COL32(255, 255, 255, 30),
                      2.f * uiScale);
    dl->AddRectFilled(ImVec2(pos.x + pad, progY), ImVec2(pos.x + pad + progW * pct, progY + progH),
                      IM_COL32(cfg.theme.accent[0], cfg.theme.accent[1], cfg.theme.accent[2], 230),
                      2.f * uiScale);

    if (settings.show_waveform) {
        const float waveTop = progY + progH + pad * 0.75f;
        RenderMiniWaveform(dl, ImVec2(pos.x + pad, waveTop), ImVec2(pos.x + cardW - pad, pos.y + cardH - pad),
                           music.waveform, myiui::shared::kMusicWaveformBins, cfg.theme.accent, 1.f, music.playing != 0);
    }
}

}  // namespace myiui::ui::hud
