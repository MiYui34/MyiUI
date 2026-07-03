#include "gui_injector.h"

#include "inject_core.h"
#include "logo_assets.h"

#include <windows.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

namespace {

constexpr float kLogH = 120.f;
constexpr float kStatusW = 228.f;
constexpr float kInjectDuration = 3.f;
constexpr float kAutoRefreshSec = 8.f;

struct Theme {
    ImVec4 bg{0.10f, 0.11f, 0.15f, 1.f};
    ImVec4 surface{0.14f, 0.15f, 0.20f, 0.92f};
    ImVec4 glass{0.14f, 0.15f, 0.20f, 0.72f};
    ImVec4 fg{0.96f, 0.97f, 0.99f, 1.f};
    ImVec4 muted{0.58f, 0.61f, 0.67f, 1.f};
    ImVec4 border{0.38f, 0.42f, 0.50f, 0.35f};
    ImVec4 accent{0.45f, 0.68f, 1.00f, 1.f};
    ImVec4 accentSoft{0.45f, 0.68f, 1.00f, 0.18f};
    ImVec4 success{0.42f, 0.78f, 0.55f, 1.f};
    ImVec4 danger{0.88f, 0.42f, 0.38f, 1.f};
    ImVec4 warn{0.92f, 0.74f, 0.38f, 1.f};
    ImVec4 fgSoft{1.f, 1.f, 1.f, 0.06f};
};

Theme gTheme;

ImVec4 LogColor(myiui::injector_ui::LogLevel level) {
    switch (level) {
        case myiui::injector_ui::LogLevel::Ok:
            return ImVec4(0.55f, 0.88f, 0.62f, 1.f);
        case myiui::injector_ui::LogLevel::Warn:
            return ImVec4(0.95f, 0.78f, 0.45f, 1.f);
        case myiui::injector_ui::LogLevel::Err:
            return ImVec4(0.95f, 0.50f, 0.45f, 1.f);
        default:
            return gTheme.fg;
    }
}

bool IsMcProcess(const myiui::JavaProcessInfo& proc) {
    auto lower = proc.commandLine;
    for (wchar_t& c : lower) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    if (lower.find(L"net.minecraft") != std::wstring::npos) return true;
    if (lower.find(L"minecraft") != std::wstring::npos) return true;
    return proc.recommended && lower.find(L"minecraft") != std::wstring::npos;
}

std::wstring BuildTag(const myiui::JavaProcessInfo& proc) {
    std::wstring tag = proc.hint.empty() ? L"Java 进程" : proc.hint;
    if (proc.javaMajor > 0 && tag.find(L"JDK") == std::wstring::npos) {
        tag += L" · JDK " + std::to_wstring(proc.javaMajor);
    }
    if (IsMcProcess(proc) && tag.find(L"MC") == std::wstring::npos) {
        tag += L" · MC";
    }
    return tag;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr,
                                        nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), len, nullptr, nullptr);
    return out;
}

void ShowToast(myiui::injector_ui::GuiState& state, const std::wstring& msg, bool isError) {
    state.toastMessage = msg;
    state.toastError = isError;
    state.toastTimer = isError ? 3.6f : 2.8f;
}

void AppendLog(myiui::injector_ui::GuiState& state, const std::wstring& line, myiui::injector_ui::LogLevel level) {
    std::lock_guard<std::mutex> lock(state.logMutex);
    state.logs.push_back({line, level});
    if (state.logs.size() > 400) {
        state.logs.erase(state.logs.begin(), state.logs.begin() + 100);
    }
}

void UpdateSelectionUi(myiui::injector_ui::GuiState& state) {
    if (state.selectedIndex < 0 || state.selectedIndex >= static_cast<int>(state.processes.size())) {
        state.pillState = myiui::injector_ui::PillState::Idle;
        state.pillText = L"未选择";
        return;
    }
    const auto& proc = state.processes[state.selectedIndex];
    if (state.injectedPid == proc.pid) {
        state.pillState = myiui::injector_ui::PillState::Success;
        state.pillText = L"已注入 · " + myiui::FormatMemory(proc.workingSetBytes);
    } else {
        state.pillState = myiui::injector_ui::PillState::Ready;
        state.pillText = L"已选中 · " + myiui::FormatMemory(proc.workingSetBytes);
    }
}

void CancelInjection(myiui::injector_ui::GuiState& state) {
    state.injectCancel.store(true);
    state.injectFinished.store(false);
    state.injecting = false;
    state.injectDismissPending = false;
    state.injectOverlayAlpha = 0.f;
    state.injectPanelScale = 0.88f;
    state.injectProgressVisual = 0.f;
    state.injectStep = -1;
    state.injectorState = L"就绪";
    state.injectorMeta = L"注入已取消";
    UpdateSelectionUi(state);
    AppendLog(state, L"[MyiUI] 注入已取消", myiui::injector_ui::LogLevel::Warn);
    ShowToast(state, L"注入已取消", false);
}

void FinishInjectionUi(myiui::injector_ui::GuiState& state, bool ok) {
    state.injecting = false;
    state.injectDismissPending = false;
    state.injectOverlayAlpha = 0.f;
    state.injectPanelScale = 0.88f;
    state.injectProgressVisual = 0.f;
    state.injectStep = -1;
    if (ok) {
        state.injectedPid = state.injectTargetPid;
        state.injectorState = L"就绪";
        state.injectorMeta = L"Agent 已挂载 · Overlay 待写入";
        state.pillState = myiui::injector_ui::PillState::Success;
        if (state.selectedIndex >= 0 && state.selectedIndex < static_cast<int>(state.processes.size())) {
            const auto& proc = state.processes[state.selectedIndex];
            state.pillText = L"已注入 · " + myiui::FormatMemory(proc.workingSetBytes);
        }
        wchar_t buf[16]{};
        SYSTEMTIME st{};
        GetLocalTime(&st);
        swprintf(buf, 16, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        state.lastInjectTime = buf;
        AppendLog(state, L"[MyiUI] 注入流程完成，PID " + std::to_wstring(state.injectTargetPid),
                  myiui::injector_ui::LogLevel::Ok);
        ShowToast(state, L"MyiUI 注入成功", false);
    } else {
        state.pillState = myiui::injector_ui::PillState::Error;
        state.pillText = L"注入失败";
        state.injectorState = L"失败";
        state.injectorMeta = L"请查看运行日志并以管理员身份重试";
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
    state.injectStep = 0;
    state.injectAnim = 0.f;
    state.injectOverlayAlpha = 0.f;
    state.injectProgressVisual = 0.f;
    state.injectPanelScale = 0.88f;
    state.injectDismissPending = false;
    state.injectFlash = false;
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

void DrawRoundedRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col, float rounding, float thickness = 0.f) {
    if (thickness > 0.f) {
        dl->AddRect(min, max, col, rounding, 0, thickness);
    } else {
        dl->AddRectFilled(min, max, col, rounding);
    }
}

void DrawGlassPanel(ImDrawList* dl, ImVec2 min, ImVec2 max) {
    DrawRoundedRect(dl, min, max, ImGui::ColorConvertFloat4ToU32(gTheme.glass), 14.f);
    dl->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(gTheme.border), 14.f, 0, 1.f);
    dl->AddLine(ImVec2(min.x + 1.f, min.y + 1.f), ImVec2(max.x - 1.f, min.y + 1.f),
                IM_COL32(255, 255, 255, 18), 1.f);
}

bool DrawSmallButton(const char* id, const char* label, ImVec2 pos, ImVec2 size, bool active = false,
                     bool disabled = false) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::BeginDisabled(disabled);
    ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool activated = ImGui::IsItemActivated();
    ImGui::EndDisabled();
    ImDrawList* wdl = ImGui::GetWindowDrawList();
    const ImU32 border = ImGui::ColorConvertFloat4ToU32(active ? gTheme.fg : (hovered ? gTheme.fg : gTheme.border));
    const ImU32 text = ImGui::ColorConvertFloat4ToU32(disabled ? ImVec4(gTheme.muted.x, gTheme.muted.y, gTheme.muted.z, 0.5f)
                                                                 : (active || hovered ? gTheme.fg : gTheme.muted));
    wdl->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y + size.y), border, 7.f, 0, 1.f);
    const ImVec2 ts = ImGui::CalcTextSize(label);
    wdl->AddText(ImVec2(pos.x + (size.x - ts.x) * 0.5f, pos.y + (size.y - ts.y) * 0.5f), text, label);
    return activated && !disabled;
}

void DrawPill(ImDrawList* dl, ImVec2 pos, myiui::injector_ui::PillState state, const std::wstring& text) {
    const ImVec2 ts = ImGui::CalcTextSize(WideToUtf8(text).c_str());
    const float padX = 10.f;
    const float padY = 4.f;
    const ImVec2 min = pos;
    const ImVec2 max = ImVec2(pos.x + ts.x + padX * 2.f + 14.f, pos.y + ts.y + padY * 2.f);
    dl->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(gTheme.fgSoft), 999.f);
    ImVec4 dot = gTheme.accent;
    switch (state) {
        case myiui::injector_ui::PillState::Idle:
            dot = gTheme.muted;
            break;
        case myiui::injector_ui::PillState::Ready:
            dot = gTheme.success;
            break;
        case myiui::injector_ui::PillState::Success:
            dot = gTheme.success;
            break;
        case myiui::injector_ui::PillState::Error:
            dot = gTheme.danger;
            break;
        case myiui::injector_ui::PillState::Loading:
            dot = gTheme.accent;
            break;
    }
    const float pulse = state == myiui::injector_ui::PillState::Loading
                            ? (0.55f + 0.45f * std::sin(ImGui::GetTime() * 6.28f))
                            : 1.f;
    dl->AddCircleFilled(ImVec2(min.x + 10.f, (min.y + max.y) * 0.5f), 3.5f,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(dot.x, dot.y, dot.z, pulse)));
    dl->AddText(ImVec2(min.x + 20.f, min.y + padY), ImGui::ColorConvertFloat4ToU32(gTheme.fg),
                WideToUtf8(text).c_str());
}

float LerpF(float a, float b, float t) { return a + (b - a) * t; }

float EaseOutCubic(float t) { return 1.f - std::pow(1.f - t, 3.f); }

void DrawFrostedBackdrop(ImDrawList* dl, ImVec2 min, ImVec2 max, float alpha, float time) {
    const int baseA = static_cast<int>(210 * alpha);
    dl->AddRectFilled(min, max, IM_COL32(8, 10, 18, baseA));

    const ImVec2 size = ImVec2(max.x - min.x, max.y - min.y);
    const ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);

    for (int i = 0; i < 5; ++i) {
        const float phase = time * (0.15f + i * 0.04f) + i * 1.7f;
        const float rx = size.x * (0.22f + 0.06f * i);
        const float ry = size.y * (0.18f + 0.05f * i);
        const ImVec2 c(center.x + std::sin(phase) * 40.f, center.y + std::cos(phase * 0.9f) * 28.f);
        dl->AddRectFilled(ImVec2(c.x - rx, c.y - ry), ImVec2(c.x + rx, c.y + ry),
                          IM_COL32(70, 110, 180, static_cast<int>(18 * alpha)), rx);
    }

    const float gridStep = 14.f;
    for (float y = min.y; y < max.y; y += gridStep) {
        for (float x = min.x; x < max.x; x += gridStep) {
            const float n = 0.5f + 0.5f * std::sin(x * 0.08f + y * 0.06f + time * 1.2f);
            const int a = static_cast<int>(n * 10.f * alpha);
            if (a < 2) continue;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 2.f, y + 2.f), IM_COL32(255, 255, 255, a));
        }
    }

    const float vignetteR = std::max(size.x, size.y) * 0.72f;
    dl->AddCircleFilled(center, vignetteR, IM_COL32(0, 0, 0, static_cast<int>(90 * alpha)));

    const float scanY = min.y + std::fmod(time * 120.f, size.y);
    dl->AddRectFilledMultiColor(ImVec2(min.x, scanY), ImVec2(max.x, scanY + 3.f), IM_COL32(120, 168, 255, 0),
                                IM_COL32(120, 168, 255, 0), IM_COL32(120, 168, 255, static_cast<int>(35 * alpha)),
                                IM_COL32(120, 168, 255, static_cast<int>(35 * alpha)));
}

void DrawGlowRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col, float rounding, float expand) {
    for (int i = 3; i >= 1; --i) {
        const float e = expand * static_cast<float>(i);
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
        c.w *= 0.12f / static_cast<float>(i);
        dl->AddRect(ImVec2(min.x - e, min.y - e), ImVec2(max.x + e, max.y + e), ImGui::ColorConvertFloat4ToU32(c),
                    rounding + e, 0, 2.f);
    }
}

void DrawInjectOverlay(myiui::injector_ui::GuiState& state, ImVec2 origin, ImVec2 size) {
    const bool visible = state.injecting || state.injectDismissPending || state.injectOverlayAlpha > 0.01f;
    if (!visible) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 min = origin;
    const ImVec2 max = ImVec2(origin.x + size.x, origin.y + size.y);
    const float alpha = state.injectOverlayAlpha;
    const float time = static_cast<float>(ImGui::GetTime());
    const float dt = ImGui::GetIO().DeltaTime;

    DrawFrostedBackdrop(dl, min, max, alpha, time);

    ImGui::PushID("InjectOverlay");
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton("##blocker", size);
    ImGui::PopID();

    const ImVec2 center = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    state.injectAnim += dt;

    const float stepDur = state.reduceMotion ? 0.2f : 0.85f;
    if (state.injecting && state.injectStep >= 0 && state.injectStep < 4) {
        const int targetStep = std::min(3, static_cast<int>(state.injectAnim / stepDur));
        if (targetStep > state.injectStep) state.injectStep = targetStep;
    }

    const float pctTargets[] = {0.22f, 0.48f, 0.76f, 1.f};
    const int step = std::clamp(state.injectStep, 0, 3);
    const float pctTarget = pctTargets[step];
    const float progressSpeed = state.reduceMotion ? 10.f : 5.f;
    state.injectProgressVisual = LerpF(state.injectProgressVisual, pctTarget, std::min(1.f, dt * progressSpeed));
    const float progress = state.injectProgressVisual;

    if (state.injectDismissPending) {
        state.injectPanelScale = 1.f;
    } else {
        const float panelSpeed = state.reduceMotion ? 12.f : 6.f;
        state.injectPanelScale = LerpF(state.injectPanelScale, 1.f, std::min(1.f, dt * panelSpeed));
        if (state.injecting && state.injectPanelScale < 0.98f) {
            state.injectPanelScale =
                LerpF(0.88f, 1.f, EaseOutCubic(std::min(1.f, state.injectAnim * 2.5f)));
        }
    }

    const ImVec2 panelSize(580.f, 500.f);
    const ImVec2 panelHalf(panelSize.x * 0.5f * state.injectPanelScale, panelSize.y * 0.5f * state.injectPanelScale);
    const ImVec2 panelMin(center.x - panelHalf.x, center.y - panelHalf.y);
    const ImVec2 panelMax(center.x + panelHalf.x, center.y + panelHalf.y);

    DrawGlowRect(dl, panelMin, panelMax, ImGui::ColorConvertFloat4ToU32(gTheme.accent), 18.f, 4.f);
    dl->AddRectFilled(panelMin, panelMax, IM_COL32(22, 26, 36, static_cast<int>(235 * alpha)), 18.f);
    dl->AddRect(panelMin, panelMax, IM_COL32(120, 168, 255, static_cast<int>(80 * alpha)), 18.f, 0, 1.5f);

    dl->PushClipRect(panelMin, panelMax, true);

    const char* header = state.injectDismissPending && state.injectDismissOk ? "注入成功" : "正在注入 MyiUI";
    const ImVec2 headerSize = ImGui::CalcTextSize(header);
    dl->AddText(ImVec2(center.x - headerSize.x * 0.5f, panelMin.y + 20.f),
                IM_COL32(245, 247, 252, static_cast<int>(255 * alpha)), header);

    // ── 固定行高布局（自上而下，互不重叠）──
    constexpr float kNodeSize = 64.f;
    constexpr float kBeamW = 132.f;
    constexpr float kBeamGap = 14.f;
    constexpr float kStageW = kNodeSize * 2.f + kBeamGap * 2.f + kBeamW;
    const float stageLeft = center.x - kStageW * 0.5f;
    const float rowNodesTop = panelMin.y + 56.f;
    const float nodeCenterY = rowNodesTop + kNodeSize * 0.5f;

    const float srcCenterX = stageLeft + kNodeSize * 0.5f;
    const float dstCenterX = stageLeft + kStageW - kNodeSize * 0.5f;
    const float beamLeft = stageLeft + kNodeSize + kBeamGap;
    const float beamRight = beamLeft + kBeamW;
    const float beamY = nodeCenterY;

    // 1) 光束轨道（节点之间的独立区域）
    dl->AddRectFilled(ImVec2(beamLeft, beamY - 2.f), ImVec2(beamRight, beamY + 2.f),
                      IM_COL32(255, 255, 255, static_cast<int>(20 * alpha)), 2.f);
    const float beamProgX = beamLeft + kBeamW * progress;
    for (int g = 2; g >= 1; --g) {
        const float h = 1.5f + g * 1.5f;
        dl->AddRectFilled(ImVec2(beamLeft, beamY - h), ImVec2(beamProgX, beamY + h),
                          IM_COL32(70, 130, 255, static_cast<int>(20 * alpha / g)), h);
    }
    dl->AddRectFilled(ImVec2(beamLeft, beamY - 1.5f), ImVec2(beamProgX, beamY + 1.5f),
                      ImGui::ColorConvertFloat4ToU32(gTheme.accent), 2.f);
    if (progress > 0.02f) {
        dl->AddCircleFilled(ImVec2(beamProgX, beamY), 4.f, IM_COL32(200, 230, 255, static_cast<int>(220 * alpha)));
    }
    for (int i = 0; i < 6; ++i) {
        const float t = std::fmod(state.injectAnim * (0.3f + i * 0.06f) + i * 0.14f, 1.f);
        const float px = beamLeft + kBeamW * t;
        if (px > beamProgX) continue;
        dl->AddCircleFilled(ImVec2(px, beamY), 2.f, IM_COL32(140, 190, 255, static_cast<int>(160 * alpha)));
    }

    auto drawNode = [&](float cx, const char* glyph, const char* caption, bool highlight, bool agentLogo) {
        const float half = kNodeSize * 0.5f;
        const ImVec2 nmin(cx - half, rowNodesTop);
        const ImVec2 nmax(cx + half, rowNodesTop + kNodeSize);
        const InjectorLogoTexture& markLogo = GetInjectorMarkLogo();
        const bool useLogo = agentLogo && markLogo.valid();

        if (!useLogo) {
            dl->AddRectFilled(nmin, nmax,
                              highlight ? IM_COL32(32, 58, 42, 200) : ImGui::ColorConvertFloat4ToU32(gTheme.accentSoft),
                              12.f);
            dl->AddRect(nmin, nmax,
                        highlight ? IM_COL32(90, 200, 120, 220) : ImGui::ColorConvertFloat4ToU32(gTheme.accent), 12.f,
                        0, highlight ? 2.f : 1.f);
        }

        const ImVec2 capSize = ImGui::CalcTextSize(caption);
        if (useLogo) {
            constexpr float kCaptionH = 16.f;
            const ImVec2 iconMax(nmax.x, nmax.y - kCaptionH);
            DrawInjectorLogoFit(dl, markLogo, nmin, iconMax, alpha);
            dl->AddText(ImVec2(cx - capSize.x * 0.5f, nmax.y - capSize.y - 3.f),
                        ImGui::ColorConvertFloat4ToU32(gTheme.muted), caption);
        } else {
            const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
            dl->AddText(ImVec2(cx - glyphSize.x * 0.5f, nodeCenterY - glyphSize.y * 0.5f - 6.f),
                        IM_COL32(245, 247, 252, static_cast<int>(255 * alpha)), glyph);
            dl->AddText(ImVec2(cx - capSize.x * 0.5f, nmax.y - capSize.y - 6.f),
                        ImGui::ColorConvertFloat4ToU32(gTheme.muted), caption);
        }
    };

    const bool receiving = step >= 2;
    drawNode(srcCenterX, "M", "Agent", false, true);

    std::string procCaption = "进程";
    if (state.selectedIndex >= 0 && state.selectedIndex < static_cast<int>(state.processes.size())) {
        procCaption = WideToUtf8(state.processes[state.selectedIndex].exeName);
    }
    drawNode(dstCenterX, "JVM", procCaption.c_str(), receiving, false);

    // 2) 进度环（独立一行，与节点区留足间距）
    const float ringR = 34.f;
    const float ringCenterY = rowNodesTop + kNodeSize + 44.f + ringR;
    const ImVec2 ringC(center.x, ringCenterY);
    constexpr float kPi = 3.14159265358979323846f;

    dl->PathClear();
    dl->PathArcTo(ringC, ringR, 0.f, kPi * 2.f, 48);
    dl->PathStroke(IM_COL32(255, 255, 255, static_cast<int>(30 * alpha)), 0, 3.f);
    dl->PathClear();
    dl->PathArcTo(ringC, ringR, -kPi * 0.5f, -kPi * 0.5f + kPi * 2.f * progress, 48);
    dl->PathStroke(IM_COL32(120, 168, 255, static_cast<int>(255 * alpha)), 0, 4.f);

    const int pctDisplay = (step >= 3 && progress > 0.95f) ? 100 : static_cast<int>(progress * 100.f);
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pctDisplay);
    const ImVec2 pctSize = ImGui::CalcTextSize(pctBuf);
    dl->AddText(ImVec2(ringC.x - pctSize.x * 0.5f, ringC.y - pctSize.y * 0.5f),
                IM_COL32(245, 247, 252, static_cast<int>(255 * alpha)), pctBuf);

    // 3) 状态文案
    const char* titles[] = {"连接进程", "映射内存", "加载 Agent", "验证完成"};
    const char* subs[] = {"正在附加到目标 JVM…", "解析模块基址与导出表…", "写入 MyiUI payload…",
                          "Agent 已挂载，可以进入游戏"};
    const float statusTitleY = ringCenterY + ringR + 22.f;
    dl->AddText(ImVec2(center.x - ImGui::CalcTextSize(titles[step]).x * 0.5f, statusTitleY),
                IM_COL32(245, 247, 252, static_cast<int>(255 * alpha)), titles[step]);
    dl->AddText(ImVec2(center.x - ImGui::CalcTextSize(subs[step]).x * 0.5f, statusTitleY + 22.f),
                IM_COL32(160, 168, 182, static_cast<int>(255 * alpha)), subs[step]);

    // 4) 步骤芯片（单行居中，无发光外扩）
    const char* steps[] = {"连接进程", "映射内存", "加载 Agent", "验证完成"};
    constexpr float kChipH = 24.f;
    constexpr float kChipPadX = 8.f;
    constexpr float kChipGap = 8.f;
    float chipsTotalW = 0.f;
    for (int i = 0; i < 4; ++i) {
        chipsTotalW += ImGui::CalcTextSize(steps[i]).x + kChipPadX * 2.f;
        if (i < 3) chipsTotalW += kChipGap;
    }
    float chipX = center.x - chipsTotalW * 0.5f;
    const float chipY = statusTitleY + 52.f;
    for (int i = 0; i < 4; ++i) {
        const ImVec2 ts = ImGui::CalcTextSize(steps[i]);
        const float chipW = ts.x + kChipPadX * 2.f;
        const ImVec2 cmin(chipX, chipY);
        const ImVec2 cmax(chipX + chipW, chipY + kChipH);
        ImVec4 chipBg = gTheme.fgSoft;
        ImVec4 chipFg = gTheme.muted;
        ImVec4 chipBorder = gTheme.border;
        if (i < step) {
            chipFg = gTheme.success;
        } else if (i == step) {
            chipBg = gTheme.accentSoft;
            chipFg = gTheme.accent;
            chipBorder = gTheme.accent;
        }
        dl->AddRectFilled(cmin, cmax, ImGui::ColorConvertFloat4ToU32(chipBg), 5.f);
        dl->AddRect(cmin, cmax, ImGui::ColorConvertFloat4ToU32(chipBorder), 5.f);
        const ImVec2 chipTextSize = ImGui::CalcTextSize(steps[i]);
        dl->AddText(ImVec2(chipX + kChipPadX, chipY + (kChipH - chipTextSize.y) * 0.5f),
                    ImGui::ColorConvertFloat4ToU32(chipFg), steps[i]);
        chipX += chipW + kChipGap;
    }

    dl->PopClipRect();

    if (state.injecting && !state.injectDismissPending) {
        const char* escHint = "按 Esc 可取消注入";
        const ImVec2 escSize = ImGui::CalcTextSize(escHint);
        dl->AddText(ImVec2(center.x - escSize.x * 0.5f, panelMax.y - 22.f),
                    IM_COL32(160, 168, 182, static_cast<int>(220 * alpha)), escHint);
    } else if (state.injectDismissPending && state.injectDismissOk) {
        const char* dismissHint = "点击任意处或按 Esc 关闭";
        const ImVec2 hintSize = ImGui::CalcTextSize(dismissHint);
        dl->AddText(ImVec2(center.x - hintSize.x * 0.5f, panelMax.y - 22.f),
                    IM_COL32(160, 168, 182, static_cast<int>(220 * alpha)), dismissHint);
    }
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
            AppendLog(state, L"[MyiUI] 未发现 Java 进程。启动 Minecraft 并停在主菜单，然后点击刷新。",
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
    state.logs.push_back({line, level});
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
        if (state.injectCancel.load()) {
            // 用户已取消，忽略后台线程迟到的完成信号
        } else {
            state.injecting = false;
            state.injectOverlayAlpha = 0.f;
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

    if (state.injecting) {
        const float fadeInSpeed = state.reduceMotion ? 5.f : 3.5f;
        state.injectOverlayAlpha = std::min(1.f, state.injectOverlayAlpha + dt * fadeInSpeed);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && state.injecting && !state.injectDismissPending) {
        CancelInjection(state);
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, gTheme.bg);
    ImGui::Begin("##InjectorRoot", nullptr, rootFlags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 ws = ImGui::GetWindowSize();

    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), IM_COL32(70, 110, 180, 25), IM_COL32(20, 24, 32, 0),
                                IM_COL32(20, 24, 32, 0), IM_COL32(70, 110, 180, 15));

    const float pad = 20.f;
    const float bodyTop = wp.y + 16.f;
    const float footerH = 20.f;
    const float mainBottom = wp.y + ws.y - kLogH - footerH - 24.f;

    const ImVec2 headerLogoMin(wp.x + pad, bodyTop);
    const ImVec2 headerLogoMax(headerLogoMin.x + 36.f, bodyTop + 36.f);
    const InjectorLogoTexture& headerLogo = GetInjectorMarkLogo();
    if (headerLogo.valid()) {
        DrawInjectorLogoFit(dl, headerLogo, headerLogoMin, headerLogoMax, 1.f);
    } else {
        dl->AddRectFilled(headerLogoMin, headerLogoMax, ImGui::ColorConvertFloat4ToU32(gTheme.fgSoft), 10.f);
        dl->AddText(ImVec2(wp.x + pad + 11.f, bodyTop + 8.f), ImGui::ColorConvertFloat4ToU32(gTheme.fg), "M");
    }
    dl->AddText(ImVec2(wp.x + pad + 48.f, bodyTop + 4.f), ImGui::ColorConvertFloat4ToU32(gTheme.fg), "MyiUI");
    dl->AddText(ImVec2(wp.x + pad + 48.f, bodyTop + 22.f), ImGui::ColorConvertFloat4ToU32(gTheme.muted),
                "Injector · 启动器");

    const float gridTop = bodyTop + 52.f;
    const float gridH = mainBottom - gridTop;
    const float procW = ws.x - pad * 2.f - kStatusW - 14.f;
    const ImVec2 procMin(wp.x + pad, gridTop);
    const ImVec2 procMax(procMin.x + procW, gridTop + gridH);
    DrawGlassPanel(dl, procMin, procMax);

    dl->AddText(ImVec2(procMin.x + 14.f, procMin.y + 12.f), ImGui::ColorConvertFloat4ToU32(gTheme.fg), "Java 进程");
    if (DrawSmallButton("##refresh", state.refreshing ? "刷新中…" : "刷新",
                        ImVec2(procMax.x - 168.f, procMin.y + 8.f), ImVec2(72.f, 28.f), false, state.refreshing)) {
        RefreshProcesses(state, true);
    }
    if (DrawSmallButton("##auto", state.autoRefresh ? "自动刷新：开" : "自动刷新：关",
                        ImVec2(procMax.x - 88.f, procMin.y + 8.f), ImVec2(74.f, 28.f), state.autoRefresh)) {
        state.autoRefresh = !state.autoRefresh;
        state.autoRefreshAccum = 0.f;
        AppendLog(state, std::wstring(L"[MyiUI] 自动刷新已") + (state.autoRefresh ? L"开启" : L"关闭"), LogLevel::Info);
    }

    const ImVec2 tableMin(procMin.x + 1.f, procMin.y + 44.f);
    const ImVec2 tableMax(procMax.x - 1.f, procMax.y - 96.f);
    dl->AddRectFilled(tableMin, tableMax, IM_COL32(0, 0, 0, 20), 0.f);

    ImGui::SetCursorScreenPos(ImVec2(tableMin.x + 8.f, tableMin.y + 6.f));
    ImGui::BeginChild("##proc_table", ImVec2(tableMax.x - tableMin.x - 16.f, tableMax.y - tableMin.y - 12.f), false);
    if (ImGui::BeginTable("##procs", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("进程", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("内存", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("识别", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableHeadersRow();
        if (state.processes.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(gTheme.muted, "未发现 Java 进程");
            ImGui::TextColored(gTheme.muted, "启动 Minecraft 并停在主菜单，然后点击刷新");
        } else {
            for (int i = 0; i < static_cast<int>(state.processes.size()); ++i) {
                const auto& proc = state.processes[i];
                ImGui::PushID(i);
                ImGui::TableNextRow();
                const bool selected = i == state.selectedIndex;
                if (selected) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                           ImGui::ColorConvertFloat4ToU32(gTheme.accentSoft));
                }
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(WideToUtf8(proc.exeName).c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    state.selectedIndex = i;
                    UpdateSelectionUi(state);
                    AppendLog(state, L"[MyiUI] 已选中 PID " + std::to_wstring(proc.pid), LogLevel::Info);
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%lu", proc.pid);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", WideToUtf8(myiui::FormatMemory(proc.workingSetBytes)).c_str());
                ImGui::TableSetColumnIndex(3);
                const bool mc = IsMcProcess(proc);
                ImGui::TextColored(mc ? gTheme.fg : gTheme.muted, "%s", WideToUtf8(BuildTag(proc)).c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    std::wstring hint = L"请先选择目标进程（推荐带 MC 标记的进程）";
    bool hintError = false;
    if (state.injecting) {
        hint = L"正在注入，按 Esc 可取消…";
    } else if (state.selectedIndex >= 0) {
        const auto& proc = state.processes[state.selectedIndex];
        if (state.injectedPid == proc.pid) {
            hint = L"当前进程已注入，可重新注入或切换进程";
        } else {
            hint = L"已选中 PID " + std::to_wstring(proc.pid) + L"，点击注入";
        }
    }

    dl->AddLine(ImVec2(procMin.x, procMax.y - 88.f), ImVec2(procMax.x, procMax.y - 88.f),
                ImGui::ColorConvertFloat4ToU32(gTheme.border));
    const ImVec2 hintSize = ImGui::CalcTextSize(WideToUtf8(hint).c_str());
    dl->AddText(ImVec2(procMin.x + (procW - hintSize.x) * 0.5f, procMax.y - 78.f),
                ImGui::ColorConvertFloat4ToU32(hintError ? gTheme.danger : gTheme.muted), WideToUtf8(hint).c_str());

    ImGui::SetCursorScreenPos(ImVec2(procMin.x + 14.f, procMax.y - 52.f));
    ImGui::BeginDisabled(state.selectedIndex < 0 || state.injecting);
    const char* injectLabel = state.injecting
                                  ? "注入中…"
                                  : (state.injectedPid > 0 && state.selectedIndex >= 0 &&
                                             state.processes[state.selectedIndex].pid == state.injectedPid
                                         ? "重新注入"
                                         : "注入 MyiUI");
    const float injectGlow = 0.22f + 0.12f * std::sin(static_cast<float>(ImGui::GetTime()) * 3.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, gTheme.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(gTheme.accent.x, gTheme.accent.y, gTheme.accent.z, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.12f, 0.13f, 0.16f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(gTheme.accent.x, gTheme.accent.y, gTheme.accent.z, injectGlow));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    if (ImGui::Button("##inject_myui", ImVec2(procW - 28.f, 40.f))) {
        StartInjection(state);
    }
    {
        const ImVec2 btnMin = ImGui::GetItemRectMin();
        const ImVec2 btnMax = ImGui::GetItemRectMax();
        const ImVec2 ts = ImGui::CalcTextSize(injectLabel);
        dl->AddText(ImVec2((btnMin.x + btnMax.x - ts.x) * 0.5f, (btnMin.y + btnMax.y - ts.y) * 0.5f),
                    IM_COL32(31, 33, 41, 255), injectLabel);
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::EndDisabled();

    const ImVec2 sideMin(procMax.x + 14.f, gridTop);
    const ImVec2 sideMax(wp.x + ws.x - pad, gridTop + gridH);
    DrawGlassPanel(dl, sideMin, sideMax);

    const float cardInsetX = 12.f;
    const float cardGap = 10.f;
    float cardY = sideMin.y + 12.f;

    auto drawStatusCard = [&](float y, float height, const char* label, const std::wstring& value,
                              const std::wstring& meta = L"") {
        const ImVec2 cmin(sideMin.x + cardInsetX, y);
        const ImVec2 cmax(sideMax.x - cardInsetX, y + height);
        dl->AddRectFilled(cmin, cmax, ImGui::ColorConvertFloat4ToU32(gTheme.fgSoft), 10.f);
        dl->AddRect(cmin, cmax, ImGui::ColorConvertFloat4ToU32(gTheme.border), 10.f);
        dl->AddText(ImVec2(cmin.x + 10.f, cmin.y + 8.f), ImGui::ColorConvertFloat4ToU32(gTheme.muted), label);
        dl->AddText(ImVec2(cmin.x + 10.f, cmin.y + 28.f), ImGui::ColorConvertFloat4ToU32(gTheme.fg),
                    WideToUtf8(value).c_str());
        if (!meta.empty()) {
            dl->AddText(ImVec2(cmin.x + 10.f, cmin.y + 50.f), ImGui::ColorConvertFloat4ToU32(gTheme.muted),
                        WideToUtf8(meta).c_str());
        }
        return height;
    };

    std::wstring selectedName = L"—";
    if (state.selectedIndex >= 0 && state.selectedIndex < static_cast<int>(state.processes.size())) {
        const auto& p = state.processes[state.selectedIndex];
        selectedName = p.exeName + L" · " + std::to_wstring(p.pid);
    }

    constexpr float kSelectedCardH = 98.f;
    drawStatusCard(cardY, kSelectedCardH, "选中进程", selectedName);
    DrawPill(dl, ImVec2(sideMin.x + cardInsetX + 10.f, cardY + 58.f), state.pillState, state.pillText);
    cardY += kSelectedCardH + cardGap;

    constexpr float kInjectorCardH = 88.f;
    drawStatusCard(cardY, kInjectorCardH, "注入器状态", state.injectorState, state.injectorMeta);
    cardY += kInjectorCardH + cardGap;

    drawStatusCard(cardY, 58.f, "上次注入", state.lastInjectTime);

    const ImVec2 logMin(wp.x + pad, mainBottom + 10.f);
    const ImVec2 logMax(wp.x + ws.x - pad, mainBottom + 10.f + kLogH);
    DrawGlassPanel(dl, logMin, logMax);
    dl->AddText(ImVec2(logMin.x + 14.f, logMin.y + 8.f), ImGui::ColorConvertFloat4ToU32(gTheme.fg), "运行日志");
    if (DrawSmallButton("##clearlog", "清空", ImVec2(logMax.x - 62.f, logMin.y + 6.f), ImVec2(48.f, 26.f))) {
        state.logs.clear();
    }
    ImGui::SetCursorScreenPos(ImVec2(logMin.x + 10.f, logMin.y + 36.f));
    ImGui::BeginChild("##logs", ImVec2(logMax.x - logMin.x - 20.f, kLogH - 46.f), false);
    {
        std::lock_guard<std::mutex> lock(state.logMutex);
        if (state.logs.empty()) {
        ImGui::TextColored(gTheme.muted, "暂无日志输出");
    } else {
        for (const auto& line : state.logs) {
            wchar_t ts[16]{};
            SYSTEMTIME st{};
            GetLocalTime(&st);
            swprintf(ts, 16, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            ImGui::TextColored(gTheme.muted, "[%s]", WideToUtf8(ts).c_str());
            ImGui::SameLine();
            ImGui::TextColored(LogColor(line.level), "%s", WideToUtf8(line.text).c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f) {
            ImGui::SetScrollHereY(1.f);
        }
    }
    }
    ImGui::EndChild();

    const char* footer = "需管理员权限时请以管理员运行 · 每次重启 MC 需重新注入";
    const ImVec2 fts = ImGui::CalcTextSize(footer);
    dl->AddText(ImVec2(wp.x + (ws.x - fts.x) * 0.5f, logMax.y + 8.f), ImGui::ColorConvertFloat4ToU32(gTheme.muted),
                footer);

    if (state.toastTimer > 0.f && !state.toastMessage.empty()) {
        const std::string toast = WideToUtf8(state.toastMessage);
        const ImVec2 ts = ImGui::CalcTextSize(toast.c_str());
        constexpr float kPadX = 18.f;
        constexpr float kPadY = 10.f;
        const float boxW = ts.x + kPadX * 2.f;
        const float boxH = ts.y + kPadY * 2.f;
        const ImVec2 tmin(wp.x + (ws.x - boxW) * 0.5f, wp.y + ws.y - 56.f - boxH);
        const ImVec2 tmax(tmin.x + boxW, tmin.y + boxH);
        const ImVec4 borderCol = state.toastError ? gTheme.danger : gTheme.success;
        const ImVec4 textCol = state.toastError ? gTheme.danger : gTheme.success;
        dl->AddRectFilled(tmin, tmax, ImGui::ColorConvertFloat4ToU32(gTheme.surface), 10.f);
        dl->AddRect(tmin, tmax, ImGui::ColorConvertFloat4ToU32(borderCol), 10.f);
        dl->AddText(ImVec2((tmin.x + tmax.x - ts.x) * 0.5f, (tmin.y + tmax.y - ts.y) * 0.5f),
                    ImGui::ColorConvertFloat4ToU32(textCol), toast.c_str());
    }

    // Inject overlay removed — v2 injection is instant, just show toast.

    if (state.injectDismissPending &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(0))) {
        FinishInjectionUi(state, state.injectDismissOk);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    if (uiFont) ImGui::PopFont();
}

}  // namespace myiui::injector_ui
