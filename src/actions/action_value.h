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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ActionValueState
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    // -------------------------------------------------------------------
    // Encoder / delta parameters
    // -------------------------------------------------------------------

    // Fixed delta per encoder tick (0.0 = use raw encoder delta).
    double deltaValue = 0.0;

    // -------------------------------------------------------------------
    // Range clamping
    // -------------------------------------------------------------------

    double rangeMinimum = 0.0;
    double rangeMaximum = 1.0;

    // -------------------------------------------------------------------
    // Stepped values (button cycling / encoder snapping)
    // -------------------------------------------------------------------

    vector<double> steppedValues;
    int            steppedValuesIndex = 0;

    // -------------------------------------------------------------------
    // Acceleration
    // -------------------------------------------------------------------

    // Per-tick delta values indexed by acceleration level.
    vector<double> acceleratedDeltaValues;

    // Tick-count thresholds per acceleration level (for stepped acceleration).
    vector<int>    acceleratedTickValues;

    // Accumulated tick counters for accelerated stepped-value actions.
    int accumulatedIncTicks = 0;
    int accumulatedDecTicks = 0;

    // -------------------------------------------------------------------
    // Inversion flags
    // -------------------------------------------------------------------

    // Invert the value before it is sent to action_->Do().
    bool isValueInverted    = false;

    // Invert feedback: flip the displayed value (e.g. button lit = off).
    bool isFeedbackInverted = false;
};
