#pragma once

#include "imgui.h"

#include <string>

namespace myiui::ui::music {

struct CoverTexture {
    unsigned int tex = 0;
    int w = 0;
    int h = 0;
    bool valid() const { return tex != 0 && w > 0 && h > 0; }
};

// 请求加载一张封面（URL 异步下载 + WIC 解码 + 渲染线程 GL 上传）。
// 相同 URL 重复请求会去重；已加载的直接返回缓存。
void CoverRequest(const std::string& url);

// 渲染线程调用：处理待上传纹理。
void CoverProcessPending();

// 取得某 URL 对应的已加载纹理（未加载返回空纹理）。
CoverTexture CoverGet(const std::string& url);

// 立即从 base64 PNG 创建一张纹理（用于二维码登录图片）。返回纹理 id。
// 也在渲染线程调用（同步解码 + 上传）。
CoverTexture CoverFromBase64Png(const std::string& b64);

// 释放所有缓存的纹理（关闭时调用）。
void CoverShutdown();

}  // namespace myiui::ui::music
