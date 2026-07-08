#pragma once

#include <string>

namespace myiui::bridge {

void EnqueueOverlayCommand(std::string command);
void ShutdownOverlayCommandQueue();

}  // namespace myiui::bridge
