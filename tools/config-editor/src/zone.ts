import path from "node:path";
import type { ActionTraits } from "./action-catalog.ts";
import { validateFormat2ZoneGestures } from "./format2-zone-gesture.ts";
import { addDiagnostic, type Diagnostic, type LosslessDocument } from "./model.ts";
import type { SettingsSchema } from "./settings-schema.ts";
import { analysisText, initializeLine, parseFormatMarker, parseProperties, propertyValue, splitSourceLines, tokenizeLine } from "./text.ts";

export interface ZoneBinding {
    action: string;
    inputSelectors: string[];
    line: number;
    modifiers: string[];
    params: string[];
    properties: Map<string, string>;
    widget: string;
}

export interface ZoneDependencyReference {
    line: number;
    name: string;
    type: "EnterZoneLayer" | "GoSubZone" | "GoZone" | "IncludedZones" | "SubZones" | "ZoneLayers";
}

export interface ZoneSemantic {
    alias?: string;
    bindings: ZoneBinding[];
    dependencyReferences: ZoneDependencyReference[];
    dependencies: string[];
    includedZones: string[];
    name?: string;
    navigator?: string;
    role?: string;
    subZones: string[];
    target?: string;
}

function parseWidgetExpression(expression: string): { modifiers: string[]; widget: string } {
    const parts = expression.split("+").filter(Boolean);
    return { modifiers: parts.slice(0, -1), widget: parts.at(-1) ?? "" };
}

const LEARN_TEMPLATE_DIRECTIVES = new Set(["#WidgetType", "#DisplayRow", "#RingStyle", "#DisplayFont", "#SupportsColor"]);

export function isLearnTemplateDirective(keyword: string | undefined): boolean {
    return Boolean(keyword && LEARN_TEMPLATE_DIRECTIVES.has(keyword));
}

function braceDelta(text: string): number {
    let delta = 0;
    let insideQuote = false;
    let escaped = false;
    for (const character of text) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (insideQuote && character === "\\") {
            escaped = true;
            continue;
        }
        if (character === "\"") insideQuote = !insideQuote;
        else if (!insideQuote && character === "{") delta++;
        else if (!insideQuote && character === "}") delta--;
    }
    return delta;
}

function format2MetadataEntries(text: string): Array<{ key: string; value: string }> {
    const entries: Array<{ key: string; value: string }> = [];
    for (const match of text.matchAll(/(?:^|\s)([A-Za-z][A-Za-z0-9]*)=("(?:\\.|[^"\\])*"|[^\s}]+)/g)) {
        let value = match[2];
        if (value.startsWith("\"")) {
            try {
                value = JSON.parse(value) as string;
            } catch {
                value = "";
            }
        }
        entries.push({ key: match[1], value });
    }
    return entries;
}

function parseFormat2BindingExpression(expression: string): { inputSelectors: string[]; modifiers: string[]; widget: string } {
    const inputSelectors: string[] = [];
    const modifiers: string[] = [];
    let remaining = expression;
    for (;;) {
        const modifier = remaining.match(/^\[([^\]]+)\]\+/);
        if (modifier) {
            modifiers.push(modifier[1]);
            remaining = remaining.slice(modifier[0].length);
            continue;
        }
        const input = remaining.match(/^\(([^)]+)\)\+/);
        if (input) {
            inputSelectors.push(input[1]);
            remaining = remaining.slice(input[0].length);
            continue;
        }
        return { inputSelectors, modifiers, widget: remaining };
    }
}

function parseFormat2Zone(source: string, documentPath?: string, knownActions?: Set<string>, settingsSchema?: SettingsSchema, actionTraits: ReadonlyMap<string, ActionTraits> = new Map()): LosslessDocument<ZoneSemantic> {
    const lines = splitSourceLines(source);
    const diagnostics: Diagnostic[] = [];
    const semantic: ZoneSemantic = { bindings: [], dependencies: [], dependencyReferences: [], includedZones: [], subZones: [] };
    semantic.name = documentPath ? path.basename(documentPath, path.extname(documentPath)) : undefined;
    let metadataDepth = 0;
    let metadataLine: number | undefined;
    let relation: "included" | "layers" | undefined;
    let relationDepth = 0;
    let lifecycleDepth = 0;
    const metadata = new Map<string, { line: number; value: string }>();
    const buttonEvents = new Set(["Press", "Tap", "Release", "Hold", "LongHold", "DoublePress"]);
    const directionEvents = new Set(["Increase", "Decrease"]);
    const inputTransforms = new Set(["Invert", "InvertFB"]);
    const standardModifiers = new Set(["Shift", "Option", "Control", "Alt", "Flip", "Marker", "Nudge", "Scrub", "Zoom", "Global", "Touch", "Toggle"]);

    const addMetadata = (text: string, lineNumber: number): void => {
        for (const entry of format2MetadataEntries(text)) {
            if (metadata.has(entry.key)) addDiagnostic(diagnostics, "error", "format2.metadata.duplicate", `Metadata key is duplicated: ${entry.key}`, lineNumber, documentPath);
            else metadata.set(entry.key, { line: lineNumber, value: entry.value });
        }
    };

    const addRelationEntry = (name: string, lineNumber: number): void => {
        if (!/^[A-Za-z][A-Za-z0-9_-]*$/.test(name)) {
            addDiagnostic(diagnostics, "error", "format2.zone.reference.id", `Zone reference is not a valid ID: ${name}`, lineNumber, documentPath);
            return;
        }
        const type = relation === "included" ? "IncludedZones" : "ZoneLayers";
        if (relation === "included") semantic.includedZones.push(name);
        else semantic.subZones.push(name);
        semantic.dependencies.push(name);
        semantic.dependencyReferences.push({ line: lineNumber, name, type });
    };

    for (const line of lines) {
        const text = initializeLine(line);
        if (!text || line.kind === "comment") continue;
        const delta = braceDelta(text);
        if (!metadataLine) {
            if (line.tokens[0] !== "@Meta") {
                line.kind = "unknown";
                addDiagnostic(diagnostics, "error", "format2.metadata.required", "The first configuration line must be an @Meta block", line.lineNumber, documentPath);
                continue;
            }
            metadataLine = line.lineNumber;
            metadataDepth = delta;
            line.kind = "format";
            if (!text.includes("{")) addDiagnostic(diagnostics, "error", "format2.metadata.block", "@Meta must start a brace block", line.lineNumber, documentPath);
            addMetadata(text, line.lineNumber);
            continue;
        }
        if (metadataDepth > 0) {
            addMetadata(text, line.lineNumber);
            metadataDepth += delta;
            line.kind = metadataDepth > 0 ? "format" : "block-end";
            continue;
        }
        if (relation) {
            const entries = line.tokens.filter((token) => token !== "{" && token !== "}");
            for (const entry of entries) addRelationEntry(entry, line.lineNumber);
            relationDepth += delta;
            line.kind = relationDepth > 0 ? "entry" : "block-end";
            if (relationDepth <= 0) relation = undefined;
            continue;
        }
        if (lifecycleDepth > 0) {
            lifecycleDepth += delta;
            line.kind = lifecycleDepth > 0 ? "entry" : "block-end";
            const action = line.tokens[0];
            if (lifecycleDepth > 0 && action && knownActions && !knownActions.has(action)) addDiagnostic(diagnostics, "warning", "zone.action.unknown", `Unknown runtime action: ${action}`, line.lineNumber, documentPath);
            continue;
        }
        if (line.tokens[0] === "IncludedZones" || line.tokens[0] === "ZoneLayers") {
            relation = line.tokens[0] === "IncludedZones" ? "included" : "layers";
            relationDepth = delta;
            line.kind = "block-start";
            const openBrace = text.indexOf("{");
            const closeBrace = text.lastIndexOf("}");
            if (openBrace < 0) {
                addDiagnostic(diagnostics, "error", "format2.zone.reference.block", `${line.tokens[0]} must start a brace block`, line.lineNumber, documentPath);
                relation = undefined;
            } else {
                const inlineEnd = closeBrace > openBrace ? closeBrace : text.length;
                const inlineEntries = tokenizeLine(text.slice(openBrace + 1, inlineEnd));
                for (const entry of inlineEntries) addRelationEntry(entry, line.lineNumber);
                if (closeBrace > openBrace) relation = undefined;
            }
            continue;
        }
        if (line.tokens[0] === "On") {
            lifecycleDepth = delta;
            line.kind = "block-start";
            if (lifecycleDepth <= 0) addDiagnostic(diagnostics, "error", "format2.zone.lifecycle.block", "A lifecycle declaration must start a brace block", line.lineNumber, documentPath);
            continue;
        }
        if (text === "}") {
            line.kind = "block-end";
            addDiagnostic(diagnostics, "error", "format2.zone.block.end", "Closing brace has no matching block", line.lineNumber, documentPath);
            continue;
        }
        if (line.tokens.length < 2) {
            line.kind = "unknown";
            addDiagnostic(diagnostics, "error", "format2.zone.binding", `A binding requires a Widget and an action: ${text}`, line.lineNumber, documentPath);
            continue;
        }
        const expressionText = text.match(/^\S+/)?.[0] ?? "";
        const actionLineTokens = tokenizeLine(text.slice(expressionText.length).trimStart());
        const expression = parseFormat2BindingExpression(expressionText);
        const action = actionLineTokens[0];
        const actionTokens = actionLineTokens.slice(1);
        const properties = parseProperties(actionTokens);
        const params = actionTokens.filter((token) => !token.includes("="));
        semantic.bindings.push({ action, inputSelectors: expression.inputSelectors, line: line.lineNumber, modifiers: expression.modifiers, params, properties, widget: expression.widget });
        line.kind = "entry";
        for (const propertyName of ["DelayMs", "RepeatIntervalMs", "RunCount"]) if (new RegExp(`(?:^|\\s)${propertyName}\\s*=\\s*"`).test(text)) addDiagnostic(diagnostics, "error", "format2.zone.gesture.integer-property", `${propertyName} must be one complete unquoted integer`, line.lineNumber, documentPath);
        if (!/^[A-Za-z][A-Za-z0-9_-]*#?$/.test(expression.widget)) addDiagnostic(diagnostics, "error", "format2.zone.widget.selector", `Widget selector must be an exact ID or one terminal # channel family: ${expression.widget}`, line.lineNumber, documentPath);
        const selectedButtonEvents = expression.inputSelectors.filter((selector) => buttonEvents.has(selector));
        if (selectedButtonEvents.length > 1) addDiagnostic(diagnostics, "error", "format2.zone.binding.event", "A binding cannot select more than one button event", line.lineNumber, documentPath);
        const selectedDirections = expression.inputSelectors.filter((selector) => directionEvents.has(selector));
        if (selectedDirections.length > 1) addDiagnostic(diagnostics, "error", "format2.zone.binding.direction", "A binding cannot contain both Increase and Decrease", line.lineNumber, documentPath);
        if (selectedButtonEvents.length && selectedDirections.length) addDiagnostic(diagnostics, "error", "format2.zone.binding.event-direction", "A button event cannot be combined with a relative direction", line.lineNumber, documentPath);
        if (new Set(expression.inputSelectors).size !== expression.inputSelectors.length) addDiagnostic(diagnostics, "error", "format2.zone.binding.selector.duplicate", "A binding selector is repeated", line.lineNumber, documentPath);
        for (const selector of expression.inputSelectors) if (!buttonEvents.has(selector) && !directionEvents.has(selector) && !inputTransforms.has(selector)) addDiagnostic(diagnostics, "error", "format2.zone.binding.input", `Unknown input selector: ${selector}`, line.lineNumber, documentPath);
        for (const modifier of expression.modifiers) if (!standardModifiers.has(modifier)) addDiagnostic(diagnostics, "error", "format2.zone.binding.modifier", `Unknown modifier selector: ${modifier}`, line.lineNumber, documentPath);
        if (knownActions && !knownActions.has(action)) addDiagnostic(diagnostics, "warning", "zone.action.unknown", `Unknown runtime action: ${action}`, line.lineNumber, documentPath);
        if ((action === "GoZone" || action === "EnterZoneLayer") && params[0]) {
            semantic.dependencies.push(params[0]);
            semantic.dependencyReferences.push({ line: line.lineNumber, name: params[0], type: action });
        }
    }

    if (!metadataLine) addDiagnostic(diagnostics, "error", "format2.metadata.required", "Format 2 Zone requires @Meta", undefined, documentPath);
    if (metadataDepth > 0) addDiagnostic(diagnostics, "error", "format2.metadata.unclosed", "@Meta block has no closing brace", metadataLine, documentPath);
    if (relation) addDiagnostic(diagnostics, "error", "format2.zone.reference.unclosed", "Zone relation block has no closing brace", undefined, documentPath);
    if (lifecycleDepth > 0) addDiagnostic(diagnostics, "error", "format2.zone.lifecycle.unclosed", "Lifecycle block has no closing brace", undefined, documentPath);
    const version = metadata.get("Version");
    if (!version || version.value !== "2") addDiagnostic(diagnostics, "error", "format2.metadata.version", "@Meta requires Version=2", version?.line ?? metadataLine, documentPath);
    semantic.role = metadata.get("Role")?.value;
    semantic.target = metadata.get("Target")?.value;
    semantic.alias = metadata.get("Alias")?.value;
    const matchFx = metadata.get("MatchFX")?.value;
    if (semantic.role && semantic.target) addDiagnostic(diagnostics, "error", "format2.metadata.role-target", "Role and Target cannot be used together", metadata.get("Target")?.line, documentPath);
    if (matchFx && (semantic.role || semantic.target)) addDiagnostic(diagnostics, "error", "format2.zone.fx.metadata", "An FX zone cannot declare Main zone Role or Target metadata", metadata.get("Role")?.line ?? metadata.get("Target")?.line, documentPath);
    if (semantic.role === "Layer" && semantic.includedZones.length) addDiagnostic(diagnostics, "error", "format2.zone.layer.included", "A Zone Layer cannot declare IncludedZones", semantic.dependencyReferences.find((reference) => reference.type === "IncludedZones")?.line, documentPath);
    validateFormat2ZoneGestures(semantic.bindings, actionTraits, diagnostics, settingsSchema, documentPath);
    semantic.dependencies = [...new Set(semantic.dependencies)];
    return { diagnostics, format: "zone", lines, path: documentPath, semantic, source, version: "2" };
}

export function parseZone(source: string, documentPath?: string, knownActions?: Set<string>, settingsSchema?: SettingsSchema, actionTraits?: ReadonlyMap<string, ActionTraits>): LosslessDocument<ZoneSemantic> {
    const firstConfigurationLine = splitSourceLines(source).map(analysisText).find((text) => text && !text.startsWith("//"));
    if (firstConfigurationLine?.replace(/^\uFEFF/, "").startsWith("@Meta")) return parseFormat2Zone(source, documentPath, knownActions, settingsSchema, actionTraits);
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
        semantic.bindings.push({ action, inputSelectors: [], line: line.lineNumber, modifiers: widgetExpression.modifiers, params, properties, widget: widgetExpression.widget });
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
