#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace myiui {

struct JavaProcessInfo {
    DWORD pid = 0;
    std::wstring exeName;
    std::uint64_t workingSetBytes = 0;
    std::wstring commandLine;
    std::wstring hint;
    int javaMajor = 0;
    std::wstring javaHome;
    bool recommended = false;
    int score = 0;
};

std::vector<JavaProcessInfo> ScanJavaProcesses();
std::wstring FormatMemory(std::uint64_t bytes);

}  // namespace myiui
