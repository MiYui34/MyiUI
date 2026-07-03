#include "process_scanner.h"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwchar>

namespace myiui {

std::wstring FormatMemory(std::uint64_t bytes) {
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        wchar_t buf[32]{};
        swprintf(buf, 32, L"%.1f GB", mb / 1024.0);
        return buf;
    }
    wchar_t buf[32]{};
    swprintf(buf, 32, L"%.0f MB", mb);
    return buf;
}

static std::wstring ToLower(std::wstring s) {
    for (wchar_t& c : s) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return s;
}

static bool ContainsInsensitive(const std::wstring& haystack, const wchar_t* needle) {
    return ToLower(haystack).find(needle) != std::wstring::npos;
}

static std::wstring ReadProcessCommandLine(HANDLE process) {
    using NtQueryInformationProcessFn = LONG(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static const auto fn = reinterpret_cast<NtQueryInformationProcessFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
    if (!fn) return L"";

    struct UnicodeString {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    };
    struct ProcessCommandLineInfo {
        UnicodeString CommandLine;
    } info{};

    constexpr ULONG kProcessCommandLineInformation = 60;
    ULONG returnLength = 0;
    const LONG status = fn(process, kProcessCommandLineInformation, &info, sizeof(info), &returnLength);
    if (status < 0 || !info.CommandLine.Buffer || info.CommandLine.Length == 0) {
        return L"";
    }

    const size_t charCount = info.CommandLine.Length / sizeof(wchar_t);
    std::vector<wchar_t> buffer(charCount + 1, L'\0');
    SIZE_T read = 0;
    if (!ReadProcessMemory(process, info.CommandLine.Buffer, buffer.data(), info.CommandLine.Length, &read)) {
        return L"";
    }
    return std::wstring(buffer.data(), charCount);
}

static std::wstring SummarizeCommandLine(const std::wstring& cmd) {
    if (cmd.empty()) return L"—";
    std::wstring lower = ToLower(cmd);
    std::wstring summary;

    auto appendToken = [&](const wchar_t* token) {
        if (ContainsInsensitive(cmd, token)) {
            if (!summary.empty()) summary += L" · ";
            summary += token;
        }
    };

    appendToken(L"net.minecraft");
    appendToken(L"minecraft");
    appendToken(L"fabric");
    appendToken(L"forge");
    appendToken(L"launcher");

    if (!summary.empty()) return summary;

    if (cmd.size() > 48) {
        return cmd.substr(0, 45) + L"…";
    }
    return cmd;
}

static int ScoreProcess(const JavaProcessInfo& info) {
    int score = 0;
    const std::wstring lower = ToLower(info.commandLine);

    if (lower.find(L"net.minecraft.client.main.main") != std::wstring::npos) score += 200;
    if (lower.find(L"net.minecraft") != std::wstring::npos) score += 120;
    else if (lower.find(L"minecraft") != std::wstring::npos) score += 100;

    if (lower.find(L"fabric") != std::wstring::npos) score += 40;
    if (lower.find(L"forge") != std::wstring::npos) score += 30;

    if (info.workingSetBytes >= 1024ULL * 1024 * 1024) score += 35;
    else if (info.workingSetBytes >= 512ULL * 1024 * 1024) score += 20;

    if (lower.find(L"launcher") != std::wstring::npos && lower.find(L"minecraft") == std::wstring::npos) {
        score -= 60;
    }

    return score;
}

static std::wstring BuildHint(const JavaProcessInfo& info) {
    const std::wstring lower = ToLower(info.commandLine);
    std::wstring hint;
    if (info.recommended) {
        if (lower.find(L"minecraft") != std::wstring::npos) hint = L"推荐 · Minecraft";
        else hint = L"推荐 · 高内存 Java";
    } else if (lower.find(L"launcher") != std::wstring::npos && lower.find(L"minecraft") == std::wstring::npos) {
        hint = L"可能是启动器";
    } else if (lower.find(L"minecraft") != std::wstring::npos) {
        hint = L"Minecraft 相关";
    } else {
        hint = L"Java 进程";
    }

    if (info.javaMajor > 0) {
        hint += L" · Java " + std::to_wstring(info.javaMajor);
    }
    return hint;
}

std::vector<JavaProcessInfo> ScanJavaProcesses() {
    std::vector<JavaProcessInfo> result;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"javaw.exe") != 0 && _wcsicmp(pe.szExeFile, L"java.exe") != 0) {
                continue;
            }

            JavaProcessInfo info{};
            info.pid = pe.th32ProcessID;
            info.exeName = pe.szExeFile;

            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, info.pid);
            if (process) {
                PROCESS_MEMORY_COUNTERS pmc{};
                if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
                    info.workingSetBytes = pmc.WorkingSetSize;
                }
                info.commandLine = ReadProcessCommandLine(process);
                CloseHandle(process);
            }

            info.javaMajor = 21;
            info.javaHome = L"";

            info.score = ScoreProcess(info);
            info.hint = SummarizeCommandLine(info.commandLine);
            result.push_back(std::move(info));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    std::sort(result.begin(), result.end(), [](const JavaProcessInfo& a, const JavaProcessInfo& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.workingSetBytes > b.workingSetBytes;
    });

    if (!result.empty()) {
        const bool topLooksLikeGame =
            result.front().score >= 100 ||
            ToLower(result.front().commandLine).find(L"net.minecraft.client.main.main") != std::wstring::npos;
        if (topLooksLikeGame) {
            result.front().recommended = true;
            result.front().hint = BuildHint(result.front());
        }
    }

    for (size_t i = 1; i < result.size(); ++i) {
        result[i].hint = BuildHint(result[i]);
    }

    return result;
}

}  // namespace myiui
