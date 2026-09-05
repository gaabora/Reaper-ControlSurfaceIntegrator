#pragma once

#include "../actions/button_gesture.h"
#include "format2_lexer.h"

struct Format2GestureBinding {
    ButtonGestureBinding gesture;
    std::string actionName;
    Format2SourceLocation location;
};

// Call once per resolved physical Widget and normalized modifier context.
std::vector<Format2Diagnostic> ValidateFormat2GestureBindings(const std::vector<Format2GestureBinding>& bindings, int doublePressWindowMs, bool exclusiveDoublePress);
