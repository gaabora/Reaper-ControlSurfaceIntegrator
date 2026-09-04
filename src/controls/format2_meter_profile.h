#pragma once

#include "format2_surface_document.h"

#include <algorithm>
#include <cmath>

inline const Format2MeterProfile* FindFormat2MeterProfile(const Format2SurfaceDocument& document, const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* profileProperty = nullptr;
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == "MeterProfile") profileProperty = &property;
    if (!profileProperty || profileProperty->value.list) return nullptr;
    for (const Format2MeterProfile& profile : document.meterProfiles) if (profile.id == profileProperty->value.scalar.text) return &profile;
    return nullptr;
}

inline int EncodeFormat2MeterProfile(const Format2MeterProfile& profile, double value) {
    const double input = profile.inputUnit == Format2MeterInputUnit::Decibels ? VAL2DB(normalizedToVol(value)) : value;
    if (profile.mode == Format2MeterMode::Steps) {
        int output = profile.defaultValue.value_or(0);
        for (const Format2MeterStep& step : profile.steps) {
            if (input < step.minimum) break;
            output = step.output;
        }
        return output;
    }
    if (!profile.inputRange || !profile.outputRange) return 0;
    const double minimum = (*profile.inputRange)[0];
    const double maximum = (*profile.inputRange)[1];
    const double normalized = std::clamp((input - minimum) / (maximum - minimum), 0.0, 1.0);
    const double mapped = (*profile.outputRange)[0] + normalized * ((*profile.outputRange)[1] - (*profile.outputRange)[0]);
    return profile.quantize == Format2Quantize::Round ? (int) std::round(mapped) : (int) std::floor(mapped);
}

inline int ClearFormat2MeterProfileValue(const Format2MeterProfile& profile) {
    if (profile.mode == Format2MeterMode::Steps) return profile.defaultValue.value_or(0);
    return profile.outputRange ? (*profile.outputRange)[0] : 0;
}
