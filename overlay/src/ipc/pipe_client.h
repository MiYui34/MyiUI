#pragma once

#include <string>

struct PipeQueryResult {
    bool ok = false;
    std::string body;
    std::string error;
};

bool PipeSendCommand(const std::string& command);
bool PipeSendCommandWait(const std::string& command);
bool PipeSendCommandWaitMs(const std::string& command, int timeoutMs);
void PipeSendCommandAsync(const std::string& command);
PipeQueryResult PipeQueryJson(const std::string& command, int timeoutMs);
