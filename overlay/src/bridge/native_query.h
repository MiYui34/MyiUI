#pragma once

#include <string>

namespace myiui::bridge {

struct QueryResult {
    bool ok = false;
    std::string body;
    std::string error;
};

QueryResult QueryJava(const std::string& command, int timeoutMs = 8000);
bool ActionJava(const std::string& command);
void ActionJavaAsync(const std::string& command);

}  // namespace myiui::bridge
