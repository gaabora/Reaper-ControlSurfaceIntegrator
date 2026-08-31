import { addDiagnostic, type Diagnostic } from "./model.ts";
import { analysisText, splitSourceLines, tokenizeLine } from "./text.ts";
import { parseSurface, type OskLayoutCell, type SurfaceWidget } from "./surface.ts";

export interface LegacySurfaceConversion {
    diagnostics: Diagnostic[];
    source: string;
}

function midiByte(value: string): string {
    const normalized = value.replace(/^0x/i, "");
    return `0x${normalized.padStart(2, "0").toUpperCase()}`;
}

function midiList(values: string[]): string {
    return `[ ${values.map(midiByte).join(", ")} ]`;
}

function numberList(values: string[]): string {
    return `[ ${values.join(", ")} ]`;
}

function propertyText(properties: Map<string, string>, ignored = new Set<string>()): string {
    const result: string[] = [];
    for (const [name, rawValue] of properties) {
        if (ignored.has(name.toLowerCase())) continue;
        let value = rawValue;
        if (name.toLowerCase() === "color") value = `#${rawValue.replace(/^#/, "").slice(0, 6).toUpperCase()}`;
        if (name.toLowerCase() === "label") value = JSON.stringify(rawValue);
        result.push(`${name}=${value}`);
    }
    return result.length ? ` ${result.join(" ")}` : "";
}

function legacyBlocks(source: string): Map<string, string[][]> {
    const result = new Map<string, string[][]>();
    let blockName = "";
    for (const line of splitSourceLines(source)) {
        const text = analysisText(line);
        const tokens = tokenizeLine(text);
        if (["StepSize", "AccelerationValues", "ColorCalibration"].includes(tokens[0] ?? "")) {
            blockName = tokens[0];
            result.set(blockName, []);
        } else if (tokens[0] === `${blockName}End`) blockName = "";
        else if (blockName && tokens.length) result.get(blockName)!.push(tokens);
    }
    return result;
}

function encoderProfiles(blocks: Map<string, string[][]>): string[] {
    const profiles = new Map<string, { decrease: string[]; delta?: string; increase: string[]; values: string[] }>();
    for (const tokens of blocks.get("StepSize") ?? []) profiles.set(tokens[0], { decrease: [], delta: tokens[1], increase: [], values: [] });
    for (const tokens of blocks.get("AccelerationValues") ?? []) {
        const profile = profiles.get(tokens[0]) ?? { decrease: [], increase: [], values: [] };
        if (tokens[1]?.toLowerCase() === "dec") profile.decrease = tokens.slice(2);
        else if (tokens[1]?.toLowerCase() === "inc") profile.increase = tokens.slice(2);
        else if (tokens[1]?.toLowerCase() === "val") profile.values = tokens.slice(2);
        profiles.set(tokens[0], profile);
    }
    const result: string[] = [];
    for (const [name, profile] of profiles) {
        result.push(`EncoderProfile ${name} {`);
        if (profile.delta) result.push(`  Delta=${profile.delta}`);
        if (profile.increase.length) result.push(`  Increase=${midiList(profile.increase)}`);
        if (profile.decrease.length) result.push(`  Decrease=${midiList(profile.decrease)}`);
        if (profile.values.length) result.push(`  AccelerationDeltas=${numberList(profile.values)}`);
        result.push("}", "");
    }
    return result;
}

function colorCalibration(blocks: Map<string, string[][]>): string[] {
    const entries = (blocks.get("ColorCalibration") ?? []).filter((tokens) => tokens[0]?.toLowerCase() !== "enabled");
    if (!entries.length) return [];
    return ["ColorCalibration {", ...entries.map((tokens) => `  ${tokens[0]}=${tokens[1]}`), "}", ""];
}

function convertProcessor(widget: SurfaceWidget, tokens: string[], lineNumber: number, diagnostics: Diagnostic[], documentPath: string): string | undefined {
    const processor = (tokens[0] ?? "").toLowerCase();
    if ((processor === "press" || processor === "anypress") && tokens.length >= 7) return `Input Press { Encoding=MIDIExact On=${midiList(tokens.slice(1, 4))} Off=${midiList(tokens.slice(4, 7))} }`;
    if (processor === "touch" && tokens.length >= 7) return `Input Touch { Encoding=MIDIExact On=${midiList(tokens.slice(1, 4))} Off=${midiList(tokens.slice(4, 7))} }`;
    if (processor === "fader14bit" && tokens[1]) return `Input Value { Encoding=MIDI14 Status=${midiByte(tokens[1])} }`;
    if (processor === "fb_fader14bit" && tokens[1]) return `Feedback Value { Encoding=MIDI14 Status=${midiByte(tokens[1])} SuppressWhileTouched=true }`;
    if (processor === "encoder" && tokens[1] && tokens[2]) {
        const profile = widget.widgetClass ? ` Profile=${widget.widgetClass}` : " Mode=SignedBit";
        return `Input Encoder { Encoding=MIDI7 Message=${midiList(tokens.slice(1, 3))}${profile} }`;
    }
    if (processor === "fb_twostate" && tokens.length >= 7) return `Feedback State { Encoding=MIDIExact On=${midiList(tokens.slice(1, 4))} Off=${midiList(tokens.slice(4, 7))} }`;
    if (processor === "fb_faderportrgb" && tokens.length >= 4) {
        const dataByte = midiByte(tokens[2]);
        return `Feedback Color { Encoding=MIDIRGB Enable=${midiList(tokens.slice(1, 4))} Red=${midiList(["91", dataByte])} Green=${midiList(["92", dataByte])} Blue=${midiList(["93", dataByte])} }`;
    }
    addDiagnostic(diagnostics, "error", "legacy.surface.processor.unsupported", `Legacy Surface processor is not converted yet: ${tokens[0]}`, lineNumber, documentPath);
    return undefined;
}

function isHiddenWidget(sourceLines: ReturnType<typeof splitSourceLines>, widget: SurfaceWidget): boolean {
    return /\bOSKHidden\b/i.test(sourceLines[widget.line - 1]?.text ?? "");
}

function visibleWidgets(sourceLines: ReturnType<typeof splitSourceLines>, widgets: SurfaceWidget[]): SurfaceWidget[] {
    return widgets.filter((widget) => {
        if (isHiddenWidget(sourceLines, widget)) return false;
        return widget.body.some((line) => ["press", "anypress", "fader14bit", "encoder", "x32fader", "x32rotarytoencoder"].includes((line.tokens[0] ?? "").toLowerCase()));
    });
}

function convertedLayoutCell(cell: OskLayoutCell): string {
    if (cell.type === "spacer") return `    Spacer${propertyText(cell.properties)}`;
    return `    Widget ${cell.widgetName}${propertyText(cell.properties)}`;
}

function explicitLayout(source: string): string[] | undefined {
    const parsed = parseSurface(source);
    if (!parsed.semantic.oskLayout) return undefined;
    const result = ["OSKLayout {"];
    for (const row of parsed.semantic.oskLayout.rows) result.push("  Row {", ...row.cells.map(convertedLayoutCell), "  }");
    result.push("}");
    return result;
}

function automaticLayoutProperties(sourceLines: ReturnType<typeof splitSourceLines>, widget: SurfaceWidget, widgets: SurfaceWidget[], fader: boolean): Map<string, string> {
    const properties = new Map(widget.oskProperties);
    if (fader) {
        properties.set("Shape", "Fader");
        properties.set("Height", "7");
        if (widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "touch")) properties.set("TouchTarget", widget.name);
        properties.set("ValueTarget", widget.name);
    }
    if (widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase().includes("encoder"))) {
        properties.set("ScrollTarget", widget.name);
        const group = [...widget.oskProperties].find(([name]) => name.toLowerCase() === "group")?.[1];
        const pressTarget = group ? widgets.find((candidate) => candidate !== widget && isHiddenWidget(sourceLines, candidate) && [...candidate.oskProperties].some(([name, value]) => name.toLowerCase() === "group" && value.toLowerCase() === group.toLowerCase()) && candidate.body.some((line) => ["press", "anypress"].includes((line.tokens[0] ?? "").toLowerCase()))) : undefined;
        if (pressTarget) properties.set("PressTarget", pressTarget.name);
    }
    return properties;
}

function isFaderWidget(widget: SurfaceWidget): boolean {
    return widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase().includes("fader") && !(line.tokens[0] ?? "").toLowerCase().startsWith("fb_"));
}

function legacyCommentLayout(source: string, widgets: SurfaceWidget[]): string[] | undefined {
    const sourceLines = splitSourceLines(source);
    type LegacyLayoutCell = { properties?: Map<string, string>; type: "spacer" | "widget"; widget?: SurfaceWidget };
    const rows: LegacyLayoutCell[][] = [];
    let currentRow: LegacyLayoutCell[] | undefined;
    const widgetsByLine = new Map(widgets.map((widget) => [widget.line, widget]));
    for (const line of sourceLines) {
        const text = analysisText(line);
        if (/^\/\/\s*OSKRow\b/i.test(text)) {
            currentRow = [];
            rows.push(currentRow);
            continue;
        }
        const spacerMatch = text.match(/^\/\/\s*OSKSpacer(?:\s+Width=([^\s]+))?/i);
        if (spacerMatch && currentRow) {
            currentRow.push({ properties: new Map([["Width", spacerMatch[1] ?? "0.5"]]), type: "spacer" });
            continue;
        }
        const widget = widgetsByLine.get(line.lineNumber);
        if (widget && currentRow && !isHiddenWidget(sourceLines, widget) && !isFaderWidget(widget)) currentRow.push({ type: "widget", widget });
    }
    if (!rows.length) return undefined;
    const faders = widgets.filter((widget) => !isHiddenWidget(sourceLines, widget) && isFaderWidget(widget));
    for (let faderIdx = faders.length - 1; faderIdx >= 0; faderIdx--) rows[0].unshift({ type: "widget", widget: faders[faderIdx] });
    const result = ["OSKLayout {"];
    for (const row of rows) {
        result.push("  Row {");
        for (const cell of row) {
            if (cell.type === "spacer") result.push(`    Spacer${propertyText(cell.properties!)}`);
            else if (cell.widget) {
                const properties = automaticLayoutProperties(sourceLines, cell.widget, widgets, isFaderWidget(cell.widget));
                if (isFaderWidget(cell.widget)) properties.set("Height", String(Math.max(7, rows.length)));
                result.push(`    Widget ${cell.widget.name}${propertyText(properties)}`);
            }
        }
        result.push("  }");
    }
    result.push("}");
    return result;
}

function automaticLayout(source: string, widgets: SurfaceWidget[]): string[] {
    const sourceLines = splitSourceLines(source);
    const visible = visibleWidgets(sourceLines, widgets);
    if (!visible.length) return [];
    const faders = visible.filter(isFaderWidget);
    const normal = visible.filter((widget) => !faders.includes(widget));
    const rowCount = faders.length ? 7 : Math.max(1, Math.ceil(Math.sqrt(normal.length || visible.length)));
    const normalColumnCount = Math.max(1, Math.ceil(normal.length / rowCount));
    const rows: string[][] = Array.from({ length: Math.max(rowCount, Math.ceil(normal.length / normalColumnCount)) }, () => []);
    for (let faderIdx = 0; faderIdx < faders.length; faderIdx++) {
        const widget = faders[faderIdx];
        const metadata = automaticLayoutProperties(sourceLines, widget, widgets, true);
        metadata.set("Height", String(Math.max(7, rows.length)));
        rows[0].push(`    Widget ${widget.name}${propertyText(metadata)}`);
    }
    for (let widgetIdx = 0; widgetIdx < normal.length; widgetIdx++) {
        const rowIdx = widgetIdx % rows.length;
        const widget = normal[widgetIdx];
        rows[rowIdx].push(`    Widget ${widget.name}${propertyText(automaticLayoutProperties(sourceLines, widget, widgets, false))}`);
    }
    return ["OSKLayout {", ...rows.flatMap((row) => ["  Row {", ...row, "  }"]), "}"];
}

function legacyProtocol(document: ReturnType<typeof parseSurface>): "MIDI" | "OSC" {
    for (const widget of document.semantic.widgets) for (const line of widget.body) {
        const processor = (line.tokens[0] ?? "").toLowerCase();
        if (processor.startsWith("x32") || line.tokens.slice(1).some((token) => token.startsWith("/"))) return "OSC";
    }
    return "MIDI";
}

function inferredChannelCount(document: ReturnType<typeof parseSurface>): number {
    const suffixesByFamily = new Map<string, Set<number>>();
    for (const widget of document.semantic.widgets) {
        const match = widget.name.match(/^(.*?)(\d+)$/);
        if (!match) continue;
        const suffixes = suffixesByFamily.get(match[1]) ?? new Set<number>();
        suffixes.add(Number(match[2]));
        suffixesByFamily.set(match[1], suffixes);
    }
    let channels = 1;
    for (const suffixes of suffixesByFamily.values()) {
        let contiguousCount = 0;
        while (suffixes.has(contiguousCount + 1)) contiguousCount++;
        channels = Math.max(channels, contiguousCount);
    }
    return channels;
}

export function convertLegacySurfaceToFormat2(source: string, surfaceName: string, documentPath: string): LegacySurfaceConversion {
    if (/^\s*(?:\uFEFF)?@Meta\s*\{[^}]*\bVersion=2\b/i.test(source)) {
        const currentDocument = parseSurface(source, documentPath);
        return { diagnostics: currentDocument.diagnostics, source };
    }
    const document = parseSurface(source, documentPath);
    const diagnostics: Diagnostic[] = document.diagnostics.filter((diagnostic) => diagnostic.code !== "surface.format.missing");
    const blocks = legacyBlocks(source);
    const output: string[] = [`@Meta { Version=2 Protocol=${legacyProtocol(document)} Channels=${inferredChannelCount(document)} Name=${JSON.stringify(surfaceName)} }`, "", ...encoderProfiles(blocks), ...colorCalibration(blocks)];
    for (const widget of document.semantic.widgets) {
        output.push(`Widget ${widget.name} {`);
        for (const line of widget.body) {
            const converted = convertProcessor(widget, line.tokens, line.lineNumber, diagnostics, documentPath);
            if (converted) output.push(`  ${converted}`);
        }
        output.push("}", "");
    }
    output.push(...(explicitLayout(source) ?? legacyCommentLayout(source, document.semantic.widgets) ?? automaticLayout(source, document.semantic.widgets)), "");
    return { diagnostics, source: output.join("\n") };
}
