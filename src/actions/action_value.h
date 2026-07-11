#pragma once
//
//  action_value.h — ActionValueState: range, stepped-value, and acceleration state for ActionContext.
//
//  Extracted from ActionContext in Phase 6 of the refactoring plan.
//  ActionContext owns an ActionValueState value_ member.
//
//  The Do* action methods (DoRangeBoundAction, DoSteppedValueAction, etc.) remain on
//  ActionContext because they call action_->Do(this, …) and therefore need the full
//  ActionContext pointer.  They simply read/write their state through value_.*.
//
#include "../controls/preamble.h"

struct ActionValueState {
    // Encoder / delta parameters
    double deltaValue = 0.0; // Fixed delta per encoder tick (0.0 = use raw encoder delta).

    // Range clamping
    double rangeMinimum = 0.0;
    double rangeMaximum = 1.0;

    // Stepped values (button cycling / encoder snapping)
    vector<double> steppedValues;
    int steppedValuesIndex = 0;

    // Acceleration
    vector<double> acceleratedDeltaValues; // Per-tick delta values indexed by acceleration level.

    vector<int> acceleratedTickValues; // Tick-count thresholds per acceleration level (for stepped acceleration).

    // Accumulated tick counters for accelerated stepped-value actions.
    int accumulatedIncTicks = 0;
    int accumulatedDecTicks = 0;

    // Inversion flags

    bool isValueInverted = false; // Invert the value before it is sent to action_->Do().

    bool isFeedbackInverted = false; // Invert feedback: flip the displayed value (e.g. button lit = off).
};
