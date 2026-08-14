import path from "node:path";
import { parseByPath, type AnyDocument } from "./formats.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { ProductTreeEntry } from "./paths.ts";
import type { ProductConfigSemantic } from "./product-config.ts";
import { snippetBindingCapabilities, snippetBindingRole, type SnippetBinding, type SnippetSemantic } from "./snippet.ts";
import type { ConfigurationStore } from "./store.ts";
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
    insertionLine: number;
    renamedApplicationId?: string;
    snippetPath: string;
    surfacePath: string;
    targetSource: string;
    targetZonePath: string;
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
    source: string;
    valid: boolean;
}

export interface SnippetSurfaceContext {
    automatic: boolean;
    surfaces: Array<{ path: string }>;
}

interface ApplicationBlock {
    endIndex: number;
    startIndex: number;
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

function applicationSource(document: AnyDocument, block: ApplicationBlock | undefined, applicationId: string, snippetId: string, actionLines: string[], insertionLine: number): string {
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
        const zoneStartIndex = document.lines.findIndex((line) => line.tokens[0] === "Zone");
        const requestedIndex = insertionLine > 1 ? Math.min(insertionLine, zoneEndIndex) : zoneEndIndex;
        const insertionIndex = requestedIndex > zoneStartIndex && requestedIndex <= zoneEndIndex ? requestedIndex : zoneEndIndex;
        if (insertionIndex > 0 && document.lines[insertionIndex - 1].text.trim()) generatedLines.unshift(lineEnding);
        if (document.lines[insertionIndex]?.text.trim()) generatedLines.push(lineEnding);
        sourceLines.splice(insertionIndex, 0, ...generatedLines);
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
    const zoneDocument = parseByPath(request.targetSource, request.targetZonePath, knownActions);
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
    let source = request.targetSource;
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
        if (snippet.id && isStableId(applicationId) && (!existingBlock || conflictAction === "replace" || conflictAction === "rename")) source = applicationSource(zoneDocument, existingBlock && conflictAction === "replace" ? existingBlock : undefined, applicationId, snippet.id, actionLines, request.insertionLine);
        const resolvedZone = parseByPath(source, request.targetZonePath, knownActions);
        diagnostics.push(...resolvedZone.diagnostics.filter((diagnostic) => diagnostic.severity === "error"));
    }

    return {
        applicationId,
        bindings: bindingPreviews,
        conflict: { action: conflictAction, existingApplicationId: existingBlock ? requestedApplicationId : undefined },
        diagnostics,
        source,
        valid: !diagnostics.some((diagnostic) => diagnostic.severity === "error"),
    };
}

function flattenSurfacePaths(entries: ProductTreeEntry[], paths: string[] = []): string[] {
    for (const entry of entries) {
        if (entry.kind === "file" && entry.type === "surface") paths.push(entry.path);
        else if (entry.kind === "directory") flattenSurfacePaths(entry.children || [], paths);
    }
    return paths;
}

async function existingSurfacePath(store: ConfigurationStore, surfaceId: string): Promise<string | undefined> {
    for (const owner of ["User", "Vendor"]) {
        const surfacePath = `Surfaces/${owner}/${surfaceId}.txt`;
        try {
            await store.openDocument(surfacePath);
            return surfacePath;
        } catch (error) {
            if ((error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        }
    }
    return undefined;
}

export async function snippetSurfaceContext(store: ConfigurationStore, knownActions: Set<string>, configFilename: string, zonePath: string): Promise<SnippetSurfaceContext> {
    const match = zonePath.match(/^Zones\/User\/([a-z0-9][a-z0-9_-]*)\/(Main|FX)\//);
    if (!match) throw new EditorOperationError("snippet.zone.target", "Choose an editable user zone before inserting a snippet");
    const profileId = match[1];
    const profileProperty = match[2] === "FX" ? "FXZoneFolder" : "ZoneFolder";
    const productConfig = await store.openDocument(configFilename);
    const document = parseByPath(productConfig.source, configFilename, knownActions);
    const records = (document.semantic as ProductConfigSemantic).records.filter((record) => record.kind === "surface-assignment" && record.properties.get(profileProperty) === profileId);
    const configuredIds = [...new Set(records.map((record) => record.properties.get("SurfaceFolder")).filter((surfaceId): surfaceId is string => Boolean(surfaceId)))];
    const configuredPaths = (await Promise.all(configuredIds.map((surfaceId) => existingSurfacePath(store, surfaceId)))).filter((surfacePath): surfacePath is string => Boolean(surfacePath));
    if (configuredPaths.length) return { automatic: configuredPaths.length === 1, surfaces: configuredPaths.map((surfacePath) => ({ path: surfacePath })) };
    const availablePaths = flattenSurfacePaths(await store.tree());
    const preferredPaths = new Map<string, string>();
    for (const surfacePath of availablePaths.sort((left, right) => Number(right.includes("/User/")) - Number(left.includes("/User/")))) {
        const surfaceId = path.posix.basename(surfacePath, ".txt");
        if (!preferredPaths.has(surfaceId)) preferredPaths.set(surfaceId, surfacePath);
    }
    return { automatic: false, surfaces: [...preferredPaths.values()].sort().map((surfacePath) => ({ path: surfacePath })) };
}
