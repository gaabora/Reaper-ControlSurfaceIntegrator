import { addDiagnostic, type Diagnostic, type SourceLine } from "./model.ts";
import { initializeLine, propertyValue, splitSourceLines } from "./text.ts";

export interface LegacySce24RingMigration {
    diagnostics: Diagnostic[];
    source: string;
}

interface LegacyRingBinding {
    colors: Array<string | undefined>;
    existingRingColors: boolean;
    invalid: boolean;
    line: SourceLine;
    lineIdx: number;
    touched: boolean;
    widget: string;
    widgetExpression: string;
}

const DEFAULT_RING_COLOR = "#000000";
const LEGACY_COLOR_PROPERTY = /\s+(?:LEDRingColor|LEDRingColors|PushColor)=(?:"[^"]*"|\S+)/gi;

function canonicalColor(value: string | undefined): string | undefined {
    const match = value?.match(/^#([0-9a-f]{6})(?:[0-9a-f]{2})?$/i);
    return match ? `#${match[1].toUpperCase()}` : undefined;
}

function addMigrationError(diagnostics: Diagnostic[], code: string, message: string, binding: LegacyRingBinding, documentPath?: string): void {
    binding.invalid = true;
    addDiagnostic(diagnostics, "error", code, message, binding.line.lineNumber, documentPath);
}

function assignColor(binding: LegacyRingBinding, position: number, color: string, diagnostics: Diagnostic[], documentPath?: string): void {
    if (binding.colors[position] && binding.colors[position] !== color) {
        addMigrationError(diagnostics, "legacy.zone.sce24-ring.overlap", `Several legacy colors target SCE24 ring position ${position}. Keep only one color for each position.`, binding, documentPath);
        return;
    }
    binding.colors[position] = color;
    binding.touched = true;
}

function applyUniformColor(binding: LegacyRingBinding, value: string | undefined, firstPosition: number, lastPosition: number, propertyName: string, diagnostics: Diagnostic[], documentPath?: string): void {
    const color = canonicalColor(value);
    if (!color) {
        addMigrationError(diagnostics, "legacy.zone.sce24-ring.color", `${propertyName} requires #RRGGBB or legacy #RRGGBBAA.`, binding, documentPath);
        return;
    }
    for (let position = firstPosition; position <= lastPosition; position++) assignColor(binding, position, color, diagnostics, documentPath);
}

function applyRangeColors(binding: LegacyRingBinding, value: string | undefined, diagnostics: Diagnostic[], documentPath?: string): void {
    if (!value) {
        addMigrationError(diagnostics, "legacy.zone.sce24-ring.ranges", "LEDRingColors requires one or more start-end-color ranges.", binding, documentPath);
        return;
    }
    for (const range of value.split("+")) {
        const match = range.match(/^(\d+)-(\d+)-(#[0-9a-f]{6}(?:[0-9a-f]{2})?)$/i);
        if (!match) {
            addMigrationError(diagnostics, "legacy.zone.sce24-ring.ranges", `Invalid LEDRingColors range: ${range}. Use start-end-#RRGGBB.`, binding, documentPath);
            continue;
        }
        const firstPosition = Number(match[1]);
        const lastPosition = Number(match[2]);
        const color = canonicalColor(match[3])!;
        if (firstPosition > lastPosition || firstPosition < 0 || lastPosition > 17) {
            addMigrationError(diagnostics, "legacy.zone.sce24-ring.range", `SCE24 ring range ${firstPosition}-${lastPosition} must stay inside 0-17 and start before it ends.`, binding, documentPath);
        }
        for (let position = Math.max(0, firstPosition); position <= Math.min(17, lastPosition); position++) assignColor(binding, position, color, diagnostics, documentPath);
    }
}

function appendRingColors(text: string, colors: string[]): string {
    const commentOffset = text.indexOf("//");
    const content = (commentOffset >= 0 ? text.slice(0, commentOffset) : text).replace(LEGACY_COLOR_PROPERTY, "").trimEnd();
    const comment = commentOffset >= 0 ? ` ${text.slice(commentOffset)}` : "";
    return `${content} RingColors=[ ${colors.join(", ")} ]${comment}`;
}

function createBinding(lines: SourceLine[], lineIdx: number): LegacyRingBinding | undefined {
    const line = lines[lineIdx];
    initializeLine(line, ["#WidgetType", "#DisplayRow", "#RingStyle", "#DisplayFont", "#SupportsColor"]);
    if (line.kind === "comment" || line.tokens.length < 2 || line.tokens[0] === "Zone" || line.tokens[0] === "ZoneEnd" || line.tokens[0].startsWith("#")) return undefined;
    const widgetExpression = line.tokens[0];
    const widget = widgetExpression.split("+").at(-1) ?? "";
    return { colors: Array.from<string | undefined>({ length: 18 }), existingRingColors: line.tokens.slice(2).some((token) => /^RingColors=/i.test(token)), invalid: false, line, lineIdx, touched: false, widget, widgetExpression };
}

export function migrateLegacySce24RingColors(source: string, documentPath?: string): LegacySce24RingMigration {
    const diagnostics: Diagnostic[] = [];
    const lines = splitSourceLines(source);
    const bindings = lines.map((_, lineIdx) => createBinding(lines, lineIdx)).filter((binding): binding is LegacyRingBinding => Boolean(binding));
    const bindingsByExpression = new Map(bindings.map((binding) => [binding.widgetExpression.toLowerCase(), binding]));
    const movedPushColors: Array<{ source: LegacyRingBinding; target: LegacyRingBinding }> = [];

    for (const binding of bindings) {
        const properties = new Map<string, string>();
        for (const token of binding.line.tokens.slice(2)) {
            const separator = token.indexOf("=");
            if (separator > 0) properties.set(token.slice(0, separator), token.slice(separator + 1));
        }
        const uniformRingColor = propertyValue(properties, "LEDRingColor");
        const rangeRingColors = propertyValue(properties, "LEDRingColors");
        const pushColor = propertyValue(properties, "PushColor");
        if (!uniformRingColor && !rangeRingColors && !pushColor) continue;
        if (binding.existingRingColors) {
            addMigrationError(diagnostics, "legacy.zone.sce24-ring.existing", "RingColors cannot be combined with legacy LEDRingColor, LEDRingColors, or PushColor.", binding, documentPath);
            continue;
        }
        if (uniformRingColor && rangeRingColors) {
            addMigrationError(diagnostics, "legacy.zone.sce24-ring.sources", "Use either LEDRingColor or LEDRingColors on one binding, not both.", binding, documentPath);
            continue;
        }

        if (/^RotaryPush/i.test(binding.widget)) {
            if (uniformRingColor || rangeRingColors) addMigrationError(diagnostics, "legacy.zone.sce24-ring.push-widget", "LEDRingColor and LEDRingColors belong to the paired Rotary widget, not RotaryPush.", binding, documentPath);
            if (!pushColor || binding.invalid) continue;
            const pairedWidget = binding.widget.replace(/^RotaryPush/i, "Rotary");
            const pairedExpression = binding.widgetExpression.slice(0, binding.widgetExpression.length - binding.widget.length) + pairedWidget;
            let pairedBinding = bindingsByExpression.get(pairedExpression.toLowerCase());
            if (!pairedBinding) {
                const indentation = binding.line.text.match(/^\s*/)?.[0] ?? "";
                const insertedLine: SourceLine = { ending: binding.line.ending, kind: "entry", lineNumber: binding.line.lineNumber, text: `${indentation}${pairedExpression} NoAction`, tokens: [pairedExpression, "NoAction"] };
                pairedBinding = { colors: Array.from<string | undefined>({ length: 18 }), existingRingColors: false, invalid: false, line: insertedLine, lineIdx: binding.lineIdx, touched: false, widget: pairedWidget, widgetExpression: pairedExpression };
                bindingsByExpression.set(pairedExpression.toLowerCase(), pairedBinding);
                bindings.push(pairedBinding);
            }
            if (pairedBinding.existingRingColors) {
                addMigrationError(diagnostics, "legacy.zone.sce24-ring.existing", `PushColor cannot be moved because ${pairedExpression} already has RingColors.`, binding, documentPath);
                continue;
            }
            applyUniformColor(pairedBinding, pushColor, 0, 2, "PushColor", diagnostics, documentPath);
            movedPushColors.push({ source: binding, target: pairedBinding });
            continue;
        }

        if (!/^Rotary/i.test(binding.widget)) {
            addMigrationError(diagnostics, "legacy.zone.sce24-ring.widget", "SCE24 ring colors require a Rotary or RotaryPush widget.", binding, documentPath);
            continue;
        }
        if (uniformRingColor) applyUniformColor(binding, uniformRingColor, 3, 17, "LEDRingColor", diagnostics, documentPath);
        if (rangeRingColors) applyRangeColors(binding, rangeRingColors, diagnostics, documentPath);
        if (pushColor) applyUniformColor(binding, pushColor, 0, 2, "PushColor", diagnostics, documentPath);
    }

    for (const moved of movedPushColors) if (!moved.source.invalid && !moved.target.invalid) moved.source.line.text = moved.source.line.text.replace(LEGACY_COLOR_PROPERTY, "");

    const insertions = new Map<number, LegacyRingBinding[]>();
    for (const binding of bindings) {
        if (!binding.touched || binding.invalid) continue;
        const colors = binding.colors.map((color) => color ?? DEFAULT_RING_COLOR);
        if (lines[binding.lineIdx] === binding.line) binding.line.text = appendRingColors(binding.line.text, colors);
        else {
            binding.line.text = appendRingColors(binding.line.text, colors);
            const pending = insertions.get(binding.lineIdx) ?? [];
            pending.push(binding);
            insertions.set(binding.lineIdx, pending);
        }
    }
    const output: string[] = [];
    for (let lineIdx = 0; lineIdx < lines.length; lineIdx++) {
        for (const binding of insertions.get(lineIdx) ?? []) output.push(binding.line.text + binding.line.ending);
        output.push(lines[lineIdx].text + lines[lineIdx].ending);
    }
    return { diagnostics, source: output.join("") };
}
