import { addDiagnostic, type Diagnostic, type LosslessDocument, type SourceLine } from "./model.ts";
import { analysisText, initializeLine, parseFormatMarker, parseProperties, propertyValue, splitSourceLines, splitWidgetComment, tokenizeLine } from "./text.ts";

export interface SurfaceWidget {
    body: SourceLine[];
    line: number;
    name: string;
    oskProperties: Map<string, string>;
    widgetClass?: string;
}

export interface OskLayoutCell {
    line: number;
    properties: Map<string, string>;
    type: "spacer" | "widget";
    widgetName?: string;
}

export interface OskLayoutRow {
    cells: OskLayoutCell[];
    line: number;
}

export interface SurfaceSemantic {
    legacyOskLayout: boolean;
    oskLayout?: { line: number; rows: OskLayoutRow[]; version: string };
    widgets: SurfaceWidget[];
}

const BLOCK_ENDS = new Map([
    ["StepSize", "StepSizeEnd"],
    ["AccelerationValues", "AccelerationValuesEnd"],
    ["ColorCalibration", "ColorCalibrationEnd"],
]);

const OSK_TARGET_CAPABILITIES = new Map([
    ["PressTarget", ["press", "anypress"]],
    ["ScrollTarget", ["encoder", "mftencoder", "encoderplain", "encoder7bit", "x32rotarytoencoder"]],
    ["ValueTarget", ["fader14bit", "faderportclassicfader14bit", "fader7bit", "x32fader"]],
    ["TouchTarget", ["touch"]],
]);

function supportsOskTarget(widget: SurfaceWidget, supportedTypes: string[]): boolean {
    return widget.body.some((line) => supportedTypes.includes((line.tokens[0] ?? "").toLowerCase()));
}

export function parseSurface(source: string, documentPath?: string): LosslessDocument<SurfaceSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const widgets: SurfaceWidget[] = [];
    let version = "unversioned";
    let markerLine: number | undefined;
    let currentBlock: { end: string; line: number; name: string } | undefined;
    let currentWidget: SurfaceWidget | undefined;
    let layout: SurfaceSemantic["oskLayout"];
    let currentLayoutRow: OskLayoutRow | undefined;
    let inLayout = false;
    let legacyOskLayout = false;

    for (const line of lines) {
        const text = initializeLine(line);
        const marker = parseFormatMarker(text);
        if (marker) {
            line.kind = "format";
            if (marker.format !== "surface") addDiagnostic(diagnostics, "error", "surface.format.type", `Expected surface format marker, got ${marker.format}`, line.lineNumber, documentPath);
            else if (markerLine) addDiagnostic(diagnostics, "error", "surface.format.duplicate", "Surface format marker is duplicated", line.lineNumber, documentPath);
            else {
                markerLine = line.lineNumber;
                version = marker.version;
                if (version !== "1") addDiagnostic(diagnostics, "error", "surface.format.version", `Unsupported surface format version: ${version}`, line.lineNumber, documentPath);
            }
            continue;
        }
        if (!text || line.kind === "comment") {
            if (/^#\s*OSK(?:Row|Spacer)\b/.test(text)) legacyOskLayout = true;
            continue;
        }

        if (currentWidget) {
            if (line.tokens[0] === "WidgetEnd") {
                line.kind = "block-end";
                currentWidget = undefined;
            } else {
                line.kind = "entry";
                currentWidget.body.push(line);
            }
            continue;
        }

        if (inLayout) {
            if (line.tokens[0] === "OSKLayoutEnd") {
                if (currentLayoutRow) addDiagnostic(diagnostics, "error", "surface.layout.row.unclosed", "OSK layout row has no RowEnd", currentLayoutRow.line, documentPath);
                line.kind = "block-end";
                currentLayoutRow = undefined;
                inLayout = false;
            } else if (line.tokens[0] === "Row") {
                if (currentLayoutRow) addDiagnostic(diagnostics, "error", "surface.layout.row.nested", "OSK layout rows cannot be nested", line.lineNumber, documentPath);
                currentLayoutRow = { cells: [], line: line.lineNumber };
                layout?.rows.push(currentLayoutRow);
                line.kind = "block-start";
            } else if (line.tokens[0] === "RowEnd") {
                if (!currentLayoutRow) addDiagnostic(diagnostics, "error", "surface.layout.row.end", "RowEnd has no matching Row", line.lineNumber, documentPath);
                line.kind = "block-end";
                currentLayoutRow = undefined;
            } else if (line.tokens[0] === "Widget" || line.tokens[0] === "Spacer") {
                if (!currentLayoutRow) addDiagnostic(diagnostics, "error", "surface.layout.cell.outside-row", `${line.tokens[0]} must be inside a Row block`, line.lineNumber, documentPath);
                else {
                    const cellProperties = parseProperties(line.tokens.slice(line.tokens[0] === "Widget" ? 2 : 1));
                    currentLayoutRow.cells.push({ line: line.lineNumber, properties: cellProperties, type: line.tokens[0] === "Widget" ? "widget" : "spacer", widgetName: line.tokens[0] === "Widget" ? line.tokens[1] : undefined });
                    if (line.tokens[0] === "Widget" && !line.tokens[1]) addDiagnostic(diagnostics, "error", "surface.layout.widget.name", "OSK layout Widget requires a name", line.lineNumber, documentPath);
                }
                line.kind = "entry";
            } else {
                line.kind = "unknown";
                addDiagnostic(diagnostics, "warning", "surface.layout.unknown", `Unknown OSK layout line: ${text}`, line.lineNumber, documentPath);
            }
            continue;
        }

        if (currentBlock) {
            if (line.tokens[0] === currentBlock.end) {
                line.kind = "block-end";
                currentBlock = undefined;
            } else {
                line.kind = "entry";
            }
            continue;
        }

        const blockEnd = BLOCK_ENDS.get(line.tokens[0]);
        if (blockEnd) {
            currentBlock = { end: blockEnd, line: line.lineNumber, name: line.tokens[0] };
            line.kind = "block-start";
            continue;
        }
        if ([...BLOCK_ENDS.values()].includes(line.tokens[0])) {
            line.kind = "block-end";
            addDiagnostic(diagnostics, "error", "surface.block.end", `${line.tokens[0]} has no matching block start`, line.lineNumber, documentPath);
            continue;
        }
        if (line.tokens[0] === "OSKLayout") {
            if (layout) addDiagnostic(diagnostics, "error", "surface.layout.duplicate", "Surface can contain only one OSKLayout block", line.lineNumber, documentPath);
            const properties = parseProperties(line.tokens.slice(1));
            const layoutVersion = propertyValue(properties, "Version") ?? "";
            layout = { line: line.lineNumber, rows: [], version: layoutVersion };
            if (!layoutVersion) addDiagnostic(diagnostics, "error", "surface.layout.version.missing", "OSKLayout requires Version", line.lineNumber, documentPath);
            else if (layoutVersion !== "1") addDiagnostic(diagnostics, "error", "surface.layout.version", `Unsupported OSK layout version: ${layoutVersion}`, line.lineNumber, documentPath);
            line.kind = "block-start";
            inLayout = true;
            continue;
        }
        if (line.tokens[0] === "Widget") {
            const split = splitWidgetComment(analysisText(line));
            const definitionTokens = tokenizeLine(split.definition);
            if (!definitionTokens[1]) {
                addDiagnostic(diagnostics, "error", "surface.widget.name", "Widget requires a name", line.lineNumber, documentPath);
                continue;
            }
            currentWidget = { body: [], line: line.lineNumber, name: definitionTokens[1], oskProperties: parseProperties(tokenizeLine(split.properties)), widgetClass: definitionTokens[2] };
            widgets.push(currentWidget);
            line.kind = "block-start";
            continue;
        }
        if (line.tokens[0] === "WidgetEnd" || line.tokens[0] === "OSKLayoutEnd" || line.tokens[0] === "RowEnd") {
            line.kind = "block-end";
            addDiagnostic(diagnostics, "error", "surface.block.end", `${line.tokens[0]} has no matching block start`, line.lineNumber, documentPath);
            continue;
        }
        line.kind = "unknown";
        addDiagnostic(diagnostics, "warning", "surface.line.unknown", `Unknown surface line: ${text}`, line.lineNumber, documentPath);
    }

    if (!markerLine) addDiagnostic(diagnostics, "warning", "surface.format.missing", "Surface has no // @format surface 1 marker", undefined, documentPath);
    if (currentBlock) addDiagnostic(diagnostics, "error", "surface.block.unclosed", `${currentBlock.name} block has no ${currentBlock.end}`, currentBlock.line, documentPath);
    if (currentWidget) addDiagnostic(diagnostics, "error", "surface.widget.unclosed", `Widget ${currentWidget.name} has no WidgetEnd`, currentWidget.line, documentPath);
    if (inLayout) addDiagnostic(diagnostics, "error", "surface.layout.unclosed", "OSKLayout has no OSKLayoutEnd", layout?.line, documentPath);

    const widgetsByName = new Map<string, SurfaceWidget>();
    for (const widget of widgets) {
        const lowercaseName = widget.name.toLowerCase();
        const existing = widgetsByName.get(lowercaseName);
        if (existing) addDiagnostic(diagnostics, "error", "surface.widget.duplicate", `Widget names differ only by case or are duplicated: ${existing.name}, ${widget.name}`, widget.line, documentPath);
        else widgetsByName.set(lowercaseName, widget);
    }
    for (const row of layout?.rows ?? []) {
        for (const cell of row.cells) {
            if (cell.type !== "widget" || !cell.widgetName) continue;
            if (!widgetsByName.has(cell.widgetName.toLowerCase())) addDiagnostic(diagnostics, "error", "surface.layout.widget.missing", `OSK layout references unknown widget: ${cell.widgetName}`, cell.line, documentPath);
            for (const [propertyName, supportedTypes] of OSK_TARGET_CAPABILITIES) {
                const targetName = propertyValue(cell.properties, propertyName);
                if (!targetName) continue;
                const targetWidget = widgetsByName.get(targetName.toLowerCase());
                if (!targetWidget) addDiagnostic(diagnostics, "error", "surface.layout.target.missing", `${propertyName} references unknown widget: ${targetName}`, cell.line, documentPath);
                else if (!supportsOskTarget(targetWidget, supportedTypes)) addDiagnostic(diagnostics, "error", "surface.layout.target.capability", `${propertyName} widget does not provide the required input: ${targetName}`, cell.line, documentPath);
            }
        }
    }
    if (layout && legacyOskLayout) addDiagnostic(diagnostics, "warning", "surface.layout.mixed", "Surface contains both formal and legacy OSK layout data", layout.line, documentPath);

    return { diagnostics, format: "surface", lines, path: documentPath, semantic: { legacyOskLayout, oskLayout: layout, widgets }, source, version };
}
