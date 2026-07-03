#include "native_state.h"

#include <cstring>

namespace myiui::bridge {

NativeState& NativeState::Instance() {
    static NativeState instance;
    return instance;
}

void NativeState::PushScreen(uint8_t kind, uint32_t seq, bool overlayActive, bool islandActive,
                             bool overlayAck) {
    std::lock_guard lock(mutex_);
    screen_.kind = static_cast<myiui::shared::ScreenKind>(kind);
    screen_.seq = seq;
    screen_.overlay_active = overlayActive;
    screen_.island_active = islandActive;
    screen_.overlay_ack = overlayAck;
}

void NativeState::PushIsland(const myiui::shared::IslandState& state) {
    std::lock_guard lock(mutex_);
    island_ = state;
}

void NativeState::PushHud(const myiui::shared::HudState& state) {
    std::lock_guard lock(mutex_);
    hud_ = state;
}

void NativeState::PushTabList(const myiui::shared::TabListState& state) {
    std::lock_guard lock(mutex_);
    tab_list_ = state;
}

void NativeState::PushVideoFrame(const uint8_t* rgba, int width, int height, int frameIndex) {
    if (!rgba || width <= 0 || height <= 0) {
        return;
    }
    const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    std::lock_guard lock(mutex_);
    video_.width = static_cast<uint32_t>(width);
    video_.height = static_cast<uint32_t>(height);
    video_.frame_index = static_cast<uint32_t>(frameIndex);
    video_.rgba.assign(rgba, rgba + bytes);
}

bool NativeState::ReadScreen(ScreenState& out) const {
    std::lock_guard lock(mutex_);
    out = screen_;
    return true;
}

bool NativeState::ReadIsland(myiui::shared::IslandState& out) const {
    std::lock_guard lock(mutex_);
    if (!island_.valid) {
        return false;
    }
    out = island_;
    return true;
}

bool NativeState::ReadHud(myiui::shared::HudState& out) const {
    std::lock_guard lock(mutex_);
    if (!hud_.valid) {
        return false;
    }
    out = hud_;
    return true;
}

bool NativeState::ReadTabList(myiui::shared::TabListState& out) const {
    std::lock_guard lock(mutex_);
    if (!tab_list_.valid) {
        return false;
    }
    out = tab_list_;
    return true;
}

bool NativeState::PeekFrame(uint32_t& frameIndex, uint32_t& width, uint32_t& height) const {
    std::lock_guard lock(mutex_);
    if (video_.rgba.empty()) {
        return false;
    }
    frameIndex = video_.frame_index;
    width = video_.width;
    height = video_.height;
    return true;
}

bool NativeState::ReadFrame(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height,
                                 uint32_t& frameIndex) {
    std::lock_guard lock(mutex_);
    if (video_.rgba.empty()) {
        return false;
    }
    rgba = video_.rgba;
    width = video_.width;
    height = video_.height;
    frameIndex = video_.frame_index;
    return true;
}

bool NativeState::IsOverlayActive() const {
    std::lock_guard lock(mutex_);
    return screen_.overlay_active;
}

bool NativeState::IsIslandActive() const {
    std::lock_guard lock(mutex_);
    return screen_.island_active;
}

myiui::shared::ScreenKind NativeState::GetScreenKind() const {
    std::lock_guard lock(mutex_);
    return screen_.kind;
}

uint32_t NativeState::GetScreenSeq() const {
    std::lock_guard lock(mutex_);
    return screen_.seq;
}

}  // namespace myiui::bridge
