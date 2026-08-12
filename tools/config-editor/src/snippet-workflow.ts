import { createHash } from "node:crypto";
import path from "node:path";
import { parseByPath, type AnyDocument } from "./formats.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import { snippetBindingCapabilities, snippetBindingRole, type SnippetBinding, type SnippetSemantic } from "./snippet.ts";
import type { ConfigurationStore, OperationReport, SaveChange } from "./store.ts";
import { EditorOperationError } from "./store.ts";
import { isStableId } from "./text.ts";
import { normalizedWidgetName, surfaceWidgetSlots, type WidgetCandidate, type WidgetCapability } from "./widget-capabilities.ts";
import type { ZoneSemantic } from "./zone.ts";

export type SnippetConflictAction = "" | "create" | "rename" | "replace" | "skip";

export interface SnippetBindingChoice {
    allowIncompatible: boolean;
    bindingId: string;
    confirmed: boolean;
    widgetName: string;
}

export interface SnippetApplicationRequest {
    applicationId: string;
    bindingChoices: SnippetBindingChoice[];
    conflictAction: SnippetConflictAction;
    renamedApplicationId?: string;
    snippetPath: string;
    surfacePath: string;
    targetZonePath: string;
}

export interface SnippetApplyRequest extends SnippetApplicationRequest {
    snippetHash: string;
    surfaceHash: string;
    targetHash: string;
}

export interface SnippetWidgetCandidate extends WidgetCandidate {
    compatible: boolean;
    mismatchReasons: string[];
}

export interface SnippetBindingPreview {
    automatic: boolean;
    candidates: SnippetWidgetCandidate[];
    confirmed: boolean;
    id: string;
    recommendedWidgetName?: string;
    required: boolean;
    requiredCapabilities: WidgetCapability[];
    requiredRole: string;
    selectedWidgetName: string;
}

export interface SnippetApplicationPreview {
    applicationId: string;
    bindings: SnippetBindingPreview[];
    conflict: { action: SnippetConflictAction; existingApplicationId?: string };
    diagnostics: Diagnostic[];
    snippetHash: string;
    source: string;
    surfaceHash: string;
    targetHash: string;
    targetZonePath: string;
    valid: boolean;
}

export interface SnippetImportPreview {
    defaultTargetPath: string;
    diagnostics: Diagnostic[];
    source: string;
    sourceHash: string;
    targetExists: boolean;
    targetHash: string | null;
    targetPath: string;
    valid: boolean;
}

export interface SnippetImportRequest {
    action: SnippetConflictAction;
    fileName: string;
    source: string;
    sourceHash: string;
    targetHash: string | null;
    targetPath: string;
}

interface ApplicationBlock {
    endIndex: number;
    startIndex: number;
}

function sha256(source: string): string {
    return createHash("sha256").update(source).digest("hex");
}

function uniqueChoices(choices: SnippetBindingChoice[]): Map<string, SnippetBindingChoice> {
    const result = new Map<string, SnippetBindingChoice>();
    for (const choice of choices) {
        const bindingId = choice.bindingId.toLowerCase();
        if (!bindingId || result.has(bindingId)) throw new EditorOperationError("snippet.choice.duplicate", `Snippet binding choice is missing or duplicated: ${choice.bindingId}`);
        result.set(bindingId, choice);
    }
    return result;
}

function candidateMismatch(binding: SnippetBinding, candidate: WidgetCandidate): string[] {
    const reasons: string[] = [];
    const requiredRole = snippetBindingRole(binding);
    if (candidate.role !== requiredRole) reasons.push(`requires role ${requiredRole}, widget role is ${candidate.role}`);
    const available = new Set(candidate.capabilities);
    const missingCapabilities = snippetBindingCapabilities(binding).filter((capability) => !available.has(capability));
    if (missingCapabilities.length) reasons.push(`missing ${missingCapabilities.join(", ")}`);
    return reasons;
}

function widgetCandidates(surface: AnyDocument, binding: SnippetBinding): SnippetWidgetCandidate[] {
    const candidates = [...surfaceWidgetSlots(surface, false), ...surfaceWidgetSlots(surface, true)];
    const unique = new Map<string, WidgetCandidate>();
    for (const candidate of candidates) if (!unique.has(normalizedWidgetName(candidate.name))) unique.set(normalizedWidgetName(candidate.name), candidate);
    return [...unique.values()].map((candidate) => {
        const mismatchReasons = candidateMismatch(binding, candidate);
        return { ...candidate, compatible: mismatchReasons.length === 0, mismatchReasons };
    }).sort((left, right) => Number(right.compatible) - Number(left.compatible) || left.name.localeCompare(right.name));
}

function quoteZoneToken(token: string): string {
    if (!/[\s"]/.test(token)) return token;
    const escaped = token.replaceAll("\\", "\\\\").replaceAll('"', '\\"');
    const equalsPosition = escaped.indexOf("=");
    if (equalsPosition > 0) return `${escaped.slice(0, equalsPosition + 1)}"${escaped.slice(equalsPosition + 1)}"`;
    return `"${escaped}"`;
}

function resolvedActionLines(binding: SnippetBinding, widgetName: string): string[] {
    return binding.actions.map((action) => {
        const modifierPrefix = action.modifiers.toLowerCase() === "nomod" ? "" : `${action.modifiers}+`;
        const params = action.params.map(quoteZoneToken);
        return `${modifierPrefix}${widgetName} ${action.action}${params.length ? ` ${params.join(" ")}` : ""}`;
    });
}

function findApplicationBlock(document: AnyDocument, applicationId: string, diagnostics: Diagnostic[]): ApplicationBlock | undefined {
    const startPattern = /^\/\/\s*@snippet\s+Application=([a-z0-9][a-z0-9_-]*)\s+Source=([a-z0-9][a-z0-9_-]*)\s*$/;
    const endPattern = /^\/\/\s*@snippet-end\s+Application=([a-z0-9][a-z0-9_-]*)\s*$/;
    const matches: ApplicationBlock[] = [];
    for (let lineIdx = 0; lineIdx < document.lines.length; lineIdx++) {
        const start = document.lines[lineIdx].text.trim().match(startPattern);
        if (!start || start[1] !== applicationId) continue;
        let endIndex = -1;
        for (let candidateIdx = lineIdx + 1; candidateIdx < document.lines.length; candidateIdx++) {
            const end = document.lines[candidateIdx].text.trim().match(endPattern);
            if (end?.[1] === applicationId) {
                endIndex = candidateIdx;
                break;
            }
        }
        if (endIndex < 0) addDiagnostic(diagnostics, "error", "snippet.application.end", `Applied snippet block has no end marker: ${applicationId}`, document.lines[lineIdx].lineNumber, document.path);
        else matches.push({ endIndex, startIndex: lineIdx });
    }
    if (matches.length > 1) addDiagnostic(diagnostics, "error", "snippet.application.duplicate", `Applied snippet block is duplicated: ${applicationId}`, document.lines[matches[1].startIndex].lineNumber, document.path);
    return matches[0];
}

function applicationSource(document: AnyDocument, block: ApplicationBlock | undefined, applicationId: string, snippetId: string, actionLines: string[]): string {
    const lineEnding = document.lines.find((line) => line.ending)?.ending ?? "\n";
    const indentation = block ? document.lines[block.startIndex].text.match(/^\s*/)?.[0] ?? "  " : (document.semantic as ZoneSemantic).bindings[0] ? document.lines[(document.semantic as ZoneSemantic).bindings[0].line - 1].text.match(/^\s*/)?.[0] ?? "  " : "  ";
    const generatedLines = [
        `${indentation}// @snippet Application=${applicationId} Source=${snippetId}`,
        ...actionLines.map((line) => `${indentation}${line}`),
        `${indentation}// @snippet-end Application=${applicationId}`,
    ].map((line) => `${line}${lineEnding}`);
    const sourceLines = document.lines.map((line) => line.text + line.ending);
    if (block) sourceLines.splice(block.startIndex, block.endIndex - block.startIndex + 1, ...generatedLines);
    else {
        const zoneEndIndex = document.lines.findIndex((line) => line.tokens[0] === "ZoneEnd");
        if (zoneEndIndex < 0) return document.source;
        if (zoneEndIndex > 0 && document.lines[zoneEndIndex - 1].text.trim()) generatedLines.unshift(lineEnding);
        sourceLines.splice(zoneEndIndex, 0, ...generatedLines);
    }
    return sourceLines.join("");
}

export async function previewSnippetApplication(store: ConfigurationStore, knownActions: Set<string>, request: SnippetApplicationRequest): Promise<SnippetApplicationPreview> {
    const [openedSnippet, openedSurface, openedZone] = await Promise.all([store.openDocument(request.snippetPath), store.openDocument(request.surfacePath), store.openDocument(request.targetZonePath)]);
    if (openedSnippet.document.format !== "snippet") throw new EditorOperationError("snippet.source.format", "Snippet source must be a .snippet file");
    if (openedSurface.document.format !== "surface") throw new EditorOperationError("snippet.surface.format", "Snippet surface must be a surface file");
    if (openedZone.document.format !== "zone" || !openedZone.writable) throw new EditorOperationError("snippet.zone.target", "Snippet target must be an editable user zone");

    const snippetDocument = parseByPath(openedSnippet.source, request.snippetPath, knownActions);
    const surfaceDocument = parseByPath(openedSurface.source, request.surfacePath, knownActions);
    const zoneDocument = parseByPath(openedZone.source, request.targetZonePath, knownActions);
    const snippet = snippetDocument.semantic as SnippetSemantic;
    const diagnostics = [...snippetDocument.diagnostics, ...surfaceDocument.diagnostics, ...zoneDocument.diagnostics];
    const requestedApplicationId = request.applicationId || snippet.id || "";
    if (!isStableId(requestedApplicationId)) addDiagnostic(diagnostics, "error", "snippet.application.id", "Application ID must be a stable lowercase ASCII ID", undefined, request.targetZonePath);
    const existingBlock = isStableId(requestedApplicationId) ? findApplicationBlock(zoneDocument, requestedApplicationId, diagnostics) : undefined;
    let applicationId = requestedApplicationId;
    const existingConflictActions: SnippetConflictAction[] = ["replace", "rename", "skip"];
    let conflictAction: SnippetConflictAction = existingBlock && existingConflictActions.includes(request.conflictAction) ? request.conflictAction : existingBlock ? "" : "create";
    if (existingBlock && !conflictAction) addDiagnostic(diagnostics, "error", "snippet.application.conflict", `Choose Replace, Rename, or Skip for existing application ${requestedApplicationId}`, undefined, request.targetZonePath);
    if (existingBlock && conflictAction === "rename") {
        applicationId = request.renamedApplicationId ?? "";
        if (!isStableId(applicationId) || applicationId === requestedApplicationId) addDiagnostic(diagnostics, "error", "snippet.application.rename", "Rename requires a different stable application ID", undefined, request.targetZonePath);
        else if (findApplicationBlock(zoneDocument, applicationId, diagnostics)) addDiagnostic(diagnostics, "error", "snippet.application.rename.exists", `Applied snippet block already exists: ${applicationId}`, undefined, request.targetZonePath);
    }
    if (!existingBlock && !["", "create"].includes(request.conflictAction)) addDiagnostic(diagnostics, "error", "snippet.application.action", `New application must use Create: ${requestedApplicationId}`, undefined, request.targetZonePath);

    const bindingPreviews: SnippetBindingPreview[] = [];
    let source = openedZone.source;
    if (conflictAction !== "skip") {
        const choices = uniqueChoices(request.bindingChoices);
        const actionLines: string[] = [];
        for (const binding of snippet.bindings) {
            const candidates = widgetCandidates(surfaceDocument, binding);
            const compatibleCandidates = candidates.filter((candidate) => candidate.compatible);
            const automaticCandidate = compatibleCandidates.find((candidate) => normalizedWidgetName(candidate.name) === binding.id);
            const recommendedWidgetName = (automaticCandidate ?? compatibleCandidates[0])?.name;
            const choice = choices.get(binding.id.toLowerCase());
            const selectedWidgetName = choice ? choice.widgetName : automaticCandidate?.name ?? "";
            const selectedCandidate = selectedWidgetName ? candidates.find((candidate) => normalizedWidgetName(candidate.name) === normalizedWidgetName(selectedWidgetName)) : undefined;
            const automatic = Boolean(selectedCandidate?.compatible && normalizedWidgetName(selectedCandidate.name) === binding.id);
            bindingPreviews.push({ automatic, candidates, confirmed: automatic || choice?.confirmed === true, id: binding.id, recommendedWidgetName, required: binding.required, requiredCapabilities: snippetBindingCapabilities(binding), requiredRole: snippetBindingRole(binding), selectedWidgetName });
            if (!choice && !automaticCandidate) {
                addDiagnostic(diagnostics, "error", "snippet.binding.choice", `Choose and confirm a widget for binding ${binding.id}`, binding.line, request.snippetPath);
                continue;
            }
            if (!selectedWidgetName) {
                if (binding.required) addDiagnostic(diagnostics, "error", "snippet.binding.required", `Required binding cannot be skipped: ${binding.id}`, binding.line, request.snippetPath);
                else if (!choice?.confirmed) addDiagnostic(diagnostics, "error", "snippet.binding.confirm", `Confirm that optional binding ${binding.id} will be skipped`, binding.line, request.snippetPath);
                continue;
            }
            if (!selectedCandidate) {
                addDiagnostic(diagnostics, "error", "snippet.binding.widget", `Selected widget does not exist on the surface: ${selectedWidgetName}`, binding.line, request.surfacePath);
                continue;
            }
            if (!selectedCandidate.compatible) {
                const reason = selectedCandidate.mismatchReasons.join("; ");
                if (!choice?.allowIncompatible) addDiagnostic(diagnostics, "error", "snippet.binding.incompatible", `Widget ${selectedCandidate.name} is incompatible with ${binding.id}: ${reason}`, binding.line, request.surfacePath);
                else addDiagnostic(diagnostics, "warning", "snippet.binding.override", `Compatibility override for ${binding.id} -> ${selectedCandidate.name}: ${reason}`, binding.line, request.surfacePath);
            }
            if (!automatic && !choice?.confirmed) addDiagnostic(diagnostics, "error", "snippet.binding.confirm", `Confirm widget ${selectedCandidate.name} for binding ${binding.id}`, binding.line, request.snippetPath);
            actionLines.push(...resolvedActionLines(binding, selectedCandidate.name));
        }
        for (const choice of choices.values()) if (!snippet.bindings.some((binding) => binding.id.toLowerCase() === choice.bindingId.toLowerCase())) addDiagnostic(diagnostics, "error", "snippet.binding.unknown", `Widget choice does not match a snippet binding: ${choice.bindingId}`, undefined, request.snippetPath);
        if (snippet.id && isStableId(applicationId) && (!existingBlock || conflictAction === "replace" || conflictAction === "rename")) source = applicationSource(zoneDocument, existingBlock && conflictAction === "replace" ? existingBlock : undefined, applicationId, snippet.id, actionLines);
        const resolvedZone = parseByPath(source, request.targetZonePath, knownActions);
        diagnostics.push(...resolvedZone.diagnostics.filter((diagnostic) => diagnostic.severity === "error"));
    }

    return {
        applicationId,
        bindings: bindingPreviews,
        conflict: { action: conflictAction, existingApplicationId: existingBlock ? requestedApplicationId : undefined },
        diagnostics,
        snippetHash: openedSnippet.hash,
        source,
        surfaceHash: openedSurface.hash,
        targetHash: openedZone.hash,
        targetZonePath: request.targetZonePath,
        valid: !diagnostics.some((diagnostic) => diagnostic.severity === "error"),
    };
}

export async function applySnippetApplication(store: ConfigurationStore, knownActions: Set<string>, request: SnippetApplyRequest): Promise<OperationReport> {
    const preview = await previewSnippetApplication(store, knownActions, request);
    if (request.snippetHash !== preview.snippetHash) throw new EditorOperationError("conflict.snippet-source", "Snippet changed after preview");
    if (request.surfaceHash !== preview.surfaceHash) throw new EditorOperationError("conflict.snippet-surface", "Surface changed after preview");
    if (request.targetHash !== preview.targetHash) throw new EditorOperationError("conflict.snippet-zone", "Target zone changed after preview");
    if (!preview.valid) throw new EditorOperationError("validation.failed", "Snippet application contains errors", preview.diagnostics);
    if (preview.conflict.action === "skip") return { changed: [], created: [], failed: [], restored: [], skipped: [request.targetZonePath] };
    return store.saveTransaction([{ originalHash: preview.targetHash, path: request.targetZonePath, source: preview.source }]);
}

function defaultSnippetTarget(fileName: string): string {
    const baseName = path.posix.basename(fileName);
    const snippetId = baseName.endsWith(".snippet") ? baseName.slice(0, -8) : "";
    if (baseName !== fileName || !isStableId(snippetId)) throw new EditorOperationError("snippet.import.name", "Snippet filename must use a stable lowercase ID and the .snippet extension");
    return `Snippets/User/${baseName}`;
}

function validateSnippetTarget(targetPath: string): void {
    if (!/^Snippets\/User\/[a-z0-9][a-z0-9_-]*\.snippet$/.test(targetPath)) throw new EditorOperationError("snippet.import.target", "Snippet import target must be Snippets/User/<stable-id>.snippet");
}

export async function previewSnippetImport(store: ConfigurationStore, knownActions: Set<string>, fileName: string, source: string, targetPath?: string): Promise<SnippetImportPreview> {
    const defaultTargetPath = defaultSnippetTarget(fileName);
    const resolvedTargetPath = targetPath || defaultTargetPath;
    validateSnippetTarget(resolvedTargetPath);
    const document = parseByPath(source, resolvedTargetPath, knownActions);
    const targetState = await store.fileState(resolvedTargetPath);
    return { defaultTargetPath, diagnostics: document.diagnostics, source, sourceHash: sha256(source), targetExists: targetState.exists, targetHash: targetState.hash, targetPath: resolvedTargetPath, valid: !document.diagnostics.some((diagnostic) => diagnostic.severity === "error") };
}

export async function importSnippet(store: ConfigurationStore, knownActions: Set<string>, request: SnippetImportRequest): Promise<OperationReport> {
    const preview = await previewSnippetImport(store, knownActions, request.fileName, request.source, request.targetPath);
    if (request.sourceHash !== preview.sourceHash) throw new EditorOperationError("conflict.snippet-import-source", "Imported snippet changed after preview");
    if (request.targetHash !== preview.targetHash) throw new EditorOperationError("conflict.snippet-import-target", "Snippet import target changed after preview");
    if (!preview.valid) throw new EditorOperationError("validation.failed", "Imported snippet contains errors", preview.diagnostics);
    if (request.action === "skip") {
        if (!preview.targetExists) throw new EditorOperationError("snippet.import.skip", "Skip requires an existing target");
        return { changed: [], created: [], failed: [], restored: [], skipped: [preview.targetPath] };
    }
    if (request.action === "replace" && !preview.targetExists) throw new EditorOperationError("snippet.import.replace", "Replace requires an existing target");
    if (request.action === "create" && preview.targetExists) throw new EditorOperationError("snippet.import.create", "Create cannot overwrite an existing target");
    if (request.action === "rename" && (preview.targetPath === preview.defaultTargetPath || preview.targetExists)) throw new EditorOperationError("snippet.import.rename", "Rename requires a different unused user snippet path");
    if (preview.targetExists && request.action !== "replace") throw new EditorOperationError("snippet.import.conflict", "Existing snippet import requires Replace, Rename, or Skip");
    if (!preview.targetExists && !["create", "rename"].includes(request.action)) throw new EditorOperationError("snippet.import.action", "New snippet import requires Create or Rename");
    const change: SaveChange = { originalHash: preview.targetHash, path: preview.targetPath, source: request.source };
    return store.saveTransaction([change]);
}
