#pragma once

#include <windows.h>
#include <gl/GL.h>

namespace myiui::render {

bool GlLoadExtensions();
bool GlExtensionsReady();

void GlGenFramebuffers(GLsizei n, GLuint* ids);
void GlDeleteFramebuffers(GLsizei n, const GLuint* ids);
void GlBindFramebuffer(GLenum target, GLuint id);
void GlFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void GlBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1,
                       GLint dstY1, GLbitfield mask, GLenum filter);
void GlGenerateMipmap(GLenum target);
GLuint GlCreateShader(GLenum type);
void GlShaderSource(GLuint shader, GLsizei count, const char* const* src, const GLint* length);
void GlCompileShader(GLuint shader);
void GlGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void GlGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
GLuint GlCreateProgram();
void GlAttachShader(GLuint program, GLuint shader);
void GlLinkProgram(GLuint program);
void GlGetProgramiv(GLuint program, GLenum pname, GLint* params);
void GlGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
void GlDeleteShader(GLuint shader);
void GlDeleteProgram(GLuint program);
void GlUseProgram(GLuint program);
GLint GlGetUniformLocation(GLuint program, const char* name);
void GlUniform1f(GLint loc, GLfloat v);
void GlUniform1i(GLint loc, GLint v);
void GlUniform2f(GLint loc, GLfloat x, GLfloat y);
void GlUniform3f(GLint loc, GLfloat x, GLfloat y, GLfloat z);
void GlActiveTexture(GLenum tex);
void GlBindAttribLocation(GLuint program, GLuint index, const char* name);
void GlBindBuffer(GLenum target, GLuint buffer);
void GlEnableVertexAttribArray(GLuint index);
void GlDisableVertexAttribArray(GLuint index);
void GlVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
                           const void* pointer);

} // namespace myiui::render
