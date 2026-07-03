#include "gui_app.h"
#include "gui_injector.h"
#include "logo_assets.h"

#include "inject_core.h"
#include "process_scanner.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <tchar.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

constexpr int kWindowWidth = 920;
constexpr int kWindowHeight = 640;

ID3D11Device* gPd3dDevice = nullptr;
ID3D11DeviceContext* gPd3dDeviceContext = nullptr;
IDXGISwapChain* gPSwapChain = nullptr;
ID3D11RenderTargetView* gPMainRenderTargetView = nullptr;
myiui::injector_ui::GuiState gGui;

bool CreateDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags, levels, 2,
                                      D3D11_SDK_VERSION, &sd, &gPSwapChain, &gPd3dDevice, &featureLevel,
                                      &gPd3dDeviceContext) != S_OK) {
        return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    gPSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        gPd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &gPMainRenderTargetView);
        backBuffer->Release();
    }
    return gPMainRenderTargetView != nullptr;
}

void CleanupDeviceD3D() {
    if (gPMainRenderTargetView) {
        gPMainRenderTargetView->Release();
        gPMainRenderTargetView = nullptr;
    }
    if (gPSwapChain) {
        gPSwapChain->Release();
        gPSwapChain = nullptr;
    }
    if (gPd3dDeviceContext) {
        gPd3dDeviceContext->Release();
        gPd3dDeviceContext = nullptr;
    }
    if (gPd3dDevice) {
        gPd3dDevice->Release();
        gPd3dDevice = nullptr;
    }
}

void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    gPSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        gPd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &gPMainRenderTargetView);
        backBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (gPMainRenderTargetView) {
        gPMainRenderTargetView->Release();
        gPMainRenderTargetView = nullptr;
    }
}

void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 14.f;
    style.FrameRounding = 8.f;
    style.ScrollbarRounding = 8.f;
    style.WindowBorderSize = 0.f;
    style.FrameBorderSize = 0.f;
    style.WindowPadding = ImVec2(12, 12);
    style.ItemSpacing = ImVec2(8, 6);
}

static std::string WidePathToUtf8(const wchar_t* path) {
    if (!path || !*path) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, path, -1, out.data(), len, nullptr, nullptr);
    return out;
}

static ImFont* LoadFontFromPath(const wchar_t* path, float sizePx, int fontNo = 0) {
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return nullptr;

    const std::string utf8 = WidePathToUtf8(path);
    if (utf8.empty()) return nullptr;

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;
    cfg.FontNo = fontNo;

    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseFull();
    return io.Fonts->AddFontFromFileTTF(utf8.c_str(), sizePx, &cfg, ranges);
}

bool LoadUIFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    wchar_t winDir[MAX_PATH]{};
    std::wstring fontsDir = L"C:\\Windows\\Fonts\\";
    if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
        fontsDir = std::wstring(winDir) + L"\\Fonts\\";
    }

    const wchar_t* candidates[] = {
        L"msyh.ttc",
        L"msyhbd.ttc",
        L"msyhl.ttc",
        L"simhei.ttf",
        L"simsun.ttc",
        L"msjh.ttc",
        L"msjhbd.ttc",
    };

    ImFont* font = nullptr;
    for (const wchar_t* name : candidates) {
        const std::wstring path = fontsDir + name;
        font = LoadFontFromPath(path.c_str(), 16.f, 0);
        if (font) break;
    }

    if (!font) {
        font = LoadFontFromPath(L"C:\\Windows\\Fonts\\msyh.ttc", 16.f, 0);
    }

    if (!font) {
        io.Fonts->AddFontDefault();
    }

    io.Fonts->Build();
    return font != nullptr;
}

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;

    switch (msg) {
        case WM_SIZE:
            if (gPd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                gPSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool ShouldRunCli(int argc, wchar_t** argv) {
    if (argc >= 2 && (_wcsicmp(argv[1], L"--cli") == 0 || _wcsicmp(argv[1], L"-c") == 0)) {
        return true;
    }
    if (argc == 2) {
        for (const wchar_t* p = argv[1]; *p; ++p) {
            if (*p < L'0' || *p > L'9') return false;
        }
        return true;
    }
    return false;
}

}  // namespace

int RunCli(int argc, wchar_t** argv) {
    int argIndex = 1;
    if (argc >= 2 && (_wcsicmp(argv[1], L"--cli") == 0 || _wcsicmp(argv[1], L"-c") == 0)) {
        argIndex = 2;
    }

    DWORD pid = 0;
    if (argIndex < argc) {
        pid = static_cast<DWORD>(_wtoi(argv[argIndex]));
    } else {
        const auto processes = myiui::ScanJavaProcesses();
        if (processes.empty()) {
            fwprintf(stderr, L"[MyiUI] No java.exe / javaw.exe found.\n");
            return 1;
        }
        pid = processes.front().pid;
        fwprintf(stdout, L"[MyiUI] Using PID %lu (%ls, recommended).\n", pid, processes.front().exeName.c_str());
    }

    const bool ok = myiui::RunInjection(pid, [](const std::wstring& line, bool isError) {
        if (isError) {
            fwprintf(stderr, L"%ls\n", line.c_str());
        } else {
            fwprintf(stdout, L"%ls\n", line.c_str());
        }
    });
    return ok ? 0 : 1;
}

int RunGui(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"MyiUIInjectorNative";
    RegisterClassExW(&wc);

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int posX = (screenW - kWindowWidth) / 2;
    const int posY = (screenH - kWindowHeight) / 2;

    const DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    RECT clientRect{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&clientRect, windowStyle, FALSE);
    const int winW = clientRect.right - clientRect.left;
    const int winH = clientRect.bottom - clientRect.top;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"MyiUI Injector", windowStyle, posX, posY, winW, winH,
                                nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd) return 1;

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    SetupImGuiStyle();
    LoadUIFonts();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(gPd3dDevice, gPd3dDeviceContext);

    InitInjectorLogos(gPd3dDevice, myiui::GetProjectRoot());
    myiui::injector_ui::Init(gGui, hwnd);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        myiui::injector_ui::Render(gGui);

        ImGui::Render();
        const float clear[4] = {0.08f, 0.09f, 0.12f, 1.f};
        gPd3dDeviceContext->OMSetRenderTargets(1, &gPMainRenderTargetView, nullptr);
        gPd3dDeviceContext->ClearRenderTargetView(gPMainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        gPSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    ShutdownInjectorLogos();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;

    const bool cli = ShouldRunCli(argc, argv);
    const int code = cli ? RunCli(argc, argv) : RunGui(instance);
    LocalFree(argv);
    return code;
}
