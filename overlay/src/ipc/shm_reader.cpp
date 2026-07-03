#include "ipc/shm_reader.h"

#include "bridge/native_state.h"

ShmReader::ShmReader() = default;

ShmReader::~ShmReader() = default;

void ShmReader::Close() {}

bool ShmReader::Open() {
    return true;
}

bool ShmReader::NeedsRemap() const {
    return false;
}

bool ShmReader::IsValid() const {
    return true;
}

bool ShmReader::IsMenuActive() const {
    return myiui::bridge::NativeState::Instance().IsOverlayActive();
}

myiui::shared::ScreenKind ShmReader::GetScreenKind() const {
    return myiui::bridge::NativeState::Instance().GetScreenKind();
}

uint32_t ShmReader::GetScreenSeq() const {
    return myiui::bridge::NativeState::Instance().GetScreenSeq();
}

bool ShmReader::IsOverlayActive() const {
    return myiui::bridge::NativeState::Instance().IsOverlayActive();
}

bool ShmReader::IsHudActive() const {
    return myiui::shared::IsHudScreen(GetScreenKind());
}

bool ShmReader::IsIslandActive() const {
    return myiui::bridge::NativeState::Instance().IsIslandActive();
}

bool ShmReader::ReadIslandState(myiui::shared::IslandState& out) const {
    if (!IsIslandActive()) {
        return false;
    }
    return myiui::bridge::NativeState::Instance().ReadIsland(out);
}

bool ShmReader::ReadHudState(myiui::shared::HudState& out) const {
    if (GetScreenKind() != myiui::shared::ScreenKind::InGame) {
        return false;
    }
    return myiui::bridge::NativeState::Instance().ReadHud(out);
}

bool ShmReader::ReadTabListState(myiui::shared::TabListState& out) const {
    if (!IsIslandActive()) {
        return false;
    }
    return myiui::bridge::NativeState::Instance().ReadTabList(out);
}

bool ShmReader::PeekFrame(uint32_t& frameIndex, uint32_t& width, uint32_t& height) const {
    return myiui::bridge::NativeState::Instance().PeekFrame(frameIndex, width, height);
}

bool ShmReader::ReadFrame(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height, uint32_t& frameIndex) {
    return myiui::bridge::NativeState::Instance().ReadFrame(rgba, width, height, frameIndex);
}
