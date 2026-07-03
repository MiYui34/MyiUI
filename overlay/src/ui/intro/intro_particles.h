#pragma once

#include "imgui.h"

namespace myiui::intro {

struct IntroParticleEngine {
    struct Particle {
        float x = 0.f;
        float y = 0.f;
        float vx = 0.f;
        float vy = 0.f;
        float radius = 1.f;
        float life = 1.f;
        float max_life = 1.f;
        float phase = 0.f;
        float alpha = 0.2f;
        int type = 0;  // 0 burst, 1 ambient, 2 bokeh
        bool accent = false;
    };

    int width = 0;
    int height = 0;
    bool initialized = false;
    static constexpr int kMaxParticles = 101;
    Particle particles[kMaxParticles]{};
    int particle_count = 0;
    unsigned seed = 0xC0FFEEu;

    void Init(int w, int h);
    void Render(ImDrawList* dl, float elapsed_ms, float scale);
};

}  // namespace myiui::intro
