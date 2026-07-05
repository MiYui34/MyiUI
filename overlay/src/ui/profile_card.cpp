#include "ui/profile_card.h"

#include "ui/async_pipe.h"
#include "ui/glass_panel.h"
#include "ui/menu_app.h"
#include "ui/profile_avatar.h"
#include "ui/ui_scale.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

using myiui::ui::ColorFromRGBA;
using myiui::ui::DrawGlassSurface;

namespace {

std::string g_lastAvatarSource;

bool LocalSkinFileExists(const std::string& path) {
    if (path.empty()) return false;
    wchar_t wide[MAX_PATH * 4]{};
    if (MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide, MAX_PATH * 4) <= 0) return false;
    const DWORD attrs = GetFileAttributesW(wide);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void GetProfileInitials(const ProfileData& profile, char out[4]) {
    out[0] = out[1] = out[2] = '\0';
    if (profile.name.empty()) {
        out[0] = '?';
        return;
    }
    const char* name = profile.name.c_str();
    out[0] = name[0];
    for (size_t i = 1; i < profile.name.size(); ++i) {
        if (name[i - 1] == ' ' || name[i - 1] == '_') {
            out[1] = name[i];
            break;
        }
    }
    if (out[1] == '\0' && profile.name.size() > 1) {
        out[1] = name[1];
    }
}

}  // namespace

void ProfileCardEnsureLoaded(MenuAppState& state, float deltaMs) {
    if (!state.data.profile_seeded) {
        LoadProfileFromDisk(state.data.profile);
        state.data.profile_seeded = true;
        g_lastAvatarSource.clear();
        ProfileAvatarInvalidate();
    }

    if (state.data.profile_fetch_done) return;

    if (state.data.profile_loading) {
        state.data.profile_load_ms += deltaMs;
        if (state.data.profile_load_ms > 6500.f) {
            state.data.profile_loading = false;
            state.data.profile_fetch_done = true;
            if (state.data.profile.name.empty() || state.data.profile.name == "Player") {
                LoadProfileFromDisk(state.data.profile);
                g_lastAvatarSource.clear();
                ProfileAvatarInvalidate();
            }
        }
        return;
    }

    state.data.profile_loading = true;
    state.data.profile_load_ms = 0.f;
    MenuAppStartPipeLoad(state, PipeLoadKind::Profile, "GET_PLAYER");
}

void ProfileCardUpdateAvatar(MenuAppState& state) {
    ProfileAvatarUpdate();

    std::string source;
    if (!state.data.profile.skin_path.empty() && LocalSkinFileExists(state.data.profile.skin_path)) {
        source = state.data.profile.skin_path;
    } else if (!state.data.profile.skin_url.empty()) {
        source = state.data.profile.skin_url;
    }
    if (source.empty()) return;
    if (source == g_lastAvatarSource && (ProfileAvatarGet().valid() || ProfileAvatarIsBusy())) {
        return;
    }

    g_lastAvatarSource = source;
    if (!state.data.profile.skin_path.empty() && LocalSkinFileExists(state.data.profile.skin_path)) {
        ProfileAvatarRequestLocalFile(state.data.profile.skin_path);
    } else {
        ProfileAvatarRequest(state.data.profile.skin_url);
    }
}

void DrawProfileCard(ImDrawList* dl, const ImVec2& pos, const ImVec2& size, const AppConfig& cfg,
                     const UiFonts& fonts, float scale, float hoverT, const ProfileData& profile) {
    const float alphaMul = 1.f + (cfg.theme.hover_alpha_mul - 1.f) * hoverT * 0.35f;
    const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
    const float round = Px(cfg.theme.profile_corner_radius, scale);
    DrawGlassSurface(dl, pos, rectMax, cfg.theme.glass_tint_strong, cfg.theme.border_color, round,
                     cfg.theme.border_width, alphaMul);

    const float pad = Px(20.f, scale);
    const float avatarSize = Px(48.f, scale);
    const float avatarRound = Px(12.f, scale);
    const ImVec2 avatarMin(pos.x + pad, pos.y + pad);
    const ImVec2 avatarMax(avatarMin.x + avatarSize, avatarMin.y + avatarSize);

    const ProfileAvatarTexture& avatar = ProfileAvatarGet();
    if (avatar.valid()) {
        ImDrawList* avatarDl = ImGui::GetForegroundDrawList();
        avatarDl->AddRectFilled(avatarMin, avatarMax, IM_COL32(12, 16, 24, static_cast<int>(220 * alphaMul)), avatarRound);
        avatarDl->AddImageRounded(ProfileAvatarImGuiId(), avatarMin, avatarMax, ImVec2(0, 0), ImVec2(1, 1),
                                  IM_COL32(255, 255, 255, static_cast<int>(255 * alphaMul)), avatarRound);
        avatarDl->AddRect(avatarMin, avatarMax, ColorFromRGBA(cfg.theme.border_color, alphaMul * 0.9f), avatarRound, 0,
                          cfg.theme.border_width);
    } else {
        DrawGlassSurface(dl, avatarMin, avatarMax, cfg.theme.glass_tint, cfg.theme.border_color, avatarRound,
                         cfg.theme.border_width, alphaMul);
        char initials[4]{};
        GetProfileInitials(profile, initials);
        const ImVec2 initSize = fonts.semibold
                                    ? fonts.semibold->CalcTextSizeA(fonts.semibold->FontSize, FLT_MAX, 0.f, initials)
                                    : ImGui::CalcTextSize(initials);
        DrawTextStyled(dl, fonts.semibold,
                       ImVec2(avatarMin.x + (avatarSize - initSize.x) * 0.5f,
                              avatarMin.y + (avatarSize - initSize.y) * 0.5f),
                       ColorFromRGBA(cfg.theme.text_secondary, alphaMul), initials);
    }

    const float textX = avatarMax.x + Px(14.f, scale);
    const float headerMidY = pos.y + pad + avatarSize * 0.5f;
    const char* displayName = profile.name.empty() ? "Player" : profile.name.c_str();
    DrawTextStyled(dl, fonts.profileName, ImVec2(textX, headerMidY - Px(16.f, scale)),
                   ColorFromRGBA(cfg.theme.text_primary, alphaMul), displayName);

    const char* accountLabel = profile.account_type == "premium" ? "\xe6\xad\xa3\xe7\x89\x88\xe8\xb4\xa6\xe5\x8f\xb7"
                                                                 : "\xe7\xa6\xbb\xe7\xba\xbf\xe8\xb4\xa6\xe5\x8f\xb7";
    const ImU32 accountColor = profile.account_type == "premium"
                                   ? ColorFromRGBA(cfg.theme.accent, alphaMul)
                                   : ColorFromRGBA(cfg.theme.text_dim, alphaMul);
    DrawTextStyled(dl, fonts.caption, ImVec2(textX, headerMidY + Px(2.f, scale)), accountColor, accountLabel);

    const float statusY = avatarMax.y + Px(16.f, scale);
    const ImVec2 statusMin(pos.x + pad, statusY);
    const ImVec2 statusMax(pos.x + size.x - pad, statusY + Px(36.f, scale));
    DrawGlassSurface(dl, statusMin, statusMax, cfg.theme.glass_tint, cfg.theme.border_color, Px(10.f, scale), 2.f,
                     alphaMul * 0.85f);

    const float dotR = Px(4.f, scale);
    const ImVec2 dotCenter(statusMin.x + Px(12.f, scale), (statusMin.y + statusMax.y) * 0.5f);
    const ImU32 dotColor = profile.account_type == "premium" ? ColorFromRGBA(cfg.theme.accent, alphaMul)
                                                               : ColorFromRGBA(cfg.theme.text_dim, alphaMul);
    dl->AddCircleFilled(dotCenter, dotR, dotColor);

    char statusBuf[96]{};
    if (profile.mc_version.empty() || profile.mc_version == "Unknown") {
        std::strncpy(statusBuf, "Minecraft", sizeof(statusBuf) - 1);
    } else {
        std::snprintf(statusBuf, sizeof(statusBuf), "Minecraft %s", profile.mc_version.c_str());
    }
    DrawTextStyled(dl, fonts.profileSub, ImVec2(dotCenter.x + Px(14.f, scale), statusMin.y + Px(10.f, scale)),
                   ColorFromRGBA(cfg.theme.text_secondary, alphaMul), statusBuf);

    const float metaY = statusMax.y + Px(14.f, scale);
    const char* metaLeft = profile.loader.empty() ? "Fabric" : profile.loader.c_str();
    DrawTextStyled(dl, fonts.caption, ImVec2(pos.x + pad, metaY), ColorFromRGBA(cfg.theme.text_dim, alphaMul), metaLeft);

    if (!profile.uuid.empty()) {
        std::string shortUuid = profile.uuid;
        if (shortUuid.size() > 13) {
            shortUuid = shortUuid.substr(0, 8) + "..." + shortUuid.substr(shortUuid.size() - 4);
        }
        const ImVec2 metaRightSize =
            fonts.caption ? fonts.caption->CalcTextSizeA(fonts.caption->FontSize, FLT_MAX, 0.f, shortUuid.c_str())
                          : ImGui::CalcTextSize(shortUuid.c_str());
        DrawTextStyled(dl, fonts.caption, ImVec2(rectMax.x - pad - metaRightSize.x, metaY),
                       ColorFromRGBA(cfg.theme.text_dim, alphaMul * 0.85f), shortUuid.c_str());
    }
}
