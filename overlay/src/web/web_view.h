#pragma once

#include <cstdint>
#include <string>

namespace myiui::web {

// Single WebView2 controller hosted as a child of the game HWND.
class WebView {
public:
    WebView();
    ~WebView();

    WebView(const WebView&) = delete;
    WebView& operator=(const WebView&) = delete;

    // Starts async controller creation (requires WebEngine environment).
    bool Create(uint32_t width, uint32_t height);
    void Destroy();
    bool IsValid() const;
    bool IsReady() const;

    // Bounds are in parent-client coordinates (left, top, width, height).
    void SetBounds(int x, int y, int width, int height);
    void SetVisible(bool visible);
    void NotifyParentMoved();

    void LoadURL(const std::string& url);
    void LoadHTML(const std::string& html);

    bool CanGoBack() const;
    void GoBack();
    void Focus();
    void Unfocus();
    void Reload();
    std::string CurrentUrl() const;
    bool IsLoading() const;

    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

}  // namespace myiui::web
