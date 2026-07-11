#pragma once
// blink_state.h — BlinkState: LED/display blink state component for ActionContext.
//  ActionContext owns a BlinkState blink_ member.
//  GetBlinkInterval() stays on ActionContext (needs GetSurface() for INHERIT_VALUE).

#include "preamble.h"

struct BlinkState {
    // blinkSet: true when a blink interval has been configured for this context.
    bool blinkSet = false;

    // Current visible state: true = "lit" (normal value), false = "dim" (flipped).
    bool blinkActive = true;

    // Blink interval in milliseconds. INHERIT_VALUE (-1) means use the surface default.
    int blinkIntervalMs = 0;

    // Timestamp of the last blink toggle.
    DWORD lastBlinkTs = 0;

    // Toggle the blink state if resolvedIntervalMs has elapsed since the last toggle.
    // Returns the current blinkActive value (post-toggle if the interval fired).
    // The caller is responsible for resolving INHERIT_VALUE before calling this.
    bool Update(int resolvedIntervalMs) {
        DWORD now = GetTickCount();
        if (now > lastBlinkTs + static_cast<DWORD>(resolvedIntervalMs)) {
            blinkActive = !blinkActive;
            lastBlinkTs = now;
        }
        return blinkActive;
    }
};
