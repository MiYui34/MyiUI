#include "jvm_log.h"

#include <windows.h>

#include <cstdarg>
#include <fstream>
#include <string>

namespace myiui::jvm {

namespace {

void AppendLog(const wchar_t* fileName, const wchar_t* message) {
    wchar_t localAppData[MAX_PATH * 2]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH * 2) == 0) {
        return;
    }
    const std::wstring dir = std::wstring(localAppData) + L"\\MyiUI";
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wofstream out(dir + L"\\" + fileName, std::ios::app);
    if (out) {
        out << message << L"\n";
    }
}

}  // namespace

void SpikeLog(const wchar_t* message) {
    AppendLog(L"spike.log", message);
}

void SpikeLogf(const wchar_t* fmt, ...) {
    wchar_t buf[1024]{};
    va_list args;
    va_start(args, fmt);
    vswprintf_s(buf, fmt, args);
    va_end(args);
    SpikeLog(buf);
}

}  // namespace myiui::jvm
