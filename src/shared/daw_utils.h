#pragma once
// daw_utils.h — DAW namespace: numeric utility helpers (rounding, fader comparison).
// Included by daw_api.h — do not include directly.

#include <cmath>

namespace DAW {
    inline bool CompareFaderValues(double a, double b, int decimals = 3) {
        double tolerance = std::pow(10.0, -decimals);
        return std::fabs(a - b) < tolerance;
    }

    inline double RoundDouble(double value, int decimals = 6) {
        double multiplier = std::pow(10.0, decimals);
        return std::round(value * multiplier) / multiplier;
    }

} // namespace DAW
