import { parseByPath, type AnyDocument } from "./formats.ts";
import { serializeDocument, type Diagnostic, type DiagnosticQuickFix } from "./model.ts";
import { convertHashCommentLine, convertSingleSlashCommentLine } from "./text.ts";
import type { ZoneSemantic } from "./zone.ts";

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
    acceptsSetDiagnostic?: boolean;
    fixes: (context: QuickFixContext) => DiagnosticQuickFix[];
    id: string;
}

function prependMarker(source: string, marker: string, document: AnyDocument): string {
    const lineEnding = document.lines.find((line) => line.ending)?.ending ?? "\n";
    const byteOrderMark = source.startsWith("\uFEFF") ? "\uFEFF" : "";
    return `${byteOrderMark}${marker}${lineEnding}${source.slice(byteOrderMark.length)}`;
}

function convertSingleSlashComment(context: QuickFixContext): string {
    const lineNumber = context.diagnostic.line;
    const line = lineNumber ? context.document.lines[lineNumber - 1] : undefined;
    if (!line) throw new QuickFixError("quick-fix.line", "The single-slash comment line is no longer available");
    const convertedText = convertSingleSlashCommentLine(line.text);
    if (convertedText === line.text) throw new QuickFixError("quick-fix.source", "The line is no longer a single-slash comment");
    line.text = convertedText;
    return serializeDocument(context.document);
}

function convertHashComment(context: QuickFixContext): string {
    const lineNumber = context.diagnostic.line;
    const line = lineNumber ? context.document.lines[lineNumber - 1] : undefined;
    if (!line) throw new QuickFixError("quick-fix.line", "The hash comment line is no longer available");
    const convertedText = convertHashCommentLine(line.text);
    if (convertedText === line.text) throw new QuickFixError("quick-fix.source", "The line is no longer a hash comment");
    line.text = convertedText;
    return serializeDocument(context.document);
}

function dependencyAtDiagnosticLine(context: QuickFixContext): string | undefined {
    if (context.document.format !== "zone" || !context.diagnostic.line) return undefined;
    const semantic = context.document.semantic as ZoneSemantic;
    return semantic.dependencyReferences.find((reference) => reference.line === context.diagnostic.line)?.name;
}

function commentOutDependency(context: QuickFixContext, fix: DiagnosticQuickFix): string {
    const dependency = dependencyAtDiagnosticLine(context);
    if (!dependency || dependency.toLowerCase() !== fix.data?.dependency?.toLowerCase()) throw new QuickFixError("quick-fix.source", "The dependency line no longer matches this cycle");
    const line = context.document.lines[context.diagnostic.line! - 1];
    if (!line) throw new QuickFixError("quick-fix.line", "The dependency line is no longer available");
    line.text = line.text.replace(/^(\s*)/, "$1// ");
    return serializeDocument(context.document);
}

const QUICK_FIX_DEFINITIONS: QuickFixDefinition[] = [
    {
        apply: (context) => convertSingleSlashComment(context),
        fixes: (context) => context.diagnostic.code === "comment.single-slash.unsupported" ? [{ id: "comment.single-slash.convert", label: "Convert to // comment" }] : [],
        id: "comment.single-slash.convert",
    },
    {
        apply: (context) => convertHashComment(context),
        fixes: (context) => context.diagnostic.code === "comment.hash.unsupported" ? [{ id: "comment.hash.convert", label: "Convert to // comment" }] : [],
        id: "comment.hash.convert",
    },
    {
        apply: (context) => prependMarker(context.document.source, "// @format zone 1", context.document),
        fixes: (context) => context.diagnostic.code === "zone.format.missing" && context.document.format === "zone" ? [{ id: "zone.format.add", label: "Add // @format zone 1" }] : [],
        id: "zone.format.add",
    },
    {
        acceptsSetDiagnostic: true,
        apply: (context, fix) => commentOutDependency(context, fix),
        fixes: (context) => {
            if (context.diagnostic.code !== "zones.dependency.cycle") return [];
            const dependency = dependencyAtDiagnosticLine(context);
            return dependency ? [{ data: { dependency }, id: "zones.dependency.cycle.comment-out", label: `Comment out dependency on ${dependency}` }] : [];
        },
        id: "zones.dependency.cycle.comment-out",
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
    return document.diagnostics.map((diagnostic) => diagnosticWithQuickFixes(document, diagnostic, knownActions, true));
}

export function diagnosticWithQuickFixes(document: AnyDocument, diagnostic: Diagnostic, knownActions: Set<string>, writable: boolean): Diagnostic {
    if (!writable) return diagnostic;
    const fixes = fixesForDiagnostic(document, diagnostic, knownActions);
    return fixes.length ? { ...diagnostic, fixes } : diagnostic;
}

export function applyQuickFix(source: string, relativePath: string, knownActions: Set<string>, request: QuickFixRequest): { document: AnyDocument; source: string } {
    const document = parseByPath(source, relativePath, knownActions);
    const requestedDefinition = QUICK_FIX_DEFINITIONS.find((candidate) => candidate.id === request.fix.id);
    if (!requestedDefinition) throw new QuickFixError("quick-fix.unknown", `Unknown quick fix: ${request.fix.id}`);
    const documentDiagnostic = document.diagnostics.find((candidate) => candidate.code === request.diagnostic.code && candidate.line === request.diagnostic.line && candidate.message === request.diagnostic.message);
    const diagnostic = documentDiagnostic ?? (requestedDefinition.acceptsSetDiagnostic ? { ...request.diagnostic, path: relativePath, severity: "error" as const } : undefined);
    if (!diagnostic) throw new QuickFixError("quick-fix.diagnostic", "The diagnostic is no longer present in the current source");
    const availableFixes = fixesForDiagnostic(document, diagnostic, knownActions);
    const selectedFix = availableFixes.find((fix) => fix.id === request.fix.id && dataMatches(fix.data, request.fix.data));
    if (!selectedFix) throw new QuickFixError("quick-fix.unavailable", "The selected quick fix is not available for this diagnostic");
    const fixedSource = requestedDefinition.apply({ diagnostic, document, knownActions }, selectedFix);
    return { document: parseByPath(fixedSource, relativePath, knownActions), source: fixedSource };
}
