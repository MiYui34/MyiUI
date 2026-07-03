#include "ui/menu_icons.h"

#include <cmath>

#undef min
#undef max

#include <algorithm>

namespace {

template <typename PointFn>
void StrokeCircle(ImDrawList* dl, PointFn p, float cx, float cy, float r, ImU32 color, float stroke, int segments) {
    dl->PathClear();
    for (int i = 0; i <= segments; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(segments) * 6.2831853f;
        dl->PathLineTo(p(cx + std::cos(a) * r, cy + std::sin(a) * r));
    }
    dl->PathStroke(color, ImDrawFlags_Closed, stroke);
}

template <typename PointFn>
void StrokeArc(ImDrawList* dl, PointFn p, float cx, float cy, float r, float a0, float a1, ImU32 color, float stroke,
               int segments) {
    dl->PathClear();
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float a = a0 + (a1 - a0) * t;
        dl->PathLineTo(p(cx + std::cos(a) * r, cy + std::sin(a) * r));
    }
    dl->PathStroke(color, 0, stroke);
}

}  // namespace

void DrawMenuIcon(ImDrawList* dl, MenuIcon icon, const ImVec2& center, float size, ImU32 color, float strokeWidth) {
    const float scale = size / 64.f;
    const auto p = [&](float x, float y) { return ImVec2(center.x + (x - 32.f) * scale, center.y + (y - 32.f) * scale); };
    const float stroke = (std::max)(1.f, strokeWidth * scale * 2.f);

    switch (icon) {
        case MenuIcon::None:
            break;
        case MenuIcon::Singleplayer:
            StrokeCircle(dl, p, 32.f, 18.f, 6.f, color, stroke, 24);
            StrokeArc(dl, p, 32.f, 48.f, 14.f, 3.45f, 6.0f, color, stroke, 18);
            break;
        case MenuIcon::Multiplayer:
            StrokeCircle(dl, p, 20.f, 20.f, 5.5f, color, stroke, 20);
            StrokeArc(dl, p, 20.f, 44.f, 10.f, 3.55f, 6.15f, color, stroke, 16);
            StrokeCircle(dl, p, 44.f, 20.f, 5.5f, color, stroke, 20);
            StrokeArc(dl, p, 44.f, 44.f, 10.f, 3.55f, 6.15f, color, stroke, 16);
            dl->AddLine(p(26.f, 14.f), p(38.f, 14.f), color, stroke);
            break;
        case MenuIcon::Options:
            dl->AddLine(p(14.f, 18.f), p(50.f, 18.f), color, stroke);
            StrokeCircle(dl, p, 40.f, 18.f, 5.f, color, stroke, 18);
            dl->AddLine(p(14.f, 32.f), p(50.f, 32.f), color, stroke);
            StrokeCircle(dl, p, 24.f, 32.f, 5.f, color, stroke, 18);
            dl->AddLine(p(14.f, 46.f), p(50.f, 46.f), color, stroke);
            StrokeCircle(dl, p, 36.f, 46.f, 5.f, color, stroke, 18);
            break;
        case MenuIcon::Quit:
            dl->AddRect(p(14.f, 24.f), p(50.f, 48.f), color, 12.f * scale, 0, stroke);
            StrokeCircle(dl, p, 26.f, 36.f, 7.f, color, stroke, 20);
            break;
        case MenuIcon::Manager:
            dl->AddRect(p(12.f, 14.f), p(52.f, 50.f), color, 5.f * scale, 0, stroke);
            dl->AddLine(p(12.f, 24.f), p(52.f, 24.f), color, stroke);
            StrokeCircle(dl, p, 19.f, 19.f, 1.5f, color, stroke, 10);
            StrokeCircle(dl, p, 25.f, 19.f, 1.5f, color, stroke, 10);
            StrokeCircle(dl, p, 31.f, 19.f, 1.5f, color, stroke, 10);
            dl->AddLine(p(18.f, 34.f), p(46.f, 34.f), color, stroke);
            StrokeCircle(dl, p, 36.f, 34.f, 3.f, color, stroke, 16);
            dl->AddLine(p(18.f, 42.f), p(46.f, 42.f), color, stroke);
            StrokeCircle(dl, p, 28.f, 42.f, 3.f, color, stroke, 16);
            break;
    }
}
