#include "ui/menu_data.h"

#include <regex>
#include <algorithm>
#include <windows.h>

#include <fstream>
#include <sstream>

bool ParseWorldsJson(const std::string& json, std::vector<WorldEntry>& out) {
    out.clear();
    const std::regex itemRe(
        "\\{\\s*\"name\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"mode\"\\s*:\\s*\"([^\"]*)\"(?:\\s*,\\s*\"last_played\"\\s*:\\s*\"([^\"]*)\")?\\s*\\}");
    auto begin = std::sregex_iterator(json.begin(), json.end(), itemRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        WorldEntry w;
        w.name = (*it)[1].str();
        w.mode = (*it)[2].str();
        if ((*it)[3].matched) w.last_played = (*it)[3].str();
        out.push_back(std::move(w));
    }
    return true;
}

bool ParseServersJson(const std::string& json, std::vector<ServerEntry>& out) {
    out.clear();
    // MyiUI agent / custom: { "id", "name", "address" }
    const std::regex customRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"name\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"address\"\\s*:\\s*\"([^\"]*)\"\\s*\\}");
    auto begin = std::sregex_iterator(json.begin(), json.end(), customRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        ServerEntry s;
        s.id = (*it)[1].str();
        s.name = (*it)[2].str();
        s.address = (*it)[3].str();
        out.push_back(std::move(s));
    }
    if (!out.empty()) return true;

    // Vanilla Minecraft servers.json: { "name", "ip" } or { "name", "address" }
    const std::regex vanillaRe(
        "\\{\\s*\"name\"\\s*:\\s*\"([^\"]*)\"[^}]*\"(?:ip|address)\"\\s*:\\s*\"([^\"]*)\"");
    begin = std::sregex_iterator(json.begin(), json.end(), vanillaRe);
    int idx = 0;
    for (auto it = begin; it != end; ++it) {
        ServerEntry s;
        s.name = (*it)[1].str();
        s.address = (*it)[2].str();
        s.id = s.address.empty() ? std::to_string(idx) : s.address;
        out.push_back(std::move(s));
        ++idx;
    }
    return true;
}

bool ParseKeybindsJson(const std::string& json, std::vector<KeybindEntry>& out) {
    out.clear();
    const std::regex itemRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"label\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"key\"\\s*:\\s*\"([^\"]*)\"\\s*\\}");
    auto begin = std::sregex_iterator(json.begin(), json.end(), itemRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        KeybindEntry k;
        k.id = (*it)[1].str();
        k.label = (*it)[2].str();
        k.key = (*it)[3].str();
        out.push_back(std::move(k));
    }
    return true;
}

bool ParsePacksJson(const std::string& json, std::vector<PackEntry>& out) {
    out.clear();
    const std::regex itemRe(
        "\\{\\s*\"id\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"name\"\\s*:\\s*\"([^\"]*)\"\\s*,\\s*\"enabled\"\\s*:\\s*(true|false)"
        "(?:\\s*,\\s*\"locked\"\\s*:\\s*(true|false))?\\s*\\}");
    auto begin = std::sregex_iterator(json.begin(), json.end(), itemRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        PackEntry p;
        p.id = (*it)[1].str();
        p.name = (*it)[2].str();
        p.enabled = (*it)[3].str() == "true";
        p.locked = (*it)[4].matched && (*it)[4].str() == "true";
        out.push_back(std::move(p));
    }
    return true;
}

std::string GetOptionValueFromJson(const std::string& json, const std::string& key) {
    const std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    if (std::regex_search(json, m, re) && m.size() > 1) return m[1].str();
    const std::regex numRe("\"" + key + "\"\\s*:\\s*([0-9.+-]+|true|false)");
    if (std::regex_search(json, m, numRe) && m.size() > 1) return m[1].str();
    return {};
}

void SetOptionValueInJson(std::string& json, const std::string& key, const std::string& value) {
    if (json.empty()) {
        const bool bare = value == "true" || value == "false" || value.find_first_not_of("0123456789.-") == std::string::npos;
        if (bare && value != "true" && value != "false") {
            json = "{\"" + key + "\":" + value + "}";
        } else if (value == "true" || value == "false") {
            json = "{\"" + key + "\":" + value + "}";
        } else {
            json = "{\"" + key + "\":\"" + value + "\"}";
        }
        return;
    }
    const std::regex strRe("\"" + key + "\"\\s*:\\s*\"[^\"]*\"");
    if (std::regex_search(json, strRe)) {
        const bool bare = value == "true" || value == "false" || value.find_first_not_of("0123456789.-") == std::string::npos;
        if (bare && value != "true" && value != "false") {
            json = std::regex_replace(json, strRe, "\"" + key + "\":" + value);
        } else if (value == "true" || value == "false") {
            json = std::regex_replace(json, strRe, "\"" + key + "\":" + value);
        } else {
            json = std::regex_replace(json, strRe, "\"" + key + "\":\"" + value + "\"");
        }
        return;
    }
    const std::regex numRe("\"" + key + "\"\\s*:\\s*([0-9.+-]+|true|false)");
    if (std::regex_search(json, numRe)) {
        const bool bare = value == "true" || value == "false" || value.find_first_not_of("0123456789.-") == std::string::npos;
        if (bare && value != "true" && value != "false") {
            json = std::regex_replace(json, numRe, "\"" + key + "\":" + value);
        } else if (value == "true" || value == "false") {
            json = std::regex_replace(json, numRe, "\"" + key + "\":" + value);
        } else {
            json = std::regex_replace(json, numRe, "\"" + key + "\":\"" + value + "\"");
        }
    }
}

static std::string JsonUnescape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[i + 1];
            if (next == '\\' || next == '"') {
                out.push_back(next);
                ++i;
                continue;
            }
        }
        out.push_back(value[i]);
    }
    return out;
}

static std::wstring GetDefaultMinecraftDir() {
    wchar_t appData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH * 2) == 0) return {};
    return std::wstring(appData) + L"\\.minecraft";
}

bool LoadWorldsFromDisk(std::vector<WorldEntry>& out) {
    out.clear();
    const std::wstring savesDir = GetDefaultMinecraftDir() + L"\\saves";
    WIN32_FIND_DATAW data{};
    const HANDLE find = FindFirstFileW((savesDir + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return true;

    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            const wchar_t* name = data.cFileName;
            if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
            char utf8[MAX_PATH * 3]{};
            WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, sizeof(utf8), nullptr, nullptr);
            WorldEntry w;
            w.name = utf8;
            w.mode = "survival";
            out.push_back(std::move(w));
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return true;
}

bool ParsePlayerJson(const std::string& json, ProfileData& out) {
    out.name = GetOptionValueFromJson(json, "name");
    out.uuid = GetOptionValueFromJson(json, "uuid");
    out.account_type = GetOptionValueFromJson(json, "account_type");
    out.skin_url = GetOptionValueFromJson(json, "skin_url");
    out.skin_path = JsonUnescape(GetOptionValueFromJson(json, "skin_path"));
    out.mc_version = GetOptionValueFromJson(json, "mc_version");
    out.loader = GetOptionValueFromJson(json, "loader");
    if (out.account_type.empty()) out.account_type = "offline";
    return !out.name.empty();
}

bool LoadProfileFromDisk(ProfileData& out) {
    out.account_type = "offline";
    out.loader = "Fabric";
    out.mc_version = "Unknown";

    const std::wstring mcDir = GetDefaultMinecraftDir();

    {
        const std::wstring accountsPath = mcDir + L"\\launcher_accounts.json";
        std::ifstream accountsIn(accountsPath);
        if (accountsIn) {
            std::stringstream ss;
            ss << accountsIn.rdbuf();
            const std::string json = ss.str();
            const std::regex activeRe("\"activeAccountLocalId\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch activeMatch;
            std::string activeId;
            if (std::regex_search(json, activeMatch, activeRe) && activeMatch.size() > 1) {
                activeId = activeMatch[1].str();
            }
            if (!activeId.empty()) {
                const std::regex nameRe("\"" + activeId + "\"[^\\{]*\\{[^\\}]*\"minecraftProfile\"\\s*:\\s*\\{[^\\}]*\"name\"\\s*:\\s*\"([^\"]*)\"");
                std::smatch nameMatch;
                if (std::regex_search(json, nameMatch, nameRe) && nameMatch.size() > 1) {
                    out.name = nameMatch[1].str();
                }
                const std::regex uuidRe("\"" + activeId + "\"[^\\{]*\\{[^\\}]*\"minecraftProfile\"\\s*:\\s*\\{[^\\}]*\"id\"\\s*:\\s*\"([^\"]*)\"");
                std::smatch uuidMatch;
                if (std::regex_search(json, uuidMatch, uuidRe) && uuidMatch.size() > 1) {
                    out.uuid = uuidMatch[1].str();
                }
                const std::regex typeRe("\"" + activeId + "\"[^\\{]*\\{[^\\}]*\"type\"\\s*:\\s*\"([^\"]*)\"");
                std::smatch typeMatch;
                if (std::regex_search(json, typeMatch, typeRe) && typeMatch.size() > 1) {
                    const std::string type = typeMatch[1].str();
                    if (type.find("Microsoft") != std::string::npos || type.find("Mojang") != std::string::npos) {
                        out.account_type = "premium";
                    }
                }
            }
        }
    }

    if (out.name.empty()) {
        const std::wstring profilesPath = mcDir + L"\\launcher_profiles.json";
        std::ifstream in(profilesPath);
        if (in) {
            std::stringstream ss;
            ss << in.rdbuf();
            const std::string json = ss.str();
            const std::regex selectedRe("\"selectedProfile\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch selectedMatch;
            if (std::regex_search(json, selectedMatch, selectedRe) && selectedMatch.size() > 1) {
                const std::string profileId = selectedMatch[1].str();
                const std::regex nameRe("\"" + profileId + "\"\\s*:\\s*\\{[^}]*\"name\"\\s*:\\s*\"([^\"]*)\"");
                std::smatch nameMatch;
                if (std::regex_search(json, nameMatch, nameRe) && nameMatch.size() > 1) {
                    out.name = nameMatch[1].str();
                }
            }
            const std::regex versionRe("\"lastVersion\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch versionMatch;
            if (std::regex_search(json, versionMatch, versionRe) && versionMatch.size() > 1) {
                out.mc_version = versionMatch[1].str();
            }
        }
    }

    if (out.name.empty()) {
        const std::wstring cachePath = mcDir + L"\\usercache.json";
        std::ifstream cacheIn(cachePath);
        if (cacheIn) {
            std::stringstream ss;
            ss << cacheIn.rdbuf();
            const std::string cacheJson = ss.str();
            const std::regex nameRe("\\{\\s*\"name\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch nameMatch;
            if (std::regex_search(cacheJson, nameMatch, nameRe) && nameMatch.size() > 1) {
                out.name = nameMatch[1].str();
            }
            const std::regex uuidRe("\"uuid\"\\s*:\\s*\"([^\"]*)\"");
            std::smatch uuidMatch;
            if (std::regex_search(cacheJson, uuidMatch, uuidRe) && uuidMatch.size() > 1) {
                out.uuid = uuidMatch[1].str();
            }
        }
    }

    if (out.name.empty()) {
        out.name = "Player";
    }

    if (out.mc_version == "Unknown") {
        WIN32_FIND_DATAW data{};
        const HANDLE find = FindFirstFileW((mcDir + L"\\versions\\*").c_str(), &data);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    const wchar_t* name = data.cFileName;
                    if (wcscmp(name, L".") != 0 && wcscmp(name, L"..") != 0) {
                        char utf8[MAX_PATH * 3]{};
                        WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, sizeof(utf8), nullptr, nullptr);
                        out.mc_version = utf8;
                        break;
                    }
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }
    }

    out.skin_url = "https://mc-heads.net/avatar/";
    if (out.account_type == "premium" && !out.uuid.empty()) {
        std::string flatUuid = out.uuid;
        flatUuid.erase(std::remove(flatUuid.begin(), flatUuid.end(), '-'), flatUuid.end());
        out.skin_url += flatUuid;
    } else {
        out.skin_url += out.name;
    }
    out.skin_url += "/80";

    if (!out.uuid.empty()) {
        std::string flatUuid = out.uuid;
        flatUuid.erase(std::remove(flatUuid.begin(), flatUuid.end(), '-'), flatUuid.end());
        const std::wstring skinPath = mcDir + L"\\skins\\" + std::wstring(flatUuid.begin(), flatUuid.end()) + L".png";
        if (GetFileAttributesW(skinPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            char utf8[MAX_PATH * 3]{};
            WideCharToMultiByte(CP_UTF8, 0, skinPath.c_str(), -1, utf8, sizeof(utf8), nullptr, nullptr);
            out.skin_path = utf8;
        } else {
            wchar_t localAppData[MAX_PATH * 2]{};
            if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) > 0) {
                const std::wstring cachePath = std::wstring(localAppData) + L"\\MyiUI\\skin-cache\\" +
                                               std::wstring(flatUuid.begin(), flatUuid.end()) + L".png";
                if (GetFileAttributesW(cachePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    char utf8[MAX_PATH * 3]{};
                    WideCharToMultiByte(CP_UTF8, 0, cachePath.c_str(), -1, utf8, sizeof(utf8), nullptr, nullptr);
                    out.skin_path = utf8;
                }
            }
        }
    }
    return true;
}

bool LoadServersFromDisk(std::vector<ServerEntry>& out) {
    out.clear();
    const std::wstring jsonPath = GetDefaultMinecraftDir() + L"\\servers.json";
    std::ifstream in(jsonPath);
    if (!in) return true;
    std::stringstream ss;
    ss << in.rdbuf();
    return ParseServersJson(ss.str(), out);
}
