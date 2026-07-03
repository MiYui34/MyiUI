#include "ipc/pipe_client.h"

#include "bridge/native_query.h"

#include <thread>

bool PipeSendCommand(const std::string& command) {
    return myiui::bridge::ActionJava(command);
}

bool PipeSendCommandWait(const std::string& command) {
    return myiui::bridge::ActionJava(command);
}

bool PipeSendCommandWaitMs(const std::string& command, int /*timeoutMs*/) {
    return myiui::bridge::ActionJava(command);
}

void PipeSendCommandAsync(const std::string& command) {
    std::thread([command]() { myiui::bridge::ActionJavaAsync(command); }).detach();
}

PipeQueryResult PipeQueryJson(const std::string& command, int timeoutMs) {
    const auto result = myiui::bridge::QueryJava(command, timeoutMs);
    PipeQueryResult out{};
    out.ok = result.ok;
    out.body = result.body;
    out.error = result.error;
    return out;
}
