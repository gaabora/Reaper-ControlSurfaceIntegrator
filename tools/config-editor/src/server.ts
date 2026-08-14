import { randomBytes, randomInt, timingSafeEqual } from "node:crypto";
import path from "node:path";
import type { ActionCatalogEntry } from "./action-catalog.ts";
import { actionNameSet } from "./action-catalog.ts";
import { ConfigurationDraftStore } from "./draft-store.ts";
import type { LegacyImportConflictAction, LegacyImportDraft, LegacyImportRequest, LegacyImportResolution, LegacyImportTargetPath, LegacySurfaceSummary, LegacyWidgetMapping } from "./legacy-import.ts";
import { LegacyCsiSource } from "./legacy-import.ts";
import type { ReaperDataPathCandidate } from "./paths.ts";
import { ProductPathError, ProductRootGuard } from "./paths.ts";
import type { EditorProductIdentity } from "./product-identity.ts";
import { QuickFixError, type QuickFixRequest } from "./quick-fixes.ts";
import { previewSnippetApplication, snippetSurfaceContext, type SnippetApplicationRequest, type SnippetBindingChoice, type SnippetConflictAction } from "./snippet-workflow.ts";
import type { SaveChange } from "./store.ts";
import { ConfigurationStore, EditorOperationError } from "./store.ts";
import { createEditorHtml, createEditorTranslationsJson, EDITOR_CSS } from "./ui.ts";

export interface EditorServerOptions {
    actions: ActionCatalogEntry[];
    candidates: ReaperDataPathCandidate[];
    editorJavascript: string;
    identity: EditorProductIdentity;
    port?: number;
}

export interface RunningEditorServer {
    server: ReturnType<typeof Bun.serve>;
    token: string;
    url: string;
}

const MAX_REQUEST_BYTES = 16 * 1024 * 1024;

interface LegacySourceSelection {
    path: string;
    root?: string;
    surfaces: LegacySurfaceSummary[];
}

async function trySelectLegacySource(selectedPath: string): Promise<{ selection: LegacySourceSelection; source?: LegacyCsiSource }> {
    try {
        const source = await LegacyCsiSource.create(selectedPath);
        return { selection: { path: selectedPath, root: source.getRoot(), surfaces: await source.listSurfaces() }, source };
    } catch {
        return { selection: { path: selectedPath, surfaces: [] } };
    }
}

function contentResponse(content: string, contentType: string): Response {
    return new Response(content, {
        headers: {
            "Cache-Control": "no-store",
            "Content-Security-Policy": "default-src 'self'; connect-src 'self'; img-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'",
            "Content-Type": contentType,
            "Referrer-Policy": "no-referrer",
            "X-Content-Type-Options": "nosniff",
            "X-Frame-Options": "DENY",
        },
    });
}

function jsonResponse(value: unknown, status = 200): Response {
    return new Response(JSON.stringify(value), { status, headers: { "Cache-Control": "no-store", "Content-Type": "application/json; charset=utf-8", "X-Content-Type-Options": "nosniff" } });
}

function tokenMatches(providedToken: string | null, expectedToken: string): boolean {
    if (!providedToken) return false;
    const provided = Buffer.from(providedToken);
    const expected = Buffer.from(expectedToken);
    return provided.length === expected.length && timingSafeEqual(provided, expected);
}

async function requestBody(request: Request): Promise<Record<string, unknown>> {
    const contentLength = Number(request.headers.get("content-length") ?? "0");
    if (!Number.isFinite(contentLength) || contentLength > MAX_REQUEST_BYTES) throw new EditorOperationError("request.size", "Request body is too large");
    const body = await request.text();
    if (Buffer.byteLength(body, "utf8") > MAX_REQUEST_BYTES) throw new EditorOperationError("request.size", "Request body is too large");
    const value = JSON.parse(body) as unknown;
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.body", "Request body must be a JSON object");
    return value as Record<string, unknown>;
}

function stringField(body: Record<string, unknown>, key: string): string {
    const value = body[key];
    if (typeof value !== "string") throw new EditorOperationError("request.field", `${key} must be a string`);
    return value;
}

function optionalStringField(body: Record<string, unknown>, key: string, defaultValue = ""): string {
    if (body[key] === undefined) return defaultValue;
    return stringField(body, key);
}

function booleanField(body: Record<string, unknown>, key: string): boolean {
    const value = body[key];
    if (typeof value !== "boolean") throw new EditorOperationError("request.field", `${key} must be a boolean`);
    return value;
}

function optionalBooleanField(body: Record<string, unknown>, key: string, defaultValue = false): boolean {
    if (body[key] === undefined) return defaultValue;
    return booleanField(body, key);
}

function positiveIntegerField(body: Record<string, unknown>, key: string): number {
    const value = body[key];
    if (typeof value !== "number" || !Number.isInteger(value) || value < 1) throw new EditorOperationError("request.field", `${key} must be a positive integer`);
    return value;
}

function stringArrayField(body: Record<string, unknown>, key: string, optional = false): string[] | undefined {
    const value = body[key];
    if (optional && value === undefined) return undefined;
    if (!Array.isArray(value) || value.some((item) => typeof item !== "string")) throw new EditorOperationError("request.field", `${key} must be an array of strings`);
    return value as string[];
}

function legacyResolution(value: unknown): LegacyImportResolution {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.resolution", "Each legacy import resolution must be an object");
    const body = value as Record<string, unknown>;
    const action = stringField(body, "action") as LegacyImportConflictAction;
    if (!["create", "rename", "replace", "skip"].includes(action)) throw new EditorOperationError("request.resolution.action", `Unsupported legacy import action: ${action}`);
    const targetHash = body.targetHash;
    if (targetHash !== null && typeof targetHash !== "string") throw new EditorOperationError("request.resolution.hash", "targetHash must be a string or null");
    const targetPath = body.targetPath;
    if (targetPath !== undefined && typeof targetPath !== "string") throw new EditorOperationError("request.resolution.path", "targetPath must be a string when provided");
    return { action, id: stringField(body, "id"), sourceHash: stringField(body, "sourceHash"), targetHash, targetPath };
}

function legacyWidgetMapping(value: unknown): LegacyWidgetMapping {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.widget-mapping", "Each widget mapping must be an object");
    const body = value as Record<string, unknown>;
    return { sourceWidget: stringField(body, "sourceWidget"), targetWidget: stringField(body, "targetWidget") };
}

function legacyWidgetMappings(body: Record<string, unknown>, optional = false): LegacyWidgetMapping[] {
    if (optional && body.widgetMappings === undefined) return [];
    if (!Array.isArray(body.widgetMappings)) throw new EditorOperationError("request.widget-mappings", "widgetMappings must be an array");
    return body.widgetMappings.map(legacyWidgetMapping);
}

function legacyDraft(value: unknown): LegacyImportDraft {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.legacy-draft", "Each import draft must be an object");
    const body = value as Record<string, unknown>;
    return { originalSourceHash: stringField(body, "originalSourceHash"), source: stringField(body, "source"), sourcePath: stringField(body, "sourcePath") };
}

function legacyDrafts(body: Record<string, unknown>): LegacyImportDraft[] {
    if (body.drafts === undefined) return [];
    if (!Array.isArray(body.drafts)) throw new EditorOperationError("request.legacy-drafts", "drafts must be an array");
    return body.drafts.map(legacyDraft);
}

function legacyTargetPath(value: unknown): LegacyImportTargetPath {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.legacy-target", "Each import target must be an object");
    const body = value as Record<string, unknown>;
    return { sourcePath: stringField(body, "sourcePath"), targetPath: stringField(body, "targetPath") };
}

function legacyTargetPaths(body: Record<string, unknown>): LegacyImportTargetPath[] {
    if (body.targetPaths === undefined) return [];
    if (!Array.isArray(body.targetPaths)) throw new EditorOperationError("request.legacy-targets", "targetPaths must be an array");
    return body.targetPaths.map(legacyTargetPath);
}

function legacyImportRequest(body: Record<string, unknown>): LegacyImportRequest {
    if (!Array.isArray(body.resolutions)) throw new EditorOperationError("request.resolutions", "resolutions must be an array");
    return {
        drafts: legacyDrafts(body),
        includeSurface: booleanField(body, "includeSurface"),
        resolutions: body.resolutions.map(legacyResolution),
        selectedZonePaths: stringArrayField(body, "selectedZonePaths")!,
        surfaceName: stringField(body, "surfaceName"),
        targetPaths: legacyTargetPaths(body),
        targetProfileId: optionalStringField(body, "targetProfileId") || undefined,
        widgetMappings: legacyWidgetMappings(body),
    };
}

function saveChange(value: unknown): SaveChange {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.change", "Each change must be an object");
    const body = value as Record<string, unknown>;
    const originalHash = body.originalHash;
    if (originalHash !== null && typeof originalHash !== "string") throw new EditorOperationError("request.hash", "originalHash must be a string or null");
    return { originalHash, path: stringField(body, "path"), source: stringField(body, "source") };
}

function snippetConflictAction(body: Record<string, unknown>): SnippetConflictAction {
    const action = optionalStringField(body, "conflictAction") as SnippetConflictAction;
    if (!["", "create", "rename", "replace", "skip"].includes(action)) throw new EditorOperationError("request.snippet.action", `Unsupported snippet conflict action: ${action}`);
    return action;
}

function snippetBindingChoice(value: unknown): SnippetBindingChoice {
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.snippet.choice", "Each snippet binding choice must be an object");
    const body = value as Record<string, unknown>;
    return { allowIncompatible: optionalBooleanField(body, "allowIncompatible"), bindingId: stringField(body, "bindingId"), confirmed: booleanField(body, "confirmed"), widgetName: stringField(body, "widgetName") };
}

function snippetApplicationRequest(body: Record<string, unknown>): SnippetApplicationRequest {
    if (!Array.isArray(body.bindingChoices)) throw new EditorOperationError("request.snippet.choices", "bindingChoices must be an array");
    return {
        applicationId: optionalStringField(body, "applicationId"),
        bindingChoices: body.bindingChoices.map(snippetBindingChoice),
        conflictAction: snippetConflictAction(body),
        insertionLine: positiveIntegerField(body, "insertionLine"),
        renamedApplicationId: optionalStringField(body, "renamedApplicationId") || undefined,
        snippetPath: stringField(body, "snippetPath"),
        surfacePath: stringField(body, "surfacePath"),
        targetSource: stringField(body, "targetSource"),
        targetZonePath: stringField(body, "targetZonePath"),
    };
}

function objectField(body: Record<string, unknown>, key: string): Record<string, unknown> {
    const value = body[key];
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new EditorOperationError("request.field", `${key} must be an object`);
    return value as Record<string, unknown>;
}

function quickFixRequest(body: Record<string, unknown>): QuickFixRequest {
    const diagnosticBody = objectField(body, "diagnostic");
    const fixBody = objectField(body, "fix");
    const line = diagnosticBody.line;
    if (line !== undefined && (typeof line !== "number" || !Number.isInteger(line) || line < 1)) throw new EditorOperationError("request.field", "diagnostic.line must be a positive integer when provided");
    const dataValue = fixBody.data;
    if (dataValue !== undefined && (!dataValue || typeof dataValue !== "object" || Array.isArray(dataValue) || Object.values(dataValue).some((value) => typeof value !== "string"))) throw new EditorOperationError("request.field", "fix.data must contain only string values");
    return {
        diagnostic: { code: stringField(diagnosticBody, "code"), line: line as number | undefined, message: stringField(diagnosticBody, "message") },
        fix: { data: dataValue as Record<string, string> | undefined, id: stringField(fixBody, "id") },
    };
}

function errorResponse(error: unknown): Response {
    if (error instanceof SyntaxError) return jsonResponse({ error: { code: "request.json", message: "Request body is not valid JSON" } }, 400);
    if (error instanceof ProductPathError) return jsonResponse({ error: { code: "path.invalid", message: error.message } }, 400);
    if (error instanceof QuickFixError) return jsonResponse({ error: { code: error.code, message: error.message } }, 400);
    if (error instanceof EditorOperationError) {
        const status = error.code.startsWith("conflict.") ? 409 : error.code.startsWith("validation.") || error.code.startsWith("save.validate") ? 422 : 400;
        return jsonResponse({ error: { code: error.code, details: error.details, message: error.message } }, status);
    }
    const errorCode = (error as NodeJS.ErrnoException).code;
    if (errorCode === "ENOENT") return jsonResponse({ error: { code: "path.missing", message: "Configuration path does not exist" } }, 404);
    if (errorCode === "EACCES" || errorCode === "EPERM") return jsonResponse({ error: { code: "path.permission", message: "Configuration path is not accessible" } }, 403);
    const message = error instanceof Error ? error.message : String(error);
    return jsonResponse({ error: { code: "server.error", message } }, 500);
}

export function startEditorServer(options: EditorServerOptions): RunningEditorServer {
    const token = randomBytes(32).toString("hex");
    const knownActions = actionNameSet(options.actions);
    let legacySelection: LegacySourceSelection | undefined;
    let legacySource: LegacyCsiSource | undefined;
    let draftStore: ConfigurationDraftStore | undefined;
    let store: ConfigurationStore | undefined;
    let origin = "";
    const fetchRequest = async (request: Request): Promise<Response> => {
        const requestUrl = new URL(request.url);
        if (requestUrl.pathname === "/" && request.method === "GET") return contentResponse(createEditorHtml(options.identity.displayName, token), "text/html; charset=utf-8");
        if (requestUrl.pathname === "/app.css" && request.method === "GET") return contentResponse(EDITOR_CSS, "text/css; charset=utf-8");
        if (requestUrl.pathname === "/app.js" && request.method === "GET") return contentResponse(options.editorJavascript, "text/javascript; charset=utf-8");
        if (requestUrl.pathname === "/app-translations.json" && request.method === "GET") return contentResponse(createEditorTranslationsJson(), "application/json; charset=utf-8");
        if (!requestUrl.pathname.startsWith("/api/")) return jsonResponse({ error: { code: "not-found", message: "Not found" } }, 404);
        if (!tokenMatches(request.headers.get("x-session-token"), token)) return jsonResponse({ error: { code: "auth.token", message: "Invalid session token" } }, 401);
        const requestOrigin = request.headers.get("origin");
        if (requestOrigin && requestOrigin !== origin) return jsonResponse({ error: { code: "auth.origin", message: "Invalid request origin" } }, 403);
        try {
            if (requestUrl.pathname === "/api/status" && request.method === "GET") return jsonResponse({ candidates: options.candidates, dataPath: store?.getReaperDataPath(), identity: options.identity, legacy: legacySelection });
            if (requestUrl.pathname === "/api/select-data-path" && request.method === "POST") {
                const body = await requestBody(request);
                const guard = await ProductRootGuard.createFromReaperDataPath(stringField(body, "path"), options.identity);
                store = new ConfigurationStore(guard, knownActions);
                draftStore = new ConfigurationDraftStore(options.identity.productId, store.getRoot());
                const automaticLegacy = await trySelectLegacySource(path.join(path.dirname(store.getReaperDataPath()), "CSI"));
                legacySelection = automaticLegacy.selection;
                legacySource = automaticLegacy.source;
                return jsonResponse({ dataPath: store.getReaperDataPath(), legacy: legacySelection });
            }
            if (requestUrl.pathname === "/api/legacy/select" && request.method === "POST") {
                const body = await requestBody(request);
                const selectedPath = stringField(body, "path");
                legacySource = await LegacyCsiSource.create(selectedPath);
                legacySelection = { path: selectedPath, root: legacySource.getRoot(), surfaces: await legacySource.listSurfaces() };
                return jsonResponse(legacySelection);
            }
            if (!store) throw new EditorOperationError("data-path.required", "Open a REAPER data path first");
            if (!draftStore) throw new EditorOperationError("draft.store.required", "Draft storage is unavailable");
            if (requestUrl.pathname.startsWith("/api/legacy/") && !legacySource) throw new EditorOperationError("legacy.path.required", "Open a legacy CSI path first");
            if (requestUrl.pathname === "/api/legacy/preview" && request.method === "POST") {
                const body = await requestBody(request);
                const selectedZonePaths = stringArrayField(body, "selectedZonePaths", true);
                return jsonResponse({ preview: await legacySource!.preview(store, knownActions, stringField(body, "surfaceName"), booleanField(body, "includeSurface"), selectedZonePaths, legacyWidgetMappings(body, true), optionalBooleanField(body, "useExistingSurface"), legacyDrafts(body), optionalStringField(body, "targetProfileId") || undefined, legacyTargetPaths(body)) });
            }
            if (requestUrl.pathname === "/api/legacy/import" && request.method === "POST") return jsonResponse({ report: await legacySource!.import(store, knownActions, legacyImportRequest(await requestBody(request))) });
            if (requestUrl.pathname === "/api/snippet/context" && request.method === "GET") {
                const zonePath = requestUrl.searchParams.get("zonePath");
                if (!zonePath) throw new EditorOperationError("request.path", "Zone path is required");
                return jsonResponse(await snippetSurfaceContext(store, knownActions, options.identity.configFilename, zonePath));
            }
            if (requestUrl.pathname === "/api/snippet/preview" && request.method === "POST") return jsonResponse({ preview: await previewSnippetApplication(store, knownActions, snippetApplicationRequest(await requestBody(request))) });
            if (requestUrl.pathname === "/api/tree" && request.method === "GET") return jsonResponse({ entries: await store.tree() });
            if (requestUrl.pathname === "/api/validate-all" && request.method === "POST") {
                const body = await requestBody(request);
                if (!Array.isArray(body.changes)) throw new EditorOperationError("request.changes", "changes must be an array");
                return jsonResponse(await store.validateAll(body.changes.map(saveChange)));
            }
            if (requestUrl.pathname === "/api/quick-fix" && request.method === "POST") {
                const body = await requestBody(request);
                return jsonResponse(store.applyQuickFix(stringField(body, "path"), stringField(body, "source"), quickFixRequest(body)));
            }
            if (requestUrl.pathname === "/api/drafts" && request.method === "GET") {
                const drafts = await draftStore.list();
                const availableDrafts = (await Promise.all(drafts.map(async (draft) => {
                    try {
                        const opened = await store.openDocument(draft.path);
                        return { ...draft, conflict: opened.hash !== draft.originalHash };
                    } catch {
                        return undefined;
                    }
                }))).filter((draft) => draft !== undefined);
                return jsonResponse({ drafts: availableDrafts });
            }
            if (requestUrl.pathname === "/api/draft" && request.method === "POST") {
                const body = await requestBody(request);
                const relativePath = stringField(body, "path");
                const originalHash = stringField(body, "originalHash");
                const source = stringField(body, "source");
                const opened = await store.openDocument(relativePath);
                if (!opened.writable) throw new EditorOperationError("draft.read-only", `Cannot create a draft for a read-only file: ${relativePath}`);
                if (opened.hash !== originalHash) throw new EditorOperationError("conflict.draft", `Configuration changed after the draft was created: ${relativePath}`, { currentHash: opened.hash, originalHash });
                if (opened.source === source) {
                    await draftStore.discard(relativePath);
                    return jsonResponse({ dirty: false });
                }
                return jsonResponse({ dirty: true, draft: await draftStore.write(relativePath, originalHash, source) });
            }
            if (requestUrl.pathname === "/api/draft/discard" && request.method === "POST") {
                const relativePath = stringField(await requestBody(request), "path");
                await store.openDocument(relativePath);
                await draftStore.discard(relativePath);
                return jsonResponse({ discarded: relativePath });
            }
            if (requestUrl.pathname === "/api/file" && request.method === "GET") {
                const relativePath = requestUrl.searchParams.get("path");
                if (!relativePath) throw new EditorOperationError("request.path", "File path is required");
                const opened = await store.openDocument(relativePath);
                let draft = await draftStore.read(relativePath);
                if (draft?.source === opened.source) {
                    await draftStore.discard(relativePath);
                    draft = undefined;
                }
                return jsonResponse({ ...opened, draft: draft ? { ...draft, conflict: draft.originalHash !== opened.hash, document: store.validateSource(relativePath, draft.source) } : undefined });
            }
            if (requestUrl.pathname === "/api/validate" && request.method === "POST") {
                const body = await requestBody(request);
                return jsonResponse({ document: store.validateSource(stringField(body, "path"), stringField(body, "source")) });
            }
            if (requestUrl.pathname === "/api/save" && request.method === "POST") {
                const change = saveChange(await requestBody(request));
                const result = await store.saveOne(change);
                await draftStore.discard(change.path);
                return jsonResponse(result);
            }
            if (requestUrl.pathname === "/api/transaction" && request.method === "POST") {
                const body = await requestBody(request);
                if (!Array.isArray(body.changes)) throw new EditorOperationError("request.changes", "changes must be an array");
                const changes = body.changes.map(saveChange);
                const report = await store.saveTransaction(changes);
                for (const change of changes) await draftStore.discard(change.path);
                return jsonResponse({ report });
            }
            if (requestUrl.pathname === "/api/clone" && request.method === "POST") {
                const body = await requestBody(request);
                return jsonResponse({ report: await store.cloneForEditing(stringField(body, "path")) });
            }
            return jsonResponse({ error: { code: "not-found", message: "Not found" } }, 404);
        } catch (error) {
            return errorResponse(error);
        }
    };
    let server: ReturnType<typeof Bun.serve> | undefined;
    const automaticPort = !options.port;
    for (let attemptIdx = 0; attemptIdx < 20 && !server; attemptIdx++) {
        const port = automaticPort ? randomInt(20000, 65536) : options.port;
        try {
            server = Bun.serve({ fetch: fetchRequest, hostname: "127.0.0.1", port });
        } catch (error) {
            if (!automaticPort || (error as NodeJS.ErrnoException).code !== "EADDRINUSE" || attemptIdx === 19) throw error;
        }
    }
    if (!server) throw new Error("Unable to allocate a local editor port");
    origin = `http://127.0.0.1:${server.port}`;
    return { server, token, url: `${origin}/` };
}
