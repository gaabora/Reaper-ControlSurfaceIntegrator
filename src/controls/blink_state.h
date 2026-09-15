#pragma once
// blink_state.h — BlinkState: per-ActionContext blink configuration.
//  Stores the blink condition, animation preset, and migration interval.
//  ControlSurface computes shared animation phases so matching buttons stay in sync.

enum class BlinkCondition {
    Off,
    Active,
    Inactive,
    Always,
};

enum class AnimationPreset {
    BlinkFast,
    BlinkNormal,
    BlinkSlow,
    BlinkDouble,
    BlinkHeartbeat,
};

struct BlinkState {
    // True when a blink mode has been configured for this context.
    bool blinkSet = false;

    BlinkCondition condition = BlinkCondition::Off;
    AnimationPreset animation = AnimationPreset::BlinkNormal;

    // Legacy per-binding interval retained only while old format 2 input is migrated.
    int blinkIntervalMs = 0;
};
