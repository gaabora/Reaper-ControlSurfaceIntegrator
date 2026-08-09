#pragma once
// daw_fx.h — DAW namespace: FX name, parameter, and plugin-type queries.
// Included by daw_api.h (via daw_tracks.h) — do not include directly.

#include "reaper_plugin_functions.h"
#include "utils.h"

#include <string>
#include <cstring>
using std::string;

namespace DAW {
    // Strip vendor prefix ("Vendor: Name") and trailing parenthetical ("Name (v1.0)")
    // from a full FX name, returning just the short display name.
    inline std::string GetShortFXName(const char* fullName) {
        std::string name(fullName);

        size_t colonPos = name.find(": ");
        if (colonPos != std::string::npos)
            name = name.substr(colonPos + 2);

        size_t parenPos = name.find(" (");
        if (parenPos != std::string::npos)
            name = name.substr(0, parenPos);

        return name;
    }

    // Returns "[ShortFXName] ParamName" for the given track FX parameter.
    inline std::string GetFxParamDescription(MediaTrack* track, int fxSlotNum, int fxParamNum) {
        if (!track) return "";
        char fxName[256] = "";
        char paramName[128] = "";
        TrackFX_GetFXName(track, fxSlotNum, fxName, sizeof(fxName));
        TrackFX_GetParamName(track, fxSlotNum, fxParamNum, paramName, sizeof(paramName));
        string fxShortName = DAW::GetShortFXName(fxName);
        return "[" + fxShortName + "] " + paramName;
    }

    // Returns the formatted (human-readable) value string for an FX parameter.
    inline std::string GetFxParamValue(MediaTrack* track, int fxSlotNum, int fxParamNum) {
        if (!track) return "";
        char paramValue[128] = "";
        TrackFX_GetFormattedParamValue(track, fxSlotNum, fxParamNum, paramValue, sizeof(paramValue));
        return string(paramValue);
    }

    // Returns the step size of an FX parameter (1.0 if not available).
    inline double GetTrackFxParamStepSize(MediaTrack* track, int fxSlotNum, int fxParamNum) {
        double stepSize = 1.0, smallstep, largestep;
        bool isToggle;
        TrackFX_GetParameterStepSizes(track, fxSlotNum, fxParamNum, &stepSize, &smallstep, &largestep, &isToggle);
        return stepSize;
    }

    // Returns true when the FX parameter uses a volume curve (step=1, min=0, max=2).
    // Optionally returns the raw value, min, max, mid, and step values.
    inline bool CheckTrackFxParamHasVolumeCurve(MediaTrack* track, int fxSlotNum, int fxParamNum
        , double* valueOut = nullptr, double* minOut = nullptr, double* maxOut = nullptr, double* midOut = nullptr, double* stepSizeOut = nullptr
    ) {
        double min = 0.0, max = 0.0, mid = 0.0;
        double value = TrackFX_GetParamEx(track, fxSlotNum, fxParamNum, &min, &max, &mid);
        double stepSize = DAW::GetTrackFxParamStepSize(track, fxSlotNum, fxParamNum);
        if (valueOut)    *valueOut    = value;
        if (minOut)      *minOut      = min;
        if (maxOut)      *maxOut      = max;
        if (midOut)      *midOut      = mid;
        if (stepSizeOut) *stepSizeOut = stepSize;
        return (stepSize == 1.0 && min == 0.0 && max == 2.0);
    }

    // Returns the normalized [0,1] value of an FX parameter, handling volume-curve params.
    inline double GetTrackFxParamValue(MediaTrack* track, int fxSlotNum, int fxParamNum) {
        double value = 0.0, rawValue = 0.0, min = 0.0, max = 0.0;
        if (CheckTrackFxParamHasVolumeCurve(track, fxSlotNum, fxParamNum, &rawValue, &min, &max)) {
            value = (rawValue == 0.0) ? 0.0 : volToNormalized(rawValue);
        } else {
#if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
            value = TrackFX_GetParamNormalized(track, fxSlotNum, fxParamNum);
#else
            double range = max - min;
            value = (range > 0.0) ? ((rawValue - min) / range) : 0.0;
#endif
        }
        return value;
    }

    // Sets an FX parameter from a normalized [0,1] value, handling volume-curve params.
    inline void SetTrackFxParamValue(MediaTrack* track, int fxSlotNum, int fxParamNum, double value) {
        double rawValue = 0.0, min = 0.0, max = 0.0, mid = 0.0, stepSize = 0.0;
        if (CheckTrackFxParamHasVolumeCurve(track, fxSlotNum, fxParamNum, &rawValue, &min, &max, &mid, &stepSize)) {
            TrackFX_SetParam(track, fxSlotNum, fxParamNum, normalizedToVol(value));
        } else {
#if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
            TrackFX_SetParamNormalized(track, fxSlotNum, fxParamNum, value);
#else
            TrackFX_SetParam(track, fxSlotNum, fxParamNum, min + value * (max - min));
#endif
        }
    }

    // Returns the full FX name string (as reported by REAPER) for a given FX slot.
    inline string GetTrackFxName(MediaTrack* track, int fxIdx) {
        char fxName[256];
        TrackFX_GetFXName(track, fxIdx, fxName, sizeof(fxName));
        return string(fxName);
    }

    // Returns true when the FX at fxIdx is a virtual instrument (VSTi/VST3i/AUi/CLAPi/JSi).
    inline bool IsFxInstrument(MediaTrack* track, int fxIdx) {
        char fxType[128] = {};
        if (TrackFX_GetNamedConfigParm(track, fxIdx, "fx_type", fxType, sizeof(fxType))) {
            return strncmp(fxType, "VSTi", 4) == 0
                || strncmp(fxType, "VST3i", 5) == 0
                || strncmp(fxType, "AUi", 3) == 0
                || strncmp(fxType, "CLAPi", 5) == 0
                || (strncmp(fxType, "JS", 2) == 0 && strstr(fxType, ":i") != nullptr);
        }
        return false;
    }

} // namespace DAW
