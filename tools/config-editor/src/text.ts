import type { FormatMarker, SourceLine } from "./model.ts";

export interface ParsedProperty {
    key: string;
    value: string;
}

export function splitSourceLines(source: string): SourceLine[] {
    const lines: SourceLine[] = [];
    let cursor = 0;
    let lineNumber = 1;
    while (cursor < source.length) {
        let lineEnd = cursor;
        while (lineEnd < source.length && source[lineEnd] !== "\r" && source[lineEnd] !== "\n") lineEnd++;
        let ending = "";
        if (source[lineEnd] === "\r" && source[lineEnd + 1] === "\n") ending = "\r\n";
        else if (source[lineEnd] === "\r") ending = "\r";
        else if (source[lineEnd] === "\n") ending = "\n";
        lines.push({ ending, kind: "unknown", lineNumber, text: source.slice(cursor, lineEnd), tokens: [] });
        cursor = lineEnd + ending.length;
        lineNumber++;
    }
    if (source.length === 0) return [];
    return lines;
}

export function analysisText(line: SourceLine): string {
    const withoutBom = line.lineNumber === 1 && line.text.startsWith("\uFEFF") ? line.text.slice(1) : line.text;
    return withoutBom.trim();
}

export function isComment(text: string): boolean {
    return text.startsWith("/") || text.startsWith("#");
}

export function tokenizeLine(text: string): string[] {
    const tokens: string[] = [];
    let token = "";
    let insideQuote = false;
    let escaped = false;
    for (let idx = 0; idx < text.length; idx++) {
        const character = text[idx];
        const nextCharacter = text[idx + 1];
        if (!insideQuote && character === "/" && nextCharacter === "/") break;
        if (escaped) {
            token += character;
            escaped = false;
        } else if (insideQuote && character === "\\") {
            escaped = true;
        } else if (character === "\"") {
            insideQuote = !insideQuote;
        } else if (/\s/.test(character) && !insideQuote) {
            if (token) tokens.push(token);
            token = "";
        } else {
            token += character;
        }
    }
    if (escaped) token += "\\";
    if (token) tokens.push(token);
    return tokens;
}

export function splitWidgetComment(text: string): { definition: string; properties: string } {
    let insideQuote = false;
    for (let idx = 0; idx < text.length; idx++) {
        if (text[idx] === "\"") insideQuote = !insideQuote;
        if (!insideQuote && text[idx] === "#") return { definition: text.slice(0, idx).trim(), properties: text.slice(idx + 1).trim() };
    }
    return { definition: text.trim(), properties: "" };
}

export function parseProperties(tokens: string[]): Map<string, string> {
    const properties = new Map<string, string>();
    for (const token of tokens) {
        const separator = token.indexOf("=");
        if (separator <= 0) continue;
        properties.set(token.slice(0, separator), token.slice(separator + 1));
    }
    return properties;
}

export function parseFormatMarker(text: string): FormatMarker | undefined {
    const match = text.match(/^\/\/\s*@format\s+(surface|zone)\s+(\S+)\s*$/i);
    if (!match) return undefined;
    return { format: match[1].toLowerCase() as FormatMarker["format"], version: match[2] };
}

export function initializeLine(line: SourceLine): string {
    const text = analysisText(line);
    if (!text) line.kind = "blank";
    else if (isComment(text)) line.kind = "comment";
    line.tokens = text && line.kind !== "comment" ? tokenizeLine(text) : [];
    return text;
}

export function propertyValue(properties: Map<string, string>, key: string): string | undefined {
    for (const [propertyKey, value] of properties) if (propertyKey.toLowerCase() === key.toLowerCase()) return value;
    return undefined;
}

export function isStableId(value: string): boolean {
    return /^[a-z0-9][a-z0-9_-]*$/.test(value);
}
