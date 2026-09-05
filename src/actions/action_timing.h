#pragma once
//
//  action_timing.h — ActionTiming: hold, repeat, and double-press timing state for ActionContext.
//
//  Extracted from ActionContext in Phase 6 of the refactoring plan.
//  ActionContext owns an ActionTiming timing_ member and delegates all timing-related
//  state to it. The orchestration methods (DoAction, RunDeferredActions) remain on
//  ActionContext since they need to call back into PerformAction() and GetSurface().
//
#include "../controls/preamble.h"
#include "action_input_event.h"

struct ActionTiming {
    // Hold-delay: action is deferred until button is held for holdDelayMs.
    // Use INHERIT_VALUE (-1) to inherit the surface-level default.
    int holdDelayMs = 0;

    // Hold-repeat: after the hold fires, repeat at this interval while still held.
    // Use INHERIT_VALUE (-1) to inherit the surface-level default.
    int holdRepeatIntervalMs = 0;

    DWORD lastHoldRepeatTs = 0;
    DWORD lastHoldStartTs = 0;
    bool holdActive = false;
    bool holdRepeatActive = false;

    // Value cached when the button was first pressed; used when the deferred action fires.
    double deferredValue = 0.0;

    // Double-press detection: this context fires only on double-press.
    bool isDoublePress = false;
    DWORD doublePressStartTs = 0;

    // Format 2 button event selection. One Zone-level recognizer owns its timing state.
    ActionInputEvent inputEvent = ActionInputEvent::Legacy;
};
