#include "runtime_sync.h"

#include <windows.h>

#include <fstream>
#include <string>

namespace myiui {

std::wstring GetLocalAppDataMyiuiDir() {
    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) == 0) {
        return L"";
    }
    return std::wstring(localAppData) + L"\\MyiUI";
}

static bool EnsureDirectory(const std::wstring& path) {
    return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool CopyIfExists(const std::wstring& src, const std::wstring& dst) {
    if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    EnsureDirectory(dst.substr(0, dst.find_last_of(L"\\/")));
    return CopyFileW(src.c_str(), dst.c_str(), FALSE) != 0;
}

bool WriteProjectRootMarker(const std::wstring& root) {
    const std::wstring base = GetLocalAppDataMyiuiDir();
    if (base.empty()) return false;
    EnsureDirectory(base);

    const std::wstring marker = base + L"\\project_root.txt";
    std::ofstream out(marker, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    const int bytes = WideCharToMultiByte(CP_UTF8, 0, root.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return false;
    std::string utf8(bytes - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, root.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);
    out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return out.good();
}

bool SyncRuntimeConfigToLocalAppData(const std::wstring& root) {
    const std::wstring base = GetLocalAppDataMyiuiDir();
    if (base.empty()) return false;
    EnsureDirectory(base);
    EnsureDirectory(base + L"\\config\\menu");
    EnsureDirectory(base + L"\\design\\v1");

    const struct {
        const wchar_t* rel;
    } files[] = {
        {L"\\config\\menu\\theme.json"},
        {L"\\config\\menu\\layout.json"},
        {L"\\config\\menu\\motion.json"},
        {L"\\config\\menu\\background.json"},
        {L"\\design\\v1\\layout.json"},
        {L"\\design\\v1\\motion.json"},
    };

    bool copiedAny = false;
    for (const auto& file : files) {
        const std::wstring src = root + file.rel;
        const std::wstring dst = base + file.rel;
        if (CopyIfExists(src, dst)) {
            copiedAny = true;
        }
    }
    return copiedAny;
}

}  // namespace myiui
