#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool LoadPngRgba(const std::wstring& path, std::vector<uint8_t>& rgba, int& outW, int& outH);
bool LoadImageRgbaFromBytes(const uint8_t* data, size_t size, std::vector<uint8_t>& rgba, int& outW, int& outH);
