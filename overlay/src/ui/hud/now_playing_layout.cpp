#include "ui/hud/now_playing_layout.h"

#include <algorithm>
#include <cmath>

namespace myiui::ui::hud {

namespace {

constexpr float kBaseCardW = 280.f;
constexpr float kBaseCardHWithWave = 92.f;
constexpr float kBaseCardHCompact = 64.f;

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

float NowPlayingEffectiveScale(const myiui::config::NowPlayingSettings& settings) {
    return kNowPlayingScale * std::max(0.25f, settings.scale);
}

float NowPlayingCardWidth(const myiui::config::NowPlayingSettings& settings) {
    return kBaseCardW * NowPlayingEffectiveScale(settings);
}

float NowPlayingCardHeight(const myiui::config::NowPlayingSettings& settings) {
    return (settings.show_waveform ? kBaseCardHWithWave : kBaseCardHCompact) * NowPlayingEffectiveScale(settings);
}

ImVec4 CalcNowPlayingBounds(const myiui::config::NowPlayingSettings& settings, float viewportW, float viewportH) {
    const float cardW = NowPlayingCardWidth(settings);
    const float cardH = NowPlayingCardHeight(settings);
    const ImVec2 pos = AnchorPos(settings.anchor, settings.x, settings.y, cardW, cardH, viewportW, viewportH);
    return ImVec4(pos.x, pos.y, pos.x + cardW, pos.y + cardH);
}

float ComputeLyricsAnchorTop(const myiui::config::UserSettings& settings, float viewportW, float viewportH) {
    if (settings.now_playing.enabled) {
        const ImVec4 b = CalcNowPlayingBounds(settings.now_playing, viewportW, viewportH);
        return b.y;
    }
    return viewportH - 80.f;
}

bool NowPlayingShouldShow(const myiui::config::NowPlayingSettings& settings, bool musicValid, bool playing,
                          bool paused) {
    return settings.enabled && musicValid && (playing || paused);
}

}  // namespace myiui::ui::hud
