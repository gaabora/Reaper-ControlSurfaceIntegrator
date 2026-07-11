#pragma once
// daw_display.h — DAW namespace: on-screen display (OSD) interface.
// ShowOSD is defined in daw_display.cpp so the function-local static variables (lastValue, lastUpdateTs) are true per-process singletons rather than per-translation-unit copies that would result from an inline header definition
// Included by daw_api.h — do not include directly.

#include "types.h"

namespace DAW {
    // Show an on-screen display message as an ExtState.
    // Rate-limited: repeated identical values are suppressed within timeoutMs.
    void ShowOSD(const osd_data& osdData);

} // namespace DAW
