import { createHash } from "node:crypto";
import { readdir, readFile, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { parseByPath, type AnyDocument } from "./formats.ts";
import { addDiagnostic, serializeDocument, type Diagnostic } from "./model.ts";
import type { ConfigurationStore, OperationReport, SaveChange } from "./store.ts";
import { EditorOperationError } from "./store.ts";
import { diagnosticWithQuickFixes, diagnosticsWithQuickFixes } from "./quick-fixes.ts";
import { convertLegacySurfaceToFormat2, type LegacyMcuMeterMode } from "./legacy-surface-format2.ts";
import { migrateLegacySce24RingColors } from "./legacy-sce24-ring.ts";
import { analysisText, convertHashCommentLine, convertSingleSlashCommentLine, initializeLine, isStableId, splitSourceLines } from "./text.ts";
import { validateDocumentSet } from "./validation.ts";
import { isCompatible, normalizedWidgetName, surfaceWidgetSlots, type WidgetCapability } from "./widget-capabilities.ts";
import type { SurfaceSemantic, SurfaceWidget } from "./surface.ts";
import type { ZoneBinding, ZoneSemantic } from "./zone.ts";
import type { ProductTreeEntry } from "./paths.ts";

export type LegacyImportConflictAction = "create" | "rename" | "replace" | "skip";
export type LegacyImportKind = "surface" | "zone";

export interface LegacySurfaceSummary {
    fxZoneCount: number;
    name: string;
    stableId: string;
    zoneCount: number;
}

export type LegacyWidgetCapability = WidgetCapability;

export interface LegacyWidgetCandidate {
    capabilities: LegacyWidgetCapability[];
    name: string;
}

export interface LegacyWidgetMapping {
    sourceWidget: string;
    targetWidget: string;
}

export interface LegacyWidgetMappingIssue {
    candidates: LegacyWidgetCandidate[];
    occurrences: Array<{ line: number; path: string }>;
    reason: "incompatible" | "missing";
    requiredCapabilities: LegacyWidgetCapability[];
    selectedTarget?: string;
    sourceWidget: string;
}

export interface LegacyImportDependency {
    from: string;
    matches: string[];
    name: string;
    selected: boolean;
    type: "GoSubZone" | "GoZone" | "IncludedZones" | "SubZones";
}

export interface LegacyImportItem {
    diagnostics: Diagnostic[];
    id: string;
    kind: LegacyImportKind;
    originalSourceHash: string;
    selected: boolean;
    source: string;
    sourceHash: string;
    sourcePath: string;
    targetExists: boolean;
    targetHash: string | null;
    targetPath: string;
    zoneName?: string;
}

export interface LegacyImportPreview {
    dependencies: LegacyImportDependency[];
    diagnostics: Diagnostic[];
    includeSurface: boolean;
    items: LegacyImportItem[];
    root: string;
    selectedZonePaths: string[];
    surfaceName: string;
    surfaceStableId: string;
    targetProfileId: string;
    valid: boolean;
    widgetMappings: LegacyWidgetMappingIssue[];
    widgetTarget: "existing" | "imported";
}

export interface LegacyImportResolution {
    action: LegacyImportConflictAction;
    id: string;
    sourceHash: string;
    targetHash: string | null;
    targetPath?: string;
}

export interface LegacyImportRequest {
    drafts?: LegacyImportDraft[];
    includeSurface: boolean;
    resolutions: LegacyImportResolution[];
    selectedZonePaths: string[];
    surfaceName: string;
    targetPaths?: LegacyImportTargetPath[];
    targetProfileId?: string;
    widgetMappings: LegacyWidgetMapping[];
}

export interface LegacyImportDraft {
    originalSourceHash: string;
    source: string;
    sourcePath: string;
}

export interface LegacyImportTargetPath {
    sourcePath: string;
    targetPath: string;
}

interface LegacyZoneSourceFile {
    originalSourceHash: string;
    profile: "FX" | "Main";
    relativePath: string;
    source: string;
    sourcePath: string;
}

interface LegacySurfaceFiles {
    name: string;
    stableId: string;
    surface: { originalSourceHash: string; source: string; sourcePath: string };
    zones: LegacyZoneSourceFile[];
}

const NON_HARDWARE_WIDGETS = new Set(["ontrackselection", "onpageenter", "onpageleave", "oninitialization", "onplaystart", "onplaystop", "onrecordstart", "onrecordstop", "onzoneactivation", "onzonedeactivation", "nulldisplay"]);
const MODIFIER_ACTIONS = new Set(["shift", "option", "control", "alt", "flip", "global", "marker", "nudge", "zoom", "scrub"]);

function sha256(source: string): string {
    return createHash("sha256").update(source).digest("hex");
}

function stableId(sourceName: string): string {
    const normalized = sourceName.toLowerCase().replace(/[^a-z0-9_-]+/g, "-").replace(/^[-_]+|[-_]+$/g, "");
    if (!/^[a-z0-9][a-z0-9_-]*$/.test(normalized)) throw new EditorOperationError("legacy.surface-id", `Could not create a stable ID from legacy surface name: ${sourceName}`);
    return normalized;
}

const LEGACY_LEARN_DIRECTIVE = /^#(?:WidgetType|DisplayRow|RingStyle|DisplayFont|SupportsColor)(?:\s|$)/;

export function migrateLegacyCommentSyntax(source: string): string {
    const lines = splitSourceLines(source);
    for (const line of lines) {
        line.text = convertSingleSlashCommentLine(line.text);
        const text = analysisText(line);
        if (!LEGACY_LEARN_DIRECTIVE.test(text)) line.text = convertHashCommentLine(line.text);
    }
    return lines.map((line) => line.text + line.ending).join("");
}

export function migrateLegacyZoneSyntax(source: string): string {
    const lines = splitSourceLines(source);
    for (const line of lines) {
        initializeLine(line);
        if (line.tokens.some((token) => /^BarStyle=BiPolar$/i.test(token))) line.text = line.text.replace(/\bBarStyle=BiPolar\b/i, "BarStyle=Bipolar");
    }
    return lines.map((line) => line.text + line.ending).join("");
}

function resolveLegacyMcuMeterMode(zones: Array<{ source: string; sourcePath: string }>, draftMap: Map<string, LegacyImportDraft>): { diagnostics: Diagnostic[]; mode: LegacyMcuMeterMode } {
    const canonicalModes = new Map<string, LegacyMcuMeterMode>([["iconv1m", "IconV1M"], ["mcu", "MCU"], ["sslnucleus2", "SSLNucleus2"], ["xtouch", "XTouch"]]);
    const diagnostics: Diagnostic[] = [];
    const occurrences: Array<{ line: number; mode: LegacyMcuMeterMode; path: string }> = [];
    for (const zone of zones) {
        const source = draftMap.get(zone.sourcePath)?.source ?? zone.source;
        for (const line of splitSourceLines(source)) {
            initializeLine(line);
            const property = line.tokens.find((token) => /^MeterMode=/i.test(token));
            if (!property) continue;
            const rawMode = property.slice(property.indexOf("=") + 1);
            const mode = canonicalModes.get(rawMode.toLowerCase());
            if (!mode) addDiagnostic(diagnostics, "error", "legacy.surface.meter-mode.unknown", `Unknown legacy MeterMode: ${rawMode}. Use XTouch, MCU, SSLNucleus2, or IconV1M.`, line.lineNumber, zone.sourcePath);
            else occurrences.push({ line: line.lineNumber, mode, path: zone.sourcePath });
        }
    }
    const modes = [...new Set(occurrences.map((occurrence) => occurrence.mode))];
    if (modes.length > 1) {
        const first = occurrences[0];
        addDiagnostic(diagnostics, "error", "legacy.surface.meter-mode.conflict", `The legacy zones use several meter scales (${modes.join(", ")}), but each converted meter output references one fixed profile. Keep one MeterMode before import.`, first.line, first.path, occurrences.slice(1).map((occurrence) => ({ line: occurrence.line, path: occurrence.path })));
    }
    return { diagnostics, mode: modes[0] ?? "XTouch" };
}

function formatSource(source: string, kind: LegacyImportKind, targetPath: string, knownActions: Set<string>): string {
    const document = parseByPath(source, targetPath, knownActions);
    if (document.version !== "unversioned") return source;
    const bom = source.startsWith("\uFEFF") ? "\uFEFF" : "";
    const content = bom ? source.slice(1) : source;
    const firstEnding = content.match(/\r\n|\r|\n/)?.[0] ?? "\n";
    return `${bom}// @format ${kind} 1${firstEnding}${content}`;
}

function inferredBindingCapabilities(binding: ZoneBinding): LegacyWidgetCapability[] {
    const capabilities = new Set<LegacyWidgetCapability>();
    if (binding.modifiers.some((modifier) => modifier === "Hold" || modifier === "DoublePress")) capabilities.add("press-input");
    if (binding.modifiers.some((modifier) => modifier === "Decrease" || modifier === "Increase")) capabilities.add("relative-input");
    return [...capabilities].sort();
}

function legacyGoZoneNavigators(source: string): Map<string, string> | undefined {
    const lines = splitSourceLines(source);
    const content = lines.filter((line) => Boolean(initializeLine(line)) && line.kind !== "comment");
    if (content[0]?.tokens[0] !== "Zone" || content[0]?.tokens[1]?.toLowerCase() !== "gozones") return undefined;
    const navigators = new Map<string, string>();
    for (const line of content.slice(1)) {
        if (line.tokens[0] === "ZoneEnd") break;
        if (line.tokens[0] && line.tokens[1]) navigators.set(line.tokens[0].toLowerCase(), line.tokens[1]);
    }
    return navigators;
}

function legacyZoneName(source: string): string | undefined {
    for (const line of splitSourceLines(source)) {
        initializeLine(line);
        if (line.tokens[0] === "Zone") return line.tokens[1];
    }
    return undefined;
}

function addLegacyNavigator(source: string, navigator: string | undefined): string {
    if (!navigator) return source;
    const lines = splitSourceLines(source);
    for (const line of lines) {
        const text = initializeLine(line);
        if (line.tokens[0] !== "Zone") continue;
        if (/\bNavType=/i.test(text)) return source;
        const commentPosition = line.text.indexOf("//");
        const definition = commentPosition < 0 ? line.text : line.text.slice(0, commentPosition);
        const comment = commentPosition < 0 ? "" : line.text.slice(commentPosition);
        line.text = `${definition.trimEnd()} NavType=${navigator}${comment ? ` ${comment}` : ""}`;
        return lines.map((line) => `${line.text}${line.ending}`).join("");
    }
    return source;
}

function widgetMappingMap(widgetMappings: LegacyWidgetMapping[]): Map<string, string> {
    const result = new Map<string, string>();
    for (const mapping of widgetMappings) {
        if (!mapping.sourceWidget || !mapping.targetWidget) throw new EditorOperationError("legacy.widget.mapping.value", "Widget mappings require source and target widget names");
        const key = normalizedWidgetName(mapping.sourceWidget);
        if (result.has(key)) throw new EditorOperationError("legacy.widget.mapping.duplicate", `Widget mapping is duplicated: ${mapping.sourceWidget}`);
        result.set(key, mapping.targetWidget.trim());
    }
    return result;
}

function mappedTargetWidgetName(expression: string): string | undefined {
    if (!expression || /\s/.test(expression)) return undefined;
    const parts = expression.split("+");
    if (parts.some((part) => !part)) return undefined;
    return parts.at(-1);
}

function replaceMappedWidgets(source: string, document: AnyDocument, mappings: Map<string, string>): string {
    if (!mappings.size) return source;
    const semantic = document.semantic as ZoneSemantic;
    for (const binding of semantic.bindings) {
        const targetWidget = mappings.get(normalizedWidgetName(binding.widget));
        if (!targetWidget) continue;
        const line = document.lines[binding.line - 1];
        const tokenMatch = line?.text.match(/^(\s*)(\S+)/);
        if (!line || !tokenMatch) continue;
        const widgetExpression = tokenMatch[2];
        const modifierEnd = widgetExpression.lastIndexOf("+");
        const mappedExpression = `${modifierEnd >= 0 ? widgetExpression.slice(0, modifierEnd + 1) : ""}${targetWidget}`;
        line.text = `${tokenMatch[1]}${mappedExpression}${line.text.slice(tokenMatch[0].length)}`;
    }
    return serializeDocument(document);
}

function dependencyKey(dependency: LegacyImportDependency): string {
    return `${dependency.from.toLowerCase()}\0${dependency.type.toLowerCase()}\0${dependency.name.toLowerCase()}`;
}

function collectDependencies(relativePath: string, semantic: ZoneSemantic, matchesByName: Map<string, string[]>, selectedPaths: Set<string>): LegacyImportDependency[] {
    const dependencies: LegacyImportDependency[] = [];
    const append = (type: LegacyImportDependency["type"], name: string): void => {
        const matches = matchesByName.get(name.toLowerCase()) ?? [];
        dependencies.push({ from: relativePath, matches, name, selected: selectedPaths.has(relativePath), type });
    };
    for (const name of semantic.includedZones) append("IncludedZones", name);
    for (const name of semantic.subZones) append("SubZones", name);
    for (const binding of semantic.bindings) {
        if ((binding.action === "GoZone" || binding.action === "GoSubZone") && binding.params[0]) append(binding.action, binding.params[0]);
    }
    return dependencies;
}

function collectWidgetMappings(zoneDocuments: Map<string, AnyDocument>, selectedPaths: Set<string>, sourceSurface: AnyDocument, targetSurface: AnyDocument | undefined, requestedMappings: LegacyWidgetMapping[]): { diagnostics: Diagnostic[]; issues: LegacyWidgetMappingIssue[]; validMappings: Map<string, string> } {
    const diagnostics: Diagnostic[] = [];
    const requested = widgetMappingMap(requestedMappings);
    const occurrences = new Map<string, Array<{ binding: ZoneBinding; line: number; path: string }>>();
    for (const [sourcePath, document] of zoneDocuments) {
        if (!selectedPaths.has(sourcePath)) continue;
        const modifierAliases = new Set<string>();
        for (const binding of (document.semantic as ZoneSemantic).bindings) {
            const key = normalizedWidgetName(binding.widget);
            if (!NON_HARDWARE_WIDGETS.has(key) && !modifierAliases.has(key)) {
                const entries = occurrences.get(key) ?? [];
                entries.push({ binding, line: binding.line, path: sourcePath });
                occurrences.set(key, entries);
            }
            const actionKey = normalizedWidgetName(binding.action);
            if (MODIFIER_ACTIONS.has(actionKey)) modifierAliases.add(actionKey);
        }
    }

    const issues: LegacyWidgetMappingIssue[] = [];
    const validMappings = new Map<string, string>();
    for (const [sourceKey, entries] of occurrences) {
        const sourceName = entries[0].binding.widget;
        const patternSlots = sourceName.endsWith("|");
        const sourceSlot = surfaceWidgetSlots(sourceSurface, patternSlots).find((slot) => normalizedWidgetName(slot.name) === sourceKey);
        const targetSlots = targetSurface ? surfaceWidgetSlots(targetSurface, patternSlots) : [];
        const requiredCapabilities = [...new Set([...(sourceSlot?.capabilities ?? []), ...entries.flatMap((entry) => inferredBindingCapabilities(entry.binding))])].sort();
        const sameNameTarget = targetSlots.find((slot) => normalizedWidgetName(slot.name) === sourceKey);
        if (sameNameTarget && isCompatible(requiredCapabilities, sameNameTarget.capabilities)) continue;
        const candidates = targetSlots.filter((candidate) => isCompatible(requiredCapabilities, candidate.capabilities)).map((candidate) => ({ capabilities: candidate.capabilities, name: candidate.name })).sort((left, right) => left.name.localeCompare(right.name));
        const requestedTarget = requested.get(sourceKey);
        const requestedWidgetName = requestedTarget ? mappedTargetWidgetName(requestedTarget) : undefined;
        const selectedCandidate = requestedWidgetName ? candidates.find((candidate) => normalizedWidgetName(candidate.name) === normalizedWidgetName(requestedWidgetName)) : undefined;
        if (selectedCandidate && requestedTarget) validMappings.set(sourceKey, requestedTarget);
        const issue: LegacyWidgetMappingIssue = {
            candidates,
            occurrences: entries.map((entry) => ({ line: entry.line, path: entry.path })),
            reason: sameNameTarget ? "incompatible" : "missing",
            requiredCapabilities,
            selectedTarget: requestedTarget,
            sourceWidget: sourceSlot?.name ?? sourceName,
        };
        issues.push(issue);
        if (!selectedCandidate) {
            const occurrence = issue.occurrences[0];
            const reason = issue.reason === "missing" ? "is not present on the target surface" : "does not have the required capabilities on the target surface";
            addDiagnostic(diagnostics, "error", "legacy.widget.mapping.required", `Widget ${issue.sourceWidget} ${reason}. Choose a compatible target widget.`, occurrence.line, occurrence.path);
        }
    }
    return { diagnostics, issues: issues.sort((left, right) => left.sourceWidget.localeCompare(right.sourceWidget)), validMappings };
}

function widgetUsesMidiPalette(widget: SurfaceWidget): boolean {
    return widget.body.some((line) => (line.tokens[0] ?? "").toLowerCase() === "fb_mft_rgb" || ((line.tokens[0] ?? "").toLowerCase() === "feedback" && (line.tokens[1] ?? "").toLowerCase() === "color" && /\bEncoding=MIDIPalette\b/i.test(line.text)));
}

function targetUsesMidiPalette(surface: AnyDocument, widgetExpression: string): boolean {
    const widgets = (surface.semantic as SurfaceSemantic).widgets;
    const normalized = normalizedWidgetName(widgetExpression);
    if (!normalized.endsWith("|")) return widgets.some((widget) => normalizedWidgetName(widget.name) === normalized && widgetUsesMidiPalette(widget));
    const prefix = normalized.slice(0, -1);
    return widgets.some((widget) => normalizedWidgetName(widget.name).startsWith(prefix) && /^\d+$/.test(widget.name.slice(prefix.length)) && widgetUsesMidiPalette(widget));
}

function mftCommandValues(binding: ZoneBinding): number[][] {
    const start = binding.params.indexOf("{");
    const end = binding.params.indexOf("}", start + 1);
    if (start < 0 || end < 0) return [];
    const values = binding.params.slice(start + 1, end).map((value) => Number(value));
    if (!values.length || values.some((value) => !Number.isInteger(value))) return [];
    const commands: number[][] = [];
    for (let valueIdx = 0; valueIdx + 2 < values.length; valueIdx += 3) if ((values[valueIdx] === 177 || values[valueIdx] === 181) && values[valueIdx + 1] === 31) commands.push(values.slice(valueIdx, valueIdx + 3));
    return commands;
}

function addMftCommandDiagnostics(zoneDocuments: Map<string, AnyDocument>, selectedPaths: Set<string>, targetSurface: AnyDocument, validMappings: Map<string, string>): void {
    for (const [sourcePath, document] of zoneDocuments) {
        if (!selectedPaths.has(sourcePath)) continue;
        for (const binding of (document.semantic as ZoneSemantic).bindings) {
            const targetWidget = validMappings.get(normalizedWidgetName(binding.widget)) ?? binding.widget;
            if (!targetUsesMidiPalette(targetSurface, targetWidget)) continue;
            for (const command of mftCommandValues(binding)) {
                const bytes = command.map((value) => `0x${value.toString(16).padStart(2, "0").toUpperCase()}`).join(" ");
                addDiagnostic(document.diagnostics, "error", "legacy.zone.mft-color-command", `This RGB value is a raw MIDI command on the target palette widget: ${bytes}. Replace it with a normal color before import.`, binding.line, document.path);
            }
        }
    }
}

async function isDirectory(directoryPath: string): Promise<boolean> {
    try {
        return (await stat(directoryPath)).isDirectory();
    } catch (error) {
        if ((error as NodeJS.ErrnoException).code === "ENOENT") return false;
        throw error;
    }
}

function flattenTreeEntries(entries: ProductTreeEntry[], result: ProductTreeEntry[] = []): ProductTreeEntry[] {
    for (const entry of entries) {
        result.push(entry);
        if (entry.children) flattenTreeEntries(entry.children, result);
    }
    return result;
}

async function existingTargetZoneNames(store: ConfigurationStore, targetProfileId: string, replacedTargetPaths: Set<string>): Promise<Set<string>> {
    const entries = flattenTreeEntries(await store.tree());
    const profileKey = targetProfileId.toLowerCase();
    const userMainPrefix = `zones/user/${profileKey}/main/`;
    const hasUserMain = entries.some((entry) => entry.path.toLowerCase() === `zones/user/${profileKey}/main` || entry.path.toLowerCase().startsWith(userMainPrefix)) || [...replacedTargetPaths].some((targetPath) => targetPath.startsWith(userMainPrefix));
    const names = new Set<string>();
    for (const entry of entries) {
        if (entry.kind !== "file" || entry.type !== "zone") continue;
        const normalizedPath = entry.path.replaceAll("\\", "/");
        const location = normalizedPath.match(/^Zones\/(Vendor|User)\/([^/]+)\/(Main|FX)\//i);
        if (!location || location[2].toLowerCase() !== profileKey || replacedTargetPaths.has(normalizedPath.toLowerCase())) continue;
        if (hasUserMain && location[1].toLowerCase() === "vendor" && location[3].toLowerCase() === "main") continue;
        const opened = await store.openDocument(entry.path);
        const zoneName = (opened.document.semantic as ZoneSemantic).name;
        if (zoneName) names.add(zoneName.toLowerCase());
    }
    return names;
}

export class LegacyCsiSource {
    private constructor(private readonly root: string) {}

    static async create(selectedPath: string): Promise<LegacyCsiSource> {
        if (!path.isAbsolute(selectedPath)) throw new EditorOperationError("legacy.path.absolute", "Legacy CSI path must be absolute");
        let selectedStats: Awaited<ReturnType<typeof stat>>;
        try {
            selectedStats = await stat(selectedPath);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") throw new EditorOperationError("legacy.path.missing", "Legacy CSI path does not exist");
            throw error;
        }
        if (!selectedStats.isDirectory()) throw new EditorOperationError("legacy.path.directory", "Legacy CSI path must be a directory");
        const canonicalSelection = await realpath(selectedPath);
        const candidates = [canonicalSelection, path.join(canonicalSelection, "CSI")];
        for (const candidate of candidates) if (await isDirectory(candidate) && await isDirectory(path.join(candidate, "Surfaces"))) return new LegacyCsiSource(await realpath(candidate));
        throw new EditorOperationError("legacy.path.invalid", "Could not find a legacy CSI/Surfaces directory at the selected path");
    }

    getRoot(): string {
        return this.root;
    }

    async listSurfaces(): Promise<LegacySurfaceSummary[]> {
        const summaries: LegacySurfaceSummary[] = [];
        const surfacesRoot = path.join(this.root, "Surfaces");
        for (const entry of (await readdir(surfacesRoot, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
            const surfaceRoot = path.join(surfacesRoot, entry.name);
            if (!await isDirectory(surfaceRoot)) continue;
            if (!await this.isRegularFile(path.join(surfaceRoot, "Surface.txt"))) continue;
            const [zoneCount, fxZoneCount] = await Promise.all([this.countZones(path.join(surfaceRoot, "Zones")), this.countZones(path.join(surfaceRoot, "FXZones"))]);
            summaries.push({ fxZoneCount, name: entry.name, stableId: stableId(entry.name), zoneCount: zoneCount + fxZoneCount });
        }
        return summaries;
    }

    async preview(store: ConfigurationStore, knownActions: Set<string>, surfaceName: string, includeSurface: boolean, selectedZonePaths?: string[], requestedWidgetMappings: LegacyWidgetMapping[] = [], useExistingSurface = false, drafts: LegacyImportDraft[] = [], requestedProfileId?: string, requestedTargetPaths: LegacyImportTargetPath[] = []): Promise<LegacyImportPreview> {
        const files = await this.readSurfaceFiles(surfaceName);
        const targetProfileId = requestedProfileId || files.stableId;
        if (!isStableId(targetProfileId)) throw new EditorOperationError("legacy.target.profile", "Target profile ID must be a stable lowercase ASCII ID");
        const selectedPaths = new Set(selectedZonePaths ?? files.zones.map((zone) => zone.sourcePath));
        const availableZonePaths = new Set(files.zones.map((zone) => zone.sourcePath));
        for (const selectedPath of selectedPaths) if (!availableZonePaths.has(selectedPath)) throw new EditorOperationError("legacy.zone.missing", `Legacy zone is not available in ${surfaceName}: ${selectedPath}`);

        const sourceFiles = new Map<string, { kind: LegacyImportKind; originalSourceHash: string; source: string }>([[files.surface.sourcePath, { kind: "surface", originalSourceHash: files.surface.originalSourceHash, source: files.surface.source }]]);
        for (const zone of files.zones) sourceFiles.set(zone.sourcePath, { kind: "zone", originalSourceHash: zone.originalSourceHash, source: zone.source });
        const draftMap = new Map<string, LegacyImportDraft>();
        for (const draft of drafts) {
            const sourceFile = sourceFiles.get(draft.sourcePath);
            if (!sourceFile) throw new EditorOperationError("legacy.draft.source", `Import draft does not match a legacy source file: ${draft.sourcePath}`);
            if (draftMap.has(draft.sourcePath)) throw new EditorOperationError("legacy.draft.duplicate", `Import draft is duplicated: ${draft.sourcePath}`);
            if (sourceFile.originalSourceHash !== draft.originalSourceHash) throw new EditorOperationError("conflict.legacy-source", `Legacy source changed after its import draft was opened: ${draft.sourcePath}`);
            draftMap.set(draft.sourcePath, draft);
        }
        const targetPathMap = new Map<string, string>();
        for (const target of requestedTargetPaths) {
            if (!sourceFiles.has(target.sourcePath)) throw new EditorOperationError("legacy.target.source", `Import target does not match a legacy source file: ${target.sourcePath}`);
            if (targetPathMap.has(target.sourcePath)) throw new EditorOperationError("legacy.target.duplicate-source", `Import target is duplicated: ${target.sourcePath}`);
            targetPathMap.set(target.sourcePath, target.targetPath);
        }

        const meterMode = resolveLegacyMcuMeterMode(files.zones, draftMap);
        const surfaceTargetPath = targetPathMap.get(files.surface.sourcePath) || `Surfaces/User/${targetProfileId}.txt`;
        this.validateTargetScope("surface", surfaceTargetPath, targetProfileId);
        const surfaceConversion = convertLegacySurfaceToFormat2(draftMap.get(files.surface.sourcePath)?.source ?? files.surface.source, files.name, surfaceTargetPath, meterMode.mode);
        const migratedSurface = surfaceConversion.source;
        const surfaceDocument = parseByPath(migratedSurface, surfaceTargetPath, knownActions);
        surfaceDocument.diagnostics.push(...surfaceConversion.diagnostics);
        if (includeSurface && !useExistingSurface) surfaceDocument.diagnostics.push(...meterMode.diagnostics);
        const hasSce24Ring = /^\s*FB_SCE24Encoder\b/im.test(draftMap.get(files.surface.sourcePath)?.source ?? files.surface.source);
        const zoneDocuments = new Map<string, AnyDocument>();
        const zoneMigrationDiagnostics = new Map<string, Diagnostic[]>();
        const migratedZoneSources = new Map<string, string>();
        const zoneTargetPaths = new Map<string, string>();
        for (const zone of files.zones) {
            const targetPath = targetPathMap.get(zone.sourcePath) || `Zones/User/${targetProfileId}/${zone.profile}/${zone.relativePath}`;
            this.validateTargetScope("zone", targetPath, targetProfileId);
            const ringMigration = hasSce24Ring ? migrateLegacySce24RingColors(draftMap.get(zone.sourcePath)?.source ?? zone.source, zone.sourcePath) : { diagnostics: [], source: draftMap.get(zone.sourcePath)?.source ?? zone.source };
            zoneMigrationDiagnostics.set(zone.sourcePath, ringMigration.diagnostics);
            const migratedSource = formatSource(ringMigration.source, "zone", targetPath, knownActions);
            migratedZoneSources.set(zone.sourcePath, migratedSource);
            const zoneDocument = parseByPath(migratedSource, targetPath, knownActions);
            zoneDocument.diagnostics.push(...ringMigration.diagnostics);
            zoneDocuments.set(zone.sourcePath, zoneDocument);
            zoneTargetPaths.set(zone.sourcePath, targetPath);
        }

        const widgetTarget = includeSurface && !useExistingSurface ? "imported" : "existing";
        const targetSurface = widgetTarget === "imported" ? surfaceDocument : await this.readExistingTargetSurface(store, knownActions, targetProfileId);
        const widgetMappingResult = collectWidgetMappings(zoneDocuments, selectedPaths, surfaceDocument, targetSurface, requestedWidgetMappings);
        for (const [sourcePath, document] of zoneDocuments) {
            if (!selectedPaths.has(sourcePath)) continue;
            const source = replaceMappedWidgets(migratedZoneSources.get(sourcePath)!, document, widgetMappingResult.validMappings);
            migratedZoneSources.set(sourcePath, source);
            const mappedDocument = parseByPath(source, document.path!, knownActions);
            mappedDocument.diagnostics.push(...(zoneMigrationDiagnostics.get(sourcePath) ?? []));
            zoneDocuments.set(sourcePath, mappedDocument);
        }
        if (targetSurface) addMftCommandDiagnostics(zoneDocuments, selectedPaths, targetSurface, widgetMappingResult.validMappings);

        const matchesByName = new Map<string, string[]>();
        for (const [sourcePath, document] of zoneDocuments) {
            const semantic = document.semantic as ZoneSemantic;
            if (!semantic.name) continue;
            const matches = matchesByName.get(semantic.name.toLowerCase()) ?? [];
            matches.push(sourcePath);
            matchesByName.set(semantic.name.toLowerCase(), matches);
        }
        const dependenciesByKey = new Map<string, LegacyImportDependency>();
        for (const [sourcePath, document] of zoneDocuments) for (const dependency of collectDependencies(sourcePath, document.semantic as ZoneSemantic, matchesByName, selectedPaths)) dependenciesByKey.set(dependencyKey(dependency), dependency);

        const items: LegacyImportItem[] = [];
        const appendItem = async (kind: LegacyImportKind, sourcePath: string, originalSourceHash: string, targetPath: string, source: string, document: AnyDocument, selected: boolean, zoneName?: string): Promise<void> => {
            const targetState = await store.fileState(targetPath);
            items.push({ diagnostics: diagnosticsWithQuickFixes(document, knownActions, true), id: `${kind}:${sourcePath}`, kind, originalSourceHash, selected, source, sourceHash: sha256(source), sourcePath, targetExists: targetState.exists, targetHash: targetState.hash, targetPath, zoneName });
        };
        await appendItem("surface", files.surface.sourcePath, files.surface.originalSourceHash, surfaceTargetPath, migratedSurface, surfaceDocument, includeSurface);
        for (const zone of files.zones) {
            const document = zoneDocuments.get(zone.sourcePath)!;
            await appendItem("zone", zone.sourcePath, zone.originalSourceHash, zoneTargetPaths.get(zone.sourcePath)!, migratedZoneSources.get(zone.sourcePath)!, document, selectedPaths.has(zone.sourcePath), (document.semantic as ZoneSemantic).name);
        }

        const selectedDocuments = items.filter((item) => item.selected).map((item) => item.kind === "surface" ? surfaceDocument : zoneDocuments.get(item.sourcePath)!);
        const mappingSurfaceDocuments = widgetTarget === "existing" ? [...(!includeSurface ? [surfaceDocument] : []), ...(targetSurface ? [targetSurface] : [])] : [];
        const mappingSurfaceDiagnostics = mappingSurfaceDocuments.flatMap((document) => document.diagnostics).filter((diagnostic) => diagnostic.code !== "surface.format.missing" && diagnostic.code !== "zone.format.missing");
        const selectedDocumentsByPath = new Map<string, AnyDocument>(selectedDocuments.filter((document) => document.path).map((document) => [document.path!.toLowerCase(), document] as const));
        const replacedTargetPaths = new Set([...selectedPaths].map((sourcePath) => zoneTargetPaths.get(sourcePath)!.toLowerCase()));
        const availableTargetZoneNames = await existingTargetZoneNames(store, targetProfileId, replacedTargetPaths);
        const availableZoneNamesByProfile = new Map([[targetProfileId.toLowerCase(), availableTargetZoneNames]]);
        const setDiagnostics = validateDocumentSet(selectedDocuments, { availableZoneNamesByProfile }).map((diagnostic) => {
            const document = diagnostic.path ? selectedDocumentsByPath.get(diagnostic.path.toLowerCase()) : undefined;
            let contextualDiagnostic = diagnostic;
            if (document && diagnostic.code === "zones.dependency.missing") {
                const semantic = document.semantic as ZoneSemantic;
                const reference = semantic.dependencyReferences.find((candidate) => candidate.line === diagnostic.line);
                const matchingSources = reference ? matchesByName.get(reference.name.toLowerCase()) ?? [] : [];
                if (reference && matchingSources.length) {
                    const related = matchingSources.map((sourcePath) => ({ line: zoneDocuments.get(sourcePath)?.lines.find((line) => line.kind === "header")?.lineNumber, path: sourcePath }));
                    contextualDiagnostic = { ...diagnostic, message: `Zone "${semantic.name}" references "${reference.name}", but its matching legacy zone is not selected for import and no active target zone was found.`, related };
                }
            }
            return document ? diagnosticWithQuickFixes(document, contextualDiagnostic, knownActions, true) : contextualDiagnostic;
        });
        const diagnostics = selectedDocuments.flatMap((document) => diagnosticsWithQuickFixes(document, knownActions, true)).concat(mappingSurfaceDiagnostics, setDiagnostics, widgetMappingResult.diagnostics);
        const selectedTargetPaths = new Map<string, string>();
        for (const item of items.filter((candidate) => candidate.selected)) {
            const targetKey = item.targetPath.toLowerCase();
            const existingSourcePath = selectedTargetPaths.get(targetKey);
            if (existingSourcePath) addDiagnostic(diagnostics, "error", "legacy.target.duplicate", `Import target is used by both ${existingSourcePath} and ${item.sourcePath}`, undefined, item.targetPath);
            else selectedTargetPaths.set(targetKey, item.sourcePath);
        }
        if (widgetTarget === "existing" && selectedPaths.size && !targetSurface) addDiagnostic(diagnostics, "error", "legacy.widget.surface.missing", `Import Surface.txt or create Surfaces/User/${targetProfileId}.txt before importing its zones.`);
        return {
            dependencies: [...dependenciesByKey.values()].sort((left, right) => left.from.localeCompare(right.from) || left.type.localeCompare(right.type) || left.name.localeCompare(right.name)),
            diagnostics,
            includeSurface,
            items,
            root: this.root,
            selectedZonePaths: [...selectedPaths].sort(),
            surfaceName: files.name,
            surfaceStableId: files.stableId,
            targetProfileId,
            valid: !diagnostics.some((diagnostic) => diagnostic.severity === "error"),
            widgetMappings: widgetMappingResult.issues,
            widgetTarget,
        };
    }

    async import(store: ConfigurationStore, knownActions: Set<string>, request: LegacyImportRequest): Promise<OperationReport> {
        const resolutions = new Map(request.resolutions.map((resolution) => [resolution.id, resolution]));
        if (resolutions.size !== request.resolutions.length) throw new EditorOperationError("legacy.resolution.duplicate", "Legacy import resolutions must have unique IDs");
        let preview = await this.preview(store, knownActions, request.surfaceName, request.includeSurface, request.selectedZonePaths, request.widgetMappings, false, request.drafts ?? [], request.targetProfileId, request.targetPaths ?? []);
        const surfaceItem = preview.items.find((item) => item.kind === "surface");
        const surfaceResolution = surfaceItem ? resolutions.get(surfaceItem.id) : undefined;
        if (request.includeSurface && surfaceResolution && (surfaceResolution.action === "rename" || surfaceResolution.action === "skip")) preview = await this.preview(store, knownActions, request.surfaceName, request.includeSurface, request.selectedZonePaths, request.widgetMappings, true, request.drafts ?? [], request.targetProfileId, request.targetPaths ?? []);
        if (!preview.valid) throw new EditorOperationError("validation.failed", "Legacy import contains configuration errors", preview.diagnostics);
        const changes: SaveChange[] = [];
        const skipped: string[] = [];
        for (const item of preview.items.filter((candidate) => candidate.selected)) {
            const resolution = resolutions.get(item.id);
            if (!resolution) throw new EditorOperationError("legacy.resolution.required", `Choose how to import ${item.sourcePath}`);
            if (resolution.sourceHash !== item.sourceHash) throw new EditorOperationError("conflict.legacy-source", `Legacy source changed after preview: ${item.sourcePath}`);
            if (resolution.targetHash !== item.targetHash) throw new EditorOperationError("conflict.legacy-target", `Import target changed after preview: ${item.targetPath}`);
            if (!item.targetExists && resolution.action !== "create") throw new EditorOperationError("legacy.resolution.create", `New import target must use Create: ${item.targetPath}`);
            if (item.targetExists && !["rename", "replace", "skip"].includes(resolution.action)) throw new EditorOperationError("legacy.resolution.conflict", `Choose Rename, Replace, or Skip for ${item.targetPath}`);
            if (resolution.action === "skip") {
                skipped.push(item.targetPath);
                continue;
            }
            if (resolution.action === "rename") {
                const renameTarget = resolution.targetPath;
                if (!renameTarget) throw new EditorOperationError("legacy.rename.path", `Rename requires a target path for ${item.sourcePath}`);
                this.validateRenameScope(item, renameTarget, preview.targetProfileId);
                const renamedDocument = store.validateSource(renameTarget, item.source);
                if (renamedDocument.format !== item.kind) throw new EditorOperationError("legacy.rename.kind", `Rename target has the wrong file type: ${renameTarget}`);
                const targetState = await store.fileState(renameTarget);
                if (targetState.exists) throw new EditorOperationError("conflict.exists", `Rename target already exists: ${renameTarget}`);
                changes.push({ originalHash: null, path: renameTarget, source: item.source });
                continue;
            }
            changes.push({ originalHash: item.targetHash, path: item.targetPath, source: item.source });
        }
        for (const resolution of request.resolutions) if (!preview.items.some((item) => item.selected && item.id === resolution.id)) throw new EditorOperationError("legacy.resolution.unknown", `Import resolution does not match a selected source: ${resolution.id}`);
        if (!changes.length) return { changed: [], created: [], failed: [], restored: [], skipped };
        const report = await store.saveTransaction(changes);
        report.skipped.push(...skipped);
        return report;
    }

    private validateTargetScope(kind: LegacyImportKind, targetPath: string, targetProfileId: string): void {
        if (kind === "surface" && targetPath !== `Surfaces/User/${targetProfileId}.txt`) throw new EditorOperationError("legacy.target.surface", `The surface target must be Surfaces/User/${targetProfileId}.txt`);
        if (kind === "zone" && (!targetPath.startsWith(`Zones/User/${targetProfileId}/`) || !targetPath.endsWith(".zon"))) throw new EditorOperationError("legacy.target.zone", `A zone target must be a .zon file below Zones/User/${targetProfileId}`);
    }

    private validateRenameScope(item: LegacyImportItem, targetPath: string, targetProfileId: string): void {
        if (item.kind === "surface" && (!targetPath.startsWith("Surfaces/User/") || !targetPath.endsWith(".txt"))) throw new EditorOperationError("legacy.rename.scope", "A surface rename target must be a .txt file below Surfaces/User");
        if (item.kind === "zone" && (!targetPath.startsWith(`Zones/User/${targetProfileId}/`) || !targetPath.endsWith(".zon"))) throw new EditorOperationError("legacy.rename.scope", `A zone rename target must be a .zon file below Zones/User/${targetProfileId}`);
    }

    private async readExistingTargetSurface(store: ConfigurationStore, knownActions: Set<string>, surfaceStableId: string): Promise<AnyDocument | undefined> {
        for (const targetPath of [`Surfaces/User/${surfaceStableId}.txt`, `Surfaces/Vendor/${surfaceStableId}.txt`]) {
            const state = await store.fileState(targetPath);
            if (!state.exists) continue;
            const opened = await store.openDocument(targetPath);
            return parseByPath(opened.source, targetPath, knownActions);
        }
        return undefined;
    }

    private async readSurfaceFiles(surfaceName: string): Promise<LegacySurfaceFiles> {
        if (!surfaceName || surfaceName.includes("/") || surfaceName.includes("\\") || surfaceName === "." || surfaceName === "..") throw new EditorOperationError("legacy.surface.name", "Legacy surface name is invalid");
        const surfaceRoot = path.join(this.root, "Surfaces", surfaceName);
        const stats = await stat(surfaceRoot).catch((error: NodeJS.ErrnoException) => {
            if (error.code === "ENOENT") throw new EditorOperationError("legacy.surface.missing", `Legacy surface does not exist: ${surfaceName}`);
            throw error;
        });
        if (!stats.isDirectory()) throw new EditorOperationError("legacy.surface.directory", `Legacy surface is not a directory: ${surfaceName}`);
        const surfacePath = path.join(surfaceRoot, "Surface.txt");
        if (!await this.isRegularFile(surfacePath)) throw new EditorOperationError("legacy.surface.file", `Legacy surface has no regular Surface.txt file: ${surfaceName}`);
        const mainZones = await this.readZones(path.join(surfaceRoot, "Zones"), "Main", "Zones");
        const navigators = new Map<string, string>();
        const zones: LegacyZoneSourceFile[] = [];
        for (const zone of mainZones) {
            const manifestNavigators = path.posix.basename(zone.relativePath).toLowerCase() === "gozones.zon" ? legacyGoZoneNavigators(zone.source) : undefined;
            if (manifestNavigators) {
                for (const [zoneName, navigator] of manifestNavigators) navigators.set(zoneName, navigator);
                continue;
            }
            zones.push(zone);
        }
        for (const zone of zones) zone.source = addLegacyNavigator(zone.source, navigators.get(legacyZoneName(zone.source)?.toLowerCase() ?? ""));
        zones.push(...await this.readZones(path.join(surfaceRoot, "FXZones"), "FX", "FXZones"));
        const originalSurfaceSource = await readFile(surfacePath, "utf8");
        return {
            name: surfaceName,
            stableId: stableId(surfaceName),
            surface: { originalSourceHash: sha256(originalSurfaceSource), source: migrateLegacyCommentSyntax(originalSurfaceSource), sourcePath: `Surfaces/${surfaceName}/Surface.txt` },
            zones,
        };
    }

    private async countZones(zonesRoot: string): Promise<number> {
        let count = 0;
        await this.visitZoneFiles(zonesRoot, async (filePath, relativePath) => {
            if (path.posix.basename(relativePath).toLowerCase() !== "gozones.zon" || !legacyGoZoneNavigators(migrateLegacyCommentSyntax(await readFile(filePath, "utf8")))) count++;
        });
        return count;
    }

    private async readZones(zonesRoot: string, profile: LegacyZoneSourceFile["profile"], sourceDirectory: "FXZones" | "Zones"): Promise<LegacyZoneSourceFile[]> {
        const zones: LegacyZoneSourceFile[] = [];
        await this.visitZoneFiles(zonesRoot, async (filePath, relativePath) => {
            const originalSource = await readFile(filePath, "utf8");
            zones.push({ originalSourceHash: sha256(originalSource), profile, relativePath, source: migrateLegacyZoneSyntax(migrateLegacyCommentSyntax(originalSource)), sourcePath: `${sourceDirectory}/${relativePath}` });
        });
        return zones;
    }

    private async visitZoneFiles(zonesRoot: string, visitFile: (filePath: string, relativePath: string) => Promise<void>): Promise<void> {
        let rootStats: Awaited<ReturnType<typeof stat>>;
        try {
            rootStats = await stat(zonesRoot);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") return;
            throw error;
        }
        if (!rootStats.isDirectory()) throw new EditorOperationError("legacy.zones.directory", "Legacy Zones path is not a directory");
        const visit = async (directoryPath: string, relativeDirectory: string, ancestorDirectories: Set<string>): Promise<void> => {
            const canonicalDirectory = await realpath(directoryPath);
            if (ancestorDirectories.has(canonicalDirectory)) throw new EditorOperationError("legacy.path.cycle", `Legacy zone path contains a directory link cycle: ${relativeDirectory}`);
            const currentAncestors = new Set(ancestorDirectories);
            currentAncestors.add(canonicalDirectory);
            for (const entry of (await readdir(directoryPath, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
                const entryPath = path.join(directoryPath, entry.name);
                const relativePath = relativeDirectory ? `${relativeDirectory}/${entry.name}` : entry.name;
                if (entry.name.includes("\\")) throw new EditorOperationError("legacy.path.separator", `Legacy zone path contains a backslash: ${relativePath}`);
                const entryStats = await stat(entryPath);
                if (entryStats.isDirectory()) await visit(entryPath, relativePath, currentAncestors);
                else if (entryStats.isFile() && entry.name.endsWith(".zon")) await visitFile(entryPath, relativePath);
            }
        };
        await visit(zonesRoot, "", new Set());
    }

    private async isRegularFile(filePath: string): Promise<boolean> {
        try {
            return (await stat(filePath)).isFile();
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") return false;
            throw error;
        }
    }
}
