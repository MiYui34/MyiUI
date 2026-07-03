#include "ui/intro/intro_particles.h"

#include "ui/intro/intro_easing.h"
#include "ui/intro/intro_tokens.h"

#include <algorithm>
#include <cmath>

namespace myiui::intro {

namespace {

float Rand01(unsigned& s) {
    s = s * 1664525u + 1013904223u;
    return static_cast<float>((s >> 8) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

float RandRange(unsigned& s, float lo, float hi) {
    return lo + (hi - lo) * Rand01(s);
}

void DrawGlow(ImDrawList* dl, float x, float y, float r, ImU32 col) {
    const int steps = 4;
    for (int i = steps; i >= 1; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float radius = r * (1.f + t * 3.f);
        const int a = static_cast<int>((col >> IM_COL32_A_SHIFT) & 0xFF);
        const int na = static_cast<int>(a * (0.12f * t));
        const ImU32 c = (col & 0x00FFFFFFu) | (static_cast<ImU32>(na) << IM_COL32_A_SHIFT);
        dl->AddCircleFilled(ImVec2(x, y), radius, c, 16);
    }
    dl->AddCircleFilled(ImVec2(x, y), r, col, 12);
}

}  // namespace

void IntroParticleEngine::Init(int w, int h) {
    width = (std::max)(w, 1);
    height = (std::max)(h, 1);
    seed = static_cast<unsigned>(width * 73856093 ^ height * 19349663);
    particle_count = 0;

    const int burstN = (std::min)(48, width / 18);
    const int ambientN = (std::min)(35, width / 28);
    const int bokehN = (std::min)(18, width / 50);
    const int total = burstN + ambientN + bokehN;
    particle_count = (std::min)(total, kMaxParticles);

    const float cx = width * 0.5f;
    const float cy = height * 0.5f;
    int idx = 0;
    for (int i = 0; i < burstN && idx < particle_count; ++i, ++idx) {
        auto& p = particles[idx];
        p.type = 0;
        const float angle = (6.2831853f * static_cast<float>(i) / static_cast<float>(burstN)) + RandRange(seed, -0.15f, 0.15f);
        const float speed = RandRange(seed, 0.3f, 1.7f);
        p.x = cx;
        p.y = cy;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.radius = RandRange(seed, 0.3f, 1.7f);
        p.max_life = RandRange(seed, 0.5f, 1.f);
        p.life = p.max_life;
        p.accent = Rand01(seed) > 0.8f;
    }
    for (int i = 0; i < ambientN && idx < particle_count; ++i, ++idx) {
        auto& p = particles[idx];
        p.type = 1;
        p.x = RandRange(seed, 0.f, static_cast<float>(width));
        p.y = RandRange(seed, 0.f, static_cast<float>(height));
        p.vx = RandRange(seed, -0.05f, 0.05f);
        p.vy = RandRange(seed, -0.25f, -0.05f);
        p.radius = RandRange(seed, 0.2f, 1.2f);
        p.max_life = 1.f;
        p.life = 1.f;
        p.accent = Rand01(seed) > 0.88f;
    }
    for (int i = 0; i < bokehN && idx < particle_count; ++i, ++idx) {
        auto& p = particles[idx];
        p.type = 2;
        p.x = RandRange(seed, 0.f, static_cast<float>(width));
        p.y = RandRange(seed, 0.f, static_cast<float>(height));
        p.vx = RandRange(seed, -0.04f, 0.04f);
        p.vy = RandRange(seed, -0.16f, -0.04f);
        p.radius = RandRange(seed, 1.5f, 5.f);
        p.alpha = RandRange(seed, 0.08f, 0.33f);
        p.phase = RandRange(seed, 0.f, 6.2831853f);
        p.max_life = 1.f;
        p.life = 1.f;
    }
    initialized = true;
}

void IntroParticleEngine::Render(ImDrawList* dl, float elapsed_ms, float scale) {
    if (!initialized) return;

    const float burstPhaseEnd = 3200.f;
    const float globalFadeStart = 6800.f;
    const float globalFadeDur = 1200.f;
    float burstPhase = 0.f;
    if (elapsed_ms < burstPhaseEnd) {
        burstPhase = elapsed_ms / burstPhaseEnd;
    } else {
        burstPhase = (std::max)(0.f, 1.f - (elapsed_ms - burstPhaseEnd) / 1400.f);
    }
    float globalFade = 1.f;
    if (elapsed_ms > globalFadeStart) {
        globalFade = 1.f - Clamp01((elapsed_ms - globalFadeStart) / globalFadeDur);
    }

    const float centerX = width * 0.5f;
    const float centerY = height * 0.48f;

    for (int i = 0; i < particle_count; ++i) {
        auto& p = particles[i];
        const float speedMul = (p.type == 0 && burstPhase > 0.f) ? 1.f : 0.25f;
        p.x += p.vx * speedMul * scale * 2.2f;
        p.y += p.vy * speedMul * scale * 2.2f;

        if (p.type == 1 && burstPhase <= 0.f) {
            p.life -= 0.005f;
            if (p.life <= 0.f) {
                p.x = RandRange(seed, 0.f, static_cast<float>(width));
                p.y = static_cast<float>(height) + 10.f;
                p.life = 1.f;
            }
        }
        if (p.type == 2) {
            p.phase += 0.012f;
            if (p.y < -20.f) {
                p.y = static_cast<float>(height) + 20.f;
                p.x = RandRange(seed, 0.f, static_cast<float>(width));
            }
        }

        float alpha = (p.life / p.max_life) * globalFade * (0.2f + burstPhase * 0.5f);
        if (p.type == 2) {
            alpha = p.alpha * (0.6f + std::sin(p.phase) * 0.4f) * globalFade;
        }
        if (alpha < 0.01f) continue;

        const int a = static_cast<int>(alpha * 255.f);
        ImU32 col = p.accent ? IM_COL32(90, 200, 250, static_cast<int>(a * 0.9f))
                             : IM_COL32(255, 255, 255, static_cast<int>(a * 0.55f));

        if (p.type == 2 || (p.radius > 1.2f && burstPhase < 0.5f)) {
            DrawGlow(dl, p.x, p.y, p.radius * scale, col);
        } else {
            dl->AddCircleFilled(ImVec2(p.x, p.y), p.radius * scale, col, 10);
        }
    }

    if (elapsed_ms < 5200.f && burstPhase > 0.1f) {
        const float breath = 0.5f + std::sin(elapsed_ms / 1000.f * 1.2f) * 0.15f;
        DrawGlow(dl, centerX, centerY, 40.f * breath * scale,
                 IM_COL32(90, 200, 250, static_cast<int>(0.12f * burstPhase * globalFade * 255.f)));
    }
}

}  // namespace myiui::intro
