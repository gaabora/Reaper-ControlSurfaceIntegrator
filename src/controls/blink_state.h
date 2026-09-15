#pragma once
#include "animation_registry.h"
#include <string>
// blink_state.h — BlinkState: per-ActionContext blink configuration.
//  Stores the blink condition, animation preset, and migration interval.
//  ControlSurface computes shared animation phases so matching buttons stay in sync.

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
    std::string animation = "BlinkNormal";

    // Legacy per-binding interval retained only while old format 2 input is migrated.
    int blinkIntervalMs = 0;
};
