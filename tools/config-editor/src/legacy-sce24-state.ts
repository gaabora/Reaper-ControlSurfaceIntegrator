import { addDiagnostic, type Diagnostic } from "./model.ts";
import { analysisText, splitSourceLines, tokenizeLine } from "./text.ts";

export interface LegacySce24StateMigration {
    diagnostics: Diagnostic[];
    source: string;
}

const LEGACY_STATE_COLOR_PROPERTY = /\s+(?:OnColor|OffColor)=(?:"[^"]*"|\S+)/gi;
const DEFAULT_STATE_COLOR = "#000000";

function canonicalColor(value: string | undefined): string | undefined {
    const match = value?.match(/^#([0-9a-f]{6})(?:[0-9a-f]{2})?$/i);
    return match ? `#${match[1].toUpperCase()}` : undefined;
}

export function migrateLegacySce24StateColors(source: string, documentPath?: string): LegacySce24StateMigration {
    const diagnostics: Diagnostic[] = [];
    const lines = splitSourceLines(source);
    for (const line of lines) {
        const text = analysisText(line);
        if (!/\b(?:OnColor|OffColor)=/i.test(text)) continue;
        const tokens = tokenizeLine(text);
        const properties = new Map<string, string>();
        for (const token of tokens.slice(2)) {
            const separator = token.indexOf("=");
            if (separator > 0) properties.set(token.slice(0, separator).toLowerCase(), token.slice(separator + 1));
        }
        if (properties.has("statecolors")) {
            addDiagnostic(diagnostics, "error", "legacy.zone.sce24-state.existing", "StateColors cannot be combined with legacy OnColor or OffColor.", line.lineNumber, documentPath);
            continue;
        }
        const legacyOffColor = properties.get("offcolor");
        const legacyOnColor = properties.get("oncolor");
        const parsedOffColor = canonicalColor(legacyOffColor);
        const parsedOnColor = canonicalColor(legacyOnColor);
        if ((legacyOffColor && !parsedOffColor) || (legacyOnColor && !parsedOnColor)) {
            addDiagnostic(diagnostics, "error", "legacy.zone.sce24-state.color", "OnColor and OffColor require #RRGGBB or legacy #RRGGBBAA colors.", line.lineNumber, documentPath);
            continue;
        }
        const offColor = parsedOffColor ?? DEFAULT_STATE_COLOR;
        const onColor = parsedOnColor ?? DEFAULT_STATE_COLOR;
        const commentOffset = line.text.indexOf("//");
        const content = (commentOffset >= 0 ? line.text.slice(0, commentOffset) : line.text).replace(LEGACY_STATE_COLOR_PROPERTY, "").trimEnd();
        const comment = commentOffset >= 0 ? ` ${line.text.slice(commentOffset)}` : "";
        line.text = `${content} StateColors=[ ${offColor}, ${onColor} ]${comment}`;
    }
    return { diagnostics, source: lines.map((line) => line.text + line.ending).join("") };
}
