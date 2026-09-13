import { copyFile, mkdir, open, readFile, rename, rm, stat, unlink, writeFile } from "node:fs/promises";
import { createHash, randomUUID } from "node:crypto";
import path from "node:path";
import type { ActionTraits } from "./action-catalog.ts";
import type { AnyDocument } from "./formats.ts";
import { parseByPath } from "./formats.ts";
import type { Diagnostic } from "./model.ts";
import type { ProductRootGuard, ProductTreeEntry } from "./paths.ts";
import type { SettingsSchema } from "./settings-schema.ts";
import { applyQuickFix as applyRegisteredQuickFix, diagnosticWithQuickFixes, diagnosticsWithQuickFixes, type QuickFixRequest } from "./quick-fixes.ts";
import { validateDocumentSet, validateZoneDocumentGesturesForSurface } from "./validation.ts";
import type { ZoneSemantic } from "./zone.ts";

export interface DocumentView {
    diagnostics: Diagnostic[];
    format: string;
    lines: Array<{ ending: string; kind: string; lineNumber: number; text: string }>;
    semantic: unknown;
    version: string;
}

export interface OpenDocumentResult {
    document: DocumentView;
    hash: string;
    path: string;
    source: string;
    writable: boolean;
}

export interface SaveChange {
    originalHash: string | null;
    path: string;
    source: string;
}

export interface ValidationSetResult {
    checkedPaths: string[];
    diagnostics: Diagnostic[];
    files: Array<{ diagnostics: Diagnostic[]; path: string }>;
}

export interface FileState {
    exists: boolean;
    hash: string | null;
}

export interface OperationReport {
    changed: string[];
    created: string[];
    failed: string[];
    operationId?: string;
    restored: string[];
    skipped: string[];
    targetPath?: string;
}

export interface ZoneContextResult {
    allowedNavigationActions: string[];
    parents: Array<{ line: number; path: string; relation: string }>;
    role: string;
}

export interface RenameZoneRequest {
    newId: string;
    originalHash: string;
    path: string;
    source: string;
}

export interface ConfigurationStoreHooks {
    beforeCommit?: (relativePath: string, commitIndex: number) => Promise<void> | void;
}

interface PreparedChange extends SaveChange {
    existed: boolean;
    newHash: string;
    stagedPath: string;
    targetPath: string;
}

interface BackupManifest {
    changes: Array<{ backup?: string; existed: boolean; newHash: string; originalHash: string | null; path: string }>;
    completedAt?: string;
    createdAt: string;
    operationId: string;
    report?: OperationReport;
    status: "complete" | "rolled-back" | "staged";
    version: 1;
}

export class EditorOperationError extends Error {
    constructor(public readonly code: string, message: string, public readonly details?: unknown) {
        super(message);
        this.name = "EditorOperationError";
    }
}

function sha256(data: Uint8Array | string): string {
    return createHash("sha256").update(data).digest("hex");
}

function jsonValue(value: unknown): unknown {
    if (value instanceof Map) return Object.fromEntries([...value.entries()].map(([key, entryValue]) => [key, jsonValue(entryValue)]));
    if (Array.isArray(value)) return value.map(jsonValue);
    if (value && typeof value === "object") return Object.fromEntries(Object.entries(value).map(([key, entryValue]) => [key, jsonValue(entryValue)]));
    return value;
}

function documentView(document: AnyDocument, knownActions: Set<string>, writable: boolean, extraDiagnostics: Diagnostic[] = []): DocumentView {
    return {
        diagnostics: [...diagnosticsWithQuickFixes(document, knownActions, writable), ...extraDiagnostics],
        format: document.format,
        lines: document.lines.map((line) => ({ ending: line.ending, kind: line.kind, lineNumber: line.lineNumber, text: line.text })),
        semantic: jsonValue(document.semantic),
        version: document.version,
    };
}

function flattenFileEntries(entries: ProductTreeEntry[], result: ProductTreeEntry[] = []): ProductTreeEntry[] {
    for (const entry of entries) {
        if (entry.kind === "file") result.push(entry);
        else if (entry.kind === "directory") flattenFileEntries(entry.children ?? [], result);
    }
    return result;
}

function activeProfileZoneEntries(entries: ProductTreeEntry[], profileId: string, collection: string): ProductTreeEntry[] {
    const activeById = new Map<string, ProductTreeEntry>();
    const matching = flattenFileEntries(entries).filter((entry) => {
        const location = entry.path.replaceAll("\\", "/").match(/^Zones\/(Vendor|User)\/([^/]+)\/(Main|FX)\/(.+)\.zon$/i);
        return location && location[2].toLowerCase() === profileId.toLowerCase() && location[3].toLowerCase() === collection.toLowerCase();
    });
    for (const owner of ["Vendor", "User"]) for (const entry of matching) {
        const location = entry.path.replaceAll("\\", "/").match(/^Zones\/(Vendor|User)\/([^/]+)\/(Main|FX)\/(.+)\.zon$/i)!;
        if (location[1].toLowerCase() === owner.toLowerCase()) activeById.set(path.posix.basename(location[4]).toLowerCase(), entry);
    }
    return [...activeById.values()];
}

function operationId(): string {
    const timestamp = new Date().toISOString().toLowerCase().replace(/[^a-z0-9]/g, "");
    return `operation-${timestamp}-${randomUUID()}`;
}

function emptyReport(currentOperationId?: string): OperationReport {
    return { changed: [], created: [], failed: [], operationId: currentOperationId, restored: [], skipped: [] };
}

export class ConfigurationStore {
    constructor(private readonly guard: ProductRootGuard, private readonly knownActions: Set<string>, private readonly hooks: ConfigurationStoreHooks = {}, private readonly settingsSchema?: SettingsSchema, private readonly actionTraits: ReadonlyMap<string, ActionTraits> = new Map()) {}

    getRoot(): string {
        return this.guard.getRoot();
    }

    getReaperDataPath(): string {
        return this.guard.getReaperDataPath();
    }

    getConfigFilename(): string {
        return this.guard.getIdentity().configFilename;
    }

    async vendorZoneOverridePath(relativePath: string): Promise<string | undefined> {
        const location = relativePath.replaceAll("\\", "/").match(/^Zones\/User\/([^/]+)\/(Main|FX)\/(.+)\.zon$/i);
        if (!location) return undefined;
        const zoneId = path.posix.basename(location[3]).toLowerCase();
        return flattenFileEntries(await this.tree()).find((entry) => {
            const candidate = entry.path.replaceAll("\\", "/").match(/^Zones\/Vendor\/([^/]+)\/(Main|FX)\/(.+)\.zon$/i);
            return candidate && candidate[1].toLowerCase() === location[1].toLowerCase() && candidate[2].toLowerCase() === location[2].toLowerCase() && path.posix.basename(candidate[3]).toLowerCase() === zoneId;
        })?.path;
    }

    async tree(): Promise<ProductTreeEntry[]> {
        return this.guard.listTree();
    }

    async openDocument(relativePath: string): Promise<OpenDocumentResult> {
        const info = this.guard.getPathInfo(relativePath);
        const absolutePath = await this.guard.resolveExisting(relativePath);
        const data = await readFile(absolutePath);
        const source = data.toString("utf8");
        const document = parseByPath(source, relativePath, this.knownActions, this.settingsSchema, this.actionTraits);
        return { document: documentView(document, this.knownActions, info.writable), hash: sha256(data), path: relativePath, source, writable: info.writable };
    }

    async fileState(relativePath: string): Promise<FileState> {
        this.guard.getPathInfo(relativePath);
        const absolutePath = await this.guard.resolveForWrite(relativePath);
        try {
            const stats = await stat(absolutePath);
            if (!stats.isFile()) throw new EditorOperationError("path.file", `Configuration target is not a file: ${relativePath}`);
            return { exists: true, hash: sha256(await readFile(absolutePath)) };
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code === "ENOENT") return { exists: false, hash: null };
            throw error;
        }
    }

    validateSource(relativePath: string, source: string): DocumentView {
        const info = this.guard.getPathInfo(relativePath);
        return documentView(parseByPath(source, relativePath, this.knownActions, this.settingsSchema, this.actionTraits), this.knownActions, info.writable);
    }

    parseDocument(relativePath: string, source: string): AnyDocument {
        this.guard.getPathInfo(relativePath);
        return parseByPath(source, relativePath, this.knownActions, this.settingsSchema, this.actionTraits);
    }

    validateDocuments(documents: AnyDocument[], completeProfiles = false): Diagnostic[] {
        return validateDocumentSet(documents, { actionTraits: this.actionTraits, completeProfiles, settingsSchema: this.settingsSchema });
    }

    validateZoneGesturesForSurface(document: AnyDocument, surfaceDocument: AnyDocument): Diagnostic[] {
        return validateZoneDocumentGesturesForSurface(document, surfaceDocument, this.actionTraits, this.settingsSchema);
    }

    async zoneProfileDocuments(profileId: string, replacements: AnyDocument[], sources?: ReadonlySet<"User" | "Vendor">): Promise<AnyDocument[]> {
        const documents = new Map(replacements.filter((document) => document.path).map((document) => [document.path!.toLowerCase(), document]));
        for (const entry of flattenFileEntries(await this.tree())) {
            if (entry.type !== "zone") continue;
            const location = entry.path.replaceAll("\\", "/").match(/^Zones\/(?:Vendor|User)\/([^/]+)\/(?:Main|FX)\//i);
            if (!location || location[1].toLowerCase() !== profileId.toLowerCase() || documents.has(entry.path.toLowerCase())) continue;
            const sourceName = entry.path.replaceAll("\\", "/").match(/^Zones\/(Vendor|User)\//i)?.[1];
            const source = sourceName?.toLowerCase() === "user" ? "User" : sourceName ? "Vendor" : undefined;
            if (sources && (!source || !sources.has(source))) continue;
            const opened = await this.openDocument(entry.path);
            documents.set(entry.path.toLowerCase(), this.parseDocument(entry.path, opened.source));
        }
        return [...documents.values()];
    }

    async zoneContext(relativePath: string, source: string): Promise<ZoneContextResult> {
        const location = relativePath.replaceAll("\\", "/").match(/^Zones\/(?:Vendor|User)\/([^/]+)\/(Main|FX)\/(.+)\.zon$/i);
        if (!location) throw new EditorOperationError("zone.context.path", "Zone context requires a Main or FX .zon file");
        const currentDocument = this.parseDocument(relativePath, source);
        if (currentDocument.format !== "zone") throw new EditorOperationError("zone.context.format", "Zone context requires a Zone file");
        const currentSemantic = currentDocument.semantic as ZoneSemantic;
        const zoneId = path.posix.basename(relativePath, ".zon");
        const parents: ZoneContextResult["parents"] = [];
        for (const entry of activeProfileZoneEntries(await this.tree(), location[1], location[2])) {
            if (entry.path.toLowerCase() === relativePath.toLowerCase()) continue;
            const opened = await this.openDocument(entry.path);
            const semantic = opened.document.semantic as ZoneSemantic;
            for (const reference of semantic.dependencyReferences) if (reference.name.toLowerCase() === zoneId.toLowerCase()) parents.push({ line: reference.line, path: entry.path, relation: reference.type });
        }
        const isLayer = currentSemantic.role === "Layer";
        return { allowedNavigationActions: isLayer ? ["ExitZoneLayer"] : ["GoZone", "GoHome", "EnterZoneLayer"], parents, role: isLayer ? "Layer" : location[2] === "FX" ? "FX" : currentSemantic.role || "Normal" };
    }

    async renameZone(request: RenameZoneRequest): Promise<OperationReport> {
        if (!/^[a-z0-9][a-z0-9_-]*$/.test(request.newId)) throw new EditorOperationError("zone.rename.id", "The new Zone ID must use lowercase letters, numbers, _ or -");
        const location = request.path.replaceAll("\\", "/").match(/^Zones\/User\/([^/]+)\/(Main|FX)\/(.+)\.zon$/);
        if (!location) throw new EditorOperationError("zone.rename.path", "Only a User Main or FX Zone can be renamed");
        const oldId = path.posix.basename(request.path, ".zon");
        if (oldId === request.newId) throw new EditorOperationError("zone.rename.same", "The new Zone ID is the same as the current ID");
        const targetPath = path.posix.join(path.posix.dirname(request.path), `${request.newId}.zon`);
        const caseOnlyRename = oldId.toLowerCase() === request.newId.toLowerCase();
        const profileEntries = activeProfileZoneEntries(await this.tree(), location[1], location[2]);
        const conflictingEntry = profileEntries.find((entry) => path.posix.basename(entry.path, ".zon").toLowerCase() === request.newId.toLowerCase() && entry.path.toLowerCase() !== request.path.toLowerCase());
        if (!caseOnlyRename && conflictingEntry) throw new EditorOperationError("conflict.exists", `A Zone with this ID already exists in the profile: ${conflictingEntry.path}`);
        const sourceState = await this.fileState(request.path);
        if (!sourceState.exists || sourceState.hash !== request.originalHash) throw new EditorOperationError("conflict.hash", `Configuration changed after it was opened: ${request.path}`, { currentHash: sourceState.hash, originalHash: request.originalHash });
        const escapedId = oldId.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
        const replaceReferences = (source: string, referencesByLine: ReadonlyMap<number, string[]>): string => source.split(/(?<=\n)/).map((sourceLine, lineIdx) => {
            const referenceTypes = referencesByLine.get(lineIdx + 1);
            if (!referenceTypes?.length) return sourceLine;
            const commentPosition = sourceLine.indexOf("//");
            let code = commentPosition < 0 ? sourceLine : sourceLine.slice(0, commentPosition);
            const comment = commentPosition < 0 ? "" : sourceLine.slice(commentPosition);
            for (const referenceType of referenceTypes) {
                if (["IncludedZones", "SubZones", "ZoneLayers"].includes(referenceType)) code = code.replace(new RegExp(`(^|\\s)${escapedId}(?=\\s|}|$)`, "gi"), `$1${request.newId}`);
                else {
                    const actionMatch = new RegExp(`\\b${referenceType.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}\\b`).exec(code);
                    if (actionMatch) {
                        const parameterStart = actionMatch.index + actionMatch[0].length;
                        code = code.slice(0, parameterStart) + code.slice(parameterStart).replace(new RegExp(`(^|\\s)${escapedId}(?=\\s|}|$)`, "i"), `$1${request.newId}`);
                    }
                }
            }
            return code + comment;
        }).join("");
        const sourceDocument = this.parseDocument(request.path, request.source);
        const matchingReferences = (semantic: ZoneSemantic): Map<number, string[]> => {
            const result = new Map<number, string[]>();
            for (const reference of semantic.dependencyReferences) {
                if (reference.name.toLowerCase() !== oldId.toLowerCase()) continue;
                result.set(reference.line, [...result.get(reference.line) ?? [], reference.type]);
            }
            return result;
        };
        const renamedSource = replaceReferences(request.source, matchingReferences(sourceDocument.semantic as ZoneSemantic));
        const renamedDocument = this.parseDocument(targetPath, renamedSource);
        if (renamedDocument.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new EditorOperationError("validation.failed", `Renamed Zone contains errors: ${targetPath}`, renamedDocument.diagnostics);

        const changes: SaveChange[] = [{ originalHash: null, path: targetPath, source: renamedSource }];
        for (const entry of profileEntries) {
            if (entry.path.toLowerCase() === request.path.toLowerCase()) continue;
            const userRelativePath = entry.path.replace(/^Zones\/(?:Vendor|User)\//i, "");
            const opened = await this.openDocument(entry.path);
            const semantic = opened.document.semantic as ZoneSemantic;
            const referencesByLine = matchingReferences(semantic);
            if (!referencesByLine.size) continue;
            const changedSource = replaceReferences(opened.source, referencesByLine);
            const changePath = entry.path.startsWith("Zones/Vendor/") ? `Zones/User/${userRelativePath}` : entry.path;
            const originalHash = entry.path.startsWith("Zones/Vendor/") ? null : opened.hash;
            changes.push({ originalHash, path: changePath, source: changedSource });
        }

        const sourceAbsolutePath = await this.guard.resolveExisting(request.path);
        const currentOperationId = operationId();
        const backupRelativePath = `Backups/${currentOperationId}/renamed-from/${request.path}`;
        const backupPath = await this.guard.resolveForWrite(backupRelativePath);
        await mkdir(path.dirname(backupPath), { recursive: true });
        await rename(sourceAbsolutePath, backupPath);
        try {
            const report = await this.saveTransaction(changes);
            report.changed.push(request.path);
            report.targetPath = targetPath;
            return report;
        } catch (error) {
            await rename(backupPath, sourceAbsolutePath);
            throw error;
        }
    }

    applyQuickFix(relativePath: string, source: string, request: QuickFixRequest): { document: DocumentView; source: string } {
        const info = this.guard.getPathInfo(relativePath);
        if (!info.writable) throw new EditorOperationError("quick-fix.read-only", `Cannot apply a quick fix to a read-only file: ${relativePath}`);
        const result = applyRegisteredQuickFix(source, relativePath, this.knownActions, request, this.settingsSchema, this.actionTraits);
        return { document: documentView(result.document, this.knownActions, true), source: result.source };
    }

    async validateAll(changes: SaveChange[]): Promise<ValidationSetResult> {
        const sources = new Map<string, string>();
        for (const change of changes) {
            this.guard.getPathInfo(change.path);
            if (sources.has(change.path.toLowerCase())) throw new EditorOperationError("validation.duplicate", `Validation paths must be unique case-insensitively: ${change.path}`);
            sources.set(change.path.toLowerCase(), change.source);
        }
        const entries = flattenFileEntries(await this.tree()).filter((entry) => entry.path);
        const checkedPaths = entries.map((entry) => entry.path);
        const checkedPathKeys = new Set(checkedPaths.map((relativePath) => relativePath.toLowerCase()));
        for (const change of changes) if (!checkedPathKeys.has(change.path.toLowerCase())) throw new EditorOperationError("validation.path", `Validation path is not available in the configuration tree: ${change.path}`);
        const documents: AnyDocument[] = [];
        const writableByPath = new Map<string, boolean>();
        const files: Array<{ diagnostics: Diagnostic[]; path: string }> = [];
        for (const entry of entries) {
            const opened = await this.openDocument(entry.path);
            const source = sources.get(entry.path.toLowerCase()) ?? opened.source;
            const document = parseByPath(source, entry.path, this.knownActions, this.settingsSchema, this.actionTraits);
            documents.push(document);
            writableByPath.set(entry.path.toLowerCase(), opened.writable);
            files.push({ diagnostics: diagnosticsWithQuickFixes(document, this.knownActions, opened.writable), path: entry.path });
        }
        const documentsByPath = new Map<string, AnyDocument>(documents.filter((document) => document.path).map((document) => [document.path!.toLowerCase(), document] as const));
        const diagnostics = validateDocumentSet(documents, { completeProfiles: true, actionTraits: this.actionTraits, settingsSchema: this.settingsSchema }).map((diagnostic) => {
            const document = diagnostic.path ? documentsByPath.get(diagnostic.path.toLowerCase()) : undefined;
            return document ? diagnosticWithQuickFixes(document, diagnostic, this.knownActions, writableByPath.get(diagnostic.path!.toLowerCase()) ?? false) : diagnostic;
        });
        return { checkedPaths, diagnostics, files };
    }

    async saveOne(change: SaveChange): Promise<{ hash: string; report: OperationReport }> {
        const prepared = await this.prepareChange(change);
        const report = emptyReport();
        try {
            await this.assertOriginalUnchanged(prepared);
            await rename(prepared.stagedPath, prepared.targetPath);
            if (prepared.existed) report.changed.push(change.path);
            else report.created.push(change.path);
            return { hash: prepared.newHash, report };
        } catch (error) {
            await this.removeFileIfPresent(prepared.stagedPath);
            report.failed.push(change.path);
            if (error instanceof EditorOperationError) throw error;
            throw new EditorOperationError("save.failed", error instanceof Error ? error.message : String(error), report);
        }
    }

    async saveTransaction(changes: SaveChange[]): Promise<OperationReport> {
        if (!changes.length) throw new EditorOperationError("transaction.empty", "Transaction requires at least one file");
        const uniquePaths = new Set(changes.map((change) => change.path.toLowerCase()));
        if (uniquePaths.size !== changes.length) throw new EditorOperationError("transaction.duplicate", "Transaction paths must be unique case-insensitively");
        for (const change of changes) {
            const info = this.guard.getPathInfo(change.path);
            if (!info.writable) throw new EditorOperationError("path.read-only", `Configuration is read-only: ${change.path}`);
        }

        const documents = changes.map((change) => parseByPath(change.source, change.path, this.knownActions, this.settingsSchema, this.actionTraits));
        const diagnostics = documents.flatMap((document) => document.diagnostics).concat(this.validateDocuments(documents));
        if (diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new EditorOperationError("validation.failed", "Transaction contains configuration errors", diagnostics);

        const preparedChanges: PreparedChange[] = [];
        try {
            for (const change of changes) preparedChanges.push(await this.prepareChange(change, false));
        } catch (error) {
            for (const prepared of preparedChanges) await this.removeFileIfPresent(prepared.stagedPath);
            throw error;
        }

        const currentOperationId = operationId();
        const report = emptyReport(currentOperationId);
        const backupRelativeRoot = `Backups/${currentOperationId}`;
        const backupRoot = await this.guard.resolveForWrite(backupRelativeRoot);
        const manifestPath = path.join(backupRoot, "manifest.json");
        const manifest: BackupManifest = {
            changes: [],
            createdAt: new Date().toISOString(),
            operationId: currentOperationId,
            status: "staged",
            version: 1,
        };
        const committed: PreparedChange[] = [];
        try {
            await mkdir(backupRoot, { recursive: true });
            for (const prepared of preparedChanges) {
                let backupRelativePath: string | undefined;
                if (prepared.existed) {
                    backupRelativePath = `files/${prepared.path}`;
                    const backupPath = path.join(backupRoot, ...backupRelativePath.split("/"));
                    await mkdir(path.dirname(backupPath), { recursive: true });
                    await copyFile(prepared.targetPath, backupPath);
                }
                manifest.changes.push({ backup: backupRelativePath, existed: prepared.existed, newHash: prepared.newHash, originalHash: prepared.originalHash, path: prepared.path });
            }
            await writeFile(manifestPath, JSON.stringify(manifest, null, 2) + "\n", "utf8");
            for (const prepared of preparedChanges) {
                await this.assertOriginalUnchanged(prepared);
                await this.hooks.beforeCommit?.(prepared.path, committed.length);
                await rename(prepared.stagedPath, prepared.targetPath);
                committed.push(prepared);
                if (prepared.existed) report.changed.push(prepared.path);
                else report.created.push(prepared.path);
            }
            manifest.completedAt = new Date().toISOString();
            manifest.report = report;
            manifest.status = "complete";
            await writeFile(manifestPath, JSON.stringify(manifest, null, 2) + "\n", "utf8");
            return report;
        } catch (error) {
            report.failed.push(preparedChanges[committed.length]?.path ?? "transaction");
            for (const prepared of [...committed].reverse()) {
                try {
                    if (prepared.existed) {
                        const backup = manifest.changes.find((change) => change.path === prepared.path)?.backup;
                        if (!backup) throw new Error(`Backup is missing for ${prepared.path}`);
                        await this.restoreFile(path.join(backupRoot, ...backup.split("/")), prepared.targetPath);
                    } else {
                        await this.removeFileIfPresent(prepared.targetPath);
                    }
                    report.restored.push(prepared.path);
                } catch (restoreError) {
                    report.failed.push(`${prepared.path}: rollback: ${restoreError instanceof Error ? restoreError.message : String(restoreError)}`);
                }
            }
            for (const prepared of preparedChanges) await this.removeFileIfPresent(prepared.stagedPath);
            manifest.completedAt = new Date().toISOString();
            manifest.report = report;
            manifest.status = "rolled-back";
            try {
                await writeFile(manifestPath, JSON.stringify(manifest, null, 2) + "\n", "utf8");
            } catch (manifestError) {
                report.failed.push(`manifest: ${manifestError instanceof Error ? manifestError.message : String(manifestError)}`);
            }
            const errorCode = error instanceof EditorOperationError && error.code.startsWith("conflict.") ? "conflict.transaction" : "transaction.failed";
            throw new EditorOperationError(errorCode, error instanceof Error ? error.message : String(error), report);
        }
    }

    async cloneForEditing(relativePath: string): Promise<OperationReport> {
        const info = this.guard.getPathInfo(relativePath);
        const segments = relativePath.split("/");
        if (info.owner === "vendor" && info.kind === "surface") {
            const targetPath = `Surfaces/User/${segments[2]}`;
            return this.cloneFile(relativePath, targetPath);
        }
        if (info.owner === "built-in" && info.kind === "snippet") {
            const targetPath = `Snippets/User/${segments[2]}`;
            return this.cloneFile(relativePath, targetPath);
        }
        if (info.owner === "vendor" && info.kind === "zone" && (segments[3] === "FX" || segments[3] === "Main")) return this.cloneFile(relativePath, `Zones/User/${segments.slice(2).join("/")}`);
        throw new EditorOperationError("clone.unsupported", "Only vendor surfaces, individual vendor zones, and built-in snippets can be cloned");
    }

    private async cloneFile(sourceRelativePath: string, targetRelativePath: string): Promise<OperationReport> {
        const sourcePath = await this.guard.resolveExisting(sourceRelativePath);
        const source = await readFile(sourcePath, "utf8");
        const report = await this.saveTransaction([{ originalHash: null, path: targetRelativePath, source }]);
        report.targetPath = targetRelativePath;
        return report;
    }

    private async prepareChange(change: SaveChange, validate = true): Promise<PreparedChange> {
        const info = this.guard.getPathInfo(change.path);
        if (!info.writable) throw new EditorOperationError("path.read-only", `Configuration is read-only: ${change.path}`);
        if (validate) {
            const document = parseByPath(change.source, change.path, this.knownActions, this.settingsSchema, this.actionTraits);
            if (document.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new EditorOperationError("validation.failed", `Configuration contains errors: ${change.path}`, document.diagnostics);
        }
        const targetPath = await this.guard.resolveForWrite(change.path);
        let existed = false;
        let existingMode = 0o600;
        let currentHash: string | null = null;
        try {
            const targetStats = await stat(targetPath);
            if (!targetStats.isFile()) throw new EditorOperationError("path.file", `Configuration target is not a file: ${change.path}`);
            existed = true;
            existingMode = (await stat(targetPath)).mode;
            currentHash = sha256(await readFile(targetPath));
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
        if (currentHash !== change.originalHash) throw new EditorOperationError("conflict.hash", `Configuration changed after it was opened: ${change.path}`, { currentHash, originalHash: change.originalHash });

        await mkdir(path.dirname(targetPath), { recursive: true });
        const stagedPath = path.join(path.dirname(targetPath), `.${path.basename(targetPath)}.tmp.${randomUUID()}`);
        try {
            const fileHandle = await open(stagedPath, "wx", existingMode);
            try {
                await fileHandle.writeFile(change.source, "utf8");
                await fileHandle.sync();
            } finally {
                await fileHandle.close();
            }
            const newHash = sha256(Buffer.from(change.source, "utf8"));
            if (sha256(await readFile(stagedPath)) !== newHash) throw new EditorOperationError("save.verify", `Temporary file verification failed: ${change.path}`);
            const stagedDocument = parseByPath((await readFile(stagedPath)).toString("utf8"), change.path, this.knownActions, this.settingsSchema, this.actionTraits);
            if (stagedDocument.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new EditorOperationError("save.validate", `Temporary file validation failed: ${change.path}`, stagedDocument.diagnostics);
            return { ...change, existed, newHash, stagedPath, targetPath };
        } catch (error) {
            await this.removeFileIfPresent(stagedPath);
            throw error;
        }
    }

    private async restoreFile(backupPath: string, targetPath: string): Promise<void> {
        const stagedPath = path.join(path.dirname(targetPath), `.${path.basename(targetPath)}.restore.${randomUUID()}`);
        await copyFile(backupPath, stagedPath);
        await rename(stagedPath, targetPath);
    }

    private async assertOriginalUnchanged(prepared: PreparedChange): Promise<void> {
        let currentHash: string | null = null;
        try {
            const currentStats = await stat(prepared.targetPath);
            if (!currentStats.isFile()) throw new EditorOperationError("path.file", `Configuration target is not a file: ${prepared.path}`);
            currentHash = sha256(await readFile(prepared.targetPath));
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
        if (currentHash !== prepared.originalHash) throw new EditorOperationError("conflict.hash", `Configuration changed while the save was prepared: ${prepared.path}`, { currentHash, originalHash: prepared.originalHash });
    }

    private async removeFileIfPresent(filePath: string): Promise<void> {
        try {
            await unlink(filePath);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
    }
}
