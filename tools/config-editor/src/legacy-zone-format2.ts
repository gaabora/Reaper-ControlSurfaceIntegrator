import path from "node:path";
import { isIgnoredLegacyAction, renameLegacyAction } from "./legacy-action-renames.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import { analysisText, initializeLine, splitSourceLines } from "./text.ts";

export interface LegacyZoneFormat2Options {
    bankContexts?: string[];
    isLayer?: boolean;
    profile: "FX" | "Main";
    targetPath: string;
}

export interface LegacyZoneFormat2Conversion {
    diagnostics: Diagnostic[];
    source: string;
}

const INPUT_SELECTORS = new Set(["Press", "Tap", "Release", "Hold", "LongHold", "DoublePress", "Increase", "Decrease", "Invert", "InvertFB"]);
const CONTEXT_SELECTORS = new Set(["Shift", "Option", "Control", "Alt", "Flip", "Global", "Marker", "Nudge", "Zoom", "Scrub", "Touch", "Toggle"]);
const MODIFIER_ACTIONS = new Set(["Shift", "Option", "Control", "Alt", "Flip", "Global", "Marker", "Nudge", "Zoom", "Scrub"]);
const STANDALONE_NAVIGATORS = new Set(["TrackNavigator", "SelectedTrackNavigator", "MasterTrackNavigator", "FocusedFXNavigator"]);
const LIFECYCLE_EVENTS = new Map([
    ["OnInitialization", "SurfaceInitialization"],
    ["OnTrackSelection", "TrackSelection"],
    ["OnPageEnter", "PageEnter"],
    ["OnPageLeave", "PageExit"],
    ["OnPlayStart", "PlaybackStart"],
    ["OnPlayStop", "PlaybackStop"],
    ["OnRecordStart", "RecordStart"],
    ["OnRecordStop", "RecordStop"],
    ["OnZoneActivation", "ZoneActivation"],
    ["OnZoneDeactivation", "ZoneDeactivation"],
]);
const MAGIC_MAIN_METADATA = new Map<string, string[]>([
    ["home", ["Role=Home"]],
    ["lasttouchedfxparam", ["Role=LastTouchedFXParam"]],
    ["track", ["Target=Tracks"]],
    ["selectedtrack", ["Target=SelectedTrack"]],
    ["mastertrack", ["Target=MasterTrack"]],
    ["vca", ["Target=VCA"]],
    ["folder", ["Target=Folder"]],
    ["selectedtracks", ["Target=SelectedTracks"]],
    ["tracksend", ["Target=Tracks", "BankTarget=Sends"]],
    ["trackreceive", ["Target=Tracks", "BankTarget=Receives"]],
    ["trackfxmenu", ["Target=Tracks", "BankTarget=FX"]],
    ["selectedtracksend", ["Target=SelectedTrack", "BankTarget=Sends"]],
    ["selectedtrackreceive", ["Target=SelectedTrack", "BankTarget=Receives"]],
    ["selectedtrackfxmenu", ["Target=SelectedTrack", "BankTarget=FX"]],
    ["mastertrackfxmenu", ["Target=MasterTrack", "BankTarget=FX"]],
]);

export function legacyMainBankContext(zoneName: string): { bankTarget: string; target: string } | undefined {
    const context = legacyMainTargetContext(zoneName);
    return context.target && context.bankTarget ? { bankTarget: context.bankTarget, target: context.target } : undefined;
}

export function legacyMainTargetContext(zoneName: string): { bankTarget?: string; target?: string } {
    const metadata = MAGIC_MAIN_METADATA.get(zoneName.toLowerCase()) ?? [];
    const target = metadata.find((entry) => entry.startsWith("Target="))?.slice("Target=".length);
    const bankTarget = metadata.find((entry) => entry.startsWith("BankTarget="))?.slice("BankTarget=".length);
    return { bankTarget, target };
}
const NAVIGATOR_TARGETS = new Map([
    ["track", "Tracks"],
    ["tracknavigator", "Tracks"],
    ["selectedtrack", "SelectedTrack"],
    ["selectedtracknavigator", "SelectedTrack"],
    ["mastertrack", "MasterTrack"],
    ["mastertracknavigator", "MasterTrack"],
    ["focusedfx", "FocusedFX"],
    ["focusedfxnavigator", "FocusedFX"],
]);

function inlineComment(text: string): { comment: string; content: string } {
    let insideQuote = false;
    let escaped = false;
    for (let idx = 0; idx + 1 < text.length; idx++) {
        const character = text[idx];
        if (escaped) escaped = false;
        else if (insideQuote && character === "\\") escaped = true;
        else if (character === "\"") insideQuote = !insideQuote;
        else if (!insideQuote && character === "/" && text[idx + 1] === "/") return { comment: text.slice(idx).trimEnd(), content: text.slice(0, idx).trimEnd() };
    }
    return { comment: "", content: text.trimEnd() };
}

function decimal(value: string, requireFraction = false): string | undefined {
    if (!/^-?\d+(?:\.\d+)?$/.test(value)) return undefined;
    const normalized = Number(value).toString();
    return requireFraction && !normalized.includes(".") ? `${normalized}.0` : normalized;
}

function propertyList(name: string, values: string[], spacesInside = false): string {
    return `${name}=[${spacesInside ? " " : ""}${values.join(", ")}${spacesInside ? " " : ""}]`;
}

function rgbColor(red: string, green: string, blue: string): string | undefined {
    const channels = [red, green, blue].map(Number);
    if (channels.some((channel) => !Number.isInteger(channel) || channel < 0 || channel > 255)) return undefined;
    return `#${channels.map((channel) => channel.toString(16).padStart(2, "0").toUpperCase()).join("")}`;
}

function convertAnonymousValues(tokens: string[], lineNumber: number, documentPath: string, diagnostics: Diagnostic[]): string[] {
    const result: string[] = [];
    for (let tokenIdx = 0; tokenIdx < tokens.length; tokenIdx++) {
        const token = tokens[tokenIdx];
        if (token === "{") {
            const end = tokens.indexOf("}", tokenIdx + 1);
            if (end < 0) {
                addDiagnostic(diagnostics, "error", "legacy.zone.color.unclosed", "Legacy color group has no closing brace.", lineNumber, documentPath);
                return result.concat(tokens.slice(tokenIdx));
            }
            const values = tokens.slice(tokenIdx + 1, end);
            if (values.length === 1 && values[0] === "Track") result.push("StateColors=[ Track ]");
            else if (values.length > 0 && values.length % 3 === 0) {
                const colors: string[] = [];
                for (let colorIdx = 0; colorIdx < values.length; colorIdx += 3) {
                    const color = rgbColor(values[colorIdx], values[colorIdx + 1], values[colorIdx + 2]);
                    if (!color) break;
                    colors.push(color);
                }
                if (colors.length * 3 === values.length) result.push(propertyList("StateColors", colors, true));
                else addDiagnostic(diagnostics, "error", "legacy.zone.color.invalid", "Legacy color channels must be integers from 0 through 255.", lineNumber, documentPath);
            } else addDiagnostic(diagnostics, "error", "legacy.zone.color.invalid", "Legacy color group must contain Track or complete RGB triples.", lineNumber, documentPath);
            tokenIdx = end;
            continue;
        }
        if (token !== "[") {
            result.push(normalizeProperty(token));
            continue;
        }
        const end = tokens.indexOf("]", tokenIdx + 1);
        if (end < 0) {
            addDiagnostic(diagnostics, "error", "legacy.zone.values.unclosed", "Legacy action value group has no closing bracket.", lineNumber, documentPath);
            return result.concat(tokens.slice(tokenIdx));
        }
        const values = tokens.slice(tokenIdx + 1, end);
        const ranges: string[] = [];
        const parenthesis: string[][] = [];
        const stepValues: string[] = [];
        let invalid = false;
        for (const value of values) {
            const range = value.match(/^(-?\d+(?:\.\d+)?)>(-?\d+(?:\.\d+)?)$/);
            const list = value.match(/^\(([^()]*)\)$/);
            if (range) {
                const first = decimal(range[1], true);
                const second = decimal(range[2], true);
                const minimum = first && second && Number(first) > Number(second) ? second : first;
                const maximum = first && second && Number(first) > Number(second) ? first : second;
                if (!minimum || !maximum) invalid = true;
                else ranges.push(minimum, maximum);
            } else if (list) {
                const entries = list[1].split(",").map((entry) => entry.trim());
                if (!entries.length || entries.some((entry) => decimal(entry) === undefined)) invalid = true;
                else parenthesis.push(entries.map((entry) => decimal(entry, entry.includes("."))!));
            } else {
                const normalized = decimal(value, value.includes("."));
                if (!normalized) invalid = true;
                else stepValues.push(normalized);
            }
        }
        if (invalid || ranges.length > 2 || parenthesis.length > 1) addDiagnostic(diagnostics, "error", "legacy.zone.values.ambiguous", "Legacy action values cannot be converted without guessing. Edit this binding before import.", lineNumber, documentPath);
        else {
            if (ranges.length) result.push(propertyList("Range", ranges));
            if (stepValues.length === 1 && !ranges.length && !parenthesis.length) result.push(`Delta=${stepValues[0]}`);
            else if (stepValues.length) result.push(propertyList("StepValues", stepValues));
            if (parenthesis.length) {
                const valuesAreIntegers = parenthesis[0].every((value) => Number.isInteger(Number(value)));
                if (valuesAreIntegers && stepValues.length) result.push(propertyList("TicksPerStep", parenthesis[0]));
                else if (parenthesis[0].length === 1) result.push(`Delta=${parenthesis[0][0]}`);
                else result.push(propertyList("AccelerationDeltas", parenthesis[0]));
            }
        }
        tokenIdx = end;
    }
    return result;
}

function normalizeProperty(token: string): string {
    token = token.replace(/#([0-9a-fA-F]{6})[0-9a-fA-F]{2}\b/g, "#$1");
    if (token.startsWith("HoldDelay=")) return `DelayMs=${token.slice(10)}`;
    if (token.startsWith("HoldRepeatInterval=")) return `RepeatIntervalMs=${token.slice(19)}`;
    if (token === "NoFeedback") return "Feedback=No";
    if (/^[A-Za-z][A-Za-z0-9]*=\[.*\]$/.test(token)) return token;
    if (/\s/.test(token)) {
        const separator = token.indexOf("=");
        if (separator > 0) return `${token.slice(0, separator)}=${JSON.stringify(token.slice(separator + 1))}`;
        return JSON.stringify(token);
    }
    return token;
}

function convertWidgetExpression(expression: string, inferHold: boolean, declaredModifiers?: ReadonlySet<string>): string {
    const parts = expression.split("+").filter(Boolean);
    const widgetName = (parts.pop() ?? "").replace(/\|$/, "#");
    const widget = declaredModifiers?.has(widgetName) ? `[${widgetName}]` : widgetName;
    let prefix = "";
    if (inferHold && !parts.includes("Hold") && !parts.includes("LongHold")) prefix += "(Hold)+";
    for (const selector of parts) {
        if (INPUT_SELECTORS.has(selector)) prefix += `(${selector})+`;
        else if (CONTEXT_SELECTORS.has(selector)) prefix += `[${selector}]+`;
        else prefix += `[${selector}]+`;
    }
    return prefix + widget;
}

function metadataFor(zoneName: string, headerTokens: string[], options: LegacyZoneFormat2Options, diagnostics: Diagnostic[], lineNumber?: number): string[] {
    if (options.isLayer) return ["Role=Layer"];
    if (options.profile === "FX") return [`MatchFX=${JSON.stringify(zoneName)}`];
    const metadata = [...(MAGIC_MAIN_METADATA.get(zoneName.toLowerCase()) ?? [])];
    const navigatorProperty = headerTokens.find((token) => /^NavType=/i.test(token));
    const navigator = navigatorProperty?.slice(navigatorProperty.indexOf("=") + 1) ?? headerTokens.find((token) => STANDALONE_NAVIGATORS.has(token));
    const target = navigator ? NAVIGATOR_TARGETS.get(navigator.toLowerCase()) : undefined;
    if (target && !metadata.some((entry) => entry.startsWith("Role=") || entry.startsWith("Target="))) metadata.push(`Target=${target}`);
    else if (navigator && !target && !metadata.length) addDiagnostic(diagnostics, "error", "legacy.zone.navigator.unsupported", `Legacy navigator cannot be converted to a format 2 Target: ${navigator}`, lineNumber, options.targetPath);
    return metadata;
}

export function convertLegacyZoneToFormat2(source: string, options: LegacyZoneFormat2Options): LegacyZoneFormat2Conversion {
    const diagnostics: Diagnostic[] = [];
    const lines = splitSourceLines(source);
    const firstConfigurationLine = lines.map(analysisText).find((text) => text && !text.startsWith("//"));
    if (firstConfigurationLine?.replace(/^\uFEFF/, "").startsWith("@Meta")) return { diagnostics, source };
    const header = lines.find((line) => {
        initializeLine(line);
        return line.tokens[0] === "Zone";
    });
    const fallbackName = path.basename(options.targetPath, path.extname(options.targetPath));
    const zoneName = header?.tokens[1] ?? fallbackName;
    if (!header) addDiagnostic(diagnostics, "error", "legacy.zone.header.missing", "Legacy Zone has no Zone header.", undefined, options.targetPath);
    const metadata = metadataFor(zoneName, header?.tokens.slice(2) ?? [], options, diagnostics, header?.lineNumber);
    const target = metadata.find((entry) => entry.startsWith("Target="))?.slice("Target=".length);
    const bankTarget = metadata.find((entry) => entry.startsWith("BankTarget="))?.slice("BankTarget=".length);
    const declaredModifiers = new Set(lines.flatMap((line) => {
        initializeLine(line);
        return MODIFIER_ACTIONS.has(line.tokens[1]) ? [line.tokens[1]] : [];
    }));
    const output: string[] = [`@Meta { Version=2${metadata.length ? ` ${metadata.join(" ")}` : ""} }`, ""];
    let section: "included" | "layers" | undefined;
    let sectionStart = 0;
    let sectionEntries = 0;

    const appendBlank = (): void => {
        if (output.at(-1) !== "") output.push("");
    };

    for (const line of lines) {
        const text = initializeLine(line);
        if (/^\/\/\s*@format\s+zone\s+1$/i.test(text)) continue;
        if (!text || line.kind === "comment") {
            if (line.kind === "comment") output.push(text);
            else appendBlank();
            continue;
        }
        const keyword = line.tokens[0];
        if (keyword === "Zone" || keyword === "ZoneEnd") continue;
        if (keyword === "IncludedZones" || keyword === "SubZones") {
            appendBlank();
            section = keyword === "IncludedZones" ? "included" : "layers";
            sectionStart = output.length;
            sectionEntries = 0;
            output.push(section === "included" ? "IncludedZones {" : "ZoneLayers {");
            continue;
        }
        if (keyword === "IncludedZonesEnd" || keyword === "SubZonesEnd") {
            if (!section || (keyword === "IncludedZonesEnd") !== (section === "included")) {
                addDiagnostic(diagnostics, "error", "legacy.zone.reference.end", `${keyword} has no matching relation block.`, line.lineNumber, options.targetPath);
                continue;
            }
            if (sectionEntries) output.push("}");
            else output.splice(sectionStart, 1);
            appendBlank();
            section = undefined;
            continue;
        }
        if (section) {
            if (line.tokens[0]) {
                output.push(`  ${line.tokens[0]}`);
                sectionEntries++;
            }
            continue;
        }
        if (STANDALONE_NAVIGATORS.has(keyword) && line.tokens.length === 1) continue;
        if (keyword?.startsWith("#")) {
            addDiagnostic(diagnostics, "error", "legacy.zone.learn-template", "Legacy Learn FX template directives must be imported as LearnFX.fxzon, not as a normal Zone.", line.lineNumber, options.targetPath);
            continue;
        }
        if (line.tokens.length < 2) {
            addDiagnostic(diagnostics, "error", "legacy.zone.line.unknown", `Legacy Zone line cannot be converted: ${text}`, line.lineNumber, options.targetPath);
            continue;
        }
        const lifecycle = LIFECYCLE_EVENTS.get(keyword);
        const { comment, content } = inlineComment(line.text);
        const sourceTokens = splitSourceLines(content)[0];
        if (!sourceTokens) continue;
        initializeLine(sourceTokens);
        const action = sourceTokens.tokens[1];
        if (isIgnoredLegacyAction(action)) continue;
        let actionTokens = convertAnonymousValues(sourceTokens.tokens.slice(2), line.lineNumber, options.targetPath, diagnostics).map((token) => token.replace(/\|$/, "#"));
        const invalidLayerExit = action === "LeaveSubZone" && !options.isLayer;
        let invalidBankContextMessage = "";
        const renamedAction = renameLegacyAction(action, actionTokens, { bankTarget, isLayer: options.isLayer ?? false, target });
        const convertedAction = renamedAction.action;
        actionTokens = renamedAction.arguments;
        if (action === "Bank" && actionTokens.length >= 2 && !actionTokens[0].includes("=")) {
            const bankTarget = MAGIC_MAIN_METADATA.get(actionTokens[0].toLowerCase());
            const contexts = options.bankContexts ?? (options.isLayer ? [] : [zoneName]);
            const sameContext = options.profile === "Main" && bankTarget && contexts.length > 0 && contexts.every((context) => MAGIC_MAIN_METADATA.get(context.toLowerCase())?.join(" ") === bankTarget.join(" "));
            if (sameContext) actionTokens = actionTokens.slice(1);
            else invalidBankContextMessage = `This Zone runs in ${contexts.join(", ") || "an unknown"} context, but Bank ${actionTokens[0]} needs that Zone's Target and BankTarget. Move this Bank binding to ${actionTokens[0]}.`;
        }
        const actionText = [convertedAction, ...actionTokens].filter(Boolean).join(" ");
        if (lifecycle) {
            appendBlank();
            if (invalidLayerExit) addDiagnostic(diagnostics, "error", "legacy.zone.exit.context", `Zone ${zoneName} is not a layer. Choose whether this binding returns Home or the Zone becomes a Layer.`, output.length + 2, options.targetPath);
            if (invalidBankContextMessage) addDiagnostic(diagnostics, "error", "legacy.zone.bank.context", invalidBankContextMessage, output.length + 2, options.targetPath);
            output.push(`On ${lifecycle} {`, `  ${actionText}`, "}", "");
            continue;
        }
        const inferHold = sourceTokens.tokens.slice(2).some((token) => token.startsWith("HoldDelay=") || token.startsWith("HoldRepeatInterval="));
        const modifierDeclaration = MODIFIER_ACTIONS.has(action);
        const widget = convertWidgetExpression(sourceTokens.tokens[0], inferHold, modifierDeclaration ? undefined : declaredModifiers);
        const binding = modifierDeclaration ? `${widget} Modifier ${action}${actionTokens.length ? ` ${actionTokens.join(" ")}` : ""}` : `${widget} ${actionText}`;
        if (invalidLayerExit) addDiagnostic(diagnostics, "error", "legacy.zone.exit.context", `Zone ${zoneName} is not a layer. Choose whether this binding returns Home or the Zone becomes a Layer.`, output.length + 1, options.targetPath);
        if (invalidBankContextMessage) addDiagnostic(diagnostics, "error", "legacy.zone.bank.context", invalidBankContextMessage, output.length + 1, options.targetPath);
        output.push(comment ? `${binding} ${comment}` : binding);
    }
    while (output.length > 1 && output.at(-1) === "") output.pop();
    return { diagnostics, source: output.join("\n") + "\n" };
}
