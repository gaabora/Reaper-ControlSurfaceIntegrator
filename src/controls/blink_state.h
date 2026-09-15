#pragma once
// blink_state.h — BlinkState: per-ActionContext blink configuration.
//  Stores the blink condition and interval.
//  The actual blink phase is computed globally on ControlSurface
//  (via IsBlinkLit) so that all buttons blink in sync.

enum class BlinkCondition {
    Off,
    Active,
    Inactive,
    Always,
};

struct BlinkState {
    // True when a blink mode has been configured for this context.
    bool blinkSet = false;

    BlinkCondition condition = BlinkCondition::Off;

    // Blink interval in milliseconds. INHERIT_VALUE (-1) means use the surface default.
    int blinkIntervalMs = 0;
};
