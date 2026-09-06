import { addDiagnostic, type LosslessDocument } from "./model.ts";
import { initializeLine, splitSourceLines } from "./text.ts";

export interface LearnFxSemantic {
    widgets: Array<{ line: number; properties: Map<string, string>; role: string; selector: string }>;
}

function braceDelta(text: string): number {
    let delta = 0;
    let insideQuote = false;
    for (const character of text) {
        if (character === "\"") insideQuote = !insideQuote;
        else if (!insideQuote && character === "{") delta++;
        else if (!insideQuote && character === "}") delta--;
    }
    return delta;
}

export function parseLearnFx(source: string, documentPath?: string): LosslessDocument<LearnFxSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: LosslessDocument<LearnFxSemantic>["diagnostics"] = [];
    const widgets: LearnFxSemantic["widgets"] = [];
    const selectors = new Set<string>();
    let metadataFound = false;
    let currentBlock: "FXWidgets" | "GeneratedBindings" | undefined;
    let blockDepth = 0;
    let fxWidgetsFound = false;
    let generatedBindingsFound = false;
    for (const line of lines) {
        const text = initializeLine(line);
        if (!text || line.kind === "comment") continue;
        const delta = braceDelta(text);
        if (!metadataFound && line.tokens[0] === "@Meta") {
            metadataFound = true;
            line.kind = "format";
            if (!/(?:^|\s)Version=2(?=\s|})/.test(text)) addDiagnostic(diagnostics, "error", "format2.learn-fx.version", "LearnFX.fxzon requires Version=2.", line.lineNumber, documentPath);
            continue;
        }
        if (currentBlock) {
            if (currentBlock === "FXWidgets" && blockDepth === 1 && text !== "}") {
                const role = line.tokens[0];
                const selector = line.tokens[1];
                if (!role || !selector || !["Parameter", "NameDisplay", "ValueDisplay"].includes(role)) addDiagnostic(diagnostics, "error", "format2.learn-fx.widget.entry", "An FXWidgets entry requires Parameter, NameDisplay, or ValueDisplay and one Widget selector.", line.lineNumber, documentPath);
                else {
                    if (!/^[A-Za-z][A-Za-z0-9_-]*#?$/.test(selector)) addDiagnostic(diagnostics, "error", "format2.learn-fx.widget.selector", `Invalid Learn FX Widget selector: ${selector}`, line.lineNumber, documentPath);
                    if (selectors.has(selector.toLowerCase())) addDiagnostic(diagnostics, "error", "format2.learn-fx.widget.duplicate", `Learn FX Widget selector is repeated: ${selector}`, line.lineNumber, documentPath);
                    else selectors.add(selector.toLowerCase());
                    const properties = new Map<string, string>();
                    for (const token of line.tokens.slice(2)) if (token.includes("=")) properties.set(token.slice(0, token.indexOf("=")), token.slice(token.indexOf("=") + 1));
                    widgets.push({ line: line.lineNumber, properties, role, selector });
                }
                line.kind = "entry";
            } else line.kind = text === "}" && blockDepth === 1 ? "block-end" : "entry";
            blockDepth += delta;
            if (blockDepth <= 0) currentBlock = undefined;
            continue;
        }
        if ((line.tokens[0] === "FXWidgets" || line.tokens[0] === "GeneratedBindings") && delta > 0) {
            currentBlock = line.tokens[0] as typeof currentBlock;
            blockDepth = delta;
            line.kind = "block-start";
            if (currentBlock === "FXWidgets") {
                if (fxWidgetsFound) addDiagnostic(diagnostics, "error", "format2.learn-fx.widgets.duplicate", "FXWidgets can occur only once.", line.lineNumber, documentPath);
                fxWidgetsFound = true;
            } else {
                if (generatedBindingsFound) addDiagnostic(diagnostics, "error", "format2.learn-fx.generated.duplicate", "GeneratedBindings can occur only once.", line.lineNumber, documentPath);
                generatedBindingsFound = true;
            }
            continue;
        }
        line.kind = "unknown";
        addDiagnostic(diagnostics, "error", "format2.learn-fx.block", `Unknown Learn FX line: ${text}`, line.lineNumber, documentPath);
    }
    if (!metadataFound) addDiagnostic(diagnostics, "error", "format2.learn-fx.metadata", "LearnFX.fxzon requires @Meta { Version=2 }.", undefined, documentPath);
    if (documentPath && documentPath.replaceAll("\\", "/").split("/").at(-1) !== "LearnFX.fxzon") addDiagnostic(diagnostics, "error", "format2.learn-fx.filename", "A Learn FX document must use the exact filename LearnFX.fxzon.", undefined, documentPath);
    if (!fxWidgetsFound) addDiagnostic(diagnostics, "error", "format2.learn-fx.widgets.required", "LearnFX.fxzon requires one FXWidgets block.", undefined, documentPath);
    if (!widgets.some((widget) => widget.role === "Parameter")) addDiagnostic(diagnostics, "error", "format2.learn-fx.parameter.required", "FXWidgets requires at least one Parameter entry.", undefined, documentPath);
    if (currentBlock) addDiagnostic(diagnostics, "error", "format2.learn-fx.block.unclosed", `${currentBlock} has no closing brace.`, undefined, documentPath);
    return { diagnostics, format: "learn-fx", lines, path: documentPath, semantic: { widgets }, source, version: "2" };
}
