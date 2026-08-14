import { parseByPath, type AnyDocument } from "./formats.ts";
import type { Diagnostic, DiagnosticQuickFix } from "./model.ts";

export class QuickFixError extends Error {
    constructor(public readonly code: string, message: string) {
        super(message);
        this.name = "QuickFixError";
    }
}

export interface QuickFixRequest {
    diagnostic: Pick<Diagnostic, "code" | "line" | "message">;
    fix: Pick<DiagnosticQuickFix, "data" | "id">;
}

interface QuickFixContext {
    diagnostic: Diagnostic;
    document: AnyDocument;
    knownActions: Set<string>;
}

interface QuickFixDefinition {
    apply: (context: QuickFixContext, fix: DiagnosticQuickFix) => string;
    fixes: (context: QuickFixContext) => DiagnosticQuickFix[];
    id: string;
}

function prependMarker(source: string, marker: string, document: AnyDocument): string {
    const lineEnding = document.lines.find((line) => line.ending)?.ending ?? "\n";
    const byteOrderMark = source.startsWith("\uFEFF") ? "\uFEFF" : "";
    return `${byteOrderMark}${marker}${lineEnding}${source.slice(byteOrderMark.length)}`;
}

const QUICK_FIX_DEFINITIONS: QuickFixDefinition[] = [
    {
        apply: (context) => prependMarker(context.document.source, "// @format zone 1", context.document),
        fixes: (context) => context.diagnostic.code === "zone.format.missing" && context.document.format === "zone" ? [{ id: "zone.format.add", label: "Add // @format zone 1" }] : [],
        id: "zone.format.add",
    },
];

function fixesForDiagnostic(document: AnyDocument, diagnostic: Diagnostic, knownActions: Set<string>): DiagnosticQuickFix[] {
    const context = { diagnostic, document, knownActions };
    return QUICK_FIX_DEFINITIONS.flatMap((definition) => definition.fixes(context));
}

function dataMatches(left: Record<string, string> | undefined, right: Record<string, string> | undefined): boolean {
    return JSON.stringify(left ?? {}) === JSON.stringify(right ?? {});
}

export function diagnosticsWithQuickFixes(document: AnyDocument, knownActions: Set<string>, writable: boolean): Diagnostic[] {
    if (!writable) return document.diagnostics;
    return document.diagnostics.map((diagnostic) => {
        const fixes = fixesForDiagnostic(document, diagnostic, knownActions);
        return fixes.length ? { ...diagnostic, fixes } : diagnostic;
    });
}

export function applyQuickFix(source: string, relativePath: string, knownActions: Set<string>, request: QuickFixRequest): { document: AnyDocument; source: string } {
    const document = parseByPath(source, relativePath, knownActions);
    const diagnostic = document.diagnostics.find((candidate) => candidate.code === request.diagnostic.code && candidate.line === request.diagnostic.line && candidate.message === request.diagnostic.message);
    if (!diagnostic) throw new QuickFixError("quick-fix.diagnostic", "The diagnostic is no longer present in the current source");
    const availableFixes = fixesForDiagnostic(document, diagnostic, knownActions);
    const selectedFix = availableFixes.find((fix) => fix.id === request.fix.id && dataMatches(fix.data, request.fix.data));
    if (!selectedFix) throw new QuickFixError("quick-fix.unavailable", "The selected quick fix is not available for this diagnostic");
    const definition = QUICK_FIX_DEFINITIONS.find((candidate) => candidate.id === selectedFix.id);
    if (!definition) throw new QuickFixError("quick-fix.unknown", `Unknown quick fix: ${selectedFix.id}`);
    const fixedSource = definition.apply({ diagnostic, document, knownActions }, selectedFix);
    return { document: parseByPath(fixedSource, relativePath, knownActions), source: fixedSource };
}
