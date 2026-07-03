#pragma once

#include <cstdint>

namespace myiui::shared {

inline constexpr size_t kIslandTitleLen = 48;
inline constexpr size_t kIslandSubtitleLen = 96;
inline constexpr size_t kIslandLyricsLen = 128;
inline constexpr size_t kIslandSlotCount = 4;

enum class IslandMode : uint8_t {
    Collapsed = 0,
    Expanding = 1,
    Expanded = 2,
    Collapsing = 3,
    Lyrics = 4,
};

enum class IslandSlotId : uint8_t {
    Music = 0,
    QuickMenu = 1,
    System = 2,
    Custom = 3,
};

enum class IslandIconKind : uint8_t {
    Dot = 0,
    Music = 1,
    Menu = 2,
    Bell = 3,
    Star = 4,
};

#pragma pack(push, 1)
struct IslandSlot {
    uint8_t id;
    uint8_t enabled;
    uint8_t icon_kind;
    uint8_t reserved;
};

struct IslandState {
    uint8_t valid;
    uint8_t mode;
    uint8_t active_slot;
    uint8_t notify_count;
    uint16_t island_seq;
    int16_t fps;
    char title[kIslandTitleLen];
    char subtitle[kIslandSubtitleLen];
    char lyrics_line[kIslandLyricsLen];
    IslandSlot slots[kIslandSlotCount];
    uint32_t notify_expire_ms;
};
#pragma pack(pop)

inline constexpr size_t kIslandStateSize = sizeof(IslandState);

#pragma pack(push, 1)
struct HudHotbarSlot {
    uint16_t item_id;
    uint8_t count;
    uint8_t durability_pct;
    uint8_t cooldown_pct;
    uint8_t reserved;
};

struct HudState {
    uint8_t valid;
    uint8_t flags;
    uint8_t selected_slot;
    uint8_t gui_scale;
    uint16_t hud_seq;
    uint16_t reserved16;
    float health;
    float health_max;
    float absorption;
    float food;
    float saturation;
    float exhaustion;
    int16_t armor;
    int16_t air;
    int16_t max_air;
    uint8_t underwater;
    uint8_t pad;
    int16_t hotbar_left_px;
    int16_t hotbar_top_px;
    int16_t hotbar_slot_px;
    int16_t xp_level;
    float xp_progress;
    uint16_t layout_reserved;
    HudHotbarSlot slots[9];
};
#pragma pack(pop)

inline constexpr size_t kHudStateSize = sizeof(HudState);

enum HudFlags : uint8_t {
    HudFlagLowHealth = 1u << 0,
    HudFlagDamaged = 1u << 1,
    HudFlagAppleSkin = 1u << 2,
    HudFlagShowSaturation = 1u << 3,
};

inline constexpr size_t kChatUserLen = 32;
inline constexpr size_t kChatTextLen = 192;
inline constexpr size_t kChatMaxMessages = 16;

#pragma pack(push, 1)
struct ChatMessage {
    char user[kChatUserLen];
    char text[kChatTextLen];
};

struct ChatState {
    uint8_t valid;
    uint8_t visible;
    uint8_t msg_count;
    uint8_t reserved;
    uint16_t chat_seq;
    ChatMessage messages[kChatMaxMessages];
};
#pragma pack(pop)

inline constexpr size_t kChatStateSize = sizeof(ChatState);

inline constexpr uint32_t kMaxFrameBytes = 1920u * 1080u * 4u;

enum class ScreenKind : uint8_t {
    None = 0,
    MainMenu = 1,
    SubMenu = 2,
    VideoSettings = 3,
    InGame = 4,
};

inline constexpr bool IsOverlayScreen(ScreenKind kind) {
    return kind == ScreenKind::MainMenu;
}

inline constexpr bool IsHudScreen(ScreenKind kind) {
    return kind == ScreenKind::InGame;
}

inline constexpr bool IsIslandScreen(ScreenKind kind) {
    return kind == ScreenKind::InGame;
}

inline constexpr bool IsChatScreen(ScreenKind kind) {
    return kind == ScreenKind::InGame;
}

inline constexpr bool IsOverlayOrHudScreen(ScreenKind kind) {
    return kind == ScreenKind::MainMenu || kind == ScreenKind::InGame;
}

}  // namespace myiui::shared
