import { createHash } from "node:crypto";
import { readdir, readFile, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { parseByPath, type AnyDocument } from "./formats.ts";
import { addDiagnostic, serializeDocument, type Diagnostic } from "./model.ts";
import type { ConfigurationStore, OperationReport, SaveChange } from "./store.ts";
import { EditorOperationError } from "./store.ts";
import type { SurfaceSemantic, SurfaceWidget } from "./surface.ts";
import { validateDocumentSet } from "./validation.ts";
import type { ZoneBinding, ZoneSemantic } from "./zone.ts";

export type LegacyImportConflictAction = "create" | "rename" | "replace" | "skip";
export type LegacyImportKind = "surface" | "zone";

export interface LegacySurfaceSummary {
    fxZoneCount: number;
    name: string;
    stableId: string;
    zoneCount: number;
}

export type LegacyWidgetCapability = "absolute-input" | "color-feedback" | "meter-feedback" | "press-input" | "relative-input" | "text-feedback" | "toggle-feedback" | "touch-input" | "value-feedback";

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
    includeSurface: boolean;
    resolutions: LegacyImportResolution[];
    selectedZonePaths: string[];
    surfaceName: string;
    widgetMappings: LegacyWidgetMapping[];
}

interface LegacyZoneSourceFile {
    profile: "FX" | "Main";
    relativePath: string;
    source: string;
    sourcePath: string;
}

interface LegacySurfaceFiles {
    name: string;
    stableId: string;
    surface: { source: string; sourcePath: string };
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

function formatSource(source: string, kind: LegacyImportKind, targetPath: string, knownActions: Set<string>): string {
    const document = parseByPath(source, targetPath, knownActions);
    if (document.version !== "unversioned") return source;
    const bom = source.startsWith("\uFEFF") ? "\uFEFF" : "";
    const content = bom ? source.slice(1) : source;
    const firstEnding = content.match(/\r\n|\r|\n/)?.[0] ?? "\n";
    return `${bom}// @format ${kind} 1${firstEnding}${content}`;
}

function normalizedWidgetName(widgetName: string): string {
    return widgetName.toLowerCase();
}

function widgetCapabilities(widget: SurfaceWidget): LegacyWidgetCapability[] {
    const capabilities = new Set<LegacyWidgetCapability>();
    for (const line of widget.body) {
        const widgetType = (line.tokens[0] ?? "").toLowerCase();
        if (widgetType === "press" || widgetType === "anypress") capabilities.add("press-input");
        else if (widgetType === "touch") capabilities.add("touch-input");
        else if (["encoder", "mftencoder", "encoderplain", "encoder7bit", "x32rotarytoencoder"].includes(widgetType)) {
            capabilities.add("relative-input");
            capabilities.add("value-feedback");
        } else if (["fader14bit", "faderportclassicfader14bit", "fader7bit", "x32fader"].includes(widgetType)) {
            capabilities.add("absolute-input");
            capabilities.add("value-feedback");
        } else if (widgetType.startsWith("fb_fader") || ["fb_encoder", "fb_asparionencoder", "fb_sce24encoder", "fb_faderportvaluebar"].includes(widgetType) || widgetType.includes("processor")) capabilities.add("value-feedback");
        else if (widgetType.startsWith("fb_mcu") || widgetType.includes("display") || widgetType.includes("scribble")) capabilities.add("text-feedback");
        else if (widgetType.includes("vumeter") || widgetType.includes("meter")) {
            capabilities.add("meter-feedback");
            capabilities.add("value-feedback");
        } else if (widgetType.includes("rgb") || widgetType.includes("twostate")) {
            capabilities.add("toggle-feedback");
            capabilities.add("color-feedback");
        }
    }
    return [...capabilities].sort();
}

function commonWidgetCapabilities(widgets: SurfaceWidget[]): LegacyWidgetCapability[] {
    if (!widgets.length) return [];
    const capabilities = new Set(widgetCapabilities(widgets[0]));
    for (const widget of widgets.slice(1)) {
        const current = new Set(widgetCapabilities(widget));
        for (const capability of capabilities) if (!current.has(capability)) capabilities.delete(capability);
    }
    return [...capabilities].sort();
}

function surfaceWidgetSlots(surface: AnyDocument, patternSlots: boolean): LegacyWidgetCandidate[] {
    const widgets = (surface.semantic as SurfaceSemantic).widgets;
    if (!patternSlots) return widgets.map((widget) => ({ capabilities: widgetCapabilities(widget), name: widget.name }));
    const families = new Map<string, SurfaceWidget[]>();
    for (const widget of widgets) {
        const match = widget.name.match(/^(.*\D)(\d+)$/);
        if (!match) continue;
        const familyName = `${match[1]}|`;
        const family = families.get(normalizedWidgetName(familyName)) ?? [];
        family.push(widget);
        families.set(normalizedWidgetName(familyName), family);
    }
    return [...families.values()].map((family) => ({ capabilities: commonWidgetCapabilities(family), name: `${family[0].name.replace(/\d+$/, "")}|` }));
}

function inferredBindingCapabilities(binding: ZoneBinding): LegacyWidgetCapability[] {
    const capabilities = new Set<LegacyWidgetCapability>();
    if (binding.modifiers.some((modifier) => modifier === "Hold" || modifier === "DoublePress")) capabilities.add("press-input");
    if (binding.modifiers.some((modifier) => modifier === "Decrease" || modifier === "Increase")) capabilities.add("relative-input");
    if (binding.modifiers.some((modifier) => modifier.includes("Touch"))) capabilities.add("touch-input");
    return [...capabilities].sort();
}

function isCompatible(requiredCapabilities: LegacyWidgetCapability[], candidateCapabilities: LegacyWidgetCapability[]): boolean {
    const available = new Set(candidateCapabilities);
    return requiredCapabilities.every((capability) => available.has(capability));
}

function widgetMappingMap(widgetMappings: LegacyWidgetMapping[]): Map<string, string> {
    const result = new Map<string, string>();
    for (const mapping of widgetMappings) {
        if (!mapping.sourceWidget || !mapping.targetWidget) throw new EditorOperationError("legacy.widget.mapping.value", "Widget mappings require source and target widget names");
        const key = normalizedWidgetName(mapping.sourceWidget);
        if (result.has(key)) throw new EditorOperationError("legacy.widget.mapping.duplicate", `Widget mapping is duplicated: ${mapping.sourceWidget}`);
        result.set(key, mapping.targetWidget);
    }
    return result;
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
        const candidates = targetSlots.filter((candidate) => isCompatible(requiredCapabilities, candidate.capabilities)).sort((left, right) => left.name.localeCompare(right.name));
        const requestedTarget = requested.get(sourceKey);
        const selectedCandidate = requestedTarget ? candidates.find((candidate) => normalizedWidgetName(candidate.name) === normalizedWidgetName(requestedTarget)) : undefined;
        if (selectedCandidate) validMappings.set(sourceKey, selectedCandidate.name);
        const issue: LegacyWidgetMappingIssue = {
            candidates,
            occurrences: entries.map((entry) => ({ line: entry.line, path: entry.path })),
            reason: sameNameTarget ? "incompatible" : "missing",
            requiredCapabilities,
            selectedTarget: selectedCandidate?.name,
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

async function isDirectory(directoryPath: string): Promise<boolean> {
    try {
        return (await stat(directoryPath)).isDirectory();
    } catch (error) {
        if ((error as NodeJS.ErrnoException).code === "ENOENT") return false;
        throw error;
    }
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

    async preview(store: ConfigurationStore, knownActions: Set<string>, surfaceName: string, includeSurface: boolean, selectedZonePaths?: string[], requestedWidgetMappings: LegacyWidgetMapping[] = [], useExistingSurface = false): Promise<LegacyImportPreview> {
        const files = await this.readSurfaceFiles(surfaceName);
        const selectedPaths = new Set(selectedZonePaths ?? files.zones.map((zone) => zone.sourcePath));
        const availableZonePaths = new Set(files.zones.map((zone) => zone.sourcePath));
        for (const selectedPath of selectedPaths) if (!availableZonePaths.has(selectedPath)) throw new EditorOperationError("legacy.zone.missing", `Legacy zone is not available in ${surfaceName}: ${selectedPath}`);

        const surfaceTargetPath = `Surfaces/User/${files.stableId}.txt`;
        const migratedSurface = formatSource(files.surface.source, "surface", surfaceTargetPath, knownActions);
        const surfaceDocument = parseByPath(migratedSurface, surfaceTargetPath, knownActions);
        const zoneDocuments = new Map<string, AnyDocument>();
        const migratedZoneSources = new Map<string, string>();
        for (const zone of files.zones) {
            const targetPath = `Zones/User/${files.stableId}/${zone.profile}/${zone.relativePath}`;
            const migratedSource = formatSource(zone.source, "zone", targetPath, knownActions);
            migratedZoneSources.set(zone.sourcePath, migratedSource);
            zoneDocuments.set(zone.sourcePath, parseByPath(migratedSource, targetPath, knownActions));
        }

        const widgetTarget = includeSurface && !useExistingSurface ? "imported" : "existing";
        const targetSurface = widgetTarget === "imported" ? surfaceDocument : await this.readExistingTargetSurface(store, knownActions, files.stableId);
        const widgetMappingResult = collectWidgetMappings(zoneDocuments, selectedPaths, surfaceDocument, targetSurface, requestedWidgetMappings);
        for (const [sourcePath, document] of zoneDocuments) {
            if (!selectedPaths.has(sourcePath)) continue;
            const source = replaceMappedWidgets(migratedZoneSources.get(sourcePath)!, document, widgetMappingResult.validMappings);
            migratedZoneSources.set(sourcePath, source);
            zoneDocuments.set(sourcePath, parseByPath(source, document.path!, knownActions));
        }

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
        const appendItem = async (kind: LegacyImportKind, sourcePath: string, targetPath: string, source: string, document: AnyDocument, selected: boolean, zoneName?: string): Promise<void> => {
            const targetState = await store.fileState(targetPath);
            items.push({ diagnostics: document.diagnostics, id: `${kind}:${sourcePath}`, kind, selected, source, sourceHash: sha256(source), sourcePath, targetExists: targetState.exists, targetHash: targetState.hash, targetPath, zoneName });
        };
        await appendItem("surface", files.surface.sourcePath, surfaceTargetPath, migratedSurface, surfaceDocument, includeSurface);
        for (const zone of files.zones) {
            const document = zoneDocuments.get(zone.sourcePath)!;
            await appendItem("zone", zone.sourcePath, `Zones/User/${files.stableId}/${zone.profile}/${zone.relativePath}`, migratedZoneSources.get(zone.sourcePath)!, document, selectedPaths.has(zone.sourcePath), (document.semantic as ZoneSemantic).name);
        }

        const selectedDocuments = items.filter((item) => item.selected).map((item) => item.kind === "surface" ? surfaceDocument : zoneDocuments.get(item.sourcePath)!);
        const mappingSurfaceDocuments = widgetTarget === "existing" ? [...(!includeSurface ? [surfaceDocument] : []), ...(targetSurface ? [targetSurface] : [])] : [];
        const mappingSurfaceDiagnostics = mappingSurfaceDocuments.flatMap((document) => document.diagnostics);
        const diagnostics = selectedDocuments.flatMap((document) => document.diagnostics).concat(mappingSurfaceDiagnostics, validateDocumentSet(selectedDocuments), widgetMappingResult.diagnostics);
        if (widgetTarget === "existing" && selectedPaths.size && !targetSurface) addDiagnostic(diagnostics, "error", "legacy.widget.surface.missing", `Import Surface.txt or create Surfaces/User/${files.stableId}.txt before importing its zones.`);
        return {
            dependencies: [...dependenciesByKey.values()].sort((left, right) => left.from.localeCompare(right.from) || left.type.localeCompare(right.type) || left.name.localeCompare(right.name)),
            diagnostics,
            includeSurface,
            items,
            root: this.root,
            selectedZonePaths: [...selectedPaths].sort(),
            surfaceName: files.name,
            surfaceStableId: files.stableId,
            valid: !diagnostics.some((diagnostic) => diagnostic.severity === "error"),
            widgetMappings: widgetMappingResult.issues,
            widgetTarget,
        };
    }

    async import(store: ConfigurationStore, knownActions: Set<string>, request: LegacyImportRequest): Promise<OperationReport> {
        const resolutions = new Map(request.resolutions.map((resolution) => [resolution.id, resolution]));
        if (resolutions.size !== request.resolutions.length) throw new EditorOperationError("legacy.resolution.duplicate", "Legacy import resolutions must have unique IDs");
        let preview = await this.preview(store, knownActions, request.surfaceName, request.includeSurface, request.selectedZonePaths, request.widgetMappings);
        const surfaceItem = preview.items.find((item) => item.kind === "surface");
        const surfaceResolution = surfaceItem ? resolutions.get(surfaceItem.id) : undefined;
        if (request.includeSurface && surfaceResolution && (surfaceResolution.action === "rename" || surfaceResolution.action === "skip")) preview = await this.preview(store, knownActions, request.surfaceName, request.includeSurface, request.selectedZonePaths, request.widgetMappings, true);
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
                this.validateRenameScope(item, renameTarget, preview.surfaceStableId);
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

    private validateRenameScope(item: LegacyImportItem, targetPath: string, surfaceStableId: string): void {
        if (item.kind === "surface" && !targetPath.startsWith("Surfaces/User/")) throw new EditorOperationError("legacy.rename.scope", "A surface rename target must stay below Surfaces/User");
        if (item.kind === "zone" && !targetPath.startsWith(`Zones/User/${surfaceStableId}/`)) throw new EditorOperationError("legacy.rename.scope", `A zone rename target must stay below Zones/User/${surfaceStableId}`);
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
        const zones = [
            ...await this.readZones(path.join(surfaceRoot, "Zones"), "Main", "Zones"),
            ...await this.readZones(path.join(surfaceRoot, "FXZones"), "FX", "FXZones"),
        ];
        return {
            name: surfaceName,
            stableId: stableId(surfaceName),
            surface: { source: await readFile(surfacePath, "utf8"), sourcePath: `Surfaces/${surfaceName}/Surface.txt` },
            zones,
        };
    }

    private async countZones(zonesRoot: string): Promise<number> {
        let count = 0;
        await this.visitZoneFiles(zonesRoot, async () => { count++; });
        return count;
    }

    private async readZones(zonesRoot: string, profile: LegacyZoneSourceFile["profile"], sourceDirectory: "FXZones" | "Zones"): Promise<LegacyZoneSourceFile[]> {
        const zones: LegacyZoneSourceFile[] = [];
        await this.visitZoneFiles(zonesRoot, async (filePath, relativePath) => {
            zones.push({ profile, relativePath, source: await readFile(filePath, "utf8"), sourcePath: `${sourceDirectory}/${relativePath}` });
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
