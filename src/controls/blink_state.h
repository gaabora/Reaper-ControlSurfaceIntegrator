#pragma once
// blink_state.h — BlinkState: per-ActionContext blink configuration.
//  Stores whether blink is enabled and the desired interval.
//  The actual blink phase is computed globally on ControlSurface
//  (via IsBlinkLit) so that all buttons blink in sync.

struct BlinkState {
    // True when a blink interval has been configured for this context.
    bool blinkSet = false;

    // Blink interval in milliseconds. INHERIT_VALUE (-1) means use the surface default.
    int blinkIntervalMs = 0;
};
