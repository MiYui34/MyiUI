#pragma once

#include "config/config_loader.h"

#include <string>

namespace myiui::overlay {

void OverlayLog(const wchar_t* message);
std::wstring ResolveProjectRoot();
bool LoadAppConfigWithFallback(AppConfig& out);

}  // namespace myiui::overlay
