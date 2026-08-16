import { addDiagnostic, type Diagnostic, type LosslessDocument } from "./model.ts";
import { initializeLine, parseFormatMarker, parseProperties, propertyValue, splitSourceLines } from "./text.ts";

export interface ZoneBinding {
    action: string;
    line: number;
    modifiers: string[];
    params: string[];
    properties: Map<string, string>;
    widget: string;
}

export interface ZoneDependencyReference {
    line: number;
    name: string;
    type: "GoSubZone" | "GoZone" | "IncludedZones" | "SubZones";
}

export interface ZoneSemantic {
    alias?: string;
    bindings: ZoneBinding[];
    dependencyReferences: ZoneDependencyReference[];
    dependencies: string[];
    includedZones: string[];
    name?: string;
    navigator?: string;
    subZones: string[];
}

function parseWidgetExpression(expression: string): { modifiers: string[]; widget: string } {
    const parts = expression.split("+").filter(Boolean);
    return { modifiers: parts.slice(0, -1), widget: parts.at(-1) ?? "" };
}

const LEARN_TEMPLATE_DIRECTIVES = new Set(["#WidgetType", "#DisplayRow", "#RingStyle", "#DisplayFont", "#SupportsColor"]);

export function isLearnTemplateDirective(keyword: string | undefined): boolean {
    return Boolean(keyword && LEARN_TEMPLATE_DIRECTIVES.has(keyword));
}

export function parseZone(source: string, documentPath?: string, knownActions?: Set<string>): LosslessDocument<ZoneSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const semantic: ZoneSemantic = { bindings: [], dependencies: [], dependencyReferences: [], includedZones: [], subZones: [] };
    let version = "unversioned";
    let markerLine: number | undefined;
    let zoneLine: number | undefined;
    let zoneEnded = false;
    let section: "included" | "sub" | undefined;

    for (const line of lines) {
        const text = initializeLine(line);
        const marker = parseFormatMarker(text);
        if (marker) {
            line.kind = "format";
            if (marker.format !== "zone") addDiagnostic(diagnostics, "error", "zone.format.type", `Expected zone format marker, got ${marker.format}`, line.lineNumber, documentPath);
            else if (markerLine) addDiagnostic(diagnostics, "error", "zone.format.duplicate", "Zone format marker is duplicated", line.lineNumber, documentPath);
            else {
                markerLine = line.lineNumber;
                version = marker.version;
                if (version !== "1") addDiagnostic(diagnostics, "error", "zone.format.version", `Unsupported zone format version: ${version}`, line.lineNumber, documentPath);
            }
            continue;
        }
        if (!text || line.kind === "comment") continue;

        const keyword = line.tokens[0];
        if (keyword === "Zone") {
            line.kind = "header";
            if (zoneLine) addDiagnostic(diagnostics, "error", "zone.header.duplicate", "A zone file must contain one Zone block", line.lineNumber, documentPath);
            zoneLine = line.lineNumber;
            semantic.name = line.tokens[1];
            if (!semantic.name) addDiagnostic(diagnostics, "error", "zone.name.missing", "Zone requires a name", line.lineNumber, documentPath);
            const properties = parseProperties(line.tokens.slice(2));
            semantic.navigator = propertyValue(properties, "NavType");
            const aliasCandidate = line.tokens[2];
            if (aliasCandidate && !aliasCandidate.includes("=") && aliasCandidate !== "/") semantic.alias = aliasCandidate;
            continue;
        }
        if (keyword === "ZoneEnd") {
            line.kind = "block-end";
            if (!zoneLine) addDiagnostic(diagnostics, "error", "zone.end.without-header", "ZoneEnd has no Zone header", line.lineNumber, documentPath);
            if (section) addDiagnostic(diagnostics, "error", "zone.section.unclosed", `${section} section is not closed before ZoneEnd`, line.lineNumber, documentPath);
            zoneEnded = true;
            section = undefined;
            continue;
        }
        if (keyword === "IncludedZones" || keyword === "SubZones") {
            line.kind = "block-start";
            if (section) addDiagnostic(diagnostics, "error", "zone.section.nested", "Zone sections cannot be nested", line.lineNumber, documentPath);
            section = keyword === "IncludedZones" ? "included" : "sub";
            continue;
        }
        if (keyword === "IncludedZonesEnd" || keyword === "SubZonesEnd") {
            line.kind = "block-end";
            const expected = keyword === "IncludedZonesEnd" ? "included" : "sub";
            if (section !== expected) addDiagnostic(diagnostics, "error", "zone.section.end", `${keyword} has no matching section start`, line.lineNumber, documentPath);
            section = undefined;
            continue;
        }
        if (isLearnTemplateDirective(keyword)) {
            line.kind = "entry";
            if (keyword !== "#SupportsColor" && !line.tokens[1]) addDiagnostic(diagnostics, "error", "zone.learn-template.value", `${keyword} requires a value`, line.lineNumber, documentPath);
            continue;
        }
        if (section) {
            line.kind = "entry";
            const dependency = line.tokens[0];
            if (dependency) {
                if (section === "included") semantic.includedZones.push(dependency);
                else semantic.subZones.push(dependency);
                semantic.dependencies.push(dependency);
                semantic.dependencyReferences.push({ line: line.lineNumber, name: dependency, type: section === "included" ? "IncludedZones" : "SubZones" });
            }
            continue;
        }
        if (!zoneLine || zoneEnded) {
            line.kind = "unknown";
            addDiagnostic(diagnostics, "warning", "zone.line.outside", `Line is outside the Zone block: ${text}`, line.lineNumber, documentPath);
            continue;
        }
        if (line.tokens.length < 2) {
            line.kind = "unknown";
            addDiagnostic(diagnostics, "warning", "zone.line.unknown", `Unknown zone line: ${text}`, line.lineNumber, documentPath);
            continue;
        }

        const widgetExpression = parseWidgetExpression(line.tokens[0]);
        const action = line.tokens[1];
        const actionTokens = line.tokens.slice(2);
        const properties = parseProperties(actionTokens);
        const params = actionTokens.filter((token) => !token.includes("="));
        semantic.bindings.push({ action, line: line.lineNumber, modifiers: widgetExpression.modifiers, params, properties, widget: widgetExpression.widget });
        line.kind = "entry";
        if (!widgetExpression.widget) addDiagnostic(diagnostics, "error", "zone.binding.widget", "Binding requires a widget", line.lineNumber, documentPath);
        if (knownActions && !knownActions.has(action)) addDiagnostic(diagnostics, "warning", "zone.action.unknown", `Unknown runtime action: ${action}`, line.lineNumber, documentPath);
        if ((action === "GoZone" || action === "GoSubZone") && params[0]) {
            semantic.dependencies.push(params[0]);
            semantic.dependencyReferences.push({ line: line.lineNumber, name: params[0], type: action });
        }
    }

    if (!markerLine) addDiagnostic(diagnostics, "warning", "zone.format.missing", "Zone has no // @format zone 1 marker", undefined, documentPath);
    if (!zoneLine) addDiagnostic(diagnostics, "error", "zone.header.missing", "Zone file has no Zone header", undefined, documentPath);
    else if (!zoneEnded) addDiagnostic(diagnostics, "error", "zone.end.missing", "Zone file has no ZoneEnd", zoneLine, documentPath);
    if (section) addDiagnostic(diagnostics, "error", "zone.section.unclosed", `${section} section has no end marker`, undefined, documentPath);
    semantic.dependencies = [...new Set(semantic.dependencies)];
    return { diagnostics, format: "zone", lines, path: documentPath, semantic, source, version };
}
