#include "render/gl_state_guard.h"

namespace myiui::render {

GlStateGuard::GlStateGuard() {
    blend_ = glIsEnabled(GL_BLEND);
    cullFace_ = glIsEnabled(GL_CULL_FACE);
    depthTest_ = glIsEnabled(GL_DEPTH_TEST);
    scissorTest_ = glIsEnabled(GL_SCISSOR_TEST);

    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d_);
    glGetIntegerv(GL_VIEWPORT, viewport_);
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox_);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment_);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength_);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpackSkipPixels_);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &unpackSkipRows_);
    glGetIntegerv(GL_BLEND_SRC, &blendSrc_);
    glGetIntegerv(GL_BLEND_DST, &blendDst_);
}

GlStateGuard::~GlStateGuard() {
    if (blend_) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);

    if (cullFace_) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);

    if (depthTest_) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (scissorTest_) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2d_));
    glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment_);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength_);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpackSkipPixels_);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, unpackSkipRows_);
    glBlendFunc(static_cast<GLenum>(blendSrc_), static_cast<GLenum>(blendDst_));

    while (glGetError() != GL_NO_ERROR) {}
}

}  // namespace myiui::render
