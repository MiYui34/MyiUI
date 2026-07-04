#include "ui/hud/layout_editor.h"

#include "config/user_settings.h"
#include "ui/clickgui/clickgui.h"
#include "ui/hud/now_playing_layout.h"
#include "ui/island/island_renderer.h"

#include "imgui.h"

#include <algorithm>

namespace myiui::ui::hud {

namespace {

enum class WidgetKind {
    NowPlaying,
    Island,
    Count
};

struct WidgetTarget {
    WidgetKind kind;
    ImVec2 min;
    ImVec2 max;
    float* x;
    float* y;
    float* scale;
};

struct DragSession {
    WidgetKind kind = WidgetKind::Count;
    bool resizing = false;
    float startX = 0.f;
    float startY = 0.f;
    float startScale = 1.f;
    ImVec2 startMouse{};
};

DragSession g_drag{};

float ClampScale(float v) { return std::clamp(v, 0.35f, 4.f); }

bool PointInRect(ImVec2 p, ImVec2 min, ImVec2 max) {
    return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
}

void DrawHandle(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const float r = 6.f;
    dl->AddCircleFilled(c, r, col);
    dl->AddCircle(c, r, IM_COL32(255, 255, 255, 220), 0, 1.5f);
}

void DrawWidgetFrame(ImDrawList* dl, ImVec2 min, ImVec2 max, const char* label, const int accent[4]) {
    const ImU32 frameCol = IM_COL32(accent[0], accent[1], accent[2], 200);
    dl->AddRect(min, max, frameCol, 6.f, 0, 2.f);
    dl->AddRectFilled(min, max, IM_COL32(accent[0], accent[1], accent[2], 22));
    DrawHandle(dl, ImVec2(max.x - 7.f, max.y - 7.f), IM_COL32(255, 255, 255, 240));
    ImFont* font = ImGui::GetFont();
    if (font && label) {
        dl->AddText(font, 13.f, ImVec2(min.x + 4.f, min.y - 16.f), IM_COL32(220, 224, 232, 200), label);
    }
}

bool TryBeginDrag(const ImVec2& mouse, WidgetKind kind, ImVec2 min, ImVec2 max, float* x, float* y, float* scale) {
    if (PointInRect(mouse, ImVec2(max.x - 14.f, max.y - 14.f), max)) {
        g_drag.kind = kind;
        g_drag.resizing = true;
        g_drag.startX = *x;
        g_drag.startY = *y;
        g_drag.startScale = *scale;
        g_drag.startMouse = mouse;
        return true;
    }
    if (PointInRect(mouse, min, max)) {
        g_drag.kind = kind;
        g_drag.resizing = false;
        g_drag.startX = *x;
        g_drag.startY = *y;
        g_drag.startScale = *scale;
        g_drag.startMouse = mouse;
        return true;
    }
    return false;
}

void ApplyDrag(float* x, float* y, float* scale) {
    ImGuiIO& io = ImGui::GetIO();
    if (g_drag.resizing) {
        const float dx = io.MousePos.x - g_drag.startMouse.x;
        *scale = ClampScale(g_drag.startScale + dx * 0.008f);
    } else {
        *x += io.MouseDelta.x;
        *y += io.MouseDelta.y;
    }
    myiui::config::UserSettingsRequestSave();
}

}  // namespace

bool LayoutEditorActive() {
    return myiui::ui::clickgui::IsOpen() && myiui::config::GetUserSettingsConst().layout_editor_enabled;
}

bool LayoutEditorConsumesInput() {
    return LayoutEditorActive() && g_drag.kind != WidgetKind::Count;
}

void LayoutEditorRender(const AppConfig& cfg, ShmReader& shm, float viewportW, float viewportH) {
    (void)shm;
    if (!LayoutEditorActive()) return;

    auto& settings = myiui::config::GetUserSettings();
    WidgetTarget targets[2];
    int targetCount = 0;

    auto pushTarget = [&](WidgetKind kind, const char* label, ImVec4 b, float* x, float* y, float* scale) {
        if (targetCount >= 2) return;
        targets[targetCount++] = {kind, ImVec2(b.x, b.y), ImVec2(b.z, b.w), x, y, scale};
        DrawWidgetFrame(ImGui::GetForegroundDrawList(), ImVec2(b.x, b.y), ImVec2(b.z, b.w), label, cfg.theme.accent);
    };

    {
        const ImVec4 b = CalcNowPlayingBounds(settings.now_playing, viewportW, viewportH);
        pushTarget(WidgetKind::NowPlaying, "NowPlaying", b, &settings.now_playing.x, &settings.now_playing.y,
                   &settings.now_playing.scale);
    }
    if (settings.island.visible) {
        const ImVec4 b = myiui::ui::island::CalcIslandIdleBounds(viewportW, viewportH);
        pushTarget(WidgetKind::Island, "灵动岛", b, &settings.island.offset_x, &settings.island.offset_y,
                   &settings.island.scale);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.MouseClicked[0]) {
        for (int i = targetCount - 1; i >= 0; --i) {
            const WidgetTarget& t = targets[i];
            if (TryBeginDrag(io.MousePos, t.kind, t.min, t.max, t.x, t.y, t.scale)) break;
        }
    }
    if (g_drag.kind != WidgetKind::Count && io.MouseDown[0]) {
        for (int i = 0; i < targetCount; ++i) {
            if (targets[i].kind == g_drag.kind) {
                ApplyDrag(targets[i].x, targets[i].y, targets[i].scale);
                break;
            }
        }
    } else if (!io.MouseDown[0]) {
        g_drag.kind = WidgetKind::Count;
        g_drag.resizing = false;
    }

    if (g_drag.kind != WidgetKind::Count) {
        io.WantCaptureMouse = true;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    if (font) {
        dl->AddText(font, 14.f, ImVec2(12.f, viewportH - 28.f), IM_COL32(220, 224, 232, 200),
                    "布局编辑器：拖动移动 · 右下角缩放");
    }
}

}  // namespace myiui::ui::hud
