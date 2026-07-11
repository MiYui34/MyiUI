#pragma once

#include <windows.h>

#include <string>

struct ICoreWebView2Environment;

namespace myiui::web {

// WebView2 Environment singleton (Evergreen runtime).
bool WebEngineInit();
bool WebEngineIsReady();
bool WebEngineHasFailed();
const std::wstring& WebEngineErrorMessage();
void WebEngineShutdown();

// Parent HWND for CreateCoreWebView2Controller (game window).
void WebEngineSetParentHwnd(HWND hwnd);
HWND WebEngineGetParentHwnd();

ICoreWebView2Environment* WebEngineGetEnvironment();

}  // namespace myiui::web
