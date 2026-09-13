import path from "node:path";
import { parseByPath, type AnyDocument } from "./formats.ts";
import { addDiagnostic, type Diagnostic } from "./model.ts";
import type { ProductTreeEntry } from "./paths.ts";
import type { ProductConfigSemantic } from "./product-config.ts";
import type { SnippetBinding, SnippetSemantic } from "./snippet.ts";
import type { ConfigurationStore } from "./store.ts";
import { EditorOperationError } from "./store.ts";
import { isCompatible, normalizedWidgetName, surfaceWidgetSlots, type WidgetCandidate, type WidgetCapability } from "./widget-capabilities.ts";

export interface SnippetBindingChoice {
    bindingId: string;
    confirmed: boolean;
    widgetName: string;
}

export interface SnippetApplicationRequest {
    bindingChoices: SnippetBindingChoice[];
    insertionLine: number;
    snippetPath: string;
    surfacePath: string;
    targetSource: string;
    targetZonePath: string;
}

export interface SnippetBindingPreview {
    automatic: boolean;
    candidates: WidgetCandidate[];
    confirmed: boolean;
    id: string;
    recommendedWidgetName?: string;
    required: true;
    selectedWidgetName: string;
}

export interface SnippetApplicationPreview {
    bindings: SnippetBindingPreview[];
    diagnostics: Diagnostic[];
    source: string;
    valid: boolean;
}

export interface SnippetSurfaceContext {
    automatic: boolean;
    surfaces: Array<{ path: string }>;
}

function uniqueChoices(choices: SnippetBindingChoice[]): Map<string, SnippetBindingChoice> {
    const result = new Map<string, SnippetBindingChoice>();
    for (const choice of choices) {
        const bindingId = choice.bindingId.toLowerCase();
        if (!bindingId || result.has(bindingId)) throw new EditorOperationError("snippet.choice.duplicate", `Snippet widget choice is missing or duplicated: ${choice.bindingId}`);
        result.set(bindingId, choice);
    }
    return result;
}

function requiredCapabilities(binding: SnippetBinding): WidgetCapability[] {
    const capabilities = new Set<WidgetCapability>();
    for (const action of binding.actions) {
        const selectors = action.inputSelectors;
        if (selectors.some((selector) => ["Press", "Tap", "Release", "Hold", "LongHold", "DoublePress", "Modifier"].includes(selector))) capabilities.add("press-input");
        if (selectors.some((selector) => selector === "Increase" || selector === "Decrease")) capabilities.add("relative-input");
        if (selectors.includes("Touch")) capabilities.add("touch-input");
        if (action.properties.has("RingStyle") || action.properties.has("BarStyle")) capabilities.add("value-feedback");
        if (action.properties.has("StateColors")) capabilities.add("color-feedback");
    }
    return [...capabilities];
}

function widgetCandidates(surface: AnyDocument, binding: SnippetBinding): WidgetCandidate[] {
    const unique = new Map<string, WidgetCandidate>();
    for (const candidate of [...surfaceWidgetSlots(surface, false), ...surfaceWidgetSlots(surface, true)]) if (!unique.has(normalizedWidgetName(candidate.name))) unique.set(normalizedWidgetName(candidate.name), candidate);
    const required = requiredCapabilities(binding);
    return [...unique.values()].filter((candidate) => isCompatible(required, candidate.capabilities)).sort((left, right) => left.name.localeCompare(right.name));
}

function resolvedLine(binding: SnippetBinding, targetWidget: string, sourceLines: string[]): string[] {
    return binding.actions.map((action) => {
        const originalLine = sourceLines[action.line - 1] ?? "";
        const indentation = originalLine.match(/^\s*/)?.[0] ?? "";
        const trimmed = originalLine.slice(indentation.length);
        const suffix = trimmed.slice(action.expression.length);
        const expression = action.expression.endsWith(binding.sourceWidget) ? action.expression.slice(0, -binding.sourceWidget.length) + targetWidget : targetWidget;
        return `${indentation}${expression}${suffix}`;
    });
}

function insertFragment(document: AnyDocument, fragmentLines: string[], insertionLine: number): string {
    const lineEnding = document.lines.find((line) => line.ending)?.ending ?? "\n";
    const sourceLines = document.lines.map((line) => line.text + line.ending);
    let metadataEnd = 0;
    for (let lineIdx = 0; lineIdx < document.lines.length; lineIdx++) {
        if (document.lines[lineIdx].kind === "format" || lineIdx === metadataEnd && document.lines[lineIdx].kind === "block-end") metadataEnd = lineIdx + 1;
        else if (metadataEnd) break;
    }
    const requestedIndex = insertionLine > metadataEnd ? Math.min(insertionLine, sourceLines.length) : metadataEnd;
    const generated = fragmentLines.map((line) => line.replace(/\r?\n$/, "") + lineEnding);
    if (requestedIndex > 0 && document.lines[requestedIndex - 1]?.text.trim()) generated.unshift(lineEnding);
    if (document.lines[requestedIndex]?.text.trim()) generated.push(lineEnding);
    sourceLines.splice(requestedIndex, 0, ...generated);
    return sourceLines.join("");
}

export async function previewSnippetApplication(store: ConfigurationStore, knownActions: Set<string>, request: SnippetApplicationRequest): Promise<SnippetApplicationPreview> {
    const [openedSnippet, openedSurface, openedZone] = await Promise.all([store.openDocument(request.snippetPath), store.openDocument(request.surfacePath), store.openDocument(request.targetZonePath)]);
    if (openedSnippet.document.format !== "snippet") throw new EditorOperationError("snippet.source.format", "Snippet source must be a .snippet file");
    if (openedSurface.document.format !== "surface") throw new EditorOperationError("snippet.surface.format", "Snippet surface must be a Surface file");
    if (openedZone.document.format !== "zone" || !openedZone.writable) throw new EditorOperationError("snippet.zone.target", "Snippet target must be an editable User Zone");

    const snippetDocument = parseByPath(openedSnippet.source, request.snippetPath, knownActions);
    const surfaceDocument = parseByPath(openedSurface.source, request.surfacePath, knownActions);
    const zoneDocument = parseByPath(request.targetSource, request.targetZonePath, knownActions);
    const snippet = snippetDocument.semantic as SnippetSemantic;
    const diagnostics = [...snippetDocument.diagnostics, ...surfaceDocument.diagnostics, ...zoneDocument.diagnostics];
    const choices = uniqueChoices(request.bindingChoices);
    const bindingPreviews: SnippetBindingPreview[] = [];
    const resolvedLines: Array<{ line: number; text: string }> = [];
    const snippetSourceLines = openedSnippet.source.split(/(?<=\n)/);
    for (const binding of snippet.bindings) {
        const candidates = widgetCandidates(surfaceDocument, binding);
        const exactCandidate = candidates.find((candidate) => normalizedWidgetName(candidate.name) === normalizedWidgetName(binding.sourceWidget));
        const choice = choices.get(binding.id.toLowerCase());
        const selectedWidgetName = choice?.widgetName || exactCandidate?.name || "";
        const selectedCandidate = candidates.find((candidate) => normalizedWidgetName(candidate.name) === normalizedWidgetName(selectedWidgetName));
        const automatic = Boolean(exactCandidate && selectedCandidate === exactCandidate);
        bindingPreviews.push({ automatic, candidates, confirmed: automatic || choice?.confirmed === true, id: binding.id, recommendedWidgetName: exactCandidate?.name ?? candidates[0]?.name, required: true, selectedWidgetName });
        if (!selectedCandidate) {
            addDiagnostic(diagnostics, "error", "snippet.binding.choice", `Choose a target Widget for ${binding.sourceWidget}`, binding.line, request.snippetPath);
            continue;
        }
        if (!automatic && !choice?.confirmed) {
            addDiagnostic(diagnostics, "error", "snippet.binding.confirm", `Confirm Widget ${selectedCandidate.name} for ${binding.sourceWidget}`, binding.line, request.snippetPath);
            continue;
        }
        const lines = resolvedLine(binding, selectedCandidate.name, snippetSourceLines);
        for (let lineIdx = 0; lineIdx < lines.length; lineIdx++) resolvedLines.push({ line: binding.actions[lineIdx].line, text: lines[lineIdx] });
    }
    for (const choice of choices.values()) if (!snippet.bindings.some((binding) => binding.id.toLowerCase() === choice.bindingId.toLowerCase())) addDiagnostic(diagnostics, "error", "snippet.binding.unknown", `Widget choice does not match a snippet Widget: ${choice.bindingId}`, undefined, request.snippetPath);
    resolvedLines.sort((left, right) => left.line - right.line);
    const source = insertFragment(zoneDocument, resolvedLines.map((entry) => entry.text), request.insertionLine);
    const resolvedZone = parseByPath(source, request.targetZonePath, knownActions);
    diagnostics.push(...resolvedZone.diagnostics.filter((diagnostic) => diagnostic.severity === "error"));
    diagnostics.push(...store.validateZoneGesturesForSurface(resolvedZone, surfaceDocument).filter((diagnostic) => diagnostic.severity === "error"));
    return { bindings: bindingPreviews, diagnostics, source, valid: !diagnostics.some((diagnostic) => diagnostic.severity === "error") };
}

function flattenSurfacePaths(entries: ProductTreeEntry[], paths: string[] = []): string[] {
    for (const entry of entries) {
        if (entry.kind === "file" && entry.type === "surface") paths.push(entry.path);
        else if (entry.kind === "directory") flattenSurfacePaths(entry.children || [], paths);
    }
    return paths;
}

async function existingSurfacePath(store: ConfigurationStore, surfaceId: string, requestedOwner?: string): Promise<string | undefined> {
    const owners = requestedOwner === "Vendor" || requestedOwner === "User" ? [requestedOwner] : ["User", "Vendor"];
    for (const owner of owners) {
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
    if (!match) throw new EditorOperationError("snippet.zone.target", "Choose an editable User Zone before inserting a snippet");
    const profileId = match[1];
    const profileProperty = match[2] === "FX" ? "FXProfile" : "MainProfile";
    const productConfig = await store.openDocument(configFilename);
    const document = parseByPath(productConfig.source, configFilename, knownActions);
    const records = (document.semantic as ProductConfigSemantic).records.filter((record) => record.kind === "surface-assignment" && record.properties.get(profileProperty) === profileId);
    const configuredPaths = [...new Set((await Promise.all(records.map((record) => {
        const surfaceId = record.properties.get("Template");
        return surfaceId ? existingSurfacePath(store, surfaceId, record.properties.get("TemplateSource")) : undefined;
    }))).filter((surfacePath): surfacePath is string => Boolean(surfacePath)))];
    if (configuredPaths.length) return { automatic: configuredPaths.length === 1, surfaces: configuredPaths.map((surfacePath) => ({ path: surfacePath })) };
    const preferredPaths = new Map<string, string>();
    for (const surfacePath of flattenSurfacePaths(await store.tree()).sort((left, right) => Number(right.includes("/User/")) - Number(left.includes("/User/")))) {
        const surfaceId = path.posix.basename(surfacePath, ".txt");
        if (!preferredPaths.has(surfaceId)) preferredPaths.set(surfaceId, surfacePath);
    }
    return { automatic: false, surfaces: [...preferredPaths.values()].sort().map((surfacePath) => ({ path: surfacePath })) };
}
