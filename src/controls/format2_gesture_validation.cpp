#include "format2_gesture_validation.h"
#include "format2_action_metadata.h"

static bool IsHoldEvent(ActionInputEvent event) {
    return event == ActionInputEvent::Hold || event == ActionInputEvent::LongHold;
}

std::vector<Format2Diagnostic> ValidateFormat2GestureBindings(const std::vector<Format2GestureBinding>& bindings, int doublePressWindowMs, bool exclusiveDoublePress) {
    std::vector<Format2Diagnostic> diagnostics;
    for (std::size_t bindingIdx = 0; bindingIdx < bindings.size(); bindingIdx++) {
        const Format2GestureBinding& binding = bindings[bindingIdx];
        if (!Format2ActionChangesContext(binding.actionName)) continue;
        const ActionInputEvent event = binding.gesture.inputEvent;
        if (binding.gesture.repeatIntervalMs > 0) diagnostics.push_back({"format2.zone.gesture.context-repeat", "A context-changing action cannot repeat: " + binding.actionName, binding.location});
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
            diagnostics.push_back({"format2.zone.gesture.unreachable", reason + "; related binding at line " + std::to_string(other.location.line), binding.location});
            diagnostics.push_back({"format2.zone.gesture.unreachable", reason + "; related binding at line " + std::to_string(binding.location.line), other.location});
        }
    }
    return diagnostics;
}
