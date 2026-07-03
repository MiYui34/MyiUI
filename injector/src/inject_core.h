#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace myiui {

using LogFn = std::function<void(const std::wstring& line, bool isError)>;
using CancelFn = std::function<bool()>;

std::wstring GetProjectRoot();
std::wstring FindAgentJar(const std::wstring& root);
std::wstring FindOverlayDll(const std::wstring& root);

bool EnsureMyiuiRootEnv(const std::wstring& root);
bool InjectDll(DWORD pid, const std::wstring& dllPath, const LogFn& log = {});
bool RunInjection(DWORD pid, const LogFn& log = {}, const CancelFn& shouldCancel = {});

}  // namespace myiui
