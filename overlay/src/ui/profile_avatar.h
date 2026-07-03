#pragma once

#include "imgui.h"

#include <cstdint>
#include <string>

using AvatarTexId = unsigned int;

struct ProfileAvatarTexture {
    AvatarTexId tex = 0;
    int w = 0;
    int h = 0;

    bool valid() const { return tex != 0 && w > 0 && h > 0; }
};

void ProfileAvatarInit();
void ProfileAvatarShutdown();
void ProfileAvatarInvalidate();
bool ProfileAvatarIsBusy();
void ProfileAvatarRequest(const std::string& url);
void ProfileAvatarRequestLocalFile(const std::string& path);
void ProfileAvatarUpdate();
const ProfileAvatarTexture& ProfileAvatarGet();
ImTextureID ProfileAvatarImGuiId();
