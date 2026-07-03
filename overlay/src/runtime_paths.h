#pragma once

#include <string>

namespace myiui::overlay {

std::wstring ResolveProjectRoot();
void OverlayLog(const char* message);

}  // namespace myiui::overlay
