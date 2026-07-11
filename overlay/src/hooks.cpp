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
#include "render/frame_runtime.h"
#include "render/gl_state_guard.h"
#include "ui/logo_assets.h"
#include "ui/fonts.h"
#include "ui/menu_app.h"
#include "ui/clickgui/clickgui.h"
#include "ui/hud/hud_renderer.h"
#include "ui/hud/layout_editor.h"
#include "ui/island/island_renderer.h"
#include "ui/music/music_panel.h"
#include "ui/theme/theme_runtime.h"
#include "web/web_panel.h"
#include "web/web_engine.h"

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
#include <string>
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
static uint32_t g_menuOverlayAckedSeq = 0;
static uint32_t g_lastScreenSeq = 0;
static myiui::shared::ScreenKind g_lastScreenKind = myiui::shared::ScreenKind::None;
static thread_local bool g_insideGlfwSwap = false;

enum class BootstrapState : int {
    Uninitialized = 0,
    Initializing,
    HooksReady,
    AgentReady,
    Failed,
};

static std::atomic<bool> g_initStarted{false};
static std::atomic<int> g_bootstrapState{static_cast<int>(BootstrapState::Uninitialized)};

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

// GLFW cursor modes (glfw3.h) — resolved at runtime from the game's glfw DLL.
static constexpr int kGlfwCursor = 0x00033001;
static constexpr int kGlfwCursorNormal = 0x00034001;
static constexpr int kGlfwCursorDisabled = 0x00034003;

using glfwGetCurrentContext_t = void* (*)();
using glfwGetInputMode_t = int (*)(void*, int);
using glfwSetInputMode_t = void (*)(void*, int, int);

static glfwGetCurrentContext_t g_glfwGetCurrentContext = nullptr;
static glfwGetInputMode_t g_glfwGetInputMode = nullptr;
static glfwSetInputMode_t g_glfwSetInputMode = nullptr;
static bool g_glfwInputApiResolved = false;
static bool g_savedGlfwCursorDisabled = false;
static bool g_uiMouseReleased = false;

static bool ResolveGlfwInputApi() {
    if (g_glfwInputApiResolved) {
        return g_glfwGetCurrentContext && g_glfwGetInputMode && g_glfwSetInputMode;
    }
    g_glfwInputApiResolved = true;
    HMODULE glfw = GetModuleHandleW(L"glfw.dll");
    if (!glfw) glfw = GetModuleHandleW(L"glfw3.dll");
    if (!glfw) return false;
    g_glfwGetCurrentContext =
        reinterpret_cast<glfwGetCurrentContext_t>(GetProcAddress(glfw, "glfwGetCurrentContext"));
    g_glfwGetInputMode = reinterpret_cast<glfwGetInputMode_t>(GetProcAddress(glfw, "glfwGetInputMode"));
    g_glfwSetInputMode = reinterpret_cast<glfwSetInputMode_t>(GetProcAddress(glfw, "glfwSetInputMode"));
    return g_glfwGetCurrentContext && g_glfwGetInputMode && g_glfwSetInputMode;
}

static void ShowWindowsCursor(bool show) {
    if (show) {
        while (ShowCursor(TRUE) < 0) {}
    } else {
        while (ShowCursor(FALSE) >= 0) {}
    }
}

static void ReleaseGameMouseCapture() {
    if (g_uiMouseReleased) return;
    g_savedGlfwCursorDisabled = false;
    if (ResolveGlfwInputApi()) {
        if (void* window = g_glfwGetCurrentContext()) {
            const int mode = g_glfwGetInputMode(window, kGlfwCursor);
            if (mode == kGlfwCursorDisabled) {
                g_savedGlfwCursorDisabled = true;
                g_glfwSetInputMode(window, kGlfwCursor, kGlfwCursorNormal);
            }
        }
    }
    ClipCursor(nullptr);
    ShowWindowsCursor(true);
    g_uiMouseReleased = true;
}

static void RestoreGameMouseCapture(bool inGame) {
    if (!g_uiMouseReleased) return;
    g_uiMouseReleased = false;
    if (inGame && g_savedGlfwCursorDisabled && ResolveGlfwInputApi()) {
        if (void* window = g_glfwGetCurrentContext()) {
            g_glfwSetInputMode(window, kGlfwCursor, kGlfwCursorDisabled);
        }
    }
    g_savedGlfwCursorDisabled = false;
}

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

    // Save OpenGL state
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &last_unpack_alignment);
    GLint last_unpack_row_length = 0;
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &last_unpack_row_length);
    GLint last_unpack_skip_pixels = 0;
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &last_unpack_skip_pixels);
    GLint last_unpack_skip_rows = 0;
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &last_unpack_skip_rows);

    // Clear error flag before our operations
    myiui::render::ClearGlErrors();

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

    // Restore OpenGL state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, last_unpack_alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, last_unpack_row_length);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, last_unpack_skip_pixels);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, last_unpack_skip_rows);
    
    // Clear any errors we might have caused so we don't pollute the game's GL state
    myiui::render::ClearGlErrors();
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

    // Poll ClickGui toggle key every frame (before shouldRender check)
    {
        const bool clickguiOpenBefore = myiui::ui::clickgui::IsOpen();
        bool rshiftDown = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
        static bool rshiftWasDown = false;
        if (rshiftDown && !rshiftWasDown) {
            myiui::ui::clickgui::Toggle();
        }
        rshiftWasDown = rshiftDown;

        // ESC 关闭 ClickGui (仅在打开时)；请求抑制随后的 ESC up
        bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        static bool escWasDown = false;
        if (escDown && !escWasDown && clickguiOpenBefore) {
            myiui::ui::clickgui::RequestSuppressEscUp();
            myiui::ui::clickgui::Toggle();
        }
        escWasDown = escDown;
    }

    const bool clickguiOpen = myiui::ui::clickgui::IsOpen();
    const bool webPanelActive = myiui::web::WebPanelActive();
    const bool shouldRender = overlayActive || islandActive || clickguiOpen || webPanelActive;

    // 主菜单 overlay、ClickGui 或 Web 面板需要 UI 鼠标：解除 GLFW 光标捕获并显示系统光标
    {
        const bool needsUiMouse = overlayActive || clickguiOpen || webPanelActive;
        static bool s_uiMouseActive = false;
        if (needsUiMouse && !s_uiMouseActive) {
            ReleaseGameMouseCapture();
            s_uiMouseActive = true;
        } else if (!needsUiMouse && s_uiMouseActive) {
            const bool inGame = screenKind == myiui::shared::ScreenKind::InGame;
            RestoreGameMouseCapture(inGame);
            s_uiMouseActive = false;
        }
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
        g_menuOverlayAckedSeq = 0;
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

    if (overlayActive && screenSeq != 0 && g_menuOverlayAckedSeq != screenSeq) {
        PipeSendCommandAsync("OVERLAY_READY:" + std::to_string(screenSeq));
        g_menuOverlayAckedSeq = screenSeq;
    }

    if (overlayActive || clickguiOpen || webPanelActive) {
        ResumeOverlayInput();
    }

    TryInstallGlfwHook();

    const myiui::render::FrameViewport viewport = myiui::render::ReadViewport();
    
    // 保护：如果 viewport 无效，则跳过渲染
    if (!viewport.valid()) {
        g_overlayRendering = false;
        return;
    }

    const uint32_t vw = static_cast<uint32_t>(viewport.width);
    const uint32_t vh = static_cast<uint32_t>(viewport.height);
    if (g_lastViewportW > 0 && g_lastViewportH > 0 &&
        (vw != g_lastViewportW || vh != g_lastViewportH)) {
        g_viewportResizeCooldown = 12;
        OverlayInvalidateBackgroundTexture();
        myiui::overlay::OverlayLog(L"Viewport resize — GL cooldown.");
    }
    g_lastViewportW = vw;
    g_lastViewportH = vh;
    ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(viewport.width), static_cast<float>(viewport.height));

    if (g_viewportResizeCooldown > 0) {
        g_viewportResizeCooldown--;
        g_overlayRendering = false;
        return;
    }

    if (overlayActive) {
        PollSharedFrame();
    }

    if (g_configLoaded) {
        const float scale = (std::min)(static_cast<float>(viewport.width) / 1920.f,
                                       static_cast<float>(viewport.height) / 1080.f);
        InitUiFonts(g_config.theme, scale);
    }

    myiui::render::GlStateGuard glGuard;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_configLoaded && g_userSettingsLoaded) {
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

    if (g_configLoaded) {
        if (overlayActive) {
            const float scale = (std::min)(static_cast<float>(viewport.width) / 1920.f,
                                           static_cast<float>(viewport.height) / 1080.f);
            MenuRenderContext ctx{g_config, g_menuState, (ImTextureID)(intptr_t)g_bgTexture, g_bgTexture != 0, g_bgW,
                                  g_bgH, scale};
            MenuAppRender(ctx);
        } else if (islandActive || clickguiOpen) {
            const float dt = ImGui::GetIO().DeltaTime;
            const bool layoutEdit = clickguiOpen && myiui::config::GetUserSettingsConst().layout_editor_enabled;
            const bool inGame = g_shm.GetScreenKind() == myiui::shared::ScreenKind::InGame;
            if (myiui::ui::clickgui::HudVisible() && (!clickguiOpen || layoutEdit) && inGame) {
                myiui::ui::hud::HudRender(g_config, g_shm, static_cast<float>(viewport.width),
                                          static_cast<float>(viewport.height), dt, layoutEdit);
            }
            if ((!clickguiOpen || layoutEdit) && islandActive && myiui::ui::clickgui::IslandVisible()) {
                myiui::ui::island::IslandRender(g_config.theme, g_shm, static_cast<float>(viewport.width),
                                                static_cast<float>(viewport.height), dt);
            }
            if (!clickguiOpen && inGame && myiui::ui::clickgui::HudVisible()) {
                myiui::ui::hud::HudRenderImmersiveLyrics(g_config, g_shm, static_cast<float>(viewport.width),
                                                         static_cast<float>(viewport.height));
            }
            if (layoutEdit && inGame) {
                myiui::ui::hud::LayoutEditorRender(g_config, g_shm, static_cast<float>(viewport.width),
                                                    static_cast<float>(viewport.height));
            }
            myiui::ui::clickgui::Render(static_cast<float>(viewport.width),
                                        static_cast<float>(viewport.height), dt);
        }
    }

    // WebView2 browser (in-game only; gated inside WebPanelWanted).
    myiui::web::WebPanelTickAndRender();

    ImGui::Render();
    if (ImGui::GetDrawData() != nullptr) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    g_overlayRendering = false;
}

static LRESULT CALLBACK WndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SIZE || msg == WM_MOVE || msg == WM_DISPLAYCHANGE) {
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
    // Web 面板：把输入交给 ImGui，但仅在 ImGui 需要时吞掉，避免挡住游戏点击
    if (myiui::web::WebPanelActive() && g_imguiReady && !myiui::ui::clickgui::IsOpen()) {
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            if (ImGui::GetIO().WantCaptureMouse) {
                return true;
            }
        } else if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
                   msg == WM_CHAR) {
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            if (ImGui::GetIO().WantCaptureKeyboard) {
                return true;
            }
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
            myiui::web::WebEngineSetParentHwnd(g_hwnd);
        }
        return;
    }
    g_hwnd = hwnd;
    myiui::web::WebEngineSetParentHwnd(g_hwnd);
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
    
    // Check if we have a valid OpenGL context before trying to render
    if (myiui::render::HasCurrentGlContext()) {
        // Clear any pre-existing errors before we start rendering
        myiui::render::ClearGlErrors();
        RenderOverlayFrame(hwnd);
    }
    
    return g_originalWglSwap(hdc);
}

static void __stdcall Hook_glfwSwapBuffers(void* window) {
    HWND hwnd = nullptr;
    HDC hdc = wglGetCurrentDC();
    if (hdc) hwnd = WindowFromDC(hdc);
    g_insideGlfwSwap = true;
    
    // Check if we have a valid OpenGL context before trying to render
    if (myiui::render::HasCurrentGlContext()) {
        // Clear any pre-existing errors before we start rendering
        myiui::render::ClearGlErrors();
        RenderOverlayFrame(hwnd);
    }
    
    g_originalGlfwSwap(window);
    g_insideGlfwSwap = false;
}

static DWORD WINAPI InitThread(LPVOID) {
    g_bootstrapState.store(static_cast<int>(BootstrapState::Initializing), std::memory_order_release);
    Sleep(1000);
    myiui::jvm::RunJvmSpike();
    myiui::jvm::SpikeLog(L"[build] myiui-overlay v2 multiver 2026-07-04");
    const bool lwjglHookOk = myiui::inject::InstallGameHook();

    const MH_STATUS mhInit = MH_Initialize();
    if (mhInit != MH_OK && mhInit != MH_ERROR_ALREADY_INITIALIZED) {
        myiui::overlay::OverlayLog(L"MinHook init failed.");
        g_bootstrapState.store(static_cast<int>(BootstrapState::Failed), std::memory_order_release);
        return 1;
    }

    const MH_STATUS createWgl =
        MH_CreateHookApi(L"opengl32.dll", "wglSwapBuffers", &Hook_wglSwapBuffers,
                         reinterpret_cast<LPVOID*>(&g_originalWglSwap));
    if (createWgl != MH_OK && createWgl != MH_ERROR_ALREADY_CREATED) {
        myiui::overlay::OverlayLog(L"wglSwapBuffers hook failed.");
        g_bootstrapState.store(static_cast<int>(BootstrapState::Failed), std::memory_order_release);
        return 2;
    }
    const MH_STATUS enableWgl = MH_EnableHook(reinterpret_cast<LPVOID>(g_originalWglSwap));
    if (enableWgl != MH_OK && enableWgl != MH_ERROR_ENABLED &&
        MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        myiui::overlay::OverlayLog(L"Enable wgl hook failed.");
        g_bootstrapState.store(static_cast<int>(BootstrapState::Failed), std::memory_order_release);
        return 3;
    }

    myiui::overlay::OverlayLog(L"Hooked wglSwapBuffers.");

    for (int i = 0; i < 30 && !g_glfwHookInstalled; ++i) {
        if (TryInstallGlfwHook()) break;
        Sleep(1000);
    }

    myiui::overlay::OverlayLog(L"Overlay hooks ready.");
    g_bootstrapState.store(static_cast<int>(BootstrapState::HooksReady), std::memory_order_release);

    // Agent bootstrap fallback runs on this initialization thread, never on the render/swap path.
    if (!lwjglHookOk) {
        myiui::jvm::SpikeLog(L"[hook] using bootstrap thread fallback for agent startup");
    }
    for (int i = 0; i < 60 && !myiui::inject::IsJvmEntryDone(); ++i) {
        if (myiui::jvm::IsReady()) {
            if (JNIEnv* env = myiui::jvm::AttachEnv()) {
                myiui::inject::TryRunJvmEntry(env);
            }
        }
        Sleep(500);
    }
    if (myiui::inject::IsJvmEntryDone()) {
        g_bootstrapState.store(static_cast<int>(BootstrapState::AgentReady), std::memory_order_release);
        myiui::overlay::OverlayLog(L"Agent bootstrap ready.");
    } else {
        myiui::overlay::OverlayLog(L"Agent bootstrap not ready yet; overlay will wait for Java state.");
    }
    return 0;
}

void HooksInit(HMODULE) {
    myiui::overlay::OverlayLog(L"myiui-overlay.dll loaded (v2 JVMTI+JNI).");
    if (g_initStarted.exchange(true, std::memory_order_acq_rel)) {
        myiui::overlay::OverlayLog(L"HooksInit ignored — overlay already initialized.");
        return;
    }
    HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
}

void OverlayRequestConfigReload() {
    g_configLoaded = false;
    g_lastFrameIndex = UINT32_MAX;
    InvalidateUiFonts();
    myiui::overlay::OverlayLog(L"Overlay config reload requested.");
}

void OverlayWebShutdown() {
    myiui::web::WebPanelShutdown();
}
