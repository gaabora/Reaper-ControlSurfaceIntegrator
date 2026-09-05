#include "format2_gesture_validation.h"
#include "format2_action_metadata.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

static Format2GestureBinding Binding(ActionInputEvent event, const char* action, int line, int delayMs = 0, int repeatMs = 0) {
    return {{event, ActionModifierMode::Legacy, delayMs, repeatMs}, action, {0, line, 1}, action, false, repeatMs > 0, 1, Format2ActionChangesModifier(action)};
}

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

static bool HasWarning(const std::vector<Format2Diagnostic>& diagnostics, const char* code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [code](const Format2Diagnostic& diagnostic) { return diagnostic.severity == Format2DiagnosticSeverity::Warning && diagnostic.code == code; });
}

static void ExpectConflict(Format2GestureBinding first, Format2GestureBinding second, bool exclusive = true) {
    for (const auto& bindings : {std::vector<Format2GestureBinding>{first, second}, std::vector<Format2GestureBinding>{second, first}}) {
        const auto diagnostics = ValidateFormat2GestureBindings(bindings, 400, exclusive);
        Require(std::any_of(diagnostics.begin(), diagnostics.end(), [first](const Format2Diagnostic& diagnostic) { return diagnostic.severity == Format2DiagnosticSeverity::Error && diagnostic.location.line == first.location.line; }), "conflict links first source line regardless of order");
        Require(std::any_of(diagnostics.begin(), diagnostics.end(), [second](const Format2Diagnostic& diagnostic) { return diagnostic.severity == Format2DiagnosticSeverity::Error && diagnostic.location.line == second.location.line; }), "conflict links second source line regardless of order");
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

    Require(!HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::DoublePress, "Play", 5)}, 400, true)), "exclusive double suppresses context-changing tap");
    Require(!HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::Hold, "Play", 5, 1000)}, 400, true)), "hold suppresses context-changing tap");
    Require(!HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "GoHome", 4, 401), Binding(ActionInputEvent::DoublePress, "Play", 5)}, 400, true)), "late hold with double allowed");
    Require(!ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "GoHome", 4, 1000, 100)}, 400, true).empty(), "context-changing repeat rejected");
    Require(!ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "GoHome", 4), Binding(ActionInputEvent::Tap, "Play", 5)}, 400, true).empty(), "action after context change rejected");
    Require(!HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "Play", 5), Binding(ActionInputEvent::Tap, "GoHome", 4)}, 400, true)), "context change last in macro allowed");
    Require(!HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Press, "Reaper", 4), Binding(ActionInputEvent::Hold, "Play", 5, 1000)}, 400, true)), "opaque Reaper action has no inferred context effect");
    Require(!ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "Play", 4), Binding(ActionInputEvent::Tap, "Play", 5)}, 400, true).empty(), "exact duplicate rejected");
    Format2GestureBinding differentParameter = Binding(ActionInputEvent::Tap, "Play", 5);
    differentParameter.actionIdentity = std::string("Play") + '\x1f' + "B2";
    Require(!HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "Play", 4), differentParameter}, 400, true)), "different parameters are not exact duplicates");
    Require(!ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "NoAction", 4, 1000), Binding(ActionInputEvent::Hold, "Play", 5, 1000)}, 400, true).empty(), "NoAction with another action rejected");
    Format2GestureBinding invalidDelay = Binding(ActionInputEvent::Tap, "Play", 4);
    invalidDelay.delaySpecified = true;
    Require(!ValidateFormat2GestureBindings({invalidDelay}, 400, true).empty(), "DelayMs outside hold rejected");
    Format2GestureBinding invalidRepeat = Binding(ActionInputEvent::Press, "Play", 4);
    invalidRepeat.repeatSpecified = true;
    Require(!ValidateFormat2GestureBindings({invalidRepeat}, 400, true).empty(), "RepeatIntervalMs outside hold rejected");
    Format2GestureBinding repeatedContextChange = Binding(ActionInputEvent::Hold, "GoHome", 4, 1000);
    repeatedContextChange.repeatSpecified = true;
    repeatedContextChange.gesture.repeatIntervalMs = 100;
    Require(!ValidateFormat2GestureBindings({repeatedContextChange}, 400, true).empty(), "context-changing Hold repeat rejected");
    Format2GestureBinding countedContextChange = Binding(ActionInputEvent::Tap, "GoHome", 4);
    countedContextChange.runCount = 2;
    Require(!ValidateFormat2GestureBindings({countedContextChange}, 400, true).empty(), "context-changing RunCount rejected");
    Format2GestureBinding zeroRepeat = Binding(ActionInputEvent::Hold, "Play", 4, 1000);
    zeroRepeat.repeatSpecified = true;
    Require(HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({zeroRepeat}, 400, true)), "non-positive repeat rejected");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Press, "Play", 4), Binding(ActionInputEvent::Hold, "Stop", 5, 1000)}, 400, true), "format2.zone.gesture.additive"), "additive Press and Hold warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Tap, "Play", 4), Binding(ActionInputEvent::DoublePress, "Stop", 5)}, 400, true), "format2.zone.gesture.additive"), "delayed Tap warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "Play", 4, 1000), Binding(ActionInputEvent::LongHold, "Stop", 5, 2000)}, 400, true), "format2.zone.gesture.additive"), "Hold and LongHold warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Hold, "Play", 4, 1000), Binding(ActionInputEvent::Release, "Stop", 5)}, 400, true), "format2.zone.gesture.additive"), "release after Hold warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Press, "Play", 4), Binding(ActionInputEvent::DoublePress, "Stop", 5)}, 400, true), "format2.zone.gesture.additive"), "DoublePress with Press warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Modifier, "Shift", 4), Binding(ActionInputEvent::DoublePress, "Play", 5)}, 400, true), "format2.zone.gesture.additive"), "modifier source with DoublePress warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Press, "Shift", 4), Binding(ActionInputEvent::Press, "Play", 5)}, 400, true), "format2.zone.gesture.additive"), "modifier and normal action warning");
    Require(HasWarning(ValidateFormat2GestureBindings({Binding(ActionInputEvent::Press, "Shift", 4), Binding(ActionInputEvent::Press, "Option", 5)}, 400, true), "format2.zone.gesture.additive"), "multiple modifier actions warning");
    Format2GestureBinding countedModifier = Binding(ActionInputEvent::Press, "Shift", 4);
    countedModifier.runCount = 2;
    Require(HasFormat2DiagnosticErrors(ValidateFormat2GestureBindings({countedModifier}, 400, true)), "modifier RunCount rejected");
    std::cout << "Gesture validation tests passed\n";
}
