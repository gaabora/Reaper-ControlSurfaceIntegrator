import { createHash } from "node:crypto";
import { lstat, readdir, readFile, realpath } from "node:fs/promises";
import path from "node:path";
import { parseByPath, type AnyDocument } from "./formats.ts";
import type { Diagnostic } from "./model.ts";
import type { ConfigurationStore, OperationReport, SaveChange } from "./store.ts";
import { EditorOperationError } from "./store.ts";
import { validateDocumentSet } from "./validation.ts";
import type { ZoneSemantic } from "./zone.ts";

export type LegacyImportConflictAction = "create" | "rename" | "replace" | "skip";
export type LegacyImportKind = "surface" | "zone";

export interface LegacySurfaceSummary {
    name: string;
    stableId: string;
    zoneCount: number;
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
}

interface LegacySourceFile {
    relativePath: string;
    source: string;
}

interface LegacySurfaceFiles {
    name: string;
    stableId: string;
    surface: LegacySourceFile;
    zones: LegacySourceFile[];
}

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

async function isDirectory(directoryPath: string): Promise<boolean> {
    try {
        const stats = await lstat(directoryPath);
        return stats.isDirectory() && !stats.isSymbolicLink();
    } catch (error) {
        if ((error as NodeJS.ErrnoException).code === "ENOENT") return false;
        throw error;
    }
}

export class LegacyCsiSource {
    private constructor(private readonly root: string) {}

    static async create(selectedPath: string): Promise<LegacyCsiSource> {
        if (!path.isAbsolute(selectedPath)) throw new EditorOperationError("legacy.path.absolute", "Legacy CSI path must be absolute");
        let selectedStats: Awaited<ReturnType<typeof lstat>>;
        try {
            selectedStats = await lstat(selectedPath);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") throw new EditorOperationError("legacy.path.missing", "Legacy CSI path does not exist");
            throw error;
        }
        if (!selectedStats.isDirectory() || selectedStats.isSymbolicLink()) throw new EditorOperationError("legacy.path.directory", "Legacy CSI path must be a normal directory, not a symbolic link");
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
            if (entry.isSymbolicLink() || !entry.isDirectory()) continue;
            const surfaceRoot = path.join(surfacesRoot, entry.name);
            if (!await this.isRegularFile(path.join(surfaceRoot, "Surface.txt"))) continue;
            const zoneCount = await this.countZones(path.join(surfaceRoot, "Zones"));
            summaries.push({ name: entry.name, stableId: stableId(entry.name), zoneCount });
        }
        return summaries;
    }

    async preview(store: ConfigurationStore, knownActions: Set<string>, surfaceName: string, includeSurface: boolean, selectedZonePaths?: string[]): Promise<LegacyImportPreview> {
        const files = await this.readSurfaceFiles(surfaceName);
        const selectedPaths = new Set(selectedZonePaths ?? files.zones.map((zone) => zone.relativePath));
        const availableZonePaths = new Set(files.zones.map((zone) => zone.relativePath));
        for (const selectedPath of selectedPaths) if (!availableZonePaths.has(selectedPath)) throw new EditorOperationError("legacy.zone.missing", `Legacy zone is not available in ${surfaceName}: ${selectedPath}`);

        const surfaceTargetPath = `Surfaces/User/${files.stableId}.txt`;
        const migratedSurface = formatSource(files.surface.source, "surface", surfaceTargetPath, knownActions);
        const surfaceDocument = parseByPath(migratedSurface, surfaceTargetPath, knownActions);
        const zoneDocuments = new Map<string, AnyDocument>();
        const migratedZoneSources = new Map<string, string>();
        for (const zone of files.zones) {
            const targetPath = `Zones/User/${files.stableId}/${zone.relativePath}`;
            const migratedSource = formatSource(zone.source, "zone", targetPath, knownActions);
            migratedZoneSources.set(zone.relativePath, migratedSource);
            zoneDocuments.set(zone.relativePath, parseByPath(migratedSource, targetPath, knownActions));
        }

        const matchesByName = new Map<string, string[]>();
        for (const [relativePath, document] of zoneDocuments) {
            const semantic = document.semantic as ZoneSemantic;
            if (!semantic.name) continue;
            const matches = matchesByName.get(semantic.name.toLowerCase()) ?? [];
            matches.push(relativePath);
            matchesByName.set(semantic.name.toLowerCase(), matches);
        }
        const dependenciesByKey = new Map<string, LegacyImportDependency>();
        for (const [relativePath, document] of zoneDocuments) for (const dependency of collectDependencies(relativePath, document.semantic as ZoneSemantic, matchesByName, selectedPaths)) dependenciesByKey.set(dependencyKey(dependency), dependency);

        const items: LegacyImportItem[] = [];
        const appendItem = async (kind: LegacyImportKind, sourcePath: string, targetPath: string, source: string, document: AnyDocument, selected: boolean, zoneName?: string): Promise<void> => {
            const targetState = await store.fileState(targetPath);
            items.push({ diagnostics: document.diagnostics, id: `${kind}:${sourcePath}`, kind, selected, source, sourceHash: sha256(source), sourcePath, targetExists: targetState.exists, targetHash: targetState.hash, targetPath, zoneName });
        };
        await appendItem("surface", files.surface.relativePath, surfaceTargetPath, migratedSurface, surfaceDocument, includeSurface);
        for (const zone of files.zones) {
            const document = zoneDocuments.get(zone.relativePath)!;
            await appendItem("zone", zone.relativePath, `Zones/User/${files.stableId}/${zone.relativePath}`, migratedZoneSources.get(zone.relativePath)!, document, selectedPaths.has(zone.relativePath), (document.semantic as ZoneSemantic).name);
        }

        const selectedDocuments = items.filter((item) => item.selected).map((item) => item.kind === "surface" ? surfaceDocument : zoneDocuments.get(item.sourcePath)!);
        const diagnostics = selectedDocuments.flatMap((document) => document.diagnostics).concat(validateDocumentSet(selectedDocuments));
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
        };
    }

    async import(store: ConfigurationStore, knownActions: Set<string>, request: LegacyImportRequest): Promise<OperationReport> {
        const preview = await this.preview(store, knownActions, request.surfaceName, request.includeSurface, request.selectedZonePaths);
        if (!preview.valid) throw new EditorOperationError("validation.failed", "Legacy import contains configuration errors", preview.diagnostics);
        const resolutions = new Map(request.resolutions.map((resolution) => [resolution.id, resolution]));
        if (resolutions.size !== request.resolutions.length) throw new EditorOperationError("legacy.resolution.duplicate", "Legacy import resolutions must have unique IDs");
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

    private async readSurfaceFiles(surfaceName: string): Promise<LegacySurfaceFiles> {
        if (!surfaceName || surfaceName.includes("/") || surfaceName.includes("\\") || surfaceName === "." || surfaceName === "..") throw new EditorOperationError("legacy.surface.name", "Legacy surface name is invalid");
        const surfaceRoot = path.join(this.root, "Surfaces", surfaceName);
        const stats = await lstat(surfaceRoot).catch((error: NodeJS.ErrnoException) => {
            if (error.code === "ENOENT") throw new EditorOperationError("legacy.surface.missing", `Legacy surface does not exist: ${surfaceName}`);
            throw error;
        });
        if (!stats.isDirectory() || stats.isSymbolicLink()) throw new EditorOperationError("legacy.surface.directory", `Legacy surface is not a normal directory: ${surfaceName}`);
        const surfacePath = path.join(surfaceRoot, "Surface.txt");
        if (!await this.isRegularFile(surfacePath)) throw new EditorOperationError("legacy.surface.file", `Legacy surface has no regular Surface.txt file: ${surfaceName}`);
        const zones = await this.readZones(path.join(surfaceRoot, "Zones"));
        return {
            name: surfaceName,
            stableId: stableId(surfaceName),
            surface: { relativePath: `Surfaces/${surfaceName}/Surface.txt`, source: await readFile(surfacePath, "utf8") },
            zones,
        };
    }

    private async countZones(zonesRoot: string): Promise<number> {
        return (await this.readZones(zonesRoot)).length;
    }

    private async readZones(zonesRoot: string): Promise<LegacySourceFile[]> {
        let rootStats: Awaited<ReturnType<typeof lstat>>;
        try {
            rootStats = await lstat(zonesRoot);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") return [];
            throw error;
        }
        if (rootStats.isSymbolicLink()) throw new EditorOperationError("legacy.path.symlink", "Legacy Zones directory is a symbolic link");
        if (!rootStats.isDirectory()) throw new EditorOperationError("legacy.zones.directory", "Legacy Zones path is not a directory");
        const zones: LegacySourceFile[] = [];
        const visit = async (directoryPath: string, relativeDirectory: string): Promise<void> => {
            for (const entry of (await readdir(directoryPath, { withFileTypes: true })).sort((left, right) => left.name.localeCompare(right.name))) {
                const entryPath = path.join(directoryPath, entry.name);
                const relativePath = relativeDirectory ? `${relativeDirectory}/${entry.name}` : entry.name;
                if (entry.name.includes("\\")) throw new EditorOperationError("legacy.path.separator", `Legacy zone path contains a backslash: ${relativePath}`);
                if (entry.isSymbolicLink()) throw new EditorOperationError("legacy.path.symlink", `Legacy zone path is a symbolic link: ${relativePath}`);
                if (entry.isDirectory()) await visit(entryPath, relativePath);
                else if (entry.isFile() && entry.name.endsWith(".zon")) zones.push({ relativePath, source: await readFile(entryPath, "utf8") });
            }
        };
        await visit(zonesRoot, "");
        return zones;
    }

    private async isRegularFile(filePath: string): Promise<boolean> {
        try {
            const stats = await lstat(filePath);
            return stats.isFile() && !stats.isSymbolicLink();
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") return false;
            throw error;
        }
    }
}
