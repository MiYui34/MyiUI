#pragma once

#include <string>
#include <vector>

struct WorldEntry {
    std::string name;
    std::string mode;
    std::string last_played;
};

struct ServerEntry {
    std::string id;
    std::string name;
    std::string address;
};

struct ProfileData {
    std::string name;
    std::string uuid;
    std::string account_type;
    std::string skin_url;
    std::string skin_path;
    std::string mc_version;
    std::string loader;
};

struct KeybindEntry {
    std::string id;
    std::string label;
    std::string key;
};

struct PackEntry {
    std::string id;
    std::string name;
    bool enabled = false;
    bool locked = false;
};

struct MenuDataCache {
    std::vector<WorldEntry> worlds;
    std::vector<ServerEntry> servers;
    std::vector<KeybindEntry> keybinds;
    std::vector<PackEntry> packs;
    bool worlds_loading = false;
    bool worlds_fetch_done = false;
    float worlds_load_ms = 0.f;
    bool servers_loading = false;
    bool servers_fetch_done = false;
    float servers_load_ms = 0.f;
    bool profile_loading = false;
    bool profile_fetch_done = false;
    float profile_load_ms = 0.f;
    ProfileData profile;
    std::string profile_avatar_url;
    bool profile_seeded = false;
    bool options_loading = false;
    bool options_fetch_done = false;
    float options_load_ms = 0.f;
    std::string options_json;
    std::string options_baseline_json;
    std::string category;
};

bool ParseWorldsJson(const std::string& json, std::vector<WorldEntry>& out);
bool ParseServersJson(const std::string& json, std::vector<ServerEntry>& out);
bool ParsePlayerJson(const std::string& json, ProfileData& out);
bool ParseKeybindsJson(const std::string& json, std::vector<KeybindEntry>& out);
bool ParsePacksJson(const std::string& json, std::vector<PackEntry>& out);
bool LoadWorldsFromDisk(std::vector<WorldEntry>& out);
bool LoadServersFromDisk(std::vector<ServerEntry>& out);
bool LoadProfileFromDisk(ProfileData& out);
std::string GetOptionValueFromJson(const std::string& json, const std::string& key);
void SetOptionValueInJson(std::string& json, const std::string& key, const std::string& value);
