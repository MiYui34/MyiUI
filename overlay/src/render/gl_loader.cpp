#include "render/gl_loader.h"

namespace myiui::render {

namespace {

using PFNGLGENFRAMEBUFFERSPROC = void(APIENTRY*)(GLsizei, GLuint*);
using PFNGLDELETEFRAMEBUFFERSPROC = void(APIENTRY*)(GLsizei, const GLuint*);
using PFNGLBINDFRAMEBUFFERPROC = void(APIENTRY*)(GLenum, GLuint);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using PFNGLBLITFRAMEBUFFERPROC = void(APIENTRY*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint,
                                                GLbitfield, GLenum);
using PFNGLGENERATEMIPMAPPROC = void(APIENTRY*)(GLenum);
using PFNGLCREATESHADERPROC = GLuint(APIENTRY*)(GLenum);
using PFNGLSHADERSOURCEPROC = void(APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
using PFNGLCOMPILESHADERPROC = void(APIENTRY*)(GLuint);
using PFNGLGETSHADERIVPROC = void(APIENTRY*)(GLuint, GLenum, GLint*);
using PFNGLGETSHADERINFOLOGPROC = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using PFNGLCREATEPROGRAMPROC = GLuint(APIENTRY*)();
using PFNGLATTACHSHADERPROC = void(APIENTRY*)(GLuint, GLuint);
using PFNGLLINKPROGRAMPROC = void(APIENTRY*)(GLuint);
using PFNGLGETPROGRAMIVPROC = void(APIENTRY*)(GLuint, GLenum, GLint*);
using PFNGLGETPROGRAMINFOLOGPROC = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using PFNGLDELETESHADERPROC = void(APIENTRY*)(GLuint);
using PFNGLDELETEPROGRAMPROC = void(APIENTRY*)(GLuint);
using PFNGLUSEPROGRAMPROC = void(APIENTRY*)(GLuint);
using PFNGLGETUNIFORMLOCATIONPROC = GLint(APIENTRY*)(GLuint, const char*);
using PFNGLUNIFORM1FPROC = void(APIENTRY*)(GLint, GLfloat);
using PFNGLUNIFORM1IPROC = void(APIENTRY*)(GLint, GLint);
using PFNGLUNIFORM2FPROC = void(APIENTRY*)(GLint, GLfloat, GLfloat);
using PFNGLUNIFORM3FPROC = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
using PFNGLUNIFORM4FPROC = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using PFNGLACTIVETEXTUREPROC = void(APIENTRY*)(GLenum);
using PFNGLBINDATTRIBLOCATIONPROC = void(APIENTRY*)(GLuint, GLuint, const char*);
using PFNGLBINDBUFFERPROC = void(APIENTRY*)(GLenum, GLuint);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(APIENTRY*)(GLuint);
using PFNGLDISABLEVERTEXATTRIBARRAYPROC = void(APIENTRY*)(GLuint);
using PFNGLVERTEXATTRIBPOINTERPROC = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

PFNGLGENFRAMEBUFFERSPROC pGlGenFramebuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC pGlDeleteFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC pGlBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC pGlFramebufferTexture2D = nullptr;
PFNGLBLITFRAMEBUFFERPROC pGlBlitFramebuffer = nullptr;
PFNGLGENERATEMIPMAPPROC pGlGenerateMipmap = nullptr;
PFNGLCREATESHADERPROC pGlCreateShader = nullptr;
PFNGLSHADERSOURCEPROC pGlShaderSource = nullptr;
PFNGLCOMPILESHADERPROC pGlCompileShader = nullptr;
PFNGLGETSHADERIVPROC pGlGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC pGlGetShaderInfoLog = nullptr;
PFNGLCREATEPROGRAMPROC pGlCreateProgram = nullptr;
PFNGLATTACHSHADERPROC pGlAttachShader = nullptr;
PFNGLLINKPROGRAMPROC pGlLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC pGlGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC pGlGetProgramInfoLog = nullptr;
PFNGLDELETESHADERPROC pGlDeleteShader = nullptr;
PFNGLDELETEPROGRAMPROC pGlDeleteProgram = nullptr;
PFNGLUSEPROGRAMPROC pGlUseProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC pGlGetUniformLocation = nullptr;
PFNGLUNIFORM1FPROC pGlUniform1f = nullptr;
PFNGLUNIFORM1IPROC pGlUniform1i = nullptr;
PFNGLUNIFORM2FPROC pGlUniform2f = nullptr;
PFNGLUNIFORM3FPROC pGlUniform3f = nullptr;
PFNGLUNIFORM4FPROC pGlUniform4f = nullptr;
PFNGLACTIVETEXTUREPROC pGlActiveTexture = nullptr;
PFNGLBINDATTRIBLOCATIONPROC pGlBindAttribLocation = nullptr;
PFNGLBINDBUFFERPROC pGlBindBuffer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC pGlEnableVertexAttribArray = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC pGlDisableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC pGlVertexAttribPointer = nullptr;

bool g_ready = false;

template <typename T>
T LoadFn(const char* name) {
    return reinterpret_cast<T>(wglGetProcAddress(name));
}

} // namespace

bool GlLoadExtensions() {
    if (g_ready) {
        return true;
    }
    pGlGenFramebuffers = LoadFn<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    pGlDeleteFramebuffers = LoadFn<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
    pGlBindFramebuffer = LoadFn<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    pGlFramebufferTexture2D = LoadFn<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    pGlBlitFramebuffer = LoadFn<PFNGLBLITFRAMEBUFFERPROC>("glBlitFramebuffer");
    pGlGenerateMipmap = LoadFn<PFNGLGENERATEMIPMAPPROC>("glGenerateMipmap");
    pGlCreateShader = LoadFn<PFNGLCREATESHADERPROC>("glCreateShader");
    pGlShaderSource = LoadFn<PFNGLSHADERSOURCEPROC>("glShaderSource");
    pGlCompileShader = LoadFn<PFNGLCOMPILESHADERPROC>("glCompileShader");
    pGlGetShaderiv = LoadFn<PFNGLGETSHADERIVPROC>("glGetShaderiv");
    pGlGetShaderInfoLog = LoadFn<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
    pGlCreateProgram = LoadFn<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
    pGlAttachShader = LoadFn<PFNGLATTACHSHADERPROC>("glAttachShader");
    pGlLinkProgram = LoadFn<PFNGLLINKPROGRAMPROC>("glLinkProgram");
    pGlGetProgramiv = LoadFn<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
    pGlGetProgramInfoLog = LoadFn<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
    pGlDeleteShader = LoadFn<PFNGLDELETESHADERPROC>("glDeleteShader");
    pGlDeleteProgram = LoadFn<PFNGLDELETEPROGRAMPROC>("glDeleteProgram");
    pGlUseProgram = LoadFn<PFNGLUSEPROGRAMPROC>("glUseProgram");
    pGlGetUniformLocation = LoadFn<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    pGlUniform1f = LoadFn<PFNGLUNIFORM1FPROC>("glUniform1f");
    pGlUniform1i = LoadFn<PFNGLUNIFORM1IPROC>("glUniform1i");
    pGlUniform2f = LoadFn<PFNGLUNIFORM2FPROC>("glUniform2f");
    pGlUniform3f = LoadFn<PFNGLUNIFORM3FPROC>("glUniform3f");
    pGlUniform4f = LoadFn<PFNGLUNIFORM4FPROC>("glUniform4f");
    pGlActiveTexture = LoadFn<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    pGlBindAttribLocation = LoadFn<PFNGLBINDATTRIBLOCATIONPROC>("glBindAttribLocation");
    pGlBindBuffer = LoadFn<PFNGLBINDBUFFERPROC>("glBindBuffer");
    pGlEnableVertexAttribArray = LoadFn<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray");
    pGlDisableVertexAttribArray = LoadFn<PFNGLDISABLEVERTEXATTRIBARRAYPROC>("glDisableVertexAttribArray");
    pGlVertexAttribPointer = LoadFn<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer");

    g_ready = pGlGenFramebuffers && pGlBindFramebuffer && pGlFramebufferTexture2D && pGlBlitFramebuffer &&
              pGlGenerateMipmap && pGlCreateShader && pGlCreateProgram && pGlUseProgram && pGlGetUniformLocation;
    return g_ready;
}

bool GlExtensionsReady() {
    return g_ready;
}

void GlGenFramebuffers(GLsizei n, GLuint* ids) {
    if (pGlGenFramebuffers) pGlGenFramebuffers(n, ids);
}
void GlDeleteFramebuffers(GLsizei n, const GLuint* ids) {
    if (pGlDeleteFramebuffers) pGlDeleteFramebuffers(n, ids);
}
void GlBindFramebuffer(GLenum target, GLuint id) {
    if (pGlBindFramebuffer) pGlBindFramebuffer(target, id);
}
void GlFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    if (pGlFramebufferTexture2D) pGlFramebufferTexture2D(target, attachment, textarget, texture, level);
}
void GlBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1,
                       GLint dstY1, GLbitfield mask, GLenum filter) {
    if (pGlBlitFramebuffer) pGlBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
void GlGenerateMipmap(GLenum target) {
    if (pGlGenerateMipmap) pGlGenerateMipmap(target);
}
GLuint GlCreateShader(GLenum type) {
    return pGlCreateShader ? pGlCreateShader(type) : 0;
}
void GlShaderSource(GLuint shader, GLsizei count, const char* const* src, const GLint* length) {
    if (pGlShaderSource) pGlShaderSource(shader, count, src, length);
}
void GlCompileShader(GLuint shader) {
    if (pGlCompileShader) pGlCompileShader(shader);
}
void GlGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    if (pGlGetShaderiv) pGlGetShaderiv(shader, pname, params);
}
void GlGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog) {
    if (pGlGetShaderInfoLog) pGlGetShaderInfoLog(shader, bufSize, length, infoLog);
}
GLuint GlCreateProgram() {
    return pGlCreateProgram ? pGlCreateProgram() : 0;
}
void GlAttachShader(GLuint program, GLuint shader) {
    if (pGlAttachShader) pGlAttachShader(program, shader);
}
void GlLinkProgram(GLuint program) {
    if (pGlLinkProgram) pGlLinkProgram(program);
}
void GlGetProgramiv(GLuint program, GLenum pname, GLint* params) {
    if (pGlGetProgramiv) pGlGetProgramiv(program, pname, params);
}
void GlGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog) {
    if (pGlGetProgramInfoLog) pGlGetProgramInfoLog(program, bufSize, length, infoLog);
}
void GlDeleteShader(GLuint shader) {
    if (pGlDeleteShader) pGlDeleteShader(shader);
}
void GlDeleteProgram(GLuint program) {
    if (pGlDeleteProgram) pGlDeleteProgram(program);
}
void GlUseProgram(GLuint program) {
    if (pGlUseProgram) pGlUseProgram(program);
}
GLint GlGetUniformLocation(GLuint program, const char* name) {
    return pGlGetUniformLocation ? pGlGetUniformLocation(program, name) : -1;
}
void GlUniform1f(GLint loc, GLfloat v) {
    if (pGlUniform1f) pGlUniform1f(loc, v);
}
void GlUniform1i(GLint loc, GLint v) {
    if (pGlUniform1i) pGlUniform1i(loc, v);
}
void GlUniform2f(GLint loc, GLfloat x, GLfloat y) {
    if (pGlUniform2f) pGlUniform2f(loc, x, y);
}
void GlUniform3f(GLint loc, GLfloat x, GLfloat y, GLfloat z) {
    if (pGlUniform3f) pGlUniform3f(loc, x, y, z);
}
void GlUniform4f(GLint loc, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (pGlUniform4f) pGlUniform4f(loc, x, y, z, w);
}
void GlActiveTexture(GLenum tex) {
    if (pGlActiveTexture) pGlActiveTexture(tex);
}
void GlBindAttribLocation(GLuint program, GLuint index, const char* name) {
    if (pGlBindAttribLocation) pGlBindAttribLocation(program, index, name);
}
void GlBindBuffer(GLenum target, GLuint buffer) {
    if (pGlBindBuffer) pGlBindBuffer(target, buffer);
}
void GlEnableVertexAttribArray(GLuint index) {
    if (pGlEnableVertexAttribArray) pGlEnableVertexAttribArray(index);
}
void GlDisableVertexAttribArray(GLuint index) {
    if (pGlDisableVertexAttribArray) pGlDisableVertexAttribArray(index);
}
void GlVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
                           const void* pointer) {
    if (pGlVertexAttribPointer) pGlVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

} // namespace myiui::render
