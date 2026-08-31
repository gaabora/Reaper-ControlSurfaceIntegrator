import { readFile, readdir } from "node:fs/promises";
import path from "node:path";
import { legacySurfaceProcessorTargets } from "./legacy-surface-format2.ts";
import type { Diagnostic } from "./model.ts";
import { findSurfaceIoRepresentation, type SurfaceIoSchema } from "./surface-io-schema.ts";
import { analysisText, splitSourceLines, tokenizeLine } from "./text.ts";

export interface LegacySurfaceProcessorCoverage {
    count: number;
    note?: string;
    processor: string;
    status: "invalid-target" | "planned" | "supported" | "unsupported";
    target?: string;
}

export interface LegacySurfaceCoverageReport {
    diagnostics: Diagnostic[];
    processors: LegacySurfaceProcessorCoverage[];
}

const PLANNED_PROCESSORS = new Map<string, string>();

async function collectLegacySurfacePaths(root: string): Promise<string[]> {
    const result: string[] = [];
    for (const entry of await readdir(root, { withFileTypes: true })) {
        const entryPath = path.join(root, entry.name);
        if (entry.isDirectory()) result.push(...await collectLegacySurfacePaths(entryPath));
        else if (entry.isFile() && entry.name.toLowerCase() === "surface.txt") result.push(entryPath);
    }
    return result.sort();
}

function targetLabel(direction: string, primitive: string, protocol: string, encoding: string): string {
    return `${direction} ${primitive} ${protocol}/${encoding}`;
}

function sourcePath(root: string, absolutePath: string): string {
    return path.relative(root, absolutePath).split(path.sep).join("/");
}

function countLegacyProcessors(root: string, surfacePath: string, source: string, counts: Map<string, { count: number; spelling: string }>, diagnostics: Diagnostic[]): void {
    let insideOskLayout = false;
    let widgetStartLine: number | undefined;
    for (const line of splitSourceLines(source)) {
        const text = analysisText(line);
        if (!text || text.startsWith("//") || /^\/(?!\/)/.test(text) || text.startsWith("#")) continue;
        const tokens = tokenizeLine(text);
        const keyword = tokens[0] ?? "";
        if (keyword === "OSKLayout") {
            insideOskLayout = true;
            continue;
        }
        if (keyword === "OSKLayoutEnd") {
            insideOskLayout = false;
            continue;
        }
        if (insideOskLayout) continue;
        if (keyword === "Widget") {
            if (widgetStartLine !== undefined) diagnostics.push({ code: "legacy.surface.widget.end.missing", line: line.lineNumber, message: `Widget opened at line ${widgetStartLine} has no WidgetEnd before the next Widget.`, path: sourcePath(root, surfacePath), severity: "error" });
            widgetStartLine = line.lineNumber;
            continue;
        }
        if (keyword === "WidgetEnd") {
            if (widgetStartLine === undefined) diagnostics.push({ code: "legacy.surface.widget.start.missing", line: line.lineNumber, message: "WidgetEnd has no matching Widget declaration.", path: sourcePath(root, surfacePath), severity: "error" });
            widgetStartLine = undefined;
            continue;
        }
        if (/^WidgetEnd/i.test(keyword)) {
            diagnostics.push({ code: "legacy.surface.widget.end.invalid", line: line.lineNumber, message: `Invalid Widget block terminator: ${keyword}. Expected WidgetEnd.`, path: sourcePath(root, surfacePath), severity: "error" });
            widgetStartLine = undefined;
            continue;
        }
        if (widgetStartLine === undefined) continue;
        const key = keyword.toLowerCase();
        const current = counts.get(key);
        if (current) current.count++;
        else counts.set(key, { count: 1, spelling: keyword });
    }
    if (widgetStartLine !== undefined) diagnostics.push({ code: "legacy.surface.widget.end.missing", line: widgetStartLine, message: "Widget declaration has no matching WidgetEnd.", path: sourcePath(root, surfacePath), severity: "error" });
}

export async function analyzeLegacySurfaceCoverage(root: string, schema: SurfaceIoSchema): Promise<LegacySurfaceCoverageReport> {
    const counts = new Map<string, { count: number; spelling: string }>();
    const diagnostics: Diagnostic[] = [];
    for (const surfacePath of await collectLegacySurfacePaths(root)) {
        countLegacyProcessors(root, surfacePath, await readFile(surfacePath, "utf8"), counts, diagnostics);
    }

    const coverage: LegacySurfaceProcessorCoverage[] = [];
    for (const [key, value] of counts) {
        const targets = legacySurfaceProcessorTargets(key);
        if (!targets) {
            const note = PLANNED_PROCESSORS.get(key);
            coverage.push({ count: value.count, note, processor: value.spelling, status: note ? "planned" : "unsupported" });
            continue;
        }
        const targetText = targets.map((target) => targetLabel(target.direction, target.primitive, target.protocol, target.encoding)).join(", ");
        const representations = targets.map((target) => findSurfaceIoRepresentation(schema, target.direction, target.primitive, target.protocol, target.encoding));
        coverage.push({ count: value.count, processor: value.spelling, status: representations.every(Boolean) ? "supported" : "invalid-target", target: targetText });
    }
    const statusOrder = new Map<LegacySurfaceProcessorCoverage["status"], number>([["supported", 0], ["planned", 1], ["unsupported", 2], ["invalid-target", 3]]);
    coverage.sort((left, right) => statusOrder.get(left.status)! - statusOrder.get(right.status)! || right.count - left.count || left.processor.localeCompare(right.processor));
    diagnostics.sort((left, right) => (left.path ?? "").localeCompare(right.path ?? "") || (left.line ?? 0) - (right.line ?? 0));
    return { diagnostics, processors: coverage };
}
