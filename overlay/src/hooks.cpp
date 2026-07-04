#include "hooks.h"

#include "bridge/native_state.h"
#include "config/config_loader.h"
#include "config/user_settings.h"
#include "inject/game_hook.h"
#include "jvm/jvm_context.h"
#include "jvm/jvm_log.h"
#include "ipc/pipe_client.h"
#include "ipc/shm_reader.h"
#include "jvm/jvm_spike.h"
#include "overlay_runtime.h"
#include "ui/logo_assets.h"
#include "ui/fonts.h"
#include "ui/menu_app.h"
#include "ui/clickgui/clickgui.h"
#include "ui/hud/hud_renderer.h"
#include "ui/island/island_renderer.h"
#include "ui/music/music_panel.h"
#include "ui/theme/theme_runtime.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include <MinHook.h>

#include <gl/GL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using wglSwapBuffers_t = BOOL(WINAPI*)(HDC);
using glfwSwapBuffers_t = void(__stdcall*)(void*);

static wglSwapBuffers_t g_originalWglSwap = nullptr;
static glfwSwapBuffers_t g_originalGlfwSwap = nullptr;
static bool g_glfwHookInstalled = false;

static AppConfig g_config{};
static MenuAppState g_menuState{};
static ShmReader g_shm{};
static bool g_imguiReady = false;
static bool g_wndProcHooked = false;
static bool g_configLoaded = false;
static bool g_userSettingsLoaded = false;
static HWND g_hwnd = nullptr;
static WNDPROC g_originalWndProc = nullptr;
static std::atomic<bool> g_menuActive{false};
static bool g_wasMenuActive = false;
static bool g_menuOverlayAcked = false;
static uint32_t g_lastScreenSeq = 0;
static myiui::shared::ScreenKind g_lastScreenKind = myiui::shared::ScreenKind::None;
static thread_local bool g_insideGlfwSwap = false;

static GLuint g_bgTexture = 0;
static int g_bgW = 0;
static int g_bgH = 0;
static std::vector<uint8_t> g_frameBuffer;
static std::mutex g_frameMutex;
static uint32_t g_lastFrameIndex = UINT32_MAX;
static uint32_t g_lastFrameW = 0;
static uint32_t g_lastFrameH = 0;
static uint32_t g_lastIslandSeq = 0;
static uint8_t g_lastIslandMode = 255;
static int g_leaveGameGlCooldown = 0;
static int g_viewportResizeCooldown = 0;
static uint32_t g_lastViewportW = 0;
static uint32_t g_lastViewportH = 0;

void OverlayInvalidateBackgroundTexture() {
    std::lock_guard lock(g_frameMutex);
    g_lastFrameIndex = UINT32_MAX;
    g_lastFrameW = 0;
    g_lastFrameH = 0;
}

void OverlayEnterGameScreenMode() {
    g_menuActive.store(false);
    myiui::overlay::OverlayLog(L"Game screen mode requested.");
}

static std::chrono::steady_clock::time_point g_lastBgUploadTime{};

static void UploadBackgroundTexture(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
    if (rgba.empty() || w == 0 || h == 0) return;
    if (w > 8192 || h > 8192) return;
    if (rgba.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 4u) return;
    if (g_bgTexture == 0) glGenTextures(1, &g_bgTexture);
    glBindTexture(GL_TEXTURE_2D, g_bgTexture);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    if (static_cast<int>(w) != g_bgW || static_cast<int>(h) != g_bgH) {
        g_bgW = static_cast<int>(w);
        g_bgH = static_cast<int>(h);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_bgW, g_bgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_bgW, g_bgH, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void PollSharedFrame() {
    uint32_t w = 0, h = 0, frameIndex = 0;
    if (!g_shm.PeekFrame(frameIndex, w, h)) return;
    if (frameIndex == g_lastFrameIndex && w == g_lastFrameW && h == g_lastFrameH && g_bgTexture != 0) return;

    const auto now = std::chrono::steady_clock::now();
    if (g_bgTexture != 0 && g_lastFrameIndex != UINT32_MAX) {
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastBgUploadTime).count();
        if (ms < 50) return;
    }

    std::vector<uint8_t> rgba;
    if (!g_shm.ReadFrame(rgba, w, h, frameIndex)) return;

    std::lock_guard lock(g_frameMutex);
    g_frameBuffer = std::move(rgba);
    g_lastFrameIndex = frameIndex;
    g_lastFrameW = w;
    g_lastFrameH = h;
    UploadBackgroundTexture(g_frameBuffer, w, h);
    g_lastBgUploadTime = now;
}

static void EnsureImGuiForHwnd(HWND hwnd);
static void SuspendOverlayInput();
static void ResumeOverlayInput();
static void RenderOverlayFrame(HWND hwnd);
static bool TryInstallGlfwHook();
static BOOL WINAPI Hook_wglSwapBuffers(HDC hdc);
static void __stdcall Hook_glfwSwapBuffers(void* window);

static thread_local bool g_overlayRendering = false;

static void EnsureShmConnected() {
    if (g_shm.NeedsRemap()) {
        if (g_shm.Open()) {
            myiui::overlay::OverlayLog(L"SHM connected.");
        }
    }
}

static void RenderOverlayFrame(HWND hwnd) {
    if (g_overlayRendering) return;
    g_overlayRendering = true;

    if (!myiui::inject::IsJvmEntryDone() && myiui::jvm::IsReady()) {
        if (JNIEnv* env = myiui::jvm::AttachEnv()) {
            myiui::inject::TryRunJvmEntry(env);
        }
    }

    EnsureShmConnected();

    if (!g_configLoaded) {
        g_configLoaded = myiui::overlay::LoadAppConfigWithFallback(g_config);
        myiui::overlay::OverlayLog(g_configLoaded ? L"Config ready." : L"Config load failed.");
        if (g_configLoaded) {
            MenuAppInit(g_config);
            myiui::config::UserSettingsLoad();
            myiui::ui::theme::ThemeRuntimeInit();
            g_userSettingsLoaded = true;
        }
        if (g_shm.IsValid()) {
            const auto kind = g_shm.GetScreenKind();
            wchar_t buf[96]{};
            swprintf_s(buf, L"SHM open, screen_kind=%u seq=%u", static_cast<unsigned>(kind),
                       g_shm.GetScreenSeq());
            myiui::overlay::OverlayLog(buf);
        } else {
            myiui::overlay::OverlayLog(L"SHM not open yet.");
        }
    }

    const auto screenKind = g_shm.GetScreenKind();
    const uint32_t screenSeq = g_shm.GetScreenSeq();
    if (screenSeq != g_lastScreenSeq || screenKind != g_lastScreenKind) {
        wchar_t buf[96]{};
        swprintf_s(buf, L"screen broadcast kind=%u seq=%u", static_cast<unsigned>(screenKind), screenSeq);
        myiui::overlay::OverlayLog(buf);
        if (g_lastScreenKind == myiui::shared::ScreenKind::InGame
            && screenKind == myiui::shared::ScreenKind::MainMenu) {
            g_leaveGameGlCooldown = 10;
        }
        g_lastScreenSeq = screenSeq;
        g_lastScreenKind = screenKind;
    }

    const bool overlayActive = g_shm.IsOverlayActive();
    const bool islandActive = g_shm.IsIslandActive();
    const bool clickguiOpen = myiui::ui::clickgui::IsOpen();
    const bool shouldRender = overlayActive || islandActive || clickguiOpen;

    // Poll ClickGui toggle key every frame (before shouldRender check)
    {
        bool rshiftDown = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
        static bool rshiftWasDown = false;
        if (rshiftDown && !rshiftWasDown) {
            myiui::ui::clickgui::Toggle();
        }
        rshiftWasDown = rshiftDown;

        // ESC 关闭 ClickGui (仅在打开时)；请求抑制随后的 ESC up
        bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        static bool escWasDown = false;
        if (escDown && !escWasDown && clickguiOpen) {
            myiui::ui::clickgui::RequestSuppressEscUp();
            // 不能直接 Toggle（会再次切回开），用关闭语义
            // Toggle() 在打开时按 ESC 会变成开 -> 错误，所以这里直接关闭
            // 但 Toggle 是唯一接口；这里采取：再次 Toggle 会关 -> 但若用户连按会乱。
            // 改为：仅当打开时，按 ESC 直接关闭（通过再次 Toggle 实现，因为当前是开）
            myiui::ui::clickgui::Toggle();
        }
        escWasDown = escDown;
    }

    // ClickGui 打开时释放鼠标剪裁，使光标可移出窗口；关闭时恢复让游戏接管
    {
        static bool s_clickguiWasOpen = false;
        bool nowOpen = myiui::ui::clickgui::IsOpen();
        if (nowOpen && !s_clickguiWasOpen) {
            ClipCursor(nullptr);  // 解除光标剪裁，允许移出窗口
            ShowCursor(TRUE);
        }
        s_clickguiWasOpen = nowOpen;
    }

    if (overlayActive != g_wasMenuActive) {
        wchar_t buf[64]{};
        swprintf_s(buf, L"overlay_active %d -> %d", g_wasMenuActive ? 1 : 0, overlayActive ? 1 : 0);
        myiui::overlay::OverlayLog(buf);
    }
    if (islandActive) {
        myiui::shared::IslandState island{};
        if (g_shm.ReadIslandState(island)) {
            if (island.island_seq != g_lastIslandSeq || island.mode != g_lastIslandMode) {
                wchar_t buf[96]{};
                swprintf_s(buf, L"island_active mode=%u seq=%u", static_cast<unsigned>(island.mode),
                           island.island_seq);
                myiui::overlay::OverlayLog(buf);
                g_lastIslandSeq = island.island_seq;
                g_lastIslandMode = island.mode;
            }
        }
    }
    if (overlayActive && !g_wasMenuActive) {
        MenuAppOnMenuResumed(g_menuState);
        OverlayInvalidateBackgroundTexture();
        myiui::overlay::OverlayLog(L"Main menu resumed.");
    } else if (!overlayActive && g_wasMenuActive) {
        g_menuOverlayAcked = false;
        PipeSendCommandAsync("OVERLAY_SUSPEND");
        myiui::overlay::OverlayLog(L"Overlay suspended for game screen.");
    }
    g_wasMenuActive = overlayActive;
    g_menuActive.store(overlayActive);

    if (!shouldRender) {
        g_overlayRendering = false;
        return;
    }

    if (g_leaveGameGlCooldown > 0) {
        g_leaveGameGlCooldown--;
        g_overlayRendering = false;
        return;
    }

    if (!hwnd) {
        g_overlayRendering = false;
        return;
    }

    EnsureImGuiForHwnd(hwnd);
    if (!g_imguiReady) {
        g_overlayRendering = false;
        return;
    }

    if (overlayActive && !g_menuOverlayAcked) {
        PipeSendCommandAsync("OVERLAY_READY");
        g_menuOverlayAcked = true;
    }

    if (overlayActive) {
        ResumeOverlayInput();
    }
    // Keep WndProc hooked even in-game for ClickGui key detection

    TryInstallGlfwHook();

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        const uint32_t vw = static_cast<uint32_t>(viewport[2]);
        const uint32_t vh = static_cast<uint32_t>(viewport[3]);
        if (g_lastViewportW > 0 && g_lastViewportH > 0 &&
            (vw != g_lastViewportW || vh != g_lastViewportH)) {
            g_viewportResizeCooldown = 12;
            OverlayInvalidateBackgroundTexture();
            myiui::overlay::OverlayLog(L"Viewport resize — GL cooldown.");
        }
        g_lastViewportW = vw;
        g_lastViewportH = vh;
        ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(viewport[2]), static_cast<float>(viewport[3]));
    }

    if (g_viewportResizeCooldown > 0) {
        g_viewportResizeCooldown--;
        g_overlayRendering = false;
        return;
    }

    if (overlayActive) {
        PollSharedFrame();
    }

    if (g_configLoaded && viewport[2] > 0 && viewport[3] > 0) {
        const float scale = (std::min)(static_cast<float>(viewport[2]) / 1920.f,
                                       static_cast<float>(viewport[3]) / 1080.f);
        InitUiFonts(g_config.theme, scale);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_configLoaded && g_userSettingsLoaded && viewport[2] > 0 && viewport[3] > 0) {
        const float dt = ImGui::GetIO().DeltaTime;
        myiui::config::UserSettingsTick(dt);
        myiui::ui::theme::ThemeRuntimeTick(g_config, myiui::config::GetUserSettingsConst(), dt);
        myiui::ui::clickgui::SyncTheme(g_config.theme.accent);
    }

    // 处理待上传的封面纹理（音乐面板用）
    myiui::ui::music::MusicPanelTick();

    if (g_configLoaded) {
        EnsureLogos(g_config.root_path);
    }

    if (g_configLoaded && viewport[2] > 0 && viewport[3] > 0) {
        if (overlayActive) {
            const float scale = (std::min)(static_cast<float>(viewport[2]) / 1920.f,
                                           static_cast<float>(viewport[3]) / 1080.f);
            MenuRenderContext ctx{g_config, g_menuState, (ImTextureID)(intptr_t)g_bgTexture, g_bgTexture != 0, g_bgW,
                                  g_bgH, scale};
            MenuAppRender(ctx);
        } else if (islandActive || clickguiOpen) {
            const float dt = ImGui::GetIO().DeltaTime;
            if (myiui::ui::clickgui::HudVisible() && !clickguiOpen &&
                g_shm.GetScreenKind() == myiui::shared::ScreenKind::InGame) {
                myiui::ui::hud::HudRender(g_config, g_shm, static_cast<float>(viewport[2]),
                                          static_cast<float>(viewport[3]), dt);
            }
            // ClickGui 打开时隐藏灵动岛
            if (!clickguiOpen) {
                if (islandActive && myiui::ui::clickgui::IslandVisible()) {
                    myiui::ui::island::IslandRender(g_config.theme, g_shm, static_cast<float>(viewport[2]),
                                                    static_cast<float>(viewport[3]), dt);
                }
            }
            myiui::ui::clickgui::Render(static_cast<float>(viewport[2]),
                                        static_cast<float>(viewport[3]), dt);
        }
    }

    ImGui::Render();
    if (ImGui::GetDrawData() != nullptr) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    g_overlayRendering = false;
}

static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SIZE || msg == WM_DISPLAYCHANGE) {
        g_viewportResizeCooldown = 12;
        OverlayInvalidateBackgroundTexture();
    }
    // 先处理 ESC keyup 抑制（即使 ClickGui 刚被 ESC 关闭，也要吞掉这个 keyup）
    if (msg == WM_KEYUP && wParam == VK_ESCAPE) {
        if (myiui::ui::clickgui::ConsumeSuppressEscUp()) {
            return true;  // 吞掉，防止游戏弹出暂停菜单
        }
    }

    // 当 ClickGui 打开时，屏蔽所有输入到达游戏
    if (myiui::ui::clickgui::IsOpen() && g_imguiReady) {
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            return true;  // 吞掉所有鼠标事件
        }
        if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
            msg == WM_CHAR || msg == WM_INPUT) {
            // 让 Right Shift 和 Escape 由 RenderOverlayFrame 的轮询处理；这里只吞掉
            if (msg == WM_KEYDOWN && wParam == VK_SHIFT && (lParam & 0x01000000)) {
                return true;  // 不在此 Toggle，由轮询统一处理
            }
            if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                myiui::ui::clickgui::RequestSuppressEscUp();
                return true;  // 不在此关闭，由轮询统一处理；标记抑制 keyup
            }
            if (msg == WM_KEYUP && wParam == VK_ESCAPE) {
                return true;  // 已在上面统一处理
            }
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            return true;  // 吞掉所有键盘事件
        }
        // 阻止窗口激活/失焦事件触发游戏菜单（Alt-Tab 场景）
        if (msg == WM_ACTIVATE || msg == WM_ACTIVATEAPP || msg == WM_KILLFOCUS || msg == WM_SETFOCUS) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            return true;
        }
    }
    if ((g_menuActive.load() || myiui::ui::clickgui::IsOpen()) && g_imguiReady) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    }
    return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
}

static void EnsureImGuiForHwnd(HWND hwnd) {
    if (g_imguiReady) {
        if (hwnd && hwnd != g_hwnd) {
            g_hwnd = hwnd;
            MenuAppSetWindowHandle(g_hwnd);
        }
        return;
    }
    g_hwnd = hwnd;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL3_Init("#version 150");
    ImGui::StyleColorsDark();
    WNDPROC current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(g_hwnd, GWLP_WNDPROC));
    if (current != WndProcHook) {
        g_originalWndProc = current;
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook));
        g_wndProcHooked = true;
    }
    g_imguiReady = true;
    MenuAppSetWindowHandle(g_hwnd);
    myiui::overlay::OverlayLog(L"ImGui initialized.");
}

static void SuspendOverlayInput() {
    if (!g_imguiReady || !g_hwnd || !g_originalWndProc || !g_wndProcHooked) {
        return;
    }
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
    g_wndProcHooked = false;
}

static void ResumeOverlayInput() {
    if (!g_imguiReady || !g_hwnd || !g_originalWndProc || g_wndProcHooked) {
        return;
    }
    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook));
    g_wndProcHooked = true;
}

static bool TryInstallGlfwHook() {
    if (g_glfwHookInstalled) return true;

    HMODULE glfw = GetModuleHandleW(L"glfw.dll");
    if (!glfw) glfw = GetModuleHandleW(L"glfw3.dll");
    if (!glfw) return false;

    auto glfwSwap = reinterpret_cast<LPVOID>(GetProcAddress(glfw, "glfwSwapBuffers"));
    if (!glfwSwap) return false;

    if (MH_CreateHook(glfwSwap, &Hook_glfwSwapBuffers, reinterpret_cast<LPVOID*>(&g_originalGlfwSwap)) != MH_OK) {
        return false;
    }
    if (MH_EnableHook(glfwSwap) != MH_OK) {
        return false;
    }

    g_glfwHookInstalled = true;
    myiui::overlay::OverlayLog(L"Hooked glfwSwapBuffers.");
    return true;
}

static BOOL WINAPI Hook_wglSwapBuffers(HDC hdc) {
    if (g_insideGlfwSwap) {
        return g_originalWglSwap(hdc);
    }
    const HWND hwnd = WindowFromDC(hdc);
    RenderOverlayFrame(hwnd);
    return g_originalWglSwap(hdc);
}

static void __stdcall Hook_glfwSwapBuffers(void* window) {
    HWND hwnd = nullptr;
    HDC hdc = wglGetCurrentDC();
    if (hdc) hwnd = WindowFromDC(hdc);
    g_insideGlfwSwap = true;
    RenderOverlayFrame(hwnd);
    g_originalGlfwSwap(window);
    g_insideGlfwSwap = false;
}

static DWORD WINAPI InitThread(LPVOID) {
    Sleep(1000);
    myiui::jvm::RunJvmSpike();
    myiui::jvm::SpikeLog(L"[build] myiui-overlay v2 2026-07-02i");
    myiui::inject::InstallGameHook();

    const MH_STATUS mhInit = MH_Initialize();
    if (mhInit != MH_OK && mhInit != MH_ERROR_ALREADY_INITIALIZED) {
        myiui::overlay::OverlayLog(L"MinHook init failed.");
        return 1;
    }

    if (MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers", &Hook_wglSwapBuffers,
                         reinterpret_cast<LPVOID*>(&g_originalWglSwap)) != MH_OK) {
        myiui::overlay::OverlayLog(L"wglSwapBuffers hook failed.");
        return 2;
    }
    if (MH_EnableHook(reinterpret_cast<LPVOID>(g_originalWglSwap)) != MH_OK &&
        MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        myiui::overlay::OverlayLog(L"Enable wgl hook failed.");
        return 3;
    }

    myiui::overlay::OverlayLog(L"Hooked wglSwapBuffers.");

    for (int i = 0; i < 30 && !g_glfwHookInstalled; ++i) {
        if (TryInstallGlfwHook()) break;
        Sleep(1000);
    }

    myiui::overlay::OverlayLog(L"Overlay hooks ready.");
    return 0;
}

void HooksInit(HMODULE) {
    myiui::overlay::OverlayLog(L"myiui-overlay.dll loaded (v2 JVMTI+JNI).");
    CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
}

void OverlayRequestConfigReload() {
    g_configLoaded = false;
    g_lastFrameIndex = UINT32_MAX;
    InvalidateUiFonts();
    myiui::overlay::OverlayLog(L"Overlay config reload requested.");
}
