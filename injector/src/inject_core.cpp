#include "inject_core.h"

#include <windows.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace myiui {

namespace {

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path);
    const auto pos = s.find_last_of(L"\\/");
    if (pos != std::wstring::npos) s.resize(pos);
    return s;
}

bool FileExists(const std::wstring& path) {
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void DefaultLog(const LogFn& log, const std::wstring& line, bool isError) {
    if (log) {
        log(line, isError);
        return;
    }
    if (isError) {
        fwprintf(stderr, L"%ls\n", line.c_str());
    } else {
        fwprintf(stdout, L"%ls\n", line.c_str());
    }
}

bool HasNonAsciiPath(const std::wstring& path) {
    for (wchar_t ch : path) {
        if (ch > 127) return true;
    }
    return false;
}

bool EnableDebugPrivilege() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }

    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid)) {
        CloseHandle(token);
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    tp.Privileges[0].Luid = luid;
    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    const bool ok = GetLastError() == ERROR_SUCCESS;
    CloseHandle(token);
    return ok;
}

bool GetFileWriteTime(const std::wstring& path, FILETIME* out) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
    *out = data.ftLastWriteTime;
    return true;
}

std::wstring MakeVersionedCacheName(const std::wstring& stableName, const FILETIME& srcTime) {
    const auto dot = stableName.find_last_of(L'.');
    const std::wstring base = dot != std::wstring::npos ? stableName.substr(0, dot) : stableName;
    const std::wstring ext = dot != std::wstring::npos ? stableName.substr(dot) : L"";
    wchar_t buf[256]{};
    swprintf(buf, 256, L"%ls.%08x%08x%ls", base.c_str(), srcTime.dwHighDateTime, srcTime.dwLowDateTime,
             ext.c_str());
    return buf;
}

std::wstring StageToAsciiCache(const std::wstring& srcPath, const std::wstring& cacheFileName,
                               const wchar_t* label, const LogFn& log) {
    if (!HasNonAsciiPath(srcPath)) {
        return srcPath;
    }

    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) == 0) {
        DefaultLog(log, std::wstring(L"[MyiUI] 路径含非 ASCII 字符，且无法访问 LOCALAPPDATA。"), true);
        return srcPath;
    }

    const std::wstring cacheDir = std::wstring(localAppData) + L"\\MyiUI\\attach-cache";
    CreateDirectoryW((std::wstring(localAppData) + L"\\MyiUI").c_str(), nullptr);
    CreateDirectoryW(cacheDir.c_str(), nullptr);

    const std::wstring stableDst = cacheDir + L"\\" + cacheFileName;
    FILETIME srcTime{};
    if (!GetFileWriteTime(srcPath, &srcTime)) {
        return srcPath;
    }

    FILETIME dstTime{};
    const bool hasStable = GetFileWriteTime(stableDst, &dstTime);
    if (hasStable && CompareFileTime(&srcTime, &dstTime) == 0) {
        DefaultLog(log, std::wstring(L"[MyiUI] 使用缓存 ") + label + L": " + stableDst, false);
        return stableDst;
    }

    const std::wstring versionedDst = cacheDir + L"\\" + MakeVersionedCacheName(cacheFileName, srcTime);
    if (FileExists(versionedDst)) {
        DefaultLog(log, std::wstring(L"[MyiUI] 使用版本缓存 ") + label + L": " + versionedDst, false);
        return versionedDst;
    }

    DefaultLog(log, std::wstring(L"[MyiUI] 复制 ") + label + L" 到 ASCII 缓存路径…", false);
    if (CopyFileW(srcPath.c_str(), stableDst.c_str(), FALSE)) {
        DefaultLog(log, std::wstring(L"[MyiUI] 使用缓存 ") + label + L": " + stableDst, false);
        return stableDst;
    }

    const DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION) {
        DefaultLog(log, std::wstring(L"[MyiUI] 缓存 ") + label +
                               L" 被游戏占用，写入新版本文件…",
                   false);
        if (CopyFileW(srcPath.c_str(), versionedDst.c_str(), FALSE)) {
            DefaultLog(log, std::wstring(L"[MyiUI] 使用版本缓存 ") + label + L": " + versionedDst, false);
            return versionedDst;
        }
        DefaultLog(log, std::wstring(L"[MyiUI] 复制 ") + label +
                               L" 到版本缓存失败: " + std::to_wstring(GetLastError()),
                   true);
        return srcPath;
    }

    DefaultLog(log, std::wstring(L"[MyiUI] 复制 ") + label + L" 失败: " + std::to_wstring(err), true);
    return srcPath;
}

std::wstring StageOverlayDllIfNeeded(const std::wstring& dllPath, const LogFn& log) {
    return StageToAsciiCache(dllPath, L"myiui-overlay.dll", L"overlay", log);
}

bool EnsureDirectoryTree(const std::wstring& dir) {
    if (dir.empty()) return false;
    if (FileExists(dir)) return true;
    const auto pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos && !EnsureDirectoryTree(dir.substr(0, pos))) {
        return false;
    }
    return CreateDirectoryW(dir.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool CopyFileToRuntime(const std::wstring& src, const std::wstring& dst, const LogFn& log) {
    if (!FileExists(src)) return false;
    if (!EnsureDirectoryTree(dst.substr(0, dst.find_last_of(L"\\/")))) {
        DefaultLog(log, L"[MyiUI] 创建目录失败: " + dst, true);
        return false;
    }
    if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
        DefaultLog(log, L"[MyiUI] 复制失败 (" + std::to_wstring(GetLastError()) + L"): " + src, true);
        return false;
    }
    return true;
}

int CopyRuntimeFontAssets(const std::wstring& root, const std::wstring& runtimeRoot, const LogFn& log) {
    const std::wstring srcDir = root + L"\\assets\\fonts";
    if (!FileExists(srcDir)) return 0;

    int copied = 0;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((srcDir + L"\\*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return 0;

    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::wstring name = data.cFileName;
        const auto dot = name.find_last_of(L'.');
        if (dot == std::wstring::npos) continue;
        const std::wstring ext = name.substr(dot);
        if (_wcsicmp(ext.c_str(), L".ttf") != 0 && _wcsicmp(ext.c_str(), L".otf") != 0) continue;
        if (CopyFileToRuntime(srcDir + L"\\" + name, runtimeRoot + L"\\assets\\fonts\\" + name, log)) {
            ++copied;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return copied;
}

int CopyRuntimeLogoAssets(const std::wstring& root, const std::wstring& runtimeRoot, const LogFn& log) {
    const std::wstring srcDir = root + L"\\assets\\logos\\png";
    if (!FileExists(srcDir)) return 0;

    int copied = 0;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((srcDir + L"\\*.png").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return 0;

    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::wstring name = data.cFileName;
        if (CopyFileToRuntime(srcDir + L"\\" + name, runtimeRoot + L"\\assets\\logos\\png\\" + name, log)) {
            ++copied;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return copied;
}

void PublishProjectRoot(const std::wstring& root, const LogFn& log) {
    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) == 0) {
        DefaultLog(log, L"[MyiUI] 无法访问 LOCALAPPDATA，跳过 project_root 发布。", true);
        return;
    }

    const std::wstring myiuiDir = std::wstring(localAppData) + L"\\MyiUI";
    CreateDirectoryW(myiuiDir.c_str(), nullptr);

    const std::wstring markerPath = myiuiDir + L"\\project_root.txt";
    {
        std::wofstream out(markerPath, std::ios::trunc);
        if (!out) {
            DefaultLog(log, L"[MyiUI] 无法写入 project_root.txt", true);
            return;
        }
        out << root;
    }
    DefaultLog(log, L"[MyiUI] 已写入 project_root.txt", false);

    const std::wstring runtimeRoot = myiuiDir + L"\\runtime";
    const wchar_t* relPaths[] = {
        L"config\\menu\\theme.json",
        L"config\\menu\\background.json",
        L"config\\menu\\layout.json",
        L"design\\v1\\layout.json",
        L"design\\v1\\motion.json",
        L"design\\v1\\components.json",
        L"design\\v1\\tokens.json",
        L"design\\v1\\screens\\singleplayer.json",
        L"design\\v1\\screens\\multiplayer.json",
        L"design\\v1\\screens\\options_hub.json",
        L"design\\v1\\screens\\options_video.json",
        L"design\\v1\\screens\\options_sound.json",
        L"design\\v1\\screens\\options_controls.json",
        L"design\\v1\\screens\\options_language.json",
        L"design\\v1\\screens\\options_chat.json",
        L"design\\v1\\screens\\options_accessibility.json",
        L"design\\v1\\screens\\options_skin.json",
        L"design\\v1\\screens\\options_resource_packs.json",
        L"design\\v1\\screens\\create_world.json",
        L"design\\v1\\screens\\add_server.json",
        L"design\\v1\\intro\\timeline.json",
        L"design\\v1\\intro\\tokens-intro.json",
        L"design\\v1\\intro\\particles-spec.json",
        L"design\\v1\\intro\\layer-map.json",
    };
    int copied = 0;
    for (const wchar_t* rel : relPaths) {
        if (CopyFileToRuntime(root + L"\\" + rel, runtimeRoot + L"\\" + rel, log)) {
            ++copied;
        }
    }
    DefaultLog(log, L"[MyiUI] 已同步 runtime 配置 (" + std::to_wstring(copied) + L" 个文件) 到 " + runtimeRoot,
               false);

    const int fontCopied = CopyRuntimeFontAssets(root, runtimeRoot, log);
    if (fontCopied > 0) {
        DefaultLog(log, L"[MyiUI] 已同步字体 (" + std::to_wstring(fontCopied) + L" 个文件) 到 runtime/assets/fonts",
                   false);
    }

    const int logoCopied = CopyRuntimeLogoAssets(root, runtimeRoot, log);
    if (logoCopied > 0) {
        DefaultLog(log, L"[MyiUI] 已同步 Logo (" + std::to_wstring(logoCopied) + L" 个文件) 到 runtime/assets/logos/png",
                   false);
    }
}

}  // namespace

std::wstring GetProjectRoot() {
    wchar_t env[1024]{};
    if (GetEnvironmentVariableW(L"MYIUI_ROOT", env, 1024) > 0) {
        return env;
    }
    std::wstring exe = GetExeDirectory();
    if (exe.ends_with(L"Release") || exe.ends_with(L"Debug")) {
        exe = exe.substr(0, exe.find_last_of(L"\\/"));
    }
    if (exe.ends_with(L"injector")) {
        exe = exe.substr(0, exe.find_last_of(L"\\/"));
    }
    if (exe.ends_with(L"build")) {
        exe = exe.substr(0, exe.find_last_of(L"\\/"));
    }
    return exe;
}

std::wstring FindAgentJar(const std::wstring& root) {
    const std::wstring libs = root + L"\\agent\\build\\libs\\";
    const wchar_t* candidates[] = {
        L"myiui-agent-1.0.0.jar",
        L"myiui-agent-1.0.jar",
        L"myiui-agent.jar",
    };
    for (const wchar_t* name : candidates) {
        const std::wstring path = libs + name;
        if (FileExists(path)) return path;
    }
    return libs + L"myiui-agent-1.0.0.jar";
}

std::wstring FindOverlayDll(const std::wstring& root) {
    return root + L"\\build\\overlay\\Release\\myiui-overlay.dll";
}

bool EnsureMyiuiRootEnv(const std::wstring& root) {
    const std::wstring envSet = L"MYIUI_ROOT=" + root;
    return _wputenv(envSet.c_str()) == 0;
}

bool InjectDll(DWORD pid, const std::wstring& dllPath, const LogFn& log) {
    if (!FileExists(dllPath)) {
        DefaultLog(log, L"[MyiUI] DLL not found: " + dllPath, true);
        return false;
    }

    EnableDebugPrivilege();

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                     PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        DefaultLog(log, L"[MyiUI] OpenProcess failed: " + std::to_wstring(GetLastError()) +
                               L" — 请尝试以管理员身份运行注入器。",
                   true);
        return false;
    }

    const size_t size = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        DefaultLog(log, L"[MyiUI] VirtualAllocEx failed: " + std::to_wstring(GetLastError()), true);
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, remote, dllPath.c_str(), size, nullptr)) {
        DefaultLog(log, L"[MyiUI] WriteProcessMemory failed: " + std::to_wstring(GetLastError()), true);
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(kernel32, "LoadLibraryW");
    HANDLE thread = CreateRemoteThread(process, nullptr, 0,
                                       reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary), remote, 0, nullptr);
    if (!thread) {
        DefaultLog(log, L"[MyiUI] CreateRemoteThread failed: " + std::to_wstring(GetLastError()), true);
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);

    if (exitCode == 0) {
        DefaultLog(log, L"[MyiUI] LoadLibrary returned NULL — DLL may have failed to initialize.", true);
        return false;
    }
    return true;
}

bool RunInjection(DWORD pid, const LogFn& log, const CancelFn& shouldCancel) {
    const auto cancelled = [&]() { return shouldCancel && shouldCancel(); };

    const std::wstring root = GetProjectRoot();
    const std::wstring overlayDll = FindOverlayDll(root);

    if (cancelled()) return false;

    EnsureMyiuiRootEnv(root);
    PublishProjectRoot(root, log);
    DefaultLog(log, L"[MyiUI] Root: " + root, false);
    DefaultLog(log, L"[MyiUI] Target PID: " + std::to_wstring(pid), false);

    if (cancelled()) return false;

    if (!InjectDll(pid, StageOverlayDllIfNeeded(overlayDll, log), log)) {
        DefaultLog(log, L"[MyiUI] DLL injection failed.", true);
        return false;
    }

    if (cancelled()) return false;

    DefaultLog(log, L"[MyiUI] Injection complete for PID " + std::to_wstring(pid), false);
    return true;
}

}  // namespace myiui
