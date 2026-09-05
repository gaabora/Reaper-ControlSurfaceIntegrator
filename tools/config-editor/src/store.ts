import { copyFile, lstat, mkdir, open, readFile, readdir, realpath, rename, rm, stat, unlink, writeFile } from "node:fs/promises";
import { createHash, randomUUID } from "node:crypto";
import path from "node:path";
import type { ActionTraits } from "./action-catalog.ts";
import type { AnyDocument } from "./formats.ts";
import { parseByPath } from "./formats.ts";
import type { Diagnostic } from "./model.ts";
import type { ProductRootGuard, ProductTreeEntry } from "./paths.ts";
import type { SettingsSchema } from "./settings-schema.ts";
import { applyQuickFix as applyRegisteredQuickFix, diagnosticWithQuickFixes, diagnosticsWithQuickFixes, type QuickFixRequest } from "./quick-fixes.ts";
import { validateDocumentSet } from "./validation.ts";

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
        const diagnostics = validateDocumentSet(documents).map((diagnostic) => {
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
        const diagnostics = documents.flatMap((document) => document.diagnostics).concat(validateDocumentSet(documents));
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
        if (info.owner === "vendor" && info.kind === "zone" && segments[3] === "FX") return this.cloneFile(relativePath, `Zones/User/${segments.slice(2).join("/")}`);
        if (info.owner === "vendor" && info.kind === "zone" && segments[3] === "Main") return this.cloneMainZones(segments[2]);
        throw new EditorOperationError("clone.unsupported", "Only vendor surfaces, vendor Main zones, individual vendor FX zones, and built-in snippets can be cloned");
    }

    private async cloneFile(sourceRelativePath: string, targetRelativePath: string): Promise<OperationReport> {
        const sourcePath = await this.guard.resolveExisting(sourceRelativePath);
        const source = await readFile(sourcePath, "utf8");
        return this.saveTransaction([{ originalHash: null, path: targetRelativePath, source }]);
    }

    private async cloneMainZones(profileId: string): Promise<OperationReport> {
        if (!/^[a-z0-9][a-z0-9_-]*$/.test(profileId)) throw new EditorOperationError("clone.profile-id", `Invalid vendor profile ID: ${profileId}`);
        const sourceRelativePath = `Zones/Vendor/${profileId}/Main`;
        const targetRelativePath = `Zones/User/${profileId}/Main`;
        const sourceRoot = await this.guard.resolveExisting(sourceRelativePath);
        const targetRoot = await this.guard.resolveForWrite(targetRelativePath);
        try {
            await lstat(targetRoot);
            throw new EditorOperationError("conflict.exists", `User Main zone folder already exists: ${profileId}`);
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
        const temporaryRoot = `${targetRoot}.tmp.${randomUUID()}`;
        const report = emptyReport();
        try {
            await mkdir(path.dirname(targetRoot), { recursive: true });
            await mkdir(temporaryRoot);
            await this.copyDirectory(sourceRoot, temporaryRoot, sourceRelativePath, report, new Set());
            await rename(temporaryRoot, targetRoot);
            return report;
        } catch (error) {
            await rm(temporaryRoot, { force: true, recursive: true });
            report.failed.push(targetRelativePath);
            throw new EditorOperationError("clone.failed", error instanceof Error ? error.message : String(error), report);
        }
    }

    private async copyDirectory(sourceDirectory: string, targetDirectory: string, sourceRelativeDirectory: string, report: OperationReport, ancestorDirectories: Set<string>): Promise<void> {
        const canonicalDirectory = await realpath(sourceDirectory);
        if (ancestorDirectories.has(canonicalDirectory)) throw new EditorOperationError("clone.cycle", `Vendor Main zone folder contains a directory link cycle: ${sourceRelativeDirectory}`);
        const currentAncestors = new Set(ancestorDirectories);
        currentAncestors.add(canonicalDirectory);
        for (const entry of await readdir(sourceDirectory, { withFileTypes: true })) {
            const sourcePath = path.join(sourceDirectory, entry.name);
            const targetPath = path.join(targetDirectory, entry.name);
            const sourceRelativePath = `${sourceRelativeDirectory}/${entry.name}`;
            const sourceStats = await stat(sourcePath);
            if (sourceStats.isDirectory()) {
                await mkdir(targetPath);
                await this.copyDirectory(sourcePath, targetPath, sourceRelativePath, report, currentAncestors);
            } else if (sourceStats.isFile()) {
                await copyFile(sourcePath, targetPath);
                report.created.push(sourceRelativePath.replace("Zones/Vendor/", "Zones/User/"));
            } else {
                throw new EditorOperationError("clone.file-type", `Vendor Main zone folder contains an unsupported file: ${sourceRelativePath}`);
            }
        }
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
