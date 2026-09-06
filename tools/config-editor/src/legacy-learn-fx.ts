import { addDiagnostic, type Diagnostic } from "./model.ts";
import { initializeLine, splitSourceLines } from "./text.ts";
import { convertLegacyZoneToFormat2 } from "./legacy-zone-format2.ts";
import { parseZone, type ZoneSemantic } from "./zone.ts";

export interface LegacyLearnFxSource {
    source: string;
    sourcePath: string;
}

export interface LegacyLearnFxConversion {
    diagnostics: Diagnostic[];
    source: string;
}

export interface LegacyLearnFxSources {
    epilogue?: LegacyLearnFxSource;
    layout: LegacyLearnFxSource;
    prologue?: LegacyLearnFxSource;
}

function normalizeProperty(token: string): string {
    return token.replace(/#([0-9a-fA-F]{6})[0-9a-fA-F]{2}\b/g, "#$1");
}

function widgetSelector(widget: string): string {
    return widget.replace(/\|$/, "#");
}

function layoutBindings(layout: LegacyLearnFxSource): Map<string, string> {
    const document = parseZone(layout.source, layout.sourcePath);
    const result = new Map<string, string>();
    for (const binding of (document.semantic as ZoneSemantic).bindings) result.set(binding.widget.toLowerCase(), binding.action);
    return result;
}

function convertedGeneratedBody(source: LegacyLearnFxSource, diagnostics: Diagnostic[]): string[] {
    const conversion = convertLegacyZoneToFormat2(source.source, { profile: "Main", targetPath: source.sourcePath });
    diagnostics.push(...conversion.diagnostics);
    const lines = conversion.source.split(/\r\n|\r|\n/).slice(2).filter((line) => !line.trimStart().startsWith("//"));
    while (lines[0]?.trim() === "") lines.shift();
    while (lines.at(-1)?.trim() === "") lines.pop();
    return lines;
}

export function convertLegacyLearnFxToFormat2(sources: LegacyLearnFxSources): LegacyLearnFxConversion {
    const diagnostics: Diagnostic[] = [];
    const firstConfigurationLine = splitSourceLines(sources.layout.source).map((line) => initializeLine(line)).find((line) => line && !line.startsWith("//"));
    if (firstConfigurationLine?.replace(/^\uFEFF/, "").startsWith("@Meta")) return { diagnostics, source: sources.layout.source };
    const bindings = layoutBindings(sources.layout);
    const entries: string[] = [];
    const selectors = new Set<string>();
    for (const line of splitSourceLines(sources.layout.source)) {
        const text = initializeLine(line);
        if (!text || line.kind === "comment") continue;
        const directive = line.tokens[0];
        if (directive !== "#WidgetType" && directive !== "#DisplayRow") continue;
        const widget = line.tokens[1];
        if (!widget) {
            addDiagnostic(diagnostics, "error", "legacy.learn-fx.widget.required", `${directive} requires a Widget selector.`, line.lineNumber, sources.layout.sourcePath);
            continue;
        }
        const selector = widgetSelector(widget);
        if (selectors.has(selector.toLowerCase())) {
            addDiagnostic(diagnostics, "error", "legacy.learn-fx.widget.duplicate", `Learn FX Widget selector is repeated: ${selector}`, line.lineNumber, sources.layout.sourcePath);
            continue;
        }
        let role = "Parameter";
        if (directive === "#DisplayRow") {
            const action = bindings.get(widget.toLowerCase());
            if (action === "FixedTextDisplay" || action === "FXParamNameDisplay") role = "NameDisplay";
            else if (action === "FXParamValueDisplay") role = "ValueDisplay";
            else {
                addDiagnostic(diagnostics, "error", "legacy.learn-fx.display.role", `Cannot determine whether ${widget} is a name or value display. Keep one matching FixedTextDisplay, FXParamNameDisplay, or FXParamValueDisplay binding in FXWidgetLayout before import.`, line.lineNumber, sources.layout.sourcePath);
                continue;
            }
        }
        selectors.add(selector.toLowerCase());
        const properties = line.tokens.slice(2).filter((token) => token.includes("=")).map(normalizeProperty);
        entries.push(`  ${role} ${selector}${properties.length ? ` ${properties.join(" ")}` : ""}`);
    }
    if (!entries.some((entry) => entry.trimStart().startsWith("Parameter "))) addDiagnostic(diagnostics, "error", "legacy.learn-fx.parameter.required", "FXWidgetLayout requires at least one active #WidgetType entry.", undefined, sources.layout.sourcePath);

    const generatedGroups = [sources.prologue, sources.epilogue].map((source) => source ? convertedGeneratedBody(source, diagnostics) : []).filter((lines) => lines.length);
    const generated = generatedGroups.flatMap((lines, groupIdx) => groupIdx ? ["", ...lines] : lines);
    const output = ["@Meta { Version=2 }", "", "FXWidgets {", ...entries, "}"];
    if (generated.some((line) => line.trim())) output.push("", "GeneratedBindings {", ...generated.map((line) => line ? `  ${line}` : ""), "}");
    return { diagnostics, source: output.join("\n") + "\n" };
}
