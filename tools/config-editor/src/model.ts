export type FormatId = "product-config" | "surface" | "zone" | "snippet";
export type DiagnosticSeverity = "error" | "warning";
export type SourceLineKind = "blank" | "comment" | "format" | "header" | "block-start" | "block-end" | "entry" | "unknown";

export interface DiagnosticQuickFix {
    data?: Record<string, string>;
    id: string;
    label: string;
}

export interface Diagnostic {
    code: string;
    fixes?: DiagnosticQuickFix[];
    line?: number;
    message: string;
    path?: string;
    severity: DiagnosticSeverity;
}

export interface SourceLine {
    ending: string;
    kind: SourceLineKind;
    lineNumber: number;
    text: string;
    tokens: string[];
}

export interface LosslessDocument<Semantic> {
    diagnostics: Diagnostic[];
    format: FormatId;
    lines: SourceLine[];
    path?: string;
    semantic: Semantic;
    source: string;
    version: string;
}

export interface FormatMarker {
    format: "surface" | "zone";
    version: string;
}

export function serializeDocument(document: LosslessDocument<unknown>): string {
    return document.lines.map((line) => line.text + line.ending).join("");
}

export function addDiagnostic(diagnostics: Diagnostic[], severity: DiagnosticSeverity, code: string, message: string, line?: number, path?: string): void {
    diagnostics.push({ code, line, message, path, severity });
}
