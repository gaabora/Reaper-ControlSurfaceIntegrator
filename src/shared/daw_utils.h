#pragma once
//
//  daw_utils.h — DAW namespace: numeric utility helpers (rounding, fader comparison).
//
//  Part of the Phase 7 decomposition of the DAW class.
//  Included by daw_api.h — do not include directly.
//

#include <cmath>

namespace DAW
{
    // Compare two fader-range normalized values with a given decimal precision.
    // Default precision of 3 decimals avoids noise from floating-point round-trips.
    inline bool CompareFaderValues(double a, double b, int decimals = 3)
    {
        double tolerance = std::pow(10.0, -decimals);
        return std::fabs(a - b) < tolerance;
    }

    // Round a double to a given number of decimal places.
    inline double RoundDouble(double value, int decimals = 6)
    {
        double multiplier = std::pow(10.0, decimals);
        return std::round(value * multiplier) / multiplier;
    }

} // namespace DAW
