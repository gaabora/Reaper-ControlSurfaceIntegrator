#include "format2_gesture_validation.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

static Format2GestureBinding Binding(ActionInputEvent event, const char* action, int line, int delayMs = 0, int repeatMs = 0) {
    return {{event, ActionModifierMode::Legacy, delayMs, repeatMs}, action, {0, line, 1}};
}

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

static void ExpectConflict(Format2GestureBinding first, Format2GestureBinding second, bool exclusive = true) {
    for (const auto& bindings : {std::vector<Format2GestureBinding>{first, second}, std::vector<Format2GestureBinding>{second, first}}) {
        const auto diagnostics = ValidateFormat2GestureBindings(bindings, 400, exclusive);
        Require(std::any_of(diagnostics.begin(), diagnostics.end(), [first](const Format2Diagnostic& diagnostic) { return diagnostic.location.line == first.location.line; }), "conflict links first source line regardless of order");
        Require(std::any_of(diagnostics.begin(), diagnostics.end(), [second](const Format2Diagnostic& diagnostic) { return diagnostic.location.line == second.location.line; }), "conflict links second source line regardless of order");
    }
}

int main() {
    for (ActionInputEvent event : {ActionInputEvent::Tap, ActionInputEvent::Release, ActionInputEvent::Hold, ActionInputEvent::LongHold, ActionInputEvent::DoublePress}) {
        ExpectConflict(Binding(ActionInputEvent::Press, "GoHome", 2), Binding(event, "Play", 3, 1000));
    }
    ExpectConflict(Binding(ActionInputEvent::Release, "GoZone", 4), Binding(ActionInputEvent::DoublePress, "Play", 5));
    ExpectConflict(Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::DoublePress, "Play", 5), false);
    ExpectConflict(Binding(ActionInputEvent::Hold, "GoHome", 4, 1000), Binding(ActionInputEvent::LongHold, "Play", 5, 2000));
    ExpectConflict(Binding(ActionInputEvent::LongHold, "GoHome", 4, 500), Binding(ActionInputEvent::Hold, "Play", 5, 1000));
    ExpectConflict(Binding(ActionInputEvent::Hold, "GoHome", 4, 400), Binding(ActionInputEvent::DoublePress, "Play", 5));
    ExpectConflict(Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::Tap, "NextPage", 5));

    Require(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::DoublePress, "Play", 5)}, 400, true).empty(), "exclusive double suppresses context-changing tap");
    Require(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::Hold, "Play", 5, 1000)}, 400, true).empty(), "hold suppresses context-changing tap");
    Require(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "GoHome", 4, 401), Binding(ActionInputEvent::DoublePress, "Play", 5)}, 400, true).empty(), "late hold with double allowed");
    Require(!ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "GoHome", 4, 1000, 100)}, 400, true).empty(), "context-changing repeat rejected");
    Require(!ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::Tap, "Play", 5)}, 400, true).empty(), "action after context change rejected");
    Require(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "Play", 5), Binding(ActionInputEvent::Tap, "GoHome", 4)}, 400, true).empty(), "context change last in macro allowed");
    Require(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Press, "Reaper", 4), Binding(ActionInputEvent::Hold, "Play", 5, 1000)}, 400, true).empty(), "opaque Reaper action has no inferred context effect");
    std::cout << "Gesture validation tests passed\n";
}
