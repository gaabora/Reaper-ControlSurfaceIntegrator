import path from "node:path";
import { addDiagnostic, type LosslessDocument } from "./model.ts";
import { parseZone, type ZoneBinding, type ZoneSemantic } from "./zone.ts";

export interface SnippetAction {
    action: string;
    expression: string;
    inputSelectors: string[];
    line: number;
    params: string[];
    properties: Map<string, string>;
}

export interface SnippetBinding {
    actions: SnippetAction[];
    id: string;
    line: number;
    required: true;
    sourceWidget: string;
}

export interface SnippetSemantic {
    bindings: SnippetBinding[];
    description?: string;
    id: string;
    name?: string;
}

function metadataValue(source: string, name: string): string | undefined {
    const value = source.match(new RegExp(`\\b${name}=((?:"(?:\\\\.|[^"\\\\])*")|[^\\s}]+)`))?.[1];
    return value?.startsWith('"') ? value.slice(1, -1).replace(/\\"/g, '"').replace(/\\\\/g, "\\") : value;
}

function sourceExpression(source: string, line: number): string {
    return source.split(/\r?\n/)[line - 1]?.trimStart().match(/^\S+/)?.[0] ?? "";
}

function appendBinding(bindings: Map<string, SnippetBinding>, binding: ZoneBinding, source: string): void {
    const sourceWidget = binding.widget;
    const key = sourceWidget.toLowerCase();
    let snippetBinding = bindings.get(key);
    if (!snippetBinding) {
        snippetBinding = { actions: [], id: sourceWidget, line: binding.line, required: true, sourceWidget };
        bindings.set(key, snippetBinding);
    }
    snippetBinding.actions.push({ action: binding.action, expression: sourceExpression(source, binding.line), inputSelectors: binding.inputSelectors, line: binding.line, params: binding.params, properties: binding.properties });
}

export function parseSnippet(source: string, documentPath?: string, knownActions?: Set<string>): LosslessDocument<SnippetSemantic> {
    const zoneDocument = parseZone(source, documentPath, knownActions, undefined, undefined, "@Snippet");
    const zoneSemantic = zoneDocument.semantic as ZoneSemantic;
    zoneDocument.diagnostics = zoneDocument.diagnostics.filter((diagnostic) => diagnostic.code !== "format2.zone.action.layer-only");
    const metadataSource = zoneDocument.lines.filter((line) => line.kind === "format" || line.kind === "block-end").map((line) => line.text).join(" ");
    for (const match of metadataSource.matchAll(/(?:^|\s)([A-Za-z][A-Za-z0-9]*)=/g)) if (!["Version", "Name", "Description"].includes(match[1])) addDiagnostic(zoneDocument.diagnostics, "error", "snippet.metadata.property", `Unknown snippet metadata property: ${match[1]}`, zoneDocument.lines.find((line) => line.kind === "format")?.lineNumber, documentPath);
    if (zoneSemantic.includedZones.length || zoneSemantic.subZones.length) addDiagnostic(zoneDocument.diagnostics, "error", "snippet.relation", "A snippet cannot declare IncludedZones or ZoneLayers", zoneSemantic.dependencyReferences[0]?.line, documentPath);
    const lifecycleLine = zoneDocument.lines.find((line) => line.kind === "block-start" && line.tokens[0] === "On");
    if (lifecycleLine) addDiagnostic(zoneDocument.diagnostics, "error", "snippet.lifecycle", "A snippet cannot declare Zone lifecycle blocks", lifecycleLine.lineNumber, documentPath);
    const bindings = new Map<string, SnippetBinding>();
    for (const binding of [...zoneSemantic.modifierDeclarations, ...zoneSemantic.bindings]) appendBinding(bindings, binding, source);
    if (!bindings.size) addDiagnostic(zoneDocument.diagnostics, "error", "snippet.binding.required", "A snippet requires at least one Widget binding", undefined, documentPath);
    return {
        ...zoneDocument,
        format: "snippet",
        semantic: { bindings: [...bindings.values()], description: metadataValue(metadataSource, "Description"), id: documentPath ? path.basename(documentPath, path.extname(documentPath)) : "snippet", name: metadataValue(metadataSource, "Name") },
    };
}
