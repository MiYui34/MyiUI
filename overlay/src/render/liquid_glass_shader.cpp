#include "render/liquid_glass_shader.h"

#include "overlay_runtime.h"
#include "render/gl_loader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include <gl/GL.h>

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE0 0x84C0
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

namespace myiui::render {
namespace {

constexpr GLuint kAttrPosition = 0;

const char* kVert150 = R"(#version 150
in vec2 aPosition;
uniform vec2 uQuadPos;
uniform vec2 uQuadSize;
uniform vec2 uScreenSize;
out vec2 vTexCoord;
out vec2 vMidPointNDC;
out vec2 vLocalOffsetNDC;

vec2 ToNdc(vec2 pixel) {
    return vec2(pixel.x / uScreenSize.x * 2.0 - 1.0, 1.0 - pixel.y / uScreenSize.y * 2.0);
}

void main() {
    vTexCoord = aPosition;
    vec2 pixelPos = uQuadPos + aPosition * uQuadSize;
    vec2 center = uQuadPos + uQuadSize * 0.5;
    vec2 ndc = ToNdc(pixelPos);
    vMidPointNDC = ToNdc(center);
    vLocalOffsetNDC = ndc - vMidPointNDC;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

// Ported from Jacquesqwq/LiquidGlassShader liquidGlassV2Tinted.frag (MIT)
const char* kFragTinted150 = R"(#version 150
uniform sampler2D uBlurTex;
uniform float uPowerFactor;
uniform float uNoise;
uniform float uRefractionPower;
uniform vec3 uTintColor;
uniform float uTintStrength;
uniform float uChromaStrength;
uniform float uDarkness;
uniform float uGlobalAlpha;
in vec2 vTexCoord;
in vec2 vMidPointNDC;
in vec2 vLocalOffsetNDC;
out vec4 fragColor;

const float M_E = 2.718281828459045;
const vec2 CENTER = vec2(0.5);

float fcurve(float x) {
    return 1.0 - 2.3 * pow(5.2 * M_E, -6.9 * x - 0.7);
}

float sdSuperellipse(vec2 p, float n, float r) {
    vec2 absP = abs(p);
    float numerator = pow(absP.x, n) + pow(absP.y, n) - pow(r, n);
    float denominator = n * sqrt(pow(absP.x, 2.0 * n - 2.0) + pow(absP.y, 2.0 * n - 2.0)) + 0.00001;
    return numerator / denominator;
}

bool OutOfBounds(vec2 uv) { return max(uv.x, uv.y) > 1.0 || min(uv.x, uv.y) < 0.0; }

void main() {
    vec2 localUV = vTexCoord;
    vec2 p = (localUV - CENTER) * 2.0;
    float d = sdSuperellipse(p, uPowerFactor, 1.0);
    float edge = 1.0 - smoothstep(-0.003, 0.003, d);
    if (edge <= 0.0) discard;

    float dist = clamp(max(-d, 0.0), 0.0, 1.0);
    float fresnel = pow(1.0 - dist, 3.0);
    float refraction = pow(fcurve(dist), uRefractionPower);
    vec2 sampleUV = (vMidPointNDC + vLocalOffsetNDC * refraction) * 0.5 + vec2(0.5);
    if (OutOfBounds(sampleUV)) discard;

    vec2 chromaDir = normalize(vLocalOffsetNDC + 0.00001);
    vec2 chromaOffset = chromaDir * fresnel * uChromaStrength;
    vec4 color = vec4(
        texture(uBlurTex, sampleUV + chromaOffset).r,
        texture(uBlurTex, sampleUV).g,
        texture(uBlurTex, sampleUV - chromaOffset).b,
        1.0);

    float micro = sin(gl_FragCoord.x * 0.015 + gl_FragCoord.y * 0.008) * sin(gl_FragCoord.y * 0.012 - gl_FragCoord.x * 0.006);
    color.rgb += micro * uNoise * 0.015;
    float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float grain = (micro - 0.5) * uNoise * 0.22;
    color.rgb += grain;
    vec3 grayscale = vec3(luma);
    float saturation = mix(0.45, 0.75, luma);
    color.rgb = mix(grayscale, color.rgb, saturation);
    vec3 adaptiveTintColor = mix(uTintColor, vec3(0.08, 0.09, 0.11), uDarkness);
    float adaptiveTint = uTintStrength * (1.0 - luma * 0.5);
    color.rgb = mix(color.rgb, adaptiveTintColor, adaptiveTint);
    float adaptiveFresnel = mix(0.12, 0.06, luma);
    color.rgb += uTintColor * fresnel * adaptiveFresnel;
    color.rgb *= 0.96 + smoothstep(0.0, 1.0, dist) * 0.04;
    color.rgb *= mix(1.08, 0.98, luma) * mix(1.0, 0.82, uDarkness);
    float opticalEdge = pow(edge, 1.35);
    color.rgb *= opticalEdge;
    fragColor = vec4(color.rgb, opticalEdge * uGlobalAlpha);
}
)";

struct GlassState {
    GLuint blurFbo = 0;
    GLuint blurTex = 0;
    int blurW = 0;
    int blurH = 0;
    GLuint program = 0;
    GLint uQuadPos = -1;
    GLint uQuadSize = -1;
    GLint uScreenSize = -1;
    GLint uBlurTex = -1;
    GLint uPowerFactor = -1;
    GLint uNoise = -1;
    GLint uRefractionPower = -1;
    GLint uTintColor = -1;
    GLint uTintStrength = -1;
    GLint uChromaStrength = -1;
    GLint uDarkness = -1;
    GLint uGlobalAlpha = -1;
    bool initialized = false;
    bool loggedFail = false;
};

GlassState g_glass{};

GLuint CompileShader(GLenum type, const char* src) {
    const GLuint shader = GlCreateShader(type);
    GlShaderSource(shader, 1, &src, nullptr);
    GlCompileShader(shader);
    GLint ok = 0;
    GlGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]{};
        GlGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        myiui::overlay::OverlayLog(L"LiquidGlass shader compile failed.");
        GlDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool LinkProgram(GLuint program, GLuint vert, GLuint frag) {
    GlAttachShader(program, vert);
    GlAttachShader(program, frag);
    GlBindAttribLocation(program, kAttrPosition, "aPosition");
    GlLinkProgram(program);
    GLint ok = 0;
    GlGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]{};
        GlGetProgramInfoLog(program, sizeof(log), nullptr, log);
        myiui::overlay::OverlayLog(L"LiquidGlass program link failed.");
        return false;
    }
    return true;
}

bool EnsureProgram() {
    if (g_glass.program != 0) {
        return true;
    }
    if (!GlLoadExtensions()) {
        return false;
    }
    const GLuint vert = CompileShader(GL_VERTEX_SHADER, kVert150);
    const GLuint frag = CompileShader(GL_FRAGMENT_SHADER, kFragTinted150);
    if (!vert || !frag) {
        if (vert) GlDeleteShader(vert);
        if (frag) GlDeleteShader(frag);
        return false;
    }
    const GLuint prog = GlCreateProgram();
    if (!prog || !LinkProgram(prog, vert, frag)) {
        GlDeleteShader(vert);
        GlDeleteShader(frag);
        if (prog) GlDeleteProgram(prog);
        return false;
    }
    GlDeleteShader(vert);
    GlDeleteShader(frag);
    g_glass.program = prog;
    g_glass.uQuadPos = GlGetUniformLocation(prog, "uQuadPos");
    g_glass.uQuadSize = GlGetUniformLocation(prog, "uQuadSize");
    g_glass.uScreenSize = GlGetUniformLocation(prog, "uScreenSize");
    g_glass.uBlurTex = GlGetUniformLocation(prog, "uBlurTex");
    g_glass.uPowerFactor = GlGetUniformLocation(prog, "uPowerFactor");
    g_glass.uNoise = GlGetUniformLocation(prog, "uNoise");
    g_glass.uRefractionPower = GlGetUniformLocation(prog, "uRefractionPower");
    g_glass.uTintColor = GlGetUniformLocation(prog, "uTintColor");
    g_glass.uTintStrength = GlGetUniformLocation(prog, "uTintStrength");
    g_glass.uChromaStrength = GlGetUniformLocation(prog, "uChromaStrength");
    g_glass.uDarkness = GlGetUniformLocation(prog, "uDarkness");
    g_glass.uGlobalAlpha = GlGetUniformLocation(prog, "uGlobalAlpha");
    g_glass.initialized = true;
    myiui::overlay::OverlayLog(L"LiquidGlassShader V2 tinted ready.");
    return true;
}

void EnsureBlurTarget(int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (g_glass.blurTex != 0 && g_glass.blurW == w && g_glass.blurH == h) {
        return;
    }
    if (g_glass.blurFbo != 0) {
        GlDeleteFramebuffers(1, &g_glass.blurFbo);
        g_glass.blurFbo = 0;
    }
    if (g_glass.blurTex != 0) {
        glDeleteTextures(1, &g_glass.blurTex);
        g_glass.blurTex = 0;
    }
    glGenTextures(1, &g_glass.blurTex);
    glBindTexture(GL_TEXTURE_2D, g_glass.blurTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GlGenFramebuffers(1, &g_glass.blurFbo);
    GlBindFramebuffer(GL_FRAMEBUFFER, g_glass.blurFbo);
    GlFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_glass.blurTex, 0);
    GlBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_glass.blurW = w;
    g_glass.blurH = h;
}

float SuperellipsePower(float w, float h, float cornerRadius) {
    const float minDim = (w < h) ? w : h;
    const float maxDim = (w > h) ? w : h;
    const float aspect = maxDim / (minDim < 1.f ? 1.f : minDim);
    const float roundness = std::clamp(cornerRadius / (minDim < 8.f ? 8.f : minDim * 0.5f), 0.2f, 1.f);
    const float power = 2.2f + aspect * 1.4f + (1.f - roundness) * 2.5f;
    return power < 2.5f ? 2.5f : (power > 8.f ? 8.f : power);
}

} // namespace

bool LiquidGlassReady() {
    return g_glass.initialized && g_glass.program != 0 && g_glass.blurTex != 0;
}

bool LiquidGlassBeginFrame(int screenW, int screenH) {
    if (!EnsureProgram()) {
        if (!g_glass.loggedFail) {
            g_glass.loggedFail = true;
            myiui::overlay::OverlayLog(L"LiquidGlass init failed; using ImGui fallback.");
        }
        return false;
    }

    GLint prevRead = 0;
    GLint prevDraw = 0;
    glGetIntegerv(0x8CA8, &prevRead); // GL_READ_FRAMEBUFFER_BINDING
    glGetIntegerv(0x8CA9, &prevDraw); // GL_DRAW_FRAMEBUFFER_BINDING

    EnsureBlurTarget(screenW, screenH);
    if (g_glass.blurFbo == 0) {
        return false;
    }

    GlBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    GlBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_glass.blurFbo);
    GlBlitFramebuffer(0, 0, screenW, screenH, 0, 0, screenW, screenH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GlBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevRead));
    GlBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDraw));

    glBindTexture(GL_TEXTURE_2D, g_glass.blurTex);
    GlGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void LiquidGlassDrawPanel(const ImVec2& min, const ImVec2& max, const ThemeConfig& theme, float alphaMul,
                          bool selectedBoost, float cornerRadius) {
    if (!LiquidGlassReady() || g_glass.blurTex == 0) {
        return;
    }

    const float w = max.x - min.x;
    const float h = max.y - min.y;
    if (w <= 1.f || h <= 1.f) {
        return;
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float screenW = static_cast<float>(viewport[2]);
    const float screenH = static_cast<float>(viewport[3]);

    GLboolean blend = glIsEnabled(GL_BLEND);
    GLint blendSrc = 0;
    GLint blendDst = 0;
    glGetIntegerv(GL_BLEND_SRC, &blendSrc);
    glGetIntegerv(GL_BLEND_DST, &blendDst);
    GLint prevProgram = 0;
    glGetIntegerv(0x8B8D, &prevProgram); // GL_CURRENT_PROGRAM
    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    GLint prevActive = 0;
    glGetIntegerv(0x84E0, &prevActive); // GL_ACTIVE_TEXTURE
    GLint prevArrayBuffer = 0;
    glGetIntegerv(0x8892, &prevArrayBuffer); // GL_ARRAY_BUFFER_BINDING
    GLboolean prevAttribEnabled = glIsEnabled(kAttrPosition);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GlUseProgram(g_glass.program);
    GlUniform2f(g_glass.uQuadPos, min.x, min.y);
    GlUniform2f(g_glass.uQuadSize, w, h);
    GlUniform2f(g_glass.uScreenSize, screenW, screenH);
    GlActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_glass.blurTex);
    GlUniform1i(g_glass.uBlurTex, 0);

    const float power = SuperellipsePower(w, h, cornerRadius);
    GlUniform1f(g_glass.uPowerFactor, power);
    GlUniform1f(g_glass.uNoise, selectedBoost ? 0.42f : 0.28f);
    GlUniform1f(g_glass.uRefractionPower, selectedBoost ? 1.65f : 1.45f);
    GlUniform3f(g_glass.uTintColor, theme.accent[0] / 255.f, theme.accent[1] / 255.f, theme.accent[2] / 255.f);
    GlUniform1f(g_glass.uTintStrength, selectedBoost ? 0.32f : 0.24f);
    GlUniform1f(g_glass.uChromaStrength, selectedBoost ? 0.028f : 0.018f);
    GlUniform1f(g_glass.uDarkness, 0.12f);
    GlUniform1f(g_glass.uGlobalAlpha, std::clamp(alphaMul, 0.f, 1.f));

    const std::array<float, 8> quadVerts{0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
    GlBindBuffer(0x8892, 0); // GL_ARRAY_BUFFER — unbind so attrib pointer sources from client memory
    GlEnableVertexAttribArray(kAttrPosition);
    GlVertexAttribPointer(kAttrPosition, 2, GL_FLOAT, GL_FALSE, 0, quadVerts.data());
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    if (!prevAttribEnabled) {
        GlDisableVertexAttribArray(kAttrPosition);
    }
    GlBindBuffer(0x8892, static_cast<GLuint>(prevArrayBuffer));

    GlUseProgram(static_cast<GLuint>(prevProgram));
    GlActiveTexture(static_cast<GLenum>(prevActive));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex));
    if (!blend) {
        glDisable(GL_BLEND);
    }
    glBlendFunc(blendSrc, blendDst);
}

void LiquidGlassShutdown() {
    if (g_glass.blurFbo != 0) {
        GlDeleteFramebuffers(1, &g_glass.blurFbo);
        g_glass.blurFbo = 0;
    }
    if (g_glass.blurTex != 0) {
        glDeleteTextures(1, &g_glass.blurTex);
        g_glass.blurTex = 0;
    }
    if (g_glass.program != 0) {
        GlDeleteProgram(g_glass.program);
        g_glass.program = 0;
    }
    g_glass.initialized = false;
}

} // namespace myiui::render
