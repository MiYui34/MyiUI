#include "ui/intro/intro_screen.h"

#include "ui/menu_app.h"

#include "ui/fonts.h"
#include "ui/glass_panel.h"
#include "ui/intro/intro_particles.h"
#include "ui/intro/intro_timeline.h"
#include "ui/intro/intro_tokens.h"
#include "ui/logo_assets.h"
#include "ui/ui_scale.h"

#undef min
#undef max

#include <algorithm>
#include <cmath>
#include <cstring>

namespace myiui::intro {

namespace {

IntroParticleEngine g_particles;

float IntroScreenUILayoutScale(const ImVec2& display, float ctxScale) {
    const float ref = (std::min)(display.x / 1920.f, display.y / 1080.f);
    return (std::max)(ctxScale, ref) * 1.45f;
}

void DrawPanoramaGradient(ImDrawList* dl, const ImVec2& display, const PanSharpenState& pan, float iris_pct,
                          ImTextureID bgTex, bool hasBg, int bgW, int bgH) {
    const ImVec2 center(display.x * 0.5f, display.y * 0.5f);
    const float maxDim = (std::max)(display.x, display.y);
    const float irisRadius = maxDim * (iris_pct / 100.f) * 0.5f;

    const float coverScale = hasBg && bgW > 0 && bgH > 0
                                 ? (std::max)(display.x / static_cast<float>(bgW), display.y / static_cast<float>(bgH))
                                 : 1.f;
    const ImVec2 drawSize(hasBg && bgW > 0 ? bgW * coverScale * pan.scale : display.x * pan.scale,
                          hasBg && bgH > 0 ? bgH * coverScale * pan.scale : display.y * pan.scale);
    const ImVec2 p0(center.x - drawSize.x * 0.5f, center.y - drawSize.y * 0.5f);
    const ImVec2 p1(p0.x + drawSize.x, p0.y + drawSize.y);

    const int bright = static_cast<int>(pan.brightness * 255.f);
    const int tintA = static_cast<int>(220 * pan.saturate);

    if (hasBg && bgTex) {
        const ImU32 tint = IM_COL32(bright, bright, static_cast<int>(bright * 1.02f), 255);
        dl->PushClipRect(ImVec2(center.x - irisRadius, center.y - irisRadius),
                         ImVec2(center.x + irisRadius, center.y + irisRadius), true);
        dl->AddImage(bgTex, p0, p1, ImVec2(0, 0), ImVec2(1, 1), tint);
        dl->PopClipRect();
    } else {
        dl->PushClipRect(ImVec2(center.x - irisRadius, center.y - irisRadius),
                         ImVec2(center.x + irisRadius, center.y + irisRadius), true);
        dl->AddRectFilledMultiColor(p0, p1, IM_COL32(26, 58, 92, tintA), IM_COL32(74, 144, 194, tintA),
                                    IM_COL32(45, 90, 61, tintA), IM_COL32(26, 58, 92, tintA));
        dl->AddRectFilled(ImVec2(p0.x, p0.y + drawSize.y * 0.48f), p1, IM_COL32(61, 107, 79, tintA));
        dl->PopClipRect();
    }

    if (pan.blur_px > 2.f) {
        const int blurA = static_cast<int>((pan.blur_px / 48.f) * 90.f);
        dl->AddRectFilled(ImVec2(0, 0), display, IM_COL32(8, 12, 20, blurA));
    }
}

void DrawEmblem(ImDrawList* dl, const ImVec2& center, float scale, float emblem_opacity, float emblem_scale,
                float translate_y, float elapsed_ms) {
    if (emblem_opacity <= 0.01f) return;

    const float glass = 120.f * scale * emblem_scale;
    const ImVec2 min(center.x - glass * 0.5f, center.y - glass * 0.5f + translate_y * scale);
    const ImVec2 max(center.x + glass * 0.5f, center.y + glass * 0.5f + translate_y * scale);
    const float round = 30.f * scale;

    const int alpha = static_cast<int>(emblem_opacity * 255.f);
    const LogoSet& logos = GetLogos();
    const bool hasLogo = logos.emblem.valid();
    if (!hasLogo) {
        dl->AddRectFilled(min, max, IM_COL32(255, 255, 255, static_cast<int>(36 * emblem_opacity)), round);
        dl->AddRect(min, max, IM_COL32(90, 200, 250, static_cast<int>(115 * emblem_opacity)), round, 0, 2.f * scale);
    }

    const float ring1 = GetRingProgress(elapsed_ms, 1400.f, 2200.f);
    const float ring2 = GetRingProgress(elapsed_ms, 1800.f, 2600.f);
    if (ring1 > 0.f) {
        const float r = (70.f + ring1 * 70.f) * scale;
        const int ra = static_cast<int>((1.f - ring1) * 0.65f * emblem_opacity * 255.f);
        dl->AddCircle(center, r, IM_COL32(90, 200, 250, ra), 48, 1.5f * scale);
    }
    if (ring2 > 0.f) {
        const float r = (90.f + ring2 * 90.f) * scale;
        const int ra = static_cast<int>((1.f - ring2) * 0.45f * emblem_opacity * 255.f);
        dl->AddCircle(center, r, IM_COL32(90, 200, 250, ra), 48, 1.f * scale);
    }

    const float iconPad = hasLogo ? 0.f : 28.f * scale;
    const ImVec2 i0(min.x + iconPad, min.y + iconPad);
    const ImVec2 i1(max.x - iconPad, max.y - iconPad);
    if (hasLogo) {
        DrawLogoFit(dl, logos.emblem, min, max, emblem_opacity);
    } else {
        const float strokeP = EasePremium(TrackProgress(elapsed_ms, 1500.f, 1800.f));
        const ImU32 stroke = IM_COL32(90, 200, 250, alpha);
        if (strokeP > 0.01f) {
            dl->AddRect(i0, i1, stroke, 8.f * scale, 0, 2.f * scale);
            dl->AddLine(ImVec2(i0.x + 8.f * scale, center.y + translate_y * scale),
                        ImVec2(i1.x - 8.f * scale, center.y + translate_y * scale), stroke, 1.5f * scale);
            dl->AddLine(ImVec2(center.x, i0.y + 8.f * scale), ImVec2(center.x, i1.y - 8.f * scale), stroke,
                        1.5f * scale);
        }
    }

    const UiFonts& fonts = GetUiFonts();
    const float wordY = max.y + 36.f * scale + translate_y * scale;
    const float brandSize = fonts.brand ? fonts.brand->FontSize * 1.4f : 32.f * scale;
    float wordAlpha = 0.f;
    for (int i = 0; i < 5; ++i) {
        wordAlpha = (std::max)(wordAlpha, GetLetterReveal(elapsed_ms, i));
    }
    if (wordAlpha > 0.f) {
        const float yOff = (1.f - wordAlpha) * 28.f * scale;
        const ImU32 col = IM_COL32(245, 246, 248, static_cast<int>(wordAlpha * emblem_opacity * 255.f));
        const ImVec2 wordSize = fonts.brand ? fonts.brand->CalcTextSizeA(brandSize, FLT_MAX, 0.f, kWordmark)
                                            : ImGui::CalcTextSize(kWordmark);
        const ImVec2 wordPos(center.x - wordSize.x * 0.5f, wordY + yOff);
        if (fonts.brand) {
            dl->AddText(fonts.brand, brandSize, wordPos, col, kWordmark);
        } else {
            dl->AddText(wordPos, col, kWordmark);
        }
    }

    const float tagP = GetBootLineOpacity(elapsed_ms, 2700.f);
    if (tagP > 0.f && fonts.caption) {
        const float capSize = fonts.caption->FontSize * 1.2f;
        const ImVec2 ts = fonts.caption->CalcTextSizeA(capSize, FLT_MAX, 0.f, kTagline);
        dl->AddText(fonts.caption, capSize,
                    ImVec2(center.x - ts.x * 0.5f, wordY + 44.f * scale + translate_y * scale),
                    IM_COL32(184, 188, 196, static_cast<int>(tagP * emblem_opacity * 255.f)), kTagline);
    }

    const float lineP = EasePremium(TrackProgress(elapsed_ms, 2900.f, 1000.f));
    if (lineP > 0.f) {
        const float lw = 48.f * scale * lineP;
        dl->AddRectFilled(ImVec2(center.x - lw * 0.5f, wordY + 56.f * scale), ImVec2(center.x + lw * 0.5f, wordY + 57.f * scale),
                          IM_COL32(90, 200, 250, static_cast<int>(lineP * emblem_opacity * 200.f)));
    }
}

void DrawBootHud(ImDrawList* dl, const ImVec2& display, float scale, float hud_opacity, float elapsed_ms) {
    if (hud_opacity <= 0.01f) return;
    const UiFonts& fonts = GetUiFonts();
    const float inset = 40.f * scale;
    const float panelH = 148.f * scale;
    const float logW = 440.f * scale;
    const ImVec2 logMin(inset, display.y - inset - panelH);
    const ImVec2 logMax(logMin.x + logW, display.y - inset);
    const int ha = static_cast<int>(hud_opacity * 255.f);
    dl->AddRectFilled(logMin, logMax, IM_COL32(255, 255, 255, static_cast<int>(20 * hud_opacity)), 14.f * scale);
    dl->AddRect(logMin, logMax, IM_COL32(255, 255, 255, static_cast<int>(40 * hud_opacity)), 14.f * scale, 0, 1.f);

    const char* lines[] = {kBootLog0, kBootLog1, kBootLog2, kBootLog3};
    const float delays[] = {3600.f, 4200.f, 4800.f, 5600.f};
    float y = logMin.y + 16.f * scale;
    for (int i = 0; i < 4; ++i) {
        const float lp = GetBootLineOpacity(elapsed_ms, delays[i]) * hud_opacity;
        if (lp <= 0.f) continue;
        if (fonts.caption) {
            const float monoSize = fonts.caption->FontSize * 1.15f;
            dl->AddText(fonts.caption, monoSize, ImVec2(logMin.x + 16.f * scale, y),
                        IM_COL32(132, 138, 148, static_cast<int>(lp * 255.f)), lines[i]);
        } else {
            dl->AddText(ImVec2(logMin.x + 16.f * scale, y), IM_COL32(132, 138, 148, static_cast<int>(lp * 255.f)),
                        lines[i]);
        }
        y += 22.f * scale;
    }

    const float ring = GetProgressRing(elapsed_ms);
    const ImVec2 ringCenter(display.x - inset - 64.f * scale, display.y - inset - 60.f * scale);
    const float radius = 34.f * scale;
    dl->AddCircle(ringCenter, radius, IM_COL32(255, 255, 255, static_cast<int>(15 * hud_opacity)), 48, 2.5f * scale);
    const float arc = 6.2831853f * ring * 0.85f;
    dl->PathArcTo(ringCenter, radius, -1.5707963f, -1.5707963f + arc, 32);
    dl->PathStroke(IM_COL32(90, 200, 250, ha), 0, 2.5f * scale);

    if (fonts.caption) {
        const float labelSize = fonts.caption->FontSize * 1.15f;
        const ImVec2 ls = fonts.caption->CalcTextSizeA(labelSize, FLT_MAX, 0.f, kProgressLabel);
        dl->AddText(fonts.caption, labelSize,
                    ImVec2(ringCenter.x - ls.x * 0.5f, ringCenter.y + radius + 10.f * scale),
                    IM_COL32(184, 188, 196, ha), kProgressLabel);
    }
}

void BeginIntro(float& elapsed, IntroPhase& phase, float& exit_ms, bool& awaiting_first_paint) {
    phase = IntroPhase::Playing;
    elapsed = 0.f;
    exit_ms = 0.f;
    awaiting_first_paint = true;
}

float ClampIntroDeltaMs(float delta_ms) {
    if (delta_ms > 150.f) return 0.f;
    return (std::min)(delta_ms, 48.f);
}

void IntroScreenTick(IntroScreenState& intro, float delta_ms) {
    if (intro.phase == IntroPhase::Playing) {
        float dt = ClampIntroDeltaMs(delta_ms);
        if (intro.awaiting_first_paint) {
            intro.awaiting_first_paint = false;
            intro.elapsed_ms = 0.f;
            dt = 0.f;
            g_particles.initialized = false;
        }
        intro.elapsed_ms += dt;
        if (intro.elapsed_ms >= kDurationMs) {
            intro.phase = IntroPhase::Exiting;
            intro.exit_ms = 0.f;
            intro.elapsed_ms = kDurationMs;
        }
    } else if (intro.phase == IntroPhase::Exiting) {
        intro.exit_ms += ClampIntroDeltaMs(delta_ms);
        if (intro.exit_ms >= kSkipExitFadeMs) {
            intro.phase = IntroPhase::Done;
            intro.played_this_session = true;
        }
    }
}

}  // namespace

void IntroScreenOnMenuActive(IntroScreenState& intro) {
    if (intro.played_this_session || intro.phase == IntroPhase::Playing || intro.phase == IntroPhase::Exiting) {
        return;
    }
    if (intro.pending_after_inject && intro.phase == IntroPhase::NotStarted) {
        BeginIntro(intro.elapsed_ms, intro.phase, intro.exit_ms, intro.awaiting_first_paint);
        intro.pending_after_inject = false;
    }
}

bool IntroScreenIsBlocking(const IntroScreenState& intro) {
    return intro.phase == IntroPhase::Playing || intro.phase == IntroPhase::Exiting;
}

void IntroScreenUpdate(IntroScreenState& intro, float delta_ms, bool /*reduce_motion*/) {
    (void)intro;
    (void)delta_ms;
}

void IntroScreenHandleInput(IntroScreenState& intro, bool reduce_motion) {
    if (intro.phase != IntroPhase::Playing) return;
    if (intro.elapsed_ms < kSkipDebounceMs) return;

    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k))) {
            intro.phase = IntroPhase::Exiting;
            intro.exit_ms = 0.f;
            if (reduce_motion) intro.exit_ms = kSkipExitFadeMs - 50.f;
            return;
        }
    }
    if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)) {
        intro.phase = IntroPhase::Exiting;
        intro.exit_ms = 0.f;
    }
}

void IntroScreenRender(MenuRenderContext& ctx, IntroScreenState& intro) {
    const float delta_ms = ImGui::GetIO().DeltaTime * 1000.f;
    IntroScreenTick(intro, delta_ms);

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const int iw = static_cast<int>(display.x);
    const int ih = static_cast<int>(display.y);
    if (!g_particles.initialized || iw != intro.last_width || ih != intro.last_height) {
        g_particles.Init(iw, ih);
        intro.last_width = iw;
        intro.last_height = ih;
    }

    const float scale = IntroScreenUILayoutScale(display, ctx.scale);
    const float elapsed = intro.elapsed_ms;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    dl->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, 255));

    const PanSharpenState pan = GetPanSharpen(elapsed);
    const float iris = GetIrisRadiusPct(elapsed);
    DrawPanoramaGradient(dl, display, pan, iris, ctx.bgTexture, ctx.hasBg, ctx.bgTexW, ctx.bgTexH);

    const float overlayA = GetOverlayOpacity(elapsed);
    if (overlayA > 0.f) {
        dl->AddRectFilled(ImVec2(0, 0), display, IM_COL32(8, 12, 20, static_cast<int>(overlayA * 140.f)));
    }

    const float glowA = GetAmbientGlowOpacity(elapsed);
    if (glowA > 0.f) {
        const ImVec2 c(display.x * 0.5f, display.y * 0.48f);
        dl->AddCircleFilled(c, display.x * 0.35f, IM_COL32(90, 200, 250, static_cast<int>(glowA * 36.f)), 48);
    }

    const float sweepX = GetLightSweepX(elapsed);
    const float sweepA = GetLightSweepOpacity(elapsed);
    if (sweepA > 0.f) {
        const float x0 = display.x * (0.5f + sweepX * 0.5f) - display.x * 0.25f;
        dl->AddRectFilledMultiColor(ImVec2(x0, -display.y * 0.2f), ImVec2(x0 + display.x * 0.5f, display.y * 1.2f),
                                    IM_COL32(255, 255, 255, 0), IM_COL32(90, 200, 250, static_cast<int>(sweepA * 30.f)),
                                    IM_COL32(90, 200, 250, static_cast<int>(sweepA * 30.f)), IM_COL32(255, 255, 255, 0));
    }

    const float scanY = GetScanLineY(elapsed, display.y);
    const float scanA = GetScanLineOpacity(elapsed);
    if (scanA > 0.f) {
        dl->AddRectFilled(ImVec2(0, scanY), ImVec2(display.x, scanY + 1.f),
                          IM_COL32(90, 200, 250, static_cast<int>(scanA * 180.f)));
    }

    const float voidA = GetVoidOpacity(elapsed);
    if (voidA > 0.f) {
        dl->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, static_cast<int>(voidA * 255.f)));
    }

    const ImVec2 center(display.x * 0.5f, display.y * 0.46f);
    DrawEmblem(dl, center, scale, GetEmblemOpacity(elapsed), GetEmblemScale(elapsed), GetEmblemTranslateY(elapsed),
               elapsed);

    g_particles.Render(dl, elapsed, scale);

    const float letterbox = GetLetterboxHeightNorm(elapsed);
    if (letterbox > 0.001f) {
        const float h = display.y * letterbox;
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(display.x, h), IM_COL32(0, 0, 0, 255));
        dl->AddRectFilled(ImVec2(0, display.y - h), display, IM_COL32(0, 0, 0, 255));
    }

    const float vignette = GetVignetteOpacity(elapsed);
    if (vignette > 0.f) {
        dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(display.x, display.y * 0.3f),
                                    IM_COL32(0, 0, 0, static_cast<int>(vignette * 200.f)), IM_COL32(0, 0, 0, static_cast<int>(vignette * 200.f)),
                                    IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
        dl->AddRectFilledMultiColor(ImVec2(0, display.y * 0.7f), display, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
                                    IM_COL32(0, 0, 0, static_cast<int>(vignette * 220.f)),
                                    IM_COL32(0, 0, 0, static_cast<int>(vignette * 220.f)));
    }

    DrawBootHud(dl, display, scale, GetBootHudOpacity(elapsed), elapsed);

    const float chipA = GetSuccessChipOpacity(elapsed);
    if (chipA > 0.f) {
        const UiFonts& fonts = GetUiFonts();
        const float chipFontSize = fonts.caption ? fonts.caption->FontSize * 1.2f : 14.f * scale;
        const ImVec2 textSize = fonts.caption ? fonts.caption->CalcTextSizeA(chipFontSize, FLT_MAX, 0.f, kSuccessChip)
                                              : ImGui::CalcTextSize(kSuccessChip);
        const float dotD = 8.f * scale;
        const float dotGap = 8.f * scale;
        const float padX = 16.f * scale;
        const float chipW = padX + dotD + dotGap + textSize.x + padX;
        const float chipH = 36.f * scale;
        const ImVec2 chipMin(center.x - chipW * 0.5f, center.y + 120.f * scale);
        const ImVec2 chipMax(chipMin.x + chipW, chipMin.y + chipH);
        const float midY = (chipMin.y + chipMax.y) * 0.5f;
        dl->AddRectFilled(chipMin, chipMax, IM_COL32(48, 209, 88, static_cast<int>(chipA * 40.f)), 18.f * scale);
        dl->AddRect(chipMin, chipMax, IM_COL32(48, 209, 88, static_cast<int>(chipA * 80.f)), 18.f * scale, 0, 1.f);
        dl->AddCircleFilled(ImVec2(chipMin.x + padX + dotD * 0.5f, midY), dotD * 0.5f,
                            IM_COL32(48, 209, 88, static_cast<int>(chipA * 255.f)));
        if (fonts.caption) {
            const float textX = chipMin.x + padX + dotD + dotGap;
            const float textY = midY - textSize.y * 0.5f;
            dl->AddText(fonts.caption, chipFontSize, ImVec2(textX, textY),
                        IM_COL32(245, 246, 248, static_cast<int>(chipA * 255.f)), kSuccessChip);
        }
    }

    float exitA = GetExitFadeOpacity(elapsed);
    if (intro.phase == IntroPhase::Exiting) {
        exitA = (std::max)(exitA, intro.exit_ms / kSkipExitFadeMs);
    }
    if (exitA > 0.f) {
        dl->AddRectFilled(ImVec2(0, 0), display, IM_COL32(10, 14, 20, static_cast<int>(exitA * 255.f)));
    }

    if (elapsed > 4000.f && intro.phase == IntroPhase::Playing) {
        const UiFonts& fonts = GetUiFonts();
        const float skipSize = fonts.caption ? fonts.caption->FontSize * 1.25f : 15.f * scale;
        const ImVec2 ts = fonts.caption ? fonts.caption->CalcTextSizeA(skipSize, FLT_MAX, 0.f, kSkipHint)
                                        : ImGui::CalcTextSize(kSkipHint);
        if (fonts.caption) {
            dl->AddText(fonts.caption, skipSize,
                        ImVec2(display.x - ts.x - 32.f * scale, display.y - ts.y - 32.f * scale), kFgDim, kSkipHint);
        }
    }

    ImGui::PushID("MyiUIIntro");
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display);
    ImGui::Begin("##MyiUI_IntroInput", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImGui::InvisibleButton("##capture", display);
    IntroScreenHandleInput(intro, false);
    ImGui::End();
    ImGui::PopID();
}

}  // namespace myiui::intro
