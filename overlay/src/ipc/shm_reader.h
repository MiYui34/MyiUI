#pragma once

#include <cstdint>
#include <vector>

#include "bridge/ui_state_types.h"

class ShmReader {
public:
    ShmReader();
    ~ShmReader();

    bool Open();
    void Close();
    bool IsValid() const;
    bool NeedsRemap() const;
    bool IsMenuActive() const;
    bool IsOverlayActive() const;
    bool IsHudActive() const;
    bool IsIslandActive() const;
    myiui::shared::ScreenKind GetScreenKind() const;
    uint32_t GetScreenSeq() const;
    bool ReadHudState(myiui::shared::HudState& out) const;
    bool ReadIslandState(myiui::shared::IslandState& out) const;
    bool ReadChatState(myiui::shared::ChatState& out) const;
    bool PeekFrame(uint32_t& frameIndex, uint32_t& width, uint32_t& height) const;
    bool ReadFrame(std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height, uint32_t& frameIndex);
};
