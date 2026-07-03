#pragma once

#include "imgui.h"

#include <cstdint>

namespace myiui::intro {

inline constexpr float kDurationMs = 8000.f;
inline constexpr float kSkipDebounceMs = 800.f;
inline constexpr float kSkipExitFadeMs = 900.f;
inline constexpr float kReducedMotionMs = 1000.f;

inline constexpr ImU32 kAccent = IM_COL32(90, 200, 250, 255);
inline constexpr ImU32 kBgDeep = IM_COL32(10, 14, 20, 255);
inline constexpr ImU32 kFg = IM_COL32(245, 246, 248, 255);
inline constexpr ImU32 kFgMuted = IM_COL32(184, 188, 196, 255);
inline constexpr ImU32 kFgDim = IM_COL32(132, 138, 148, 255);
inline constexpr ImU32 kSuccess = IM_COL32(48, 209, 88, 255);
inline constexpr ImU32 kGlassBgStrong = IM_COL32(255, 255, 255, 36);
inline constexpr ImU32 kGlassBorder = IM_COL32(255, 255, 255, 56);
inline constexpr ImU32 kGlassBorderAccent = IM_COL32(90, 200, 250, 115);

inline constexpr const char* kWordmark = "MyiUI";
inline constexpr const char* kTagline = "Fabric UI Mod";
inline constexpr const char* kProgressLabel = "加载中";
inline constexpr const char* kSuccessChip = "准备就绪";
inline constexpr const char* kSkipHint = "按任意键或点击跳过";

inline constexpr const char* kBootLog0 = "> init fabric-renderer … ok";
inline constexpr const char* kBootLog1 = "> load glass-shader pipeline";
inline constexpr const char* kBootLog2 = "> bind title-screen overlay";
inline constexpr const char* kBootLog3 = "> ready — entering menu";

}  // namespace myiui::intro
