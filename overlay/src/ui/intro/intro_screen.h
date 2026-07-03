#pragma once

struct MenuRenderContext;

namespace myiui::intro {

enum class IntroPhase { NotStarted, Playing, Exiting, Done };

struct IntroScreenState {
    IntroPhase phase = IntroPhase::NotStarted;
    float elapsed_ms = 0.f;
    float exit_ms = 0.f;
    bool played_this_session = false;
    bool pending_after_inject = true;
    bool awaiting_first_paint = false;
    int last_width = 0;
    int last_height = 0;
};

void IntroScreenOnMenuActive(IntroScreenState& intro);
bool IntroScreenIsBlocking(const IntroScreenState& intro);
void IntroScreenUpdate(IntroScreenState& intro, float delta_ms, bool reduce_motion);
void IntroScreenRender(MenuRenderContext& ctx, IntroScreenState& intro);
void IntroScreenHandleInput(IntroScreenState& intro, bool reduce_motion);

}  // namespace myiui::intro
