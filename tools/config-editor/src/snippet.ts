import { addDiagnostic, type Diagnostic, type LosslessDocument } from "./model.ts";
import { initializeLine, isStableId, parseProperties, propertyValue, splitSourceLines } from "./text.ts";
import type { WidgetCapability, WidgetRole } from "./widget-capabilities.ts";

const INPUT_CAPABILITIES = new Map<string, WidgetCapability>([
    ["absolute", "absolute-input"],
    ["press", "press-input"],
    ["relative", "relative-input"],
    ["touch", "touch-input"],
]);

const FEEDBACK_CAPABILITIES = new Map<string, WidgetCapability>([
    ["color", "color-feedback"],
    ["meter", "meter-feedback"],
    ["text", "text-feedback"],
    ["toggle", "toggle-feedback"],
    ["value", "value-feedback"],
]);

const SNIPPET_ROLES = new Set<WidgetRole>(["button", "display", "fader", "meter", "rotary"]);

export interface SnippetAction {
    action: string;
    line: number;
    modifiers: string;
    params: string[];
}

export interface SnippetBinding {
    actions: SnippetAction[];
    feedback?: string;
    id: string;
    input?: string;
    line: number;
    required: boolean;
    role?: string;
}

export interface SnippetSemantic {
    bindings: SnippetBinding[];
    id?: string;
    name?: string;
}

function requirementTokens(value: string | undefined): string[] {
    return value?.split("+").filter(Boolean).map((token) => token.toLowerCase()) ?? [];
}

function isValidModifierExpression(value: string): boolean {
    if (!/^[A-Za-z][A-Za-z0-9_-]*(?:\+[A-Za-z][A-Za-z0-9_-]*)*$/.test(value)) return false;
    const modifiers = value.toLowerCase().split("+");
    return !modifiers.includes("nomod") || modifiers.length === 1;
}

export function snippetBindingCapabilities(binding: SnippetBinding): WidgetCapability[] {
    const capabilities = new Set<WidgetCapability>();
    for (const input of requirementTokens(binding.input)) {
        const capability = INPUT_CAPABILITIES.get(input);
        if (capability) capabilities.add(capability);
    }
    for (const feedback of requirementTokens(binding.feedback)) {
        const capability = FEEDBACK_CAPABILITIES.get(feedback);
        if (capability) capabilities.add(capability);
    }
    return [...capabilities].sort();
}

export function snippetBindingRole(binding: SnippetBinding): WidgetRole {
    const role = binding.role?.toLowerCase() ?? "unknown";
    return SNIPPET_ROLES.has(role as WidgetRole) ? role as WidgetRole : "unknown";
}

export function parseSnippet(source: string, documentPath?: string, knownActions?: Set<string>): LosslessDocument<SnippetSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const semantic: SnippetSemantic = { bindings: [] };
    let version = "unversioned";
    let headerLine: number | undefined;
    let currentBinding: SnippetBinding | undefined;
    let snippetEnded = false;

    for (const line of lines) {
        const text = initializeLine(line);
        if (!text || line.kind === "comment") continue;
        const keyword = line.tokens[0];
        if (keyword === "Snippet") {
            line.kind = "header";
            if (headerLine) addDiagnostic(diagnostics, "error", "snippet.header.duplicate", "Snippet header is duplicated", line.lineNumber, documentPath);
            headerLine = line.lineNumber;
            const properties = parseProperties(line.tokens.slice(1));
            version = propertyValue(properties, "Version") ?? "unversioned";
            semantic.id = propertyValue(properties, "Id");
            semantic.name = propertyValue(properties, "Name");
            if (version !== "1") addDiagnostic(diagnostics, "error", "snippet.format.version", `Unsupported or missing snippet version: ${version}`, line.lineNumber, documentPath);
            if (!semantic.id || !isStableId(semantic.id)) addDiagnostic(diagnostics, "error", "snippet.id", "Snippet Id must be a stable lowercase ASCII ID", line.lineNumber, documentPath);
            if (!semantic.name) addDiagnostic(diagnostics, "error", "snippet.name", "Snippet requires a display Name", line.lineNumber, documentPath);
            continue;
        }
        if (keyword === "Binding") {
            line.kind = "block-start";
            if (!headerLine || snippetEnded) addDiagnostic(diagnostics, "error", "snippet.binding.outside", "Binding must be inside a Snippet block", line.lineNumber, documentPath);
            if (currentBinding) addDiagnostic(diagnostics, "error", "snippet.binding.nested", "Bindings cannot be nested", line.lineNumber, documentPath);
            const properties = parseProperties(line.tokens.slice(1));
            const bindingId = propertyValue(properties, "Id") ?? "";
            const requiredProperty = propertyValue(properties, "Required");
            const requiredValue = requiredProperty ?? "Yes";
            currentBinding = {
                actions: [],
                feedback: propertyValue(properties, "Feedback"),
                id: bindingId,
                input: propertyValue(properties, "Input"),
                line: line.lineNumber,
                required: requiredValue.toLowerCase() !== "no",
                role: propertyValue(properties, "Role"),
            };
            semantic.bindings.push(currentBinding);
            if (!isStableId(bindingId)) addDiagnostic(diagnostics, "error", "snippet.binding.id", "Binding Id must be a stable lowercase ASCII ID", line.lineNumber, documentPath);
            if (propertyValue(properties, "Widget")) addDiagnostic(diagnostics, "error", "snippet.binding.widget", "Snippet bindings must not contain a fixed Widget property", line.lineNumber, documentPath);
            if (!currentBinding.role) addDiagnostic(diagnostics, "error", "snippet.binding.role", "Binding requires Role", line.lineNumber, documentPath);
            else if (!SNIPPET_ROLES.has(currentBinding.role.toLowerCase() as WidgetRole)) addDiagnostic(diagnostics, "error", "snippet.binding.role.value", `Unsupported binding Role: ${currentBinding.role}`, line.lineNumber, documentPath);
            if (!currentBinding.input) addDiagnostic(diagnostics, "error", "snippet.binding.input", "Binding requires Input", line.lineNumber, documentPath);
            else for (const input of requirementTokens(currentBinding.input)) if (!INPUT_CAPABILITIES.has(input)) addDiagnostic(diagnostics, "error", "snippet.binding.input.value", `Unsupported binding Input: ${input}`, line.lineNumber, documentPath);
            if (!currentBinding.feedback) addDiagnostic(diagnostics, "error", "snippet.binding.feedback", "Binding requires Feedback or Feedback=None", line.lineNumber, documentPath);
            else {
                const feedbackTokens = requirementTokens(currentBinding.feedback);
                if (feedbackTokens.includes("none") && feedbackTokens.length > 1) addDiagnostic(diagnostics, "error", "snippet.binding.feedback.none", "Feedback=None cannot be combined with another feedback requirement", line.lineNumber, documentPath);
                for (const feedback of feedbackTokens) if (feedback !== "none" && !FEEDBACK_CAPABILITIES.has(feedback)) addDiagnostic(diagnostics, "error", "snippet.binding.feedback.value", `Unsupported binding Feedback: ${feedback}`, line.lineNumber, documentPath);
            }
            if (!requiredProperty) addDiagnostic(diagnostics, "error", "snippet.binding.required.missing", "Binding requires Required=Yes or Required=No", line.lineNumber, documentPath);
            if (!["yes", "no"].includes(requiredValue.toLowerCase())) addDiagnostic(diagnostics, "error", "snippet.binding.required", "Binding Required must be Yes or No", line.lineNumber, documentPath);
            continue;
        }
        if (keyword === "BindingEnd") {
            line.kind = "block-end";
            if (!currentBinding) addDiagnostic(diagnostics, "error", "snippet.binding.end", "BindingEnd has no matching Binding", line.lineNumber, documentPath);
            currentBinding = undefined;
            continue;
        }
        if (keyword === "Action") {
            line.kind = "entry";
            if (!currentBinding) {
                addDiagnostic(diagnostics, "error", "snippet.action.outside", "Action must be inside a Binding block", line.lineNumber, documentPath);
                continue;
            }
            const modifiers = line.tokens[1] ?? "";
            const action = line.tokens[2] ?? "";
            if (!modifiers || !action) addDiagnostic(diagnostics, "error", "snippet.action.syntax", "Action requires a modifier expression and runtime action name", line.lineNumber, documentPath);
            else if (!isValidModifierExpression(modifiers)) addDiagnostic(diagnostics, "error", "snippet.action.modifiers", `Invalid modifier expression: ${modifiers}`, line.lineNumber, documentPath);
            currentBinding.actions.push({ action, line: line.lineNumber, modifiers, params: line.tokens.slice(3) });
            if (knownActions && action && !knownActions.has(action)) addDiagnostic(diagnostics, "warning", "snippet.action.unknown", `Unknown runtime action: ${action}`, line.lineNumber, documentPath);
            continue;
        }
        if (keyword === "SnippetEnd") {
            line.kind = "block-end";
            if (currentBinding) addDiagnostic(diagnostics, "error", "snippet.binding.unclosed", `Binding ${currentBinding.id} has no BindingEnd`, currentBinding.line, documentPath);
            currentBinding = undefined;
            snippetEnded = true;
            continue;
        }
        line.kind = "unknown";
        addDiagnostic(diagnostics, "warning", "snippet.line.unknown", `Unknown snippet line: ${text}`, line.lineNumber, documentPath);
    }

    if (!headerLine) addDiagnostic(diagnostics, "error", "snippet.header.missing", "Snippet has no Snippet header", undefined, documentPath);
    if (currentBinding) addDiagnostic(diagnostics, "error", "snippet.binding.unclosed", `Binding ${currentBinding.id} has no BindingEnd`, currentBinding.line, documentPath);
    if (headerLine && !snippetEnded) addDiagnostic(diagnostics, "error", "snippet.end.missing", "Snippet has no SnippetEnd", headerLine, documentPath);
    const bindingsById = new Map<string, SnippetBinding>();
    for (const binding of semantic.bindings) {
        const existing = bindingsById.get(binding.id.toLowerCase());
        if (existing) addDiagnostic(diagnostics, "error", "snippet.binding.duplicate", `Binding IDs differ only by case or are duplicated: ${existing.id}, ${binding.id}`, binding.line, documentPath);
        else bindingsById.set(binding.id.toLowerCase(), binding);
        if (!binding.actions.length) addDiagnostic(diagnostics, "error", "snippet.binding.empty", `Binding ${binding.id} requires at least one Action`, binding.line, documentPath);
    }
    return { diagnostics, format: "snippet", lines, path: documentPath, semantic, source, version };
}
