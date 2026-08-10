import { addDiagnostic, type Diagnostic, type LosslessDocument } from "./model.ts";
import { initializeLine, isStableId, parseProperties, propertyValue, splitSourceLines } from "./text.ts";

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
            continue;
        }
        if (keyword === "Binding") {
            line.kind = "block-start";
            if (!headerLine || snippetEnded) addDiagnostic(diagnostics, "error", "snippet.binding.outside", "Binding must be inside a Snippet block", line.lineNumber, documentPath);
            if (currentBinding) addDiagnostic(diagnostics, "error", "snippet.binding.nested", "Bindings cannot be nested", line.lineNumber, documentPath);
            const properties = parseProperties(line.tokens.slice(1));
            const bindingId = propertyValue(properties, "Id") ?? "";
            currentBinding = {
                actions: [],
                feedback: propertyValue(properties, "Feedback"),
                id: bindingId,
                input: propertyValue(properties, "Input"),
                line: line.lineNumber,
                required: (propertyValue(properties, "Required") ?? "Yes").toLowerCase() !== "no",
                role: propertyValue(properties, "Role"),
            };
            semantic.bindings.push(currentBinding);
            if (!isStableId(bindingId)) addDiagnostic(diagnostics, "error", "snippet.binding.id", "Binding Id must be a stable lowercase ASCII ID", line.lineNumber, documentPath);
            if (propertyValue(properties, "Widget")) addDiagnostic(diagnostics, "error", "snippet.binding.widget", "Snippet bindings must not contain a fixed Widget property", line.lineNumber, documentPath);
            if (!currentBinding.role) addDiagnostic(diagnostics, "error", "snippet.binding.role", "Binding requires Role", line.lineNumber, documentPath);
            if (!currentBinding.input) addDiagnostic(diagnostics, "error", "snippet.binding.input", "Binding requires Input", line.lineNumber, documentPath);
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
        if (!binding.actions.length) addDiagnostic(diagnostics, "warning", "snippet.binding.empty", `Binding ${binding.id} has no actions`, binding.line, documentPath);
    }
    return { diagnostics, format: "snippet", lines, path: documentPath, semantic, source, version };
}
