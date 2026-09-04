#pragma once

#include "format2_surface_document.h"

#include <limits>

inline const Format2TextProfile* FindFormat2TextProfile(const Format2SurfaceDocument& document, const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* profileProperty = nullptr;
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == "TextProfile") profileProperty = &property;
    if (!profileProperty || profileProperty->value.list) return nullptr;
    for (const Format2TextProfile& profile : document.textProfiles) if (profile.id == profileProperty->value.scalar.text) return &profile;
    return nullptr;
}

inline std::size_t Format2Utf8CharacterSize(const std::string& source, std::size_t offset) {
    const unsigned char first = (unsigned char) source[offset];
    std::size_t size = first < 0x80 ? 1 : (first & 0xE0) == 0xC0 ? 2 : (first & 0xF0) == 0xE0 ? 3 : (first & 0xF8) == 0xF0 ? 4 : 1;
    if (offset + size > source.size()) return 1;
    for (std::size_t byteIdx = 1; byteIdx < size; ++byteIdx) if (((unsigned char) source[offset + byteIdx] & 0xC0) != 0x80) return 1;
    return size;
}

inline std::string EncodeFormat2TextProfile(const Format2TextProfile& profile, const char* inputText, std::optional<std::size_t> maximumBytes = std::nullopt) {
    const char* sourceText = profile.silenceAsEmpty && IsSameString(inputText, SILENCE_DB_STRING) ? "" : inputText;
    const std::string source = sourceText ? sourceText : "";
    const std::size_t width = profile.width ? (std::size_t) *profile.width : (std::numeric_limits<std::size_t>::max)();
    const std::size_t byteLimit = maximumBytes.value_or((std::numeric_limits<std::size_t>::max)());
    std::string result;
    std::size_t characterCount = 0;
    for (std::size_t offset = 0; offset < source.size() && characterCount < width;) {
        const std::size_t characterSize = profile.encoding == Format2TextEncoding::Utf8 ? Format2Utf8CharacterSize(source, offset) : 1;
        if (result.size() + characterSize > byteLimit) break;
        if (profile.encoding == Format2TextEncoding::Ascii7) result.push_back((unsigned char) source[offset] <= 0x7F ? source[offset] : '?');
        else result.append(source, offset, characterSize);
        offset += characterSize;
        ++characterCount;
    }
    if (profile.padding == Format2TextPadding::Space && profile.width) {
        const std::size_t padding = (std::min)(width - characterCount, byteLimit - result.size());
        result.append(padding, ' ');
    }
    return result;
}
