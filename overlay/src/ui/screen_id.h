#pragma once

enum class ScreenId {
    Home,
    Singleplayer,
    CreateWorld,
    Multiplayer,
    AddServer,
    OptionsHub,
    OptionsVideo,
    OptionsSound,
    OptionsControls,
    OptionsLanguage,
    OptionsChat,
    OptionsAccessibility,
    OptionsSkin,
    OptionsResourcePacks,
};

inline const char* ScreenIdName(ScreenId id) {
    switch (id) {
        case ScreenId::Home: return "Home";
        case ScreenId::Singleplayer: return "Singleplayer";
        case ScreenId::CreateWorld: return "CreateWorld";
        case ScreenId::Multiplayer: return "Multiplayer";
        case ScreenId::AddServer: return "AddServer";
        case ScreenId::OptionsHub: return "OptionsHub";
        case ScreenId::OptionsVideo: return "OptionsVideo";
        case ScreenId::OptionsSound: return "OptionsSound";
        case ScreenId::OptionsControls: return "OptionsControls";
        case ScreenId::OptionsLanguage: return "OptionsLanguage";
        case ScreenId::OptionsChat: return "OptionsChat";
        case ScreenId::OptionsAccessibility: return "OptionsAccessibility";
        case ScreenId::OptionsSkin: return "OptionsSkin";
        case ScreenId::OptionsResourcePacks: return "OptionsResourcePacks";
    }
    return "Unknown";
}

inline bool IsOptionsDetailScreen(ScreenId id) {
    return id >= ScreenId::OptionsVideo && id <= ScreenId::OptionsResourcePacks;
}

inline const char* OptionsScreenJsonName(ScreenId id) {
    switch (id) {
        case ScreenId::OptionsVideo: return "options_video";
        case ScreenId::OptionsSound: return "options_sound";
        case ScreenId::OptionsControls: return "options_controls";
        case ScreenId::OptionsLanguage: return "options_language";
        case ScreenId::OptionsChat: return "options_chat";
        case ScreenId::OptionsAccessibility: return "options_accessibility";
        case ScreenId::OptionsSkin: return "options_skin";
        case ScreenId::OptionsResourcePacks: return "options_resource_packs";
        default: return nullptr;
    }
}
