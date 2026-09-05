import type { ActionTraits } from "./action-catalog.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import { settingDefinition, type SettingsSchema } from "./settings-schema.ts";
import type { ZoneBinding } from "./zone.ts";

function eventName(binding: ZoneBinding): string {
    const buttonEvent = binding.inputSelectors.find((selector) => ["Press", "Tap", "Release", "Hold", "LongHold", "DoublePress"].includes(selector));
    if (buttonEvent) return buttonEvent;
    return binding.inputSelectors.length ? `Other:${[...binding.inputSelectors].sort().join("+")}` : "Default";
}

function isHoldEvent(event: string): boolean {
    return event === "Hold" || event === "LongHold";
}

function actionIdentity(binding: ZoneBinding): string {
    const properties = [...binding.properties].sort(([left], [right]) => left.localeCompare(right));
    return JSON.stringify([binding.action, binding.params, properties]);
}

function addRelatedDiagnostics(diagnostics: Diagnostic[], severity: "error" | "warning", code: string, message: string, first: ZoneBinding, second: ZoneBinding, documentPath?: string): void {
    const firstRelated = documentPath ? [{ line: second.line, path: documentPath }] : undefined;
    const secondRelated = documentPath ? [{ line: first.line, path: documentPath }] : undefined;
    addDiagnostic(diagnostics, severity, code, `${message}; related binding at line ${second.line}`, first.line, documentPath, firstRelated);
    addDiagnostic(diagnostics, severity, code, `${message}; related binding at line ${first.line}`, second.line, documentPath, secondRelated);
}

function readIntegerProperty(binding: ZoneBinding, propertyName: string, diagnostics: Diagnostic[], documentPath?: string): number | undefined {
    const source = binding.properties.get(propertyName);
    if (source === undefined) return undefined;
    if (!/^-?\d+$/.test(source)) {
        addDiagnostic(diagnostics, "error", "format2.zone.gesture.integer-property", `${propertyName} must be one complete unquoted integer`, binding.line, documentPath);
        return undefined;
    }
    const value = Number(source);
    if (!Number.isSafeInteger(value)) {
        addDiagnostic(diagnostics, "error", "format2.zone.gesture.integer-property", `${propertyName} is outside the supported integer range`, binding.line, documentPath);
        return undefined;
    }
    return value;
}

function validateTiming(binding: ZoneBinding, event: string, diagnostics: Diagnostic[], settingsSchema?: SettingsSchema, documentPath?: string): number {
    const delay = readIntegerProperty(binding, "DelayMs", diagnostics, documentPath);
    const repeat = readIntegerProperty(binding, "RepeatIntervalMs", diagnostics, documentPath);
    const runCount = readIntegerProperty(binding, "RunCount", diagnostics, documentPath) ?? 1;
    if (binding.properties.has("DelayMs") && !isHoldEvent(event)) addDiagnostic(diagnostics, "error", "format2.zone.gesture.delay-event", "DelayMs is valid only for Hold and LongHold", binding.line, documentPath);
    if (binding.properties.has("RepeatIntervalMs") && !isHoldEvent(event)) addDiagnostic(diagnostics, "error", "format2.zone.gesture.repeat-event", "RepeatIntervalMs is valid only for Hold and LongHold", binding.line, documentPath);
    if (runCount < 1) addDiagnostic(diagnostics, "error", "format2.zone.gesture.run-count", "RunCount must be a positive integer", binding.line, documentPath);
    const delayDefinition = settingsSchema && isHoldEvent(event) ? settingDefinition(settingsSchema, event === "LongHold" ? "LongHoldDelayMs" : "HoldDelayMs") : undefined;
    if (delay !== undefined && delayDefinition && (delay < delayDefinition.min! || delay > delayDefinition.max!)) addDiagnostic(diagnostics, "error", "format2.zone.gesture.delay-range", `DelayMs must be from ${delayDefinition.min} through ${delayDefinition.max}`, binding.line, documentPath);
    const repeatDefinition = settingsSchema && isHoldEvent(event) ? settingDefinition(settingsSchema, "HoldRepeatIntervalMs") : undefined;
    if (repeat !== undefined && repeatDefinition && (repeat < repeatDefinition.min! || repeat > repeatDefinition.max!)) addDiagnostic(diagnostics, "error", "format2.zone.gesture.repeat-range", `RepeatIntervalMs must be from ${repeatDefinition.min} through ${repeatDefinition.max}`, binding.line, documentPath);
    else if (repeat !== undefined && repeat <= 0) addDiagnostic(diagnostics, "error", "format2.zone.gesture.repeat-value", "RepeatIntervalMs must be a positive integer", binding.line, documentPath);
    return runCount;
}

export function validateFormat2ZoneGestures(bindings: ZoneBinding[], actionTraits: ReadonlyMap<string, ActionTraits>, diagnostics: Diagnostic[], settingsSchema?: SettingsSchema, documentPath?: string): void {
    const bindingsByWidget = new Map<string, ZoneBinding[]>();
    for (const binding of bindings) {
        const key = `${binding.widget.toLowerCase()}\0${[...binding.modifiers].map((modifier) => modifier.toLowerCase()).sort().join("+")}`;
        const group = bindingsByWidget.get(key) ?? [];
        group.push(binding);
        bindingsByWidget.set(key, group);
        const event = eventName(binding);
        const runCount = validateTiming(binding, event, diagnostics, settingsSchema, documentPath);
        const traits = actionTraits.get(binding.action);
        if (runCount > 1 && traits?.changesContext) addDiagnostic(diagnostics, "error", "format2.zone.gesture.context-run-count", `A context-changing action cannot use RunCount greater than one: ${binding.action}`, binding.line, documentPath);
        if (runCount > 1 && traits?.changesModifier) addDiagnostic(diagnostics, "error", "format2.zone.gesture.modifier-run-count", `A modifier action cannot use RunCount greater than one: ${binding.action}`, binding.line, documentPath);
        if (binding.properties.has("RepeatIntervalMs") && traits?.changesContext) addDiagnostic(diagnostics, "error", "format2.zone.gesture.context-repeat", `A context-changing action cannot repeat: ${binding.action}`, binding.line, documentPath);
    }

    for (const widgetBindings of bindingsByWidget.values()) {
        const bindingsByEvent = new Map<string, ZoneBinding[]>();
        for (const binding of widgetBindings) {
            const event = eventName(binding);
            const group = bindingsByEvent.get(event) ?? [];
            group.push(binding);
            bindingsByEvent.set(event, group);
        }
        for (const eventBindings of bindingsByEvent.values()) {
            for (let bindingIdx = 0; bindingIdx < eventBindings.length; bindingIdx++) {
                const binding = eventBindings[bindingIdx];
                for (let otherIdx = bindingIdx + 1; otherIdx < eventBindings.length; otherIdx++) {
                    const other = eventBindings[otherIdx];
                    if (actionIdentity(binding) === actionIdentity(other)) addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.action.duplicate", "The event group contains the same action, parameters, and properties more than once", binding, other, documentPath);
                    else if (binding.action === "NoAction" || other.action === "NoAction") addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.no-action", "NoAction must be the only action in an event group", binding, other, documentPath);
                }
            }
            const firstContextChangeIdx = eventBindings.findIndex((binding) => actionTraits.get(binding.action)?.changesContext);
            if (firstContextChangeIdx >= 0) {
                const first = eventBindings[firstContextChangeIdx];
                for (let bindingIdx = firstContextChangeIdx + 1; bindingIdx < eventBindings.length; bindingIdx++) {
                    const other = eventBindings[bindingIdx];
                    const message = actionTraits.get(other.action)?.changesContext ? "One event group cannot contain two context-changing actions" : "An action cannot follow a context-changing action in the same event group";
                    addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.unreachable", message, first, other, documentPath);
                }
            }
        }
        for (let bindingIdx = 0; bindingIdx < widgetBindings.length; bindingIdx++) {
            const binding = widgetBindings[bindingIdx];
            const event = eventName(binding);
            const traits = actionTraits.get(binding.action);
            for (let otherIdx = bindingIdx + 1; otherIdx < widgetBindings.length; otherIdx++) {
                const other = widgetBindings[otherIdx];
                const otherEvent = eventName(other);
                const otherTraits = actionTraits.get(other.action);
                if (traits?.changesModifier && otherTraits?.changesModifier) addRelatedDiagnostics(diagnostics, "warning", "format2.zone.gesture.additive", "More than one action changes modifier state on the same Widget", binding, other, documentPath);
                else if (traits?.changesModifier || otherTraits?.changesModifier) addRelatedDiagnostics(diagnostics, "warning", "format2.zone.gesture.additive", "A modifier action and a normal action share the same Widget", binding, other, documentPath);
                else if ((event === "Press" && isHoldEvent(otherEvent)) || (otherEvent === "Press" && isHoldEvent(event))) addRelatedDiagnostics(diagnostics, "warning", "format2.zone.gesture.additive", "Press and hold actions are additive", binding, other, documentPath);
                else if ((event === "Release" && isHoldEvent(otherEvent)) || (otherEvent === "Release" && isHoldEvent(event))) addRelatedDiagnostics(diagnostics, "warning", "format2.zone.gesture.additive", "Release also runs after a Hold or LongHold milestone", binding, other, documentPath);
                else if ((event === "Hold" && otherEvent === "LongHold") || (event === "LongHold" && otherEvent === "Hold")) addRelatedDiagnostics(diagnostics, "warning", "format2.zone.gesture.additive", "LongHold runs after Hold when both are declared", binding, other, documentPath);
                else if ((event === "DoublePress" && (otherEvent === "Press" || otherEvent === "Release")) || (otherEvent === "DoublePress" && (event === "Press" || event === "Release"))) addRelatedDiagnostics(diagnostics, "warning", "format2.zone.gesture.additive", `DoublePress is additive with ${event === "DoublePress" ? otherEvent : event}`, binding, other, documentPath);
                if (traits?.changesContext && event === "Press" && event !== otherEvent) addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.unreachable", "Press changes context before the other button event can complete", binding, other, documentPath);
                else if (otherTraits?.changesContext && otherEvent === "Press" && event !== otherEvent) addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.unreachable", "Press changes context before the other button event can complete", binding, other, documentPath);
                else if (traits?.changesContext && event === "Release" && otherEvent === "DoublePress") addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.unreachable", "Release changes context before DoublePress can complete", binding, other, documentPath);
                else if (otherTraits?.changesContext && otherEvent === "Release" && event === "DoublePress") addRelatedDiagnostics(diagnostics, "error", "format2.zone.gesture.unreachable", "Release changes context before DoublePress can complete", binding, other, documentPath);
            }
        }
    }
}
