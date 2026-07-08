#pragma once

#include <windows.h>
#include <gl/GL.h>

namespace myiui::render {

struct FrameViewport {
    GLint x = 0;
    GLint y = 0;
    GLint width = 0;
    GLint height = 0;

    bool valid() const { return width > 0 && height > 0; }
};

bool HasCurrentGlContext();
FrameViewport ReadViewport();
void ClearGlErrors();

}  // namespace myiui::render
