import path from "node:path";
import type { LosslessDocument } from "./model.ts";
import { parseProductConfig } from "./product-config.ts";
import { parseSnippet } from "./snippet.ts";
import { parseSurface } from "./surface.ts";
import { parseZone } from "./zone.ts";

export type AnyDocument = LosslessDocument<unknown>;

export function parseByPath(source: string, filePath: string, knownActions?: Set<string>): AnyDocument {
    const extension = path.extname(filePath).toLowerCase();
    if (extension === ".zon") return parseZone(source, filePath, knownActions);
    if (extension === ".snippet") return parseSnippet(source, filePath, knownActions);
    if (extension === ".txt") return parseSurface(source, filePath);
    if (extension === ".ini") return parseProductConfig(source, filePath);
    throw new Error(`Unsupported configuration extension: ${extension || "(none)"}`);
}

export function isSupportedConfigPath(filePath: string): boolean {
    return [".ini", ".snippet", ".txt", ".zon"].includes(path.extname(filePath).toLowerCase());
}
