#pragma once

#include <string>

namespace myiui {

std::wstring GetLocalAppDataMyiuiDir();
bool WriteProjectRootMarker(const std::wstring& root);
bool SyncRuntimeConfigToLocalAppData(const std::wstring& root);

}  // namespace myiui
