#pragma once
// daw_display.h - DAW namespace: on-screen display (OSD) interface.
// ShowOSD is defined in daw_display.cpp so its event id is a true process-wide singleton.
// Included by daw_api.h - do not include directly.

#include "types.h"

namespace DAW {
    // Show an on-screen display message as an ExtState.
    // Every call publishes a new event id, including repeated identical payloads.
    void ShowOSD(const osd_data& osdData);

} // namespace DAW
