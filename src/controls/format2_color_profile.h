#pragma once

#include "format2_surface_document.h"

#include <cmath>
#include <limits>

inline const Format2ColorProfile* FindFormat2ColorProfile(const Format2SurfaceDocument& document, const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* profileProperty = nullptr;
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == "ColorProfile") profileProperty = &property;
    if (!profileProperty || profileProperty->value.list) return nullptr;
    for (const Format2ColorProfile& profile : document.colorProfiles) if (profile.id == profileProperty->value.scalar.text) return &profile;
    return nullptr;
}

inline rgba_color UnpackFormat2Color(std::uint32_t color) {
    rgba_color result;
    result.r = (color >> 16) & 0xFF;
    result.g = (color >> 8) & 0xFF;
    result.b = color & 0xFF;
    return result;
}

inline double GetFormat2ColorHue(const rgba_color& color) {
    const double red = color.r / 255.0;
    const double green = color.g / 255.0;
    const double blue = color.b / 255.0;
    const double maximum = (std::max)(red, (std::max)(green, blue));
    const double minimum = (std::min)(red, (std::min)(green, blue));
    const double difference = maximum - minimum;
    if (difference == 0.0) return 0.0;
    double hue = maximum == red ? 60.0 * std::fmod((green - blue) / difference, 6.0) : maximum == green ? 60.0 * ((blue - red) / difference + 2.0) : 60.0 * ((red - green) / difference + 4.0);
    if (hue < 0.0) hue += 360.0;
    return hue;
}

inline bool Format2HueRangeContains(const Format2HueRange& range, double hue) {
    if (range.minimum < range.maximum) return hue >= range.minimum && hue < range.maximum;
    return hue >= range.minimum || hue < range.maximum;
}

inline int ResolveFormat2ColorProfileValue(const Format2ColorProfile& profile, const rgba_color& color) {
    if (profile.match == Format2ColorMatch::HueRanges) {
        const double maximum = (std::max)(color.r, (std::max)(color.g, color.b)) / 255.0;
        const double minimum = (std::min)(color.r, (std::min)(color.g, color.b)) / 255.0;
        const double saturation = maximum == 0.0 ? 0.0 : (maximum - minimum) / maximum;
        if (maximum <= profile.minimumBrightness.value_or(0.0) || saturation <= profile.maximumNeutralSaturation.value_or(0.0)) return profile.defaultValue;
        const double hue = GetFormat2ColorHue(color);
        for (const Format2HueRange& range : profile.hueRanges) if (Format2HueRangeContains(range, hue)) return range.value;
        return profile.defaultValue;
    }

    const std::uint32_t packed = ((std::uint32_t) color.r << 16) | ((std::uint32_t) color.g << 8) | (std::uint32_t) color.b;
    if (profile.match == Format2ColorMatch::Exact) {
        for (const Format2ColorProfileEntry& entry : profile.entries) if (entry.color == packed) return entry.value;
        return profile.defaultValue;
    }

    int selectedValue = profile.defaultValue;
    long long selectedDistance = (std::numeric_limits<long long>::max)();
    for (const Format2ColorProfileEntry& entry : profile.entries) {
        const rgba_color entryColor = UnpackFormat2Color(entry.color);
        const long long redDifference = color.r - entryColor.r;
        const long long greenDifference = color.g - entryColor.g;
        const long long blueDifference = color.b - entryColor.b;
        const long long distance = redDifference * redDifference + greenDifference * greenDifference + blueDifference * blueDifference;
        if (distance < selectedDistance) {
            selectedDistance = distance;
            selectedValue = entry.value;
        }
    }
    return selectedValue;
}
