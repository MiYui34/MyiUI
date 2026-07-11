#include "web/web_engine.h"

#include "overlay_runtime.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>

#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>

#include <atomic>
#include <mutex>
#include <string>

#pragma comment(lib, "version.lib")

namespace myiui::web {
namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

struct EngineState {
    std::mutex mu;
    bool init_started = false;
    bool com_inited = false;
    bool ready = false;
    bool failed = false;
    HWND parent = nullptr;
    std::wstring user_data;
    std::wstring error;
    ComPtr<ICoreWebView2Environment> env;
};

EngineState g_engine;

std::wstring ResolveUserDataDir() {
    wchar_t localAppData[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        const std::wstring root = std::wstring(localAppData) + L"\\MyiUI";
        const std::wstring dir = root + L"\\webview2-user-data";
        CreateDirectoryW(root.c_str(), nullptr);
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L".\\webview2-user-data";
}

void SetFailed(const std::wstring& msg) {
    std::lock_guard<std::mutex> lock(g_engine.mu);
    g_engine.failed = true;
    g_engine.ready = false;
    g_engine.error = msg;
    myiui::overlay::OverlayLog((L"WebEngine: " + msg).c_str());
}

}  // namespace

void WebEngineSetParentHwnd(HWND hwnd) {
    std::lock_guard<std::mutex> lock(g_engine.mu);
    g_engine.parent = hwnd;
}

HWND WebEngineGetParentHwnd() {
    std::lock_guard<std::mutex> lock(g_engine.mu);
    return g_engine.parent;
}

bool WebEngineInit() {
    {
        std::lock_guard<std::mutex> lock(g_engine.mu);
        if (g_engine.ready) {
            return true;
        }
        if (g_engine.failed) {
            return false;
        }
        if (g_engine.init_started) {
            return false;
        }
        g_engine.init_started = true;
        g_engine.user_data = ResolveUserDataDir();
    }

    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (hrCom == S_OK || hrCom == S_FALSE) {
        std::lock_guard<std::mutex> lock(g_engine.mu);
        g_engine.com_inited = (hrCom == S_OK);
    } else if (hrCom == RPC_E_CHANGED_MODE) {
        // Already initialized differently on this thread — continue.
        myiui::overlay::OverlayLog(L"WebEngine: COM already initialized (RPC_E_CHANGED_MODE).");
    } else {
        SetFailed(L"CoInitializeEx failed: 0x" + std::to_wstring(static_cast<unsigned long>(hrCom)));
        return false;
    }

    const std::wstring userData = [&]() {
        std::lock_guard<std::mutex> lock(g_engine.mu);
        return g_engine.user_data;
    }();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    wchar_t buf[256]{};
                    swprintf_s(buf,
                               L"CreateCoreWebView2Environment failed hr=0x%08lX. "
                               L"Install Evergreen WebView2 Runtime.",
                               static_cast<unsigned long>(result));
                    SetFailed(buf);
                    return S_OK;
                }
                {
                    std::lock_guard<std::mutex> lock(g_engine.mu);
                    g_engine.env = env;
                    g_engine.ready = true;
                    g_engine.failed = false;
                    g_engine.error.clear();
                }
                myiui::overlay::OverlayLog(L"WebEngine: WebView2 Environment ready.");
                return S_OK;
            })
            .Get());

    if (FAILED(hr)) {
        wchar_t buf[256]{};
        swprintf_s(buf,
                   L"CreateCoreWebView2EnvironmentWithOptions hr=0x%08lX. "
                   L"Install Evergreen WebView2 Runtime from Microsoft.",
                   static_cast<unsigned long>(hr));
        SetFailed(buf);
        return false;
    }

    myiui::overlay::OverlayLog((L"WebEngine: creating Environment, userData=" + userData).c_str());
    return true;
}

bool WebEngineIsReady() {
    std::lock_guard<std::mutex> lock(g_engine.mu);
    return g_engine.ready && g_engine.env;
}

bool WebEngineHasFailed() {
    std::lock_guard<std::mutex> lock(g_engine.mu);
    return g_engine.failed;
}

const std::wstring& WebEngineErrorMessage() {
    // Stable reference under lock is awkward; return a static copy when failed.
    static std::wstring cached;
    std::lock_guard<std::mutex> lock(g_engine.mu);
    cached = g_engine.error;
    return cached;
}

ICoreWebView2Environment* WebEngineGetEnvironment() {
    std::lock_guard<std::mutex> lock(g_engine.mu);
    return g_engine.env.Get();
}

void WebEngineShutdown() {
    ComPtr<ICoreWebView2Environment> env;
    bool com_inited = false;
    {
        std::lock_guard<std::mutex> lock(g_engine.mu);
        env.Swap(g_engine.env);
        com_inited = g_engine.com_inited;
        g_engine.ready = false;
        g_engine.failed = false;
        g_engine.init_started = false;
        g_engine.error.clear();
        g_engine.com_inited = false;
    }
    env.Reset();
    if (com_inited) {
        CoUninitialize();
    }
    myiui::overlay::OverlayLog(L"WebEngineShutdown: WebView2 Environment released.");
}

}  // namespace myiui::web
