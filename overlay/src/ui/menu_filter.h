#pragma once

#include <cctype>
#include <string>

inline bool TextContainsInsensitive(const std::string& haystack, const char* needle) {
    if (!needle || needle[0] == '\0') return true;
    std::string lowerHay;
    lowerHay.reserve(haystack.size());
    for (char c : haystack) lowerHay.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    std::string lowerNeedle;
    for (const char* p = needle; *p; ++p) {
        lowerNeedle.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }
    return lowerHay.find(lowerNeedle) != std::string::npos;
}
