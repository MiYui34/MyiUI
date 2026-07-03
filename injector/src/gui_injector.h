#pragma once

#include "process_scanner.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace myiui::injector_ui {

enum class LogLevel { Info, Ok, Warn, Err };

struct LogLine {
    std::wstring text;
    LogLevel level = LogLevel::Info;
};

enum class PillState { Idle, Ready, Loading, Success, Error };

struct GuiState {
    HWND hwnd = nullptr;
    std::vector<myiui::JavaProcessInfo> processes;
    int selectedIndex = -1;
    bool autoRefresh = false;
    float autoRefreshAccum = 0.f;
    bool refreshing = false;
    float refreshSpinner = 0.f;
    double lastRefreshTime = 0.0;
    std::atomic<bool> scanInFlight{false};
    bool scanResultReady = false;
    bool scanWasManual = false;
    DWORD scanPreservePid = 0;
    std::vector<myiui::JavaProcessInfo> scannedProcesses;
    std::mutex scanMutex;

    std::vector<LogLine> logs;
    std::mutex logMutex;

    bool injecting = false;
    std::atomic<bool> injectCancel{false};
    std::atomic<bool> injectFinished{false};
    std::atomic<bool> injectSuccess{false};
    DWORD injectTargetPid = 0;
    DWORD injectedPid = 0;
    int injectStep = -1;
    float injectAnim = 0.f;
    float injectOverlayAlpha = 0.f;
    float injectProgressVisual = 0.f;
    float injectPanelScale = 0.88f;
    bool injectFlash = false;
    float injectFlashTimer = 0.f;
    bool injectDismissPending = false;
    bool injectDismissOk = false;

    PillState pillState = PillState::Idle;
    std::wstring pillText = L"未选择";
    std::wstring injectorState = L"就绪";
    std::wstring injectorMeta = L"Agent 已加载 · Overlay 待写入";
    std::wstring lastInjectTime = L"—";

    float toastTimer = 0.f;
    std::wstring toastMessage;
    bool toastError = false;

    bool reduceMotion = false;
};

void Init(GuiState& state, HWND hwnd);
void QueueLog(GuiState& state, const std::wstring& line, LogLevel level);
void FlushPendingLogs(GuiState& state);
void RefreshProcesses(GuiState& state, bool manual = false);
void Render(GuiState& state);

}  // namespace myiui::injector_ui
