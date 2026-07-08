#pragma once

#include <windows.h>
#include <gl/GL.h>

namespace myiui::render {

class GlStateGuard {
public:
    GlStateGuard();
    ~GlStateGuard();

    GlStateGuard(const GlStateGuard&) = delete;
    GlStateGuard& operator=(const GlStateGuard&) = delete;

private:
    GLboolean blend_ = GL_FALSE;
    GLboolean cullFace_ = GL_FALSE;
    GLboolean depthTest_ = GL_FALSE;
    GLboolean scissorTest_ = GL_FALSE;
    GLint texture2d_ = 0;
    GLint viewport_[4]{};
    GLint scissorBox_[4]{};
    GLint unpackAlignment_ = 4;
    GLint unpackRowLength_ = 0;
    GLint unpackSkipPixels_ = 0;
    GLint unpackSkipRows_ = 0;
    GLint blendSrc_ = GL_ONE;
    GLint blendDst_ = GL_ZERO;
};

}  // namespace myiui::render
