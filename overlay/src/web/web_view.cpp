#include "web/web_view.h"

#include "web/web_engine.h"

#include "overlay_runtime.h"

#include <windows.h>
#include <objbase.h>

#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>

#include <mutex>
#include <string>

namespace myiui::web {
namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string WideToUtf8(const wchar_t* w) {
    if (!w || !*w) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}

}  // namespace

struct WebView::Impl {
    mutable std::mutex mu;
    bool create_started = false;
    bool ready = false;
    bool loading = false;
    bool visible = false;
    bool destroyed = false;
    RECT bounds{0, 0, 1, 1};
    std::string pending_url;
    std::string pending_html;
    std::string current_url;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    EventRegistrationToken nav_starting_token{};
    EventRegistrationToken nav_completed_token{};
    EventRegistrationToken source_changed_token{};
    EventRegistrationToken new_window_token{};
    EventRegistrationToken external_uri_token{};

    static bool IsAllowedWebScheme(const wchar_t* uri) {
        if (!uri || !*uri) {
            return false;
        }
        // Keep navigations inside the embedded browser; block app/PWA protocol launches.
        if (_wcsnicmp(uri, L"http://", 7) == 0 || _wcsnicmp(uri, L"https://", 8) == 0) {
            return true;
        }
        if (_wcsnicmp(uri, L"about:", 6) == 0 || _wcsnicmp(uri, L"data:", 5) == 0 ||
            _wcsnicmp(uri, L"blob:", 5) == 0) {
            return true;
        }
        return false;
    }

    void FlushPendingUnlocked() {
        if (!webview) {
            return;
        }
        if (!pending_html.empty()) {
            const std::wstring html = Utf8ToWide(pending_html);
            pending_html.clear();
            webview->NavigateToString(html.c_str());
        }
        if (!pending_url.empty()) {
            const std::wstring url = Utf8ToWide(pending_url);
            pending_url.clear();
            webview->Navigate(url.c_str());
        }
    }

    void ApplyBoundsUnlocked() {
        if (controller) {
            controller->put_Bounds(bounds);
        }
    }

    void ApplyVisibleUnlocked() {
        if (controller) {
            controller->put_IsVisible(visible ? TRUE : FALSE);
        }
    }

    void WireEvents() {
        if (!webview) {
            return;
        }

        webview->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                    if (!args) {
                        return S_OK;
                    }
                    LPWSTR uri = nullptr;
                    if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                        const bool allowed = IsAllowedWebScheme(uri);
                        if (!allowed) {
                            args->put_Cancel(TRUE);
                            wchar_t buf[512]{};
                            swprintf_s(buf, L"WebView: blocked external/app navigation: %s", uri);
                            myiui::overlay::OverlayLog(buf);
                        }
                        CoTaskMemFree(uri);
                        if (!allowed) {
                            std::lock_guard<std::mutex> lock(mu);
                            loading = false;
                            return S_OK;
                        }
                    }
                    std::lock_guard<std::mutex> lock(mu);
                    loading = true;
                    return S_OK;
                })
                .Get(),
            &nav_starting_token);

        webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                    std::lock_guard<std::mutex> lock(mu);
                    loading = false;
                    return S_OK;
                })
                .Get(),
            &nav_completed_token);

        webview->add_SourceChanged(
            Callback<ICoreWebView2SourceChangedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
                    if (!sender) {
                        return S_OK;
                    }
                    LPWSTR uri = nullptr;
                    if (SUCCEEDED(sender->get_Source(&uri)) && uri) {
                        std::lock_guard<std::mutex> lock(mu);
                        current_url = WideToUtf8(uri);
                        CoTaskMemFree(uri);
                    }
                    return S_OK;
                })
                .Get(),
            &source_changed_token);

        // target=_blank / window.open → stay in this WebView (no Edge/PWA popup).
        webview->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                    if (!args) {
                        return S_OK;
                    }
                    args->put_Handled(TRUE);
                    LPWSTR uri = nullptr;
                    if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                        if (IsAllowedWebScheme(uri) && sender) {
                            sender->Navigate(uri);
                        } else {
                            wchar_t buf[512]{};
                            swprintf_s(buf, L"WebView: blocked new-window to non-web URI: %s", uri);
                            myiui::overlay::OverlayLog(buf);
                        }
                        CoTaskMemFree(uri);
                    }
                    return S_OK;
                })
                .Get(),
            &new_window_token);

        // snssdk:// / bytedance:// / microsoft-edge: etc. — do not launch OS apps.
        ComPtr<ICoreWebView2_18> wv18;
        if (SUCCEEDED(webview.As(&wv18)) && wv18) {
            wv18->add_LaunchingExternalUriScheme(
                Callback<ICoreWebView2LaunchingExternalUriSchemeEventHandler>(
                    [](ICoreWebView2*, ICoreWebView2LaunchingExternalUriSchemeEventArgs* args) -> HRESULT {
                        if (!args) {
                            return S_OK;
                        }
                        LPWSTR uri = nullptr;
                        if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                            wchar_t buf[512]{};
                            swprintf_s(buf, L"WebView: cancelled external URI scheme: %s", uri);
                            myiui::overlay::OverlayLog(buf);
                            CoTaskMemFree(uri);
                        }
                        args->put_Cancel(TRUE);
                        return S_OK;
                    })
                    .Get(),
                &external_uri_token);
        }
    }

    void OnControllerCreated(ICoreWebView2Controller* ctrl) {
        if (!ctrl) {
            return;
        }
        ComPtr<ICoreWebView2> wv;
        ctrl->get_CoreWebView2(&wv);
        if (!wv) {
            myiui::overlay::OverlayLog(L"WebView: get_CoreWebView2 failed.");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mu);
            if (destroyed) {
                ctrl->Close();
                return;
            }
            controller = ctrl;
            webview = wv;
            ready = true;
            ApplyBoundsUnlocked();
            ApplyVisibleUnlocked();
            WireEvents();
            FlushPendingUnlocked();
        }

        ComPtr<ICoreWebView2Settings> settings;
        if (SUCCEEDED(wv->get_Settings(&settings)) && settings) {
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_AreDefaultContextMenusEnabled(TRUE);
            settings->put_IsZoomControlEnabled(TRUE);
            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        }

        myiui::overlay::OverlayLog(L"WebView: WebView2 Controller ready.");
    }
};

WebView::WebView() = default;

WebView::~WebView() {
    Destroy();
}

bool WebView::Create(uint32_t width, uint32_t height) {
    if (!impl_) {
        impl_ = new Impl();
    }
    width_ = width == 0 ? 1 : width;
    height_ = height == 0 ? 1 : height;

    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        impl_->bounds = {0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        if (impl_->ready || impl_->create_started) {
            return impl_->ready;
        }
    }

    if (!WebEngineIsReady()) {
        WebEngineInit();
        return false;
    }

    ICoreWebView2Environment* env = WebEngineGetEnvironment();
    HWND parent = WebEngineGetParentHwnd();
    if (!env || !parent) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        impl_->create_started = true;
        impl_->destroyed = false;
    }

    Impl* self = impl_;
    const HRESULT hr = env->CreateCoreWebView2Controller(
        parent, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [self](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(result) || !controller) {
                            wchar_t buf[128]{};
                            swprintf_s(buf, L"WebView: CreateController failed hr=0x%08lX",
                                       static_cast<unsigned long>(result));
                            myiui::overlay::OverlayLog(buf);
                            std::lock_guard<std::mutex> lock(self->mu);
                            self->create_started = false;
                            return S_OK;
                        }
                        self->OnControllerCreated(controller);
                        return S_OK;
                    })
                    .Get());

    if (FAILED(hr)) {
        wchar_t buf[128]{};
        swprintf_s(buf, L"WebView: CreateCoreWebView2Controller hr=0x%08lX",
                   static_cast<unsigned long>(hr));
        myiui::overlay::OverlayLog(buf);
        std::lock_guard<std::mutex> lock(impl_->mu);
        impl_->create_started = false;
        return false;
    }
    return false;  // async — ready later
}

void WebView::Destroy() {
    if (!impl_) {
        return;
    }

    ComPtr<ICoreWebView2Controller> ctrl;
    ComPtr<ICoreWebView2> wv;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        impl_->destroyed = true;
        impl_->ready = false;
        impl_->create_started = false;
        wv.Swap(impl_->webview);
        ctrl.Swap(impl_->controller);
    }

    if (wv) {
        if (impl_->nav_starting_token.value) {
            wv->remove_NavigationStarting(impl_->nav_starting_token);
        }
        if (impl_->nav_completed_token.value) {
            wv->remove_NavigationCompleted(impl_->nav_completed_token);
        }
        if (impl_->source_changed_token.value) {
            wv->remove_SourceChanged(impl_->source_changed_token);
        }
        if (impl_->new_window_token.value) {
            wv->remove_NewWindowRequested(impl_->new_window_token);
        }
        if (impl_->external_uri_token.value) {
            ComPtr<ICoreWebView2_18> wv18;
            if (SUCCEEDED(wv.As(&wv18)) && wv18) {
                wv18->remove_LaunchingExternalUriScheme(impl_->external_uri_token);
            }
        }
    }
    if (ctrl) {
        ctrl->put_IsVisible(FALSE);
        ctrl->Close();
    }

    delete impl_;
    impl_ = nullptr;
    width_ = 0;
    height_ = 0;
}

bool WebView::IsValid() const {
    return impl_ != nullptr;
}

bool WebView::IsReady() const {
    return impl_ && [&]() {
        std::lock_guard<std::mutex> lock(impl_->mu);
        return impl_->ready;
    }();
}

void WebView::SetBounds(int x, int y, int width, int height) {
    if (!impl_ || width <= 0 || height <= 0) {
        return;
    }
    width_ = static_cast<uint32_t>(width);
    height_ = static_cast<uint32_t>(height);
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->bounds = {x, y, x + width, y + height};
    impl_->ApplyBoundsUnlocked();
}

void WebView::SetVisible(bool visible) {
    if (!impl_) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->visible = visible;
    impl_->ApplyVisibleUnlocked();
}

void WebView::NotifyParentMoved() {
    if (!impl_) {
        return;
    }
    ComPtr<ICoreWebView2Controller> ctrl;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        ctrl = impl_->controller;
    }
    if (ctrl) {
        ctrl->NotifyParentWindowPositionChanged();
    }
}

void WebView::LoadURL(const std::string& url) {
    if (!impl_ || url.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (impl_->webview) {
        impl_->webview->Navigate(Utf8ToWide(url).c_str());
        impl_->loading = true;
    } else {
        impl_->pending_url = url;
        impl_->pending_html.clear();
    }
}

void WebView::LoadHTML(const std::string& html) {
    if (!impl_ || html.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (impl_->webview) {
        impl_->webview->NavigateToString(Utf8ToWide(html).c_str());
        impl_->loading = true;
    } else {
        impl_->pending_html = html;
        impl_->pending_url.clear();
    }
}

bool WebView::CanGoBack() const {
    if (!impl_) {
        return false;
    }
    ComPtr<ICoreWebView2> wv;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        wv = impl_->webview;
    }
    if (!wv) {
        return false;
    }
    BOOL can = FALSE;
    wv->get_CanGoBack(&can);
    return can == TRUE;
}

void WebView::GoBack() {
    if (!impl_) {
        return;
    }
    ComPtr<ICoreWebView2> wv;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        wv = impl_->webview;
    }
    if (wv) {
        wv->GoBack();
    }
}

void WebView::Focus() {
    if (!impl_) {
        return;
    }
    ComPtr<ICoreWebView2Controller> ctrl;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        ctrl = impl_->controller;
    }
    if (ctrl) {
        ctrl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void WebView::Unfocus() {
    HWND parent = WebEngineGetParentHwnd();
    if (parent) {
        SetFocus(parent);
    }
}

void WebView::Reload() {
    if (!impl_) {
        return;
    }
    ComPtr<ICoreWebView2> wv;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        wv = impl_->webview;
    }
    if (wv) {
        wv->Reload();
    }
}

std::string WebView::CurrentUrl() const {
    if (!impl_) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->mu);
    return impl_->current_url;
}

bool WebView::IsLoading() const {
    if (!impl_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->mu);
    return impl_->loading;
}

}  // namespace myiui::web
