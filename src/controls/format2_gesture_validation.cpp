#include "format2_gesture_validation.h"
#include "format2_action_metadata.h"

static bool IsHoldEvent(ActionInputEvent event) {
    return event == ActionInputEvent::Hold || event == ActionInputEvent::LongHold;
}

static void AddRelatedDiagnostics(std::vector<Format2Diagnostic>& diagnostics, const std::string& code, const std::string& reason, const Format2GestureBinding& first, const Format2GestureBinding& second, Format2DiagnosticSeverity severity = Format2DiagnosticSeverity::Error) {
    diagnostics.push_back({code, reason + "; related binding at line " + std::to_string(second.location.line), first.location, severity});
    diagnostics.push_back({code, reason + "; related binding at line " + std::to_string(first.location.line), second.location, severity});
}

std::vector<Format2Diagnostic> ValidateFormat2GestureBindings(const std::vector<Format2GestureBinding>& bindings, int doublePressWindowMs, bool exclusiveDoublePress) {
    std::vector<Format2Diagnostic> diagnostics;
    for (std::size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++) {
        const Format2GestureBinding& binding = bindings[bindingIdx];
        const ActionInputEvent event = binding.gesture.inputEvent;
        if (binding.delaySpecified && !IsHoldEvent(event)) diagnostics.push_back({"format2.zone.gesture.delay-event", "DelayMs is valid only for Hold and LongHold", binding.location});
        if (binding.repeatSpecified && !IsHoldEvent(event)) diagnostics.push_back({"format2.zone.gesture.repeat-event", "RepeatIntervalMs is valid only for Hold and LongHold", binding.location});
        else if (binding.repeatSpecified && binding.gesture.repeatIntervalMs <= 0) diagnostics.push_back({"format2.zone.gesture.repeat-value", "RepeatIntervalMs must be a positive integer", binding.location});
        if (binding.changesModifier && binding.runCount > 1) diagnostics.push_back({"format2.zone.gesture.modifier-run-count", "A modifier action cannot use RunCount greater than one: " + binding.actionName, binding.location});
        for (std::size_t otherIdx = bindingIdx + 1; otherIdx < bindings.size(); otherIdx++) {
            const Format2GestureBinding& other = bindings[otherIdx];
            if (event != other.gesture.inputEvent) continue;
            if (!binding.actionIdentity.empty() && binding.actionIdentity == other.actionIdentity) {
                AddRelatedDiagnostics(diagnostics, "format2.zone.gesture.action.duplicate", "The event group contains the same action, parameters, and properties more than once", binding, other);
            } else if (binding.actionName == "NoAction" || other.actionName == "NoAction") {
                AddRelatedDiagnostics(diagnostics, "format2.zone.gesture.no-action", "NoAction must be the only action in an event group", binding, other);
            }
        }
        if (!Format2ActionChangesContext(binding.actionName)) continue;
        if (binding.repeatSpecified && IsHoldEvent(event)) diagnostics.push_back({"format2.zone.gesture.context-repeat", "A context-changing action cannot repeat: " + binding.actionName, binding.location});
        if (binding.runCount > 1) diagnostics.push_back({"format2.zone.gesture.context-run-count", "A context-changing action cannot use RunCount greater than one: " + binding.actionName, binding.location});
        for (std::size_t otherIdx = 0; otherIdx < bindings.size(); otherIdx++) {
            if (otherIdx == bindingIdx) continue;
            const Format2GestureBinding& other = bindings[otherIdx];
            const ActionInputEvent otherEvent = other.gesture.inputEvent;
            if (otherEvent == ActionInputEvent::Legacy || otherEvent == ActionInputEvent::Modifier) continue;
            std::string reason;
            if (event == otherEvent) {
                if (Format2ActionChangesContext(other.actionName)) {
                    if (otherIdx < bindingIdx) continue;
                    reason = "One event group cannot contain two context-changing actions";
                } else if (otherIdx > bindingIdx) reason = "An action cannot follow a context-changing action in the same event group";
            } else if (event == ActionInputEvent::Press) reason = "Press changes context before the other button event can complete";
            else if (event == ActionInputEvent::Release && otherEvent == ActionInputEvent::DoublePress) reason = "Release changes context before DoublePress can complete";
            else if (event == ActionInputEvent::Tap && otherEvent == ActionInputEvent::DoublePress && !exclusiveDoublePress) reason = "Tap changes context before additive DoublePress can complete";
            else if (IsHoldEvent(event) && IsHoldEvent(otherEvent) && binding.gesture.delayMs <= other.gesture.delayMs) reason = "An earlier hold event changes context before the later hold event can complete";
            else if (IsHoldEvent(event) && otherEvent == ActionInputEvent::DoublePress && binding.gesture.delayMs <= doublePressWindowMs) reason = "A context-changing hold must have DelayMs greater than DoublePressWindowMs";
            if (reason.empty()) continue;
            AddRelatedDiagnostics(diagnostics, "format2.zone.gesture.unreachable", reason, binding, other);
        }
    }
    for (std::size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++) {
        const Format2GestureBinding& binding = bindings[bindingIdx];
        for (std::size_t otherIdx = bindingIdx + 1; otherIdx < bindings.size(); otherIdx++) {
            const Format2GestureBinding& other = bindings[otherIdx];
            const ActionInputEvent event = binding.gesture.inputEvent;
            const ActionInputEvent otherEvent = other.gesture.inputEvent;
            std::string reason;
            if (binding.changesModifier && other.changesModifier) reason = "More than one action changes modifier state on the same physical Widget";
            else if (binding.changesModifier || other.changesModifier) reason = "A modifier action and a normal action share the same physical Widget";
            else if (event == ActionInputEvent::Press && IsHoldEvent(otherEvent) && !Format2ActionChangesContext(binding.actionName)) reason = "Press and hold actions are additive";
            else if (otherEvent == ActionInputEvent::Press && IsHoldEvent(event) && !Format2ActionChangesContext(other.actionName)) reason = "Press and hold actions are additive";
            else if (exclusiveDoublePress && ((event == ActionInputEvent::Tap && otherEvent == ActionInputEvent::DoublePress) || (otherEvent == ActionInputEvent::Tap && event == ActionInputEvent::DoublePress))) reason = "Tap is delayed until the exclusive DoublePress window expires";
            else if (IsHoldEvent(event) && IsHoldEvent(otherEvent) && event != otherEvent) {
                const bool bindingBlocksOther = Format2ActionChangesContext(binding.actionName) && binding.gesture.delayMs <= other.gesture.delayMs;
                const bool otherBlocksBinding = Format2ActionChangesContext(other.actionName) && other.gesture.delayMs <= binding.gesture.delayMs;
                if (!bindingBlocksOther && !otherBlocksBinding) reason = "LongHold runs after Hold when both are declared";
            }
            else if ((event == ActionInputEvent::Release && IsHoldEvent(otherEvent)) || (otherEvent == ActionInputEvent::Release && IsHoldEvent(event))) reason = "Release also runs after a Hold or LongHold milestone";
            else if (event == ActionInputEvent::DoublePress && otherEvent == ActionInputEvent::Press && !Format2ActionChangesContext(other.actionName)) reason = "DoublePress is additive with Press";
            else if (otherEvent == ActionInputEvent::DoublePress && event == ActionInputEvent::Press && !Format2ActionChangesContext(binding.actionName)) reason = "DoublePress is additive with Press";
            else if ((event == ActionInputEvent::DoublePress && otherEvent == ActionInputEvent::Release) || (otherEvent == ActionInputEvent::DoublePress && event == ActionInputEvent::Release)) reason = "DoublePress is additive with Release";
            if (!reason.empty()) AddRelatedDiagnostics(diagnostics, "format2.zone.gesture.additive", reason, binding, other, Format2DiagnosticSeverity::Warning);
        }
    }
    return diagnostics;
}
