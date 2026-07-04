#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "bridge/ui_state_types.h"

namespace myiui::bridge {

struct ScreenState {
    myiui::shared::ScreenKind kind = myiui::shared::ScreenKind::None;
    uint32_t seq = 0;
    bool overlay_active = false;
    bool island_active = false;
    bool overlay_ack = false;
};

struct VideoFrameState {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame_index = 0;
    std::vector<uint8_t> rgba;
};

class NativeState {
public:
    static NativeState& Instance();

    void PushScreen(uint8_t kind, uint32_t seq, bool overlayActive, bool islandActive, bool overlayAck);
    void PushIsland(const myiui::shared::IslandState& state);
    void PushHud(const myiui::shared::HudState& state);
    void PushInfoHud(const myiui::shared::InfoHudState& state);
    void PushMusicHud(const myiui::shared::MusicHudState& state);
    void PushTabList(const myiui::shared::TabListState& state);
    void PushVideoFrame(const uint8_t* rgba, int width, int height, int frameIndex);

    bool ReadScreen(ScreenState& out) const;
    bool ReadIsland(myiui::shared::IslandState& out) const;
    bool ReadHud(myiui::shared::HudState& out) const;
    bool ReadInfoHud(myiui::shared::InfoHudState& out) const;
    bool ReadMusicHud(myiui::shared::MusicHudState& out) const;
    bool ReadTabList(myiui::shared::TabListState& out) const;
    bool PeekFrame(uint32_t& frameIndex, uint32_t& width, uint32_t& height) const;
    bool ReadFrame(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height, uint32_t& frameIndex);

    bool IsOverlayActive() const;
    bool IsIslandActive() const;
    myiui::shared::ScreenKind GetScreenKind() const;
    uint32_t GetScreenSeq() const;

private:
    NativeState() = default;

    mutable std::mutex mutex_;
    ScreenState screen_{};
    myiui::shared::IslandState island_{};
    myiui::shared::HudState hud_{};
    myiui::shared::InfoHudState info_{};
    myiui::shared::MusicHudState music_{};
    myiui::shared::TabListState tab_list_{};
    VideoFrameState video_{};
};

}  // namespace myiui::bridge
