#include "gui_injector.h"

#include "inject_core.h"
#include "logo_assets.h"

#include <windows.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// ─────────────────────────────────────────────────────────────────────────────
//  Palette — dark glassmorphism, consistent with the in-game overlay theme.
// ─────────────────────────────────────────────────────────────────────────────
struct Palette {
    ImU32 bgTop = IM_COL32(20, 24, 34, 255);
    ImU32 bgBottom = IM_COL32(10, 12, 18, 255);
    ImU32 glow = IM_COL32(84, 140, 230, 46);

    ImU32 card = IM_COL32(25, 29, 40, 232);
    ImU32 cardInset = IM_COL32(0, 0, 0, 46);
    ImU32 soft = IM_COL32(255, 255, 255, 12);
    ImU32 border = IM_COL32(122, 142, 178, 44);
    ImU32 highlight = IM_COL32(255, 255, 255, 18);

    ImU32 text = IM_COL32(238, 241, 247, 255);
    ImU32 muted = IM_COL32(151, 159, 174, 255);
    ImU32 faint = IM_COL32(108, 116, 132, 255);

    ImU32 accent = IM_COL32(96, 170, 255, 255);
    ImU32 accentHi = IM_COL32(132, 192, 255, 255);
    ImU32 accentLo = IM_COL32(70, 138, 236, 255);
    ImU32 accentSoft = IM_COL32(96, 170, 255, 40);
    ImU32 onAccent = IM_COL32(14, 20, 32, 255);

    ImU32 ok = IM_COL32(120, 212, 152, 255);
    ImU32 warn = IM_COL32(240, 196, 112, 255);
    ImU32 err = IM_COL32(240, 122, 112, 255);

    ImU32 rowHover = IM_COL32(255, 255, 255, 14);
    ImU32 rowSel = IM_COL32(96, 170, 255, 34);
    ImU32 rowSelBar = IM_COL32(96, 170, 255, 255);
};

Palette P;

constexpr float kPad = 22.f;
constexpr float kTopBarH = 60.f;
constexpr float kGap = 16.f;
constexpr float kRightColW = 322.f;
constexpr float kLogConsoleH = 158.f;
constexpr float kFooterH = 22.f;
constexpr float kAutoRefreshSec = 8.f;

// ─────────────────────────────────────────────────────────────────────────────
//  Small helpers
// ─────────────────────────────────────────────────────────────────────────────
ImVec4 U32ToVec4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                        nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), len, nullptr, nullptr);
    return out;
}

ImVec2 MeasureText(const char* txt, float size) {
    ImFont* f = ImGui::GetFont();
    if (size <= 0.f) size = f->FontSize;
    return f->CalcTextSizeA(size, FLT_MAX, 0.f, txt);
}

void DrawText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* txt, float size = 0.f) {
    ImFont* f = ImGui::GetFont();
    if (size <= 0.f) size = f->FontSize;
    dl->AddText(f, size, pos, col, txt);
}

void DrawTextCentered(ImDrawList* dl, ImVec2 center, ImU32 col, const char* txt, float size = 0.f) {
    const ImVec2 ts = MeasureText(txt, size);
    DrawText(dl, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), col, txt, size);
}

ImU32 LogColor(myiui::injector_ui::LogLevel level) {
    switch (level) {
        case myiui::injector_ui::LogLevel::Ok:
            return P.ok;
        case myiui::injector_ui::LogLevel::Warn:
            return P.warn;
        case myiui::injector_ui::LogLevel::Err:
            return P.err;
        default:
            return P.text;
    }
}

bool IsMcProcess(const myiui::JavaProcessInfo& proc) {
    auto lower = proc.commandLine;
    for (wchar_t& c : lower) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return lower.find(L"minecraft") != std::wstring::npos;
}

std::wstring BuildTag(const myiui::JavaProcessInfo& proc) {
    std::wstring tag = proc.hint.empty() ? L"Java 进程" : proc.hint;
    if (proc.javaMajor > 0 && tag.find(L"JDK") == std::wstring::npos) {
        tag += L" · JDK " + std::to_wstring(proc.javaMajor);
    }
    return tag;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Card & control primitives
// ─────────────────────────────────────────────────────────────────────────────
void DrawVerticalGradient(ImDrawList* dl, ImVec2 mn, ImVec2 mx, ImU32 top, ImU32 bottom) {
    dl->AddRectFilledMultiColor(mn, mx, top, top, bottom, bottom);
}

void DrawCard(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding = 14.f) {
    dl->AddRectFilled(mn, mx, P.card, rounding);
    dl->AddRect(mn, mx, P.border, rounding, 0, 1.f);
    // top inner highlight for a subtle glass edge
    dl->AddLine(ImVec2(mn.x + rounding * 0.6f, mn.y + 1.f), ImVec2(mx.x - rounding * 0.6f, mn.y + 1.f), P.highlight,
                1.f);
}

// A quiet, bordered button (refresh / auto-refresh / clear).
bool GhostButton(const char* id, const char* label, ImVec2 pos, ImVec2 size, bool active = false,
                 bool disabled = false) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginDisabled(disabled);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool activated = ImGui::IsItemActivated();
    ImGui::EndDisabled();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mx(pos.x + size.x, pos.y + size.y);
    ImU32 bg = active ? P.accentSoft : (hovered ? P.soft : IM_COL32(0, 0, 0, 0));
    ImU32 bd = active ? P.accent : (hovered ? P.border : IM_COL32(122, 142, 178, 26));
    ImU32 fg = disabled ? P.faint : (active ? P.accentHi : (hovered ? P.text : P.muted));
    dl->AddRectFilled(pos, mx, bg, 8.f);
    dl->AddRect(pos, mx, bd, 8.f, 0, 1.f);
    DrawTextCentered(dl, ImVec2((pos.x + mx.x) * 0.5f, (pos.y + mx.y) * 0.5f), fg, label);
    return activated && !disabled;
}

// The primary call-to-action (inject).
bool PrimaryButton(const char* id, const char* label, ImVec2 pos, ImVec2 size, bool disabled) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginDisabled(disabled);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool activated = ImGui::IsItemActivated();
    ImGui::EndDisabled();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mx(pos.x + size.x, pos.y + size.y);
    const float rounding = 12.f;

    if (!disabled) {
        const float pulse = 0.32f + 0.16f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.0f);
        for (int i = 3; i >= 1; --i) {
            ImVec4 c = U32ToVec4(P.accent);
            c.w = 0.11f * pulse / static_cast<float>(i);
            const float e = 4.f * static_cast<float>(i);
            dl->AddRectFilled(ImVec2(pos.x - e, pos.y - e), ImVec2(mx.x + e, mx.y + e),
                              ImGui::ColorConvertFloat4ToU32(c), rounding + e);
        }
    }

    ImU32 fill = disabled ? IM_COL32(58, 66, 84, 150) : (hovered ? P.accentHi : P.accent);
    const float sink = held ? 1.f : 0.f;
    dl->AddRectFilled(ImVec2(pos.x, pos.y + sink), ImVec2(mx.x, mx.y + sink), fill, rounding);
    if (!disabled) {
        dl->AddLine(ImVec2(pos.x + rounding, pos.y + 1.f + sink), ImVec2(mx.x - rounding, pos.y + 1.f + sink),
                    IM_COL32(255, 255, 255, 60), 1.f);
    }
    const ImU32 fg = disabled ? P.faint : P.onAccent;
    DrawTextCentered(dl, ImVec2((pos.x + mx.x) * 0.5f, (pos.y + mx.y) * 0.5f + sink), fg, label);
    return activated && !disabled;
}

void DrawStatusPill(ImDrawList* dl, ImVec2 pos, ImU32 dotColor, const char* text, bool pulse) {
    const ImVec2 ts = MeasureText(text, 0.f);
    const float h = ts.y + 10.f;
    const float w = ts.x + 34.f;
    const ImVec2 mx(pos.x + w, pos.y + h);
    dl->AddRectFilled(pos, mx, P.soft, h * 0.5f);
    dl->AddRect(pos, mx, P.border, h * 0.5f, 0, 1.f);
    const float a = pulse ? (0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f)) : 1.f;
    ImVec4 dc = U32ToVec4(dotColor);
    dc.w = a;
    dl->AddCircleFilled(ImVec2(pos.x + 14.f, (pos.y + mx.y) * 0.5f), 4.f, ImGui::ColorConvertFloat4ToU32(dc));
    DrawText(dl, ImVec2(pos.x + 26.f, pos.y + 5.f), P.text, text);
}

void DrawBadge(ImDrawList* dl, ImVec2 pos, const char* text, ImU32 fg, ImU32 bg) {
    const ImVec2 ts = MeasureText(text, 0.f);
    const ImVec2 mx(pos.x + ts.x + 12.f, pos.y + ts.y + 6.f);
    dl->AddRectFilled(pos, mx, bg, 5.f);
    DrawText(dl, ImVec2(pos.x + 6.f, pos.y + 3.f), fg, text);
}

// ─────────────────────────────────────────────────────────────────────────────
//  State helpers (log / toast / selection / injection)
// ─────────────────────────────────────────────────────────────────────────────
void ShowToast(myiui::injector_ui::GuiState& state, const std::wstring& msg, bool isError) {
    state.toastMessage = msg;
    state.toastError = isError;
    state.toastTimer = isError ? 3.6f : 2.8f;
}

std::wstring CurrentLogTimestamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t ts[16]{};
    swprintf(ts, 16, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    return ts;
}

void AppendLog(myiui::injector_ui::GuiState& state, const std::wstring& line, myiui::injector_ui::LogLevel level) {
    std::lock_guard<std::mutex> lock(state.logMutex);
    state.logs.push_back({line, CurrentLogTimestamp(), level});
    if (state.logs.size() > 400) {
        state.logs.erase(state.logs.begin(), state.logs.begin() + 100);
    }
}

void UpdateSelectionUi(myiui::injector_ui::GuiState& state) {
    if (state.selectedIndex < 0 || state.selectedIndex >= static_cast<int>(state.processes.size())) {
        state.pillState = myiui::injector_ui::PillState::Idle;
        state.pillText = L"未选择目标";
        return;
    }
    const auto& proc = state.processes[state.selectedIndex];
    if (state.injectedPid == proc.pid) {
        state.pillState = myiui::injector_ui::PillState::Success;
        state.pillText = L"已注入";
    } else {
        state.pillState = myiui::injector_ui::PillState::Ready;
        state.pillText = L"已就绪";
    }
}

void CancelInjection(myiui::injector_ui::GuiState& state) {
    state.injectCancel.store(true);
    state.injectFinished.store(false);
    state.injecting = false;
    state.injectorState = L"就绪";
    state.injectorMeta = L"注入已取消";
    UpdateSelectionUi(state);
    AppendLog(state, L"[MyiUI] 注入已取消", myiui::injector_ui::LogLevel::Warn);
    ShowToast(state, L"注入已取消", false);
}

void FinishInjectionUi(myiui::injector_ui::GuiState& state, bool ok) {
    state.injecting = false;
    if (ok) {
        state.injectedPid = state.injectTargetPid;
        state.injectorState = L"已注入";
        state.injectorMeta = L"Agent 已挂载 · 进入游戏即可使用";
        state.pillState = myiui::injector_ui::PillState::Success;
        state.pillText = L"已注入";
        state.lastInjectTime = CurrentLogTimestamp();
        AppendLog(state, L"[MyiUI] 注入流程完成，PID " + std::to_wstring(state.injectTargetPid),
                  myiui::injector_ui::LogLevel::Ok);
        ShowToast(state, L"MyiUI 注入成功", false);
    } else {
        state.pillState = myiui::injector_ui::PillState::Error;
        state.pillText = L"注入失败";
        state.injectorState = L"失败";
        state.injectorMeta = L"请查看日志并尝试以管理员身份运行";
        AppendLog(state, L"[MyiUI] 注入失败 PID " + std::to_wstring(state.injectTargetPid),
                  myiui::injector_ui::LogLevel::Err);
        ShowToast(state, L"注入失败 — 请查看日志", true);
    }
}

void StartInjection(myiui::injector_ui::GuiState& state) {
    if (state.injecting || state.selectedIndex < 0) return;
    const DWORD pid = state.processes[state.selectedIndex].pid;
    state.injecting = true;
    state.injectCancel.store(false);
    state.injectFinished.store(false);
    state.injectTargetPid = pid;
    state.pillState = myiui::injector_ui::PillState::Loading;
    state.pillText = L"注入中…";
    state.injectorState = L"注入中";
    state.injectorMeta = L"正在写入 Agent 到目标进程…";
    AppendLog(state, L"[MyiUI] 开始注入 PID " + std::to_wstring(pid) + L"…", myiui::injector_ui::LogLevel::Info);

    std::thread([pid, gui = &state]() {
        const auto shouldCancel = [gui]() { return gui->injectCancel.load(); };
        const bool ok = myiui::RunInjection(
            pid,
            [gui](const std::wstring& line, bool isError) {
                myiui::injector_ui::QueueLog(*gui, line, isError ? myiui::injector_ui::LogLevel::Err
                                                                 : myiui::injector_ui::LogLevel::Info);
            },
            shouldCancel);
        if (gui->injectCancel.load()) return;
        gui->injectSuccess.store(ok);
        gui->injectFinished.store(true);
    }).detach();
}

void ApplyProcessList(myiui::injector_ui::GuiState& state, std::vector<myiui::JavaProcessInfo> processes, bool manual) {
    state.processes = std::move(processes);
    state.selectedIndex = -1;
    if (state.scanPreservePid != 0) {
        for (size_t i = 0; i < state.processes.size(); ++i) {
            if (state.processes[i].pid == state.scanPreservePid) {
                state.selectedIndex = static_cast<int>(i);
                break;
            }
        }
    }
    if (state.selectedIndex < 0 && !state.processes.empty()) {
        for (size_t i = 0; i < state.processes.size(); ++i) {
            if (IsMcProcess(state.processes[i]) || state.processes[i].recommended) {
                state.selectedIndex = static_cast<int>(i);
                break;
            }
        }
        if (state.selectedIndex < 0) state.selectedIndex = 0;
    }
    UpdateSelectionUi(state);

    if (manual) {
        AppendLog(state, L"[MyiUI] 已刷新进程列表，当前 " + std::to_wstring(state.processes.size()) + L" 个 Java 进程",
                  myiui::injector_ui::LogLevel::Ok);
        if (state.processes.empty()) {
            AppendLog(state, L"[MyiUI] 未发现 Java 进程。启动 Minecraft 并停在主菜单后点击刷新。",
                      myiui::injector_ui::LogLevel::Warn);
        }
    }
}

void PollScanResult(myiui::injector_ui::GuiState& state) {
    if (!state.scanResultReady) return;
    std::vector<myiui::JavaProcessInfo> processes;
    bool manual = false;
    {
        std::lock_guard<std::mutex> lock(state.scanMutex);
        if (!state.scanResultReady) return;
        processes = std::move(state.scannedProcesses);
        manual = state.scanWasManual;
        state.scanResultReady = false;
    }
    state.refreshing = false;
    ApplyProcessList(state, std::move(processes), manual);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sections
// ─────────────────────────────────────────────────────────────────────────────
void DrawTopBar(myiui::injector_ui::GuiState& state, ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    // Logo mark
    const float logoSz = 40.f;
    const ImVec2 logoMin(mn.x, mn.y + (mx.y - mn.y - logoSz) * 0.5f);
    const ImVec2 logoMax(logoMin.x + logoSz, logoMin.y + logoSz);
    const InjectorLogoTexture& logo = GetInjectorMarkLogo();
    if (logo.valid()) {
        DrawInjectorLogoFit(dl, logo, logoMin, logoMax, 1.f);
    } else {
        dl->AddRectFilled(logoMin, logoMax, P.accentSoft, 10.f);
        dl->AddRect(logoMin, logoMax, P.accent, 10.f, 0, 1.f);
        DrawTextCentered(dl, ImVec2((logoMin.x + logoMax.x) * 0.5f, (logoMin.y + logoMax.y) * 0.5f), P.text, "M", 22.f);
    }

    const float textX = logoMax.x + 14.f;
    DrawText(dl, ImVec2(textX, mn.y + 8.f), P.text, "MyiUI Injector", 24.f);
    DrawText(dl, ImVec2(textX, mn.y + 36.f), P.muted, "轻量级 Fabric 注入启动器", 15.f);

    // Global status pill on the right
    ImU32 dot = P.faint;
    const char* label = "空闲";
    if (state.injecting) {
        dot = P.accent;
        label = "注入中";
    } else if (state.refreshing) {
        dot = P.accent;
        label = "扫描中";
    } else if (state.injectedPid != 0) {
        dot = P.ok;
        label = "已注入";
    } else if (!state.processes.empty()) {
        dot = P.warn;
        label = "待注入";
    }
    const ImVec2 pts = MeasureText(label, 0.f);
    const float pillW = pts.x + 34.f;
    DrawStatusPill(dl, ImVec2(mx.x - pillW, mn.y + (mx.y - mn.y - (pts.y + 10.f)) * 0.5f), dot, label,
                   state.injecting || state.refreshing);
}

void DrawProcessPanel(myiui::injector_ui::GuiState& state, ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    DrawCard(dl, mn, mx);

    DrawText(dl, ImVec2(mn.x + 16.f, mn.y + 13.f), P.text, "目标进程", 17.f);
    {
        char count[48];
        snprintf(count, sizeof(count), "%d 个 Java 进程", static_cast<int>(state.processes.size()));
        DrawText(dl, ImVec2(mn.x + 16.f, mn.y + 36.f), P.faint, count, 13.f);
    }

    // Toolbar buttons (top-right)
    const float btnY = mn.y + 12.f;
    if (GhostButton("##auto", state.autoRefresh ? "自动刷新 · 开" : "自动刷新 · 关",
                    ImVec2(mx.x - 218.f, btnY), ImVec2(112.f, 30.f), state.autoRefresh)) {
        state.autoRefresh = !state.autoRefresh;
        state.autoRefreshAccum = 0.f;
        AppendLog(state, std::wstring(L"[MyiUI] 自动刷新已") + (state.autoRefresh ? L"开启" : L"关闭"),
                  myiui::injector_ui::LogLevel::Info);
    }
    if (GhostButton("##refresh", state.refreshing ? "刷新中…" : "刷新", ImVec2(mx.x - 98.f, btnY), ImVec2(82.f, 30.f),
                    false, state.refreshing)) {
        myiui::injector_ui::RefreshProcesses(state, true);
    }

    // Table region
    const ImVec2 tblMin(mn.x + 12.f, mn.y + 54.f);
    const ImVec2 tblMax(mx.x - 12.f, mx.y - 14.f);
    dl->AddRectFilled(tblMin, tblMax, P.cardInset, 8.f);

    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, U32ToVec4(P.rowHover));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, U32ToVec4(P.rowHover));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, U32ToVec4(P.border));
    ImGui::PushStyleColor(ImGuiCol_Text, U32ToVec4(P.text));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(10.f, 8.f));

    ImGui::SetCursorScreenPos(ImVec2(tblMin.x + 6.f, tblMin.y + 6.f));
    ImGui::BeginChild("##proc_scroll", ImVec2(tblMax.x - tblMin.x - 12.f, tblMax.y - tblMin.y - 12.f), false);
    if (state.processes.empty()) {
        const float cx = (tblMax.x - tblMin.x) * 0.5f;
        ImGui::Dummy(ImVec2(0.f, 26.f));
        const char* l1 = "未发现 Java 进程";
        const char* l2 = "启动 Minecraft 并停在主菜单，然后点击刷新";
        ImGui::SetCursorPosX(cx - MeasureText(l1, 0.f).x * 0.5f);
        ImGui::TextColored(U32ToVec4(P.muted), "%s", l1);
        ImGui::SetCursorPosX(cx - MeasureText(l2, 0.f).x * 0.5f);
        ImGui::TextColored(U32ToVec4(P.faint), "%s", l2);
    } else if (ImGui::BeginTable("##procs", 4,
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("进程", ImGuiTableColumnFlags_WidthStretch, 1.35f);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 66.f);
        ImGui::TableSetupColumn("内存", ImGuiTableColumnFlags_WidthFixed, 70.f);
        ImGui::TableSetupColumn("标识", ImGuiTableColumnFlags_WidthStretch, 1.55f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(state.processes.size()); ++i) {
            const auto& proc = state.processes[i];
            const bool mc = IsMcProcess(proc);
            const bool selected = i == state.selectedIndex;
            const bool injected = state.injectedPid != 0 && proc.pid == state.injectedPid;

            ImGui::PushID(i);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, 26.f);
            if (selected) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, P.rowSel);
            }

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(WideToUtf8(proc.exeName).c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                                  ImVec2(0.f, 22.f))) {
                state.selectedIndex = i;
                UpdateSelectionUi(state);
                AppendLog(state, L"[MyiUI] 已选中 PID " + std::to_wstring(proc.pid),
                          myiui::injector_ui::LogLevel::Info);
            }
            // Selection accent bar
            if (selected) {
                const ImVec2 rmin = ImGui::GetItemRectMin();
                const ImVec2 rmax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(rmin.x - 6.f, rmin.y + 2.f),
                                                          ImVec2(rmin.x - 3.f, rmax.y - 2.f), P.rowSelBar, 2.f);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(U32ToVec4(P.muted), "%lu", proc.pid);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(U32ToVec4(P.muted), "%s", WideToUtf8(myiui::FormatMemory(proc.workingSetBytes)).c_str());

            ImGui::TableSetColumnIndex(3);
            if (mc) {
                const ImVec2 cp = ImGui::GetCursorScreenPos();
                DrawBadge(ImGui::GetWindowDrawList(), ImVec2(cp.x, cp.y + 1.f), "MC", P.onAccent, P.accent);
                ImGui::SetCursorScreenPos(ImVec2(cp.x + 34.f, cp.y));
            }
            if (injected) {
                const ImVec2 cp = ImGui::GetCursorScreenPos();
                DrawBadge(ImGui::GetWindowDrawList(), ImVec2(cp.x, cp.y + 1.f), "已注入", P.onAccent, P.ok);
                ImGui::SetCursorScreenPos(ImVec2(cp.x + 52.f, cp.y));
            }
            ImGui::TextColored(U32ToVec4(mc ? P.text : P.faint), "%s", WideToUtf8(BuildTag(proc)).c_str());

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
}

void DrawSidePanel(myiui::injector_ui::GuiState& state, ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    const float insetX = 14.f;
    float y = mn.y + 14.f;

    // ── Selected process detail card ──
    const bool hasSel = state.selectedIndex >= 0 && state.selectedIndex < static_cast<int>(state.processes.size());
    const float detailH = 118.f;
    const ImVec2 dMin(mn.x + insetX, y);
    const ImVec2 dMax(mx.x - insetX, y + detailH);
    dl->AddRectFilled(dMin, dMax, P.soft, 10.f);
    dl->AddRect(dMin, dMax, P.border, 10.f, 0, 1.f);
    DrawText(dl, ImVec2(dMin.x + 12.f, dMin.y + 10.f), P.faint, "选中进程", 13.f);
    if (hasSel) {
        const auto& p = state.processes[state.selectedIndex];
        DrawText(dl, ImVec2(dMin.x + 12.f, dMin.y + 30.f), P.text, WideToUtf8(p.exeName).c_str(), 18.f);
        char meta[128];
        snprintf(meta, sizeof(meta), "PID %lu · %s", p.pid, WideToUtf8(myiui::FormatMemory(p.workingSetBytes)).c_str());
        DrawText(dl, ImVec2(dMin.x + 12.f, dMin.y + 56.f), P.muted, meta, 14.f);

        ImU32 pdot = P.faint;
        const char* ptext = "未选择目标";
        switch (state.pillState) {
            case myiui::injector_ui::PillState::Ready:
                pdot = P.warn;
                ptext = "已就绪 · 可注入";
                break;
            case myiui::injector_ui::PillState::Loading:
                pdot = P.accent;
                ptext = "注入中…";
                break;
            case myiui::injector_ui::PillState::Success:
                pdot = P.ok;
                ptext = "已注入";
                break;
            case myiui::injector_ui::PillState::Error:
                pdot = P.err;
                ptext = "注入失败";
                break;
            default:
                break;
        }
        DrawStatusPill(dl, ImVec2(dMin.x + 12.f, dMin.y + 80.f), pdot, ptext,
                       state.pillState == myiui::injector_ui::PillState::Loading);
    } else {
        DrawText(dl, ImVec2(dMin.x + 12.f, dMin.y + 34.f), P.muted, "请从左侧选择一个进程", 15.f);
    }
    y += detailH + 12.f;

    // ── Primary CTA ──
    const bool injected = hasSel && state.injectedPid != 0 &&
                          state.processes[state.selectedIndex].pid == state.injectedPid;
    const char* label = state.injecting ? "注入中…" : (injected ? "重新注入" : "注入 MyiUI");
    const float ctaH = 46.f;
    if (PrimaryButton("##inject", label, ImVec2(mn.x + insetX, y), ImVec2(mx.x - mn.x - insetX * 2.f, ctaH),
                      !hasSel || state.injecting)) {
        StartInjection(state);
    }
    y += ctaH + 8.f;
    if (state.injecting) {
        DrawTextCentered(dl, ImVec2((mn.x + mx.x) * 0.5f, y + 8.f), P.faint, "按 Esc 可取消", 13.f);
    } else {
        DrawTextCentered(dl, ImVec2((mn.x + mx.x) * 0.5f, y + 8.f), P.faint, "每次重启 MC 需重新注入", 13.f);
    }
    y += 26.f;

    // ── Info rows ──
    auto infoRow = [&](const char* label, const std::wstring& value, ImU32 valColor) {
        const ImVec2 rMin(mn.x + insetX, y);
        const ImVec2 rMax(mx.x - insetX, y + 44.f);
        dl->AddRectFilled(rMin, rMax, P.soft, 8.f);
        DrawText(dl, ImVec2(rMin.x + 12.f, rMin.y + 6.f), P.faint, label, 13.f);
        DrawText(dl, ImVec2(rMin.x + 12.f, rMin.y + 23.f), valColor, WideToUtf8(value).c_str(), 15.f);
        y += 44.f + 8.f;
    };
    ImU32 stateColor = P.text;
    if (state.injectorState == L"失败") stateColor = P.err;
    else if (state.injectorState == L"已注入") stateColor = P.ok;
    else if (state.injectorState == L"注入中") stateColor = P.accent;
    infoRow("注入器状态", state.injectorState + (state.injectorMeta.empty() ? L"" : L" · " + state.injectorMeta),
            stateColor);
    infoRow("上次注入", state.lastInjectTime, P.muted);
}

void DrawLogConsole(myiui::injector_ui::GuiState& state, ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
    DrawCard(dl, mn, mx);
    DrawText(dl, ImVec2(mn.x + 16.f, mn.y + 11.f), P.text, "运行日志", 16.f);

    if (GhostButton("##clearlog", "清空", ImVec2(mx.x - 76.f, mn.y + 8.f), ImVec2(60.f, 26.f))) {
        std::lock_guard<std::mutex> lock(state.logMutex);
        state.logs.clear();
    }

    const ImVec2 logMin(mn.x + 12.f, mn.y + 40.f);
    const ImVec2 logMax(mx.x - 12.f, mx.y - 12.f);
    dl->AddRectFilled(logMin, logMax, P.cardInset, 8.f);

    ImGui::SetCursorScreenPos(ImVec2(logMin.x + 8.f, logMin.y + 6.f));
    ImGui::BeginChild("##logs", ImVec2(logMax.x - logMin.x - 16.f, logMax.y - logMin.y - 12.f), false);
    {
        std::lock_guard<std::mutex> lock(state.logMutex);
        if (state.logs.empty()) {
            ImGui::TextColored(U32ToVec4(P.faint), "暂无日志输出");
        } else {
            for (const auto& line : state.logs) {
                ImGui::TextColored(U32ToVec4(P.faint), "%s", WideToUtf8(line.timestamp).c_str());
                ImGui::SameLine();
                ImGui::TextColored(U32ToVec4(LogColor(line.level)), "%s", WideToUtf8(line.text).c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f) {
                ImGui::SetScrollHereY(1.f);
            }
        }
    }
    ImGui::EndChild();
}

void DrawToast(myiui::injector_ui::GuiState& state, ImDrawList* dl, ImVec2 wp, ImVec2 ws) {
    if (state.toastTimer <= 0.f || state.toastMessage.empty()) return;
    const float appear = std::min(1.f, state.toastTimer * 3.f);
    const std::string toast = WideToUtf8(state.toastMessage);
    const ImVec2 ts = MeasureText(toast.c_str(), 0.f);
    const float boxW = ts.x + 40.f;
    const float boxH = ts.y + 22.f;
    const ImVec2 mn(wp.x + (ws.x - boxW) * 0.5f, wp.y + ws.y - 64.f - boxH + (1.f - appear) * 16.f);
    const ImVec2 mx(mn.x + boxW, mn.y + boxH);
    const ImU32 accent = state.toastError ? P.err : P.ok;
    dl->AddRectFilled(mn, mx, IM_COL32(28, 32, 44, 245), 10.f);
    dl->AddRect(mn, mx, accent, 10.f, 0, 1.5f);
    dl->AddCircleFilled(ImVec2(mn.x + 18.f, (mn.y + mx.y) * 0.5f), 4.f, accent);
    DrawText(dl, ImVec2(mn.x + 32.f, mn.y + 11.f), P.text, toast.c_str());
}

}  // namespace

namespace myiui::injector_ui {

void Init(GuiState& state, HWND hwnd) {
    state.hwnd = hwnd;
    BOOL anim = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0) && !anim) {
        state.reduceMotion = true;
    }
    myiui::EnsureMyiuiRootEnv(myiui::GetProjectRoot());
    AppendLog(state, L"[MyiUI] 启动器已就绪", LogLevel::Info);
    AppendLog(state, L"[MyiUI] 根目录 " + myiui::GetProjectRoot(), LogLevel::Info);
    RefreshProcesses(state, true);
}

void QueueLog(GuiState& state, const std::wstring& line, LogLevel level) {
    std::lock_guard<std::mutex> lock(state.logMutex);
    state.logs.push_back({line, CurrentLogTimestamp(), level});
}

void FlushPendingLogs(GuiState& state) {
    std::lock_guard<std::mutex> lock(state.logMutex);
    if (state.logs.size() > 400) {
        state.logs.erase(state.logs.begin(), state.logs.begin() + static_cast<ptrdiff_t>(state.logs.size() - 400));
    }
}

void RefreshProcesses(GuiState& state, bool manual) {
    if (manual && state.refreshing) {
        return;
    }
    bool expected = false;
    if (!state.scanInFlight.compare_exchange_strong(expected, true)) {
        return;
    }
    const double now = ImGui::GetTime();
    if (state.lastRefreshTime > 0.0) {
        const double minInterval = manual ? 0.5 : static_cast<double>(kAutoRefreshSec);
        if (now - state.lastRefreshTime < minInterval) {
            state.scanInFlight.store(false);
            return;
        }
    }
    state.lastRefreshTime = now;
    if (manual) {
        state.refreshing = true;
    }
    state.scanPreservePid = 0;
    if (state.selectedIndex >= 0 && state.selectedIndex < static_cast<int>(state.processes.size())) {
        state.scanPreservePid = state.processes[state.selectedIndex].pid;
    }
    state.scanWasManual = manual;

    std::thread([&state]() {
        const auto list = myiui::ScanJavaProcesses();
        {
            std::lock_guard<std::mutex> lock(state.scanMutex);
            state.scannedProcesses = list;
            state.scanResultReady = true;
        }
        state.scanInFlight.store(false);
    }).detach();
}

void Render(GuiState& state) {
    PollScanResult(state);

    ImFont* uiFont = ImGui::GetIO().FontDefault;
    if (uiFont) ImGui::PushFont(uiFont);

    const float dt = ImGui::GetIO().DeltaTime;

    if (state.injectFinished.load()) {
        state.injectFinished.store(false);
        if (!state.injectCancel.load()) {
            FinishInjectionUi(state, state.injectSuccess.load());
        }
    }

    if (state.toastTimer > 0.f) state.toastTimer -= dt;

    if (state.autoRefresh && !state.injecting && !state.scanInFlight.load()) {
        state.autoRefreshAccum += dt;
        if (state.autoRefreshAccum >= kAutoRefreshSec) {
            state.autoRefreshAccum = 0.f;
            RefreshProcesses(state, false);
        }
    } else if (!state.autoRefresh) {
        state.autoRefreshAccum = 0.f;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && state.injecting) {
        CancelInjection(state);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, U32ToVec4(P.bgBottom));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, U32ToVec4(P.border));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, U32ToVec4(P.muted));
    ImGui::Begin("##InjectorRoot", nullptr, rootFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();

    // Background gradient + top-left accent glow
    DrawVerticalGradient(dl, wp, ImVec2(wp.x + ws.x, wp.y + ws.y), P.bgTop, P.bgBottom);
    for (int i = 0; i < 4; ++i) {
        ImVec4 g = U32ToVec4(P.glow);
        g.w *= (1.f - i * 0.22f);
        dl->AddCircleFilled(ImVec2(wp.x + ws.x * 0.14f, wp.y - 40.f), 220.f + i * 60.f,
                            ImGui::ColorConvertFloat4ToU32(g));
    }

    const float x0 = wp.x + kPad;
    const float x1 = wp.x + ws.x - kPad;
    const float y0 = wp.y + kPad;
    const float y1 = wp.y + ws.y - kPad;

    // Top bar
    DrawTopBar(state, dl, ImVec2(x0, y0), ImVec2(x1, y0 + kTopBarH));

    // Log console + footer at the bottom
    const float footerY = y1 - kFooterH;
    const float logBottom = footerY - 6.f;
    const float logTop = logBottom - kLogConsoleH;

    // Main content row
    const float mainTop = y0 + kTopBarH + kGap;
    const float mainBottom = logTop - kGap;
    const float leftRight = x1 - kRightColW - kGap;

    DrawProcessPanel(state, dl, ImVec2(x0, mainTop), ImVec2(leftRight, mainBottom));
    DrawSidePanel(state, dl, ImVec2(leftRight + kGap, mainTop), ImVec2(x1, mainBottom));
    DrawLogConsole(state, dl, ImVec2(x0, logTop), ImVec2(x1, logBottom));

    // Footer
    const char* footer = "需要权限时请以管理员身份运行 · MyiUI Injector";
    DrawTextCentered(dl, ImVec2((x0 + x1) * 0.5f, footerY + kFooterH * 0.5f), P.faint, footer, 13.f);

    // Toast
    DrawToast(state, dl, wp, ws);

    ImGui::End();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);

    if (uiFont) ImGui::PopFont();
}

}  // namespace myiui::injector_ui
