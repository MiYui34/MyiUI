#include "render/frame_runtime.h"

namespace myiui::render {

bool HasCurrentGlContext() {
    return wglGetCurrentContext() != nullptr;
}

FrameViewport ReadViewport() {
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    return FrameViewport{viewport[0], viewport[1], viewport[2], viewport[3]};
}

void ClearGlErrors() {
    while (glGetError() != GL_NO_ERROR) {}
}

}  // namespace myiui::render
