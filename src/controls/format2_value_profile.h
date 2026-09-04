#pragma once

#include "format2_surface_document.h"

inline const Format2ValueProfile* FindFormat2ValueProfile(const Format2SurfaceDocument& document, const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* profileProperty = nullptr;
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == "ValueProfile") profileProperty = &property;
    if (!profileProperty || profileProperty->value.list) return nullptr;
    for (const Format2ValueProfile& profile : document.valueProfiles) if (profile.id == profileProperty->value.scalar.text) return &profile;
    return nullptr;
}

inline double InterpolateFormat2Value(double input, double firstInput, double firstOutput, double secondInput, double secondOutput) {
    if (secondInput == firstInput) return firstOutput;
    const double position = (input - firstInput) / (secondInput - firstInput);
    return firstOutput + position * (secondOutput - firstOutput);
}

inline double DecodeFormat2ValueProfile(const Format2ValueProfile& profile, double input) {
    if (profile.points.empty()) return input;
    double output = profile.points.front().output;
    if (input >= profile.points.back().input) output = profile.points.back().output;
    else if (input > profile.points.front().input) {
        for (std::size_t pointIdx = 1; pointIdx < profile.points.size(); ++pointIdx) {
            if (input > profile.points[pointIdx].input) continue;
            output = profile.interpolation == Format2Interpolation::Step ? profile.points[pointIdx - 1].output : InterpolateFormat2Value(input, profile.points[pointIdx - 1].input, profile.points[pointIdx - 1].output, profile.points[pointIdx].input, profile.points[pointIdx].output);
            break;
        }
    }
    return profile.outputUnit == Format2ValueUnit::Decibels ? volToNormalized(DB2VAL(output)) : output;
}

inline double EncodeFormat2ValueProfile(const Format2ValueProfile& profile, double input) {
    if (profile.points.empty()) return input;
    const double output = profile.outputUnit == Format2ValueUnit::Decibels ? VAL2DB(normalizedToVol(input)) : input;
    const bool increasing = profile.points.back().output > profile.points.front().output;
    if ((increasing && output <= profile.points.front().output) || (!increasing && output >= profile.points.front().output)) return profile.points.front().input;
    if ((increasing && output >= profile.points.back().output) || (!increasing && output <= profile.points.back().output)) return profile.points.back().input;
    for (std::size_t pointIdx = 1; pointIdx < profile.points.size(); ++pointIdx) {
        if ((increasing && output > profile.points[pointIdx].output) || (!increasing && output < profile.points[pointIdx].output)) continue;
        return InterpolateFormat2Value(output, profile.points[pointIdx - 1].output, profile.points[pointIdx - 1].input, profile.points[pointIdx].output, profile.points[pointIdx].input);
    }
    return profile.points.back().input;
}
