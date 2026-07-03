#pragma once

#include "config/config_loader.h"

#include "imgui.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

struct MediaLibraryLimits {
    int64_t max_image_bytes = 15LL * 1024 * 1024;
    int64_t max_video_bytes = 300LL * 1024 * 1024;
};

struct MediaItem {
    std::wstring name;
    std::wstring path;
    bool is_video = false;
    int64_t file_size = 0;
};

struct MediaLibraryState {
    std::vector<MediaItem> items;
    std::wstring selected_path;
    std::string toast_message;
    float toast_timer = 0.f;
    bool needs_refresh = true;
    bool picker_busy = false;
    bool add_requested = false;
    bool auto_apply_on_load = false;
    std::wstring context_target_path;
};

bool MediaLibraryDeleteItem(const std::wstring& path, MediaLibraryState& state);

void MediaLibrarySetLimits(const MediaLibraryLimits& limits);
void MediaLibrarySetWindowHandle(HWND hwnd);
void MediaLibraryRefresh(MediaLibraryState& state);
void MediaLibraryApplyBackground(const std::wstring& path, MediaLibraryState& state);
void MediaLibrarySelect(const std::wstring& path, MediaLibraryState& state);
void MediaLibraryOpenAddPickerAsync(MediaLibraryState& state);
void MediaLibraryTick(MediaLibraryState& state);
void MediaLibraryRenderGrid(const AppConfig& cfg, MediaLibraryState& state, float uiScale, float gridHeight);
void MediaLibraryProcessPendingTextures();
void MediaLibraryRenderPanel(const AppConfig& cfg, MediaLibraryState& state, float uiScale, bool* open);

ImTextureID MediaLibraryGetThumb(const std::wstring& path, bool is_video, int& outW, int& outH);
