#pragma once
// action_color.h — ActionColorState: color and track-color state component for ActionContext
// ActionContext owns an ActionColorState color_ member.
// ParseColors()    — parses { R G B … } or { #RRGGBB … } or { Track } param blocks
// Methods that need Widget* or Zone* (UpdateColorValue, UpdateTrackColor) remain on ActionContext but access their state via color_.

#include "../controls/preamble.h"
#include "../shared/types.h"
#include "../shared/utils.h"

struct ActionColorState {
    // true when one or more explicit RGB / hex color values were provided.
    bool supportsColor = false;

    // true when the Track keyword was supplied — uses the REAPER track color.
    bool supportsTrackColor = false;

    // Resolved color values (0, 1, or N entries).
    vector<rgba_color> colorValues;

    // Index of the currently active color (toggled by UpdateColorValue).
    int currentColorIndex = 0;

    // Parse color parameters from an action param list and populate this state.
    // Looks for a { … } block containing R G B triplets, #RRGGBB hex strings, or "Track".
    void ParseColors(const vector<string>& params);

private:
    // Convert a list of hex-color strings (e.g. "#FF8000") to rgba_color values.
    void GetColorValues(vector<rgba_color>& result, const vector<string>& colors);
};

// ---------------------------------------------------------------------------
// Inline implementations (header-only — no separate .cpp needed)
// ---------------------------------------------------------------------------

inline void ActionColorState::GetColorValues(vector<rgba_color>& result, const vector<string>& colors) {
    for (int i = 0; i < (int) colors.size(); ++i) {
        rgba_color colorValue;
        if (GetColorValue(colors[i].c_str(), colorValue))
            result.push_back(colorValue);
    }
}

inline void ActionColorState::ParseColors(const vector<string>& params) {
    vector<int> rawValues;
    vector<string> hexColors;

    int openCurlyIndex = 0;
    int closeCurlyIndex = 0;

    for (int i = 0; i < (int) params.size(); ++i)
        if (params[i] == "{") {
            openCurlyIndex = i;
            break;
        }
    for (int i = 0; i < (int) params.size(); ++i)
        if (params[i] == "}") {
            closeCurlyIndex = i;
            break;
        }
    if (openCurlyIndex == 0 || closeCurlyIndex == 0) return;

    for (int i = openCurlyIndex + 1; i < closeCurlyIndex; ++i) {
        const string& strVal = params[i];
        if (strVal.empty()) continue;
        if (strVal[0] == '#') {
            hexColors.push_back(strVal);
            continue;
        }
        if (strVal == "Track") {
            supportsTrackColor = true;
            return;
        }
        char* ep = nullptr;
        const int value = strtol(strVal.c_str(), &ep, 10);
        if (ep && !*ep)
            rawValues.push_back(wdl_clamp(value, 0, 255));
    }

    if (!hexColors.empty()) {
        supportsColor = true;
        GetColorValues(colorValues, hexColors);
    } else if (rawValues.size() % 3 == 0 && rawValues.size() > 2) {
        supportsColor = true;
        for (int i = 0; i < (int) rawValues.size(); i += 3) {
            rgba_color color;
            color.r = rawValues[i];
            color.g = rawValues[i + 1];
            color.b = rawValues[i + 2];
            colorValues.push_back(color);
        }
    }
}
