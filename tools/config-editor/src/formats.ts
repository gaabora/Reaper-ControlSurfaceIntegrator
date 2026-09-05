import path from "node:path";
import type { ActionTraits } from "./action-catalog.ts";
import { addDiagnostic, type LosslessDocument } from "./model.ts";
import { parseProductConfig } from "./product-config.ts";
import type { SettingsSchema } from "./settings-schema.ts";
import { parseSnippet } from "./snippet.ts";
import { parseSurface } from "./surface.ts";
import { analysisText } from "./text.ts";
import { isLearnTemplateDirective, parseZone } from "./zone.ts";

export type AnyDocument = LosslessDocument<unknown>;

function addCommonSyntaxDiagnostics(document: AnyDocument): AnyDocument {
    for (const line of document.lines) {
        const text = analysisText(line);
        if (/^\/(?!\/)/.test(text)) addDiagnostic(document.diagnostics, "error", "comment.single-slash.unsupported", "Single-slash comments are not supported. Use //.", line.lineNumber, document.path);
        if (/^#/.test(text) && !(document.format === "zone" && isLearnTemplateDirective(line.tokens[0]))) addDiagnostic(document.diagnostics, "error", "comment.hash.unsupported", "Hash comments are not supported. Use //.", line.lineNumber, document.path);
    }
    return document;
}

export function parseByPath(source: string, filePath: string, knownActions?: Set<string>, settingsSchema?: SettingsSchema, actionTraits?: ReadonlyMap<string, ActionTraits>): AnyDocument {
    const extension = path.extname(filePath).toLowerCase();
    if (extension === ".zon") return addCommonSyntaxDiagnostics(parseZone(source, filePath, knownActions, settingsSchema, actionTraits));
    if (extension === ".snippet") return addCommonSyntaxDiagnostics(parseSnippet(source, filePath, knownActions));
    if (extension === ".txt") return addCommonSyntaxDiagnostics(parseSurface(source, filePath));
    if (extension === ".conf") return addCommonSyntaxDiagnostics(parseProductConfig(source, filePath, settingsSchema));
    throw new Error(`Unsupported configuration extension: ${extension || "(none)"}`);
}

export function isSupportedConfigPath(filePath: string): boolean {
    return [".conf", ".snippet", ".txt", ".zon"].includes(path.extname(filePath).toLowerCase());
}
