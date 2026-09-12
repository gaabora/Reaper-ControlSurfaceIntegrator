#!/usr/bin/env bun

import { readFile, readdir, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { actionNameSet, actionTraitsByName, loadActionCatalog, writeActionCatalog } from "./action-catalog.ts";
import { isSupportedConfigPath, parseByPath, type AnyDocument } from "./formats.ts";
import { conflictingLegacyActionRename, isIgnoredLegacyAction, renameLegacyAction } from "./legacy-action-renames.ts";
import { analyzeLegacySurfaceCoverage } from "./legacy-surface-coverage.ts";
import { legacyMainTargetContext } from "./legacy-zone-format2.ts";
import type { Diagnostic } from "./model.ts";
import { loadSettingsSchema } from "./settings-schema.ts";
import { loadSurfaceIoSchema } from "./surface-io-schema.ts";
import { validateDocumentSet } from "./validation.ts";
import { parseSurface } from "./surface.ts";
import { parseZone } from "./zone.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));

function printUsage(): void {
    console.log("Usage:");
    console.log("  bun run src/cli.ts validate [--json] <file-or-directory> [...]");
    console.log("  bun run src/cli.ts actions [--output <catalog.json>]");
    console.log("  bun run src/cli.ts legacy-actions [--all] [legacy-surfaces-directory]");
    console.log("  bun run src/cli.ts surface-coverage [legacy-surfaces-directory]");
}

async function collectConfigPaths(inputPath: string, ancestorDirectories: Set<string> = new Set()): Promise<string[]> {
    const absolutePath = path.resolve(inputPath);
    const stats = await stat(absolutePath);
    if (stats.isFile()) return isSupportedConfigPath(absolutePath) ? [absolutePath] : [];
    if (!stats.isDirectory()) return [];
    const canonicalDirectory = await realpath(absolutePath);
    if (ancestorDirectories.has(canonicalDirectory)) return [];
    const currentAncestors = new Set(ancestorDirectories);
    currentAncestors.add(canonicalDirectory);
    const result: string[] = [];
    const entries = await readdir(absolutePath, { withFileTypes: true });
    for (const entry of entries.sort((left, right) => left.name.localeCompare(right.name))) {
        const entryPath = path.join(absolutePath, entry.name);
        result.push(...await collectConfigPaths(entryPath, currentAncestors));
    }
    return result;
}

function printDiagnostic(diagnostic: Diagnostic): void {
    const location = diagnostic.path ? `${diagnostic.path}${diagnostic.line ? `:${diagnostic.line}` : ""}` : diagnostic.line ? `line ${diagnostic.line}` : "configuration";
    console.log(`${location}: ${diagnostic.severity.toUpperCase()} ${diagnostic.code}: ${diagnostic.message}`);
}

async function validateCommand(args: string[]): Promise<number> {
    const jsonOutput = args.includes("--json");
    const inputs = args.filter((arg) => arg !== "--json" && arg !== "--");
    if (!inputs.length) throw new Error("validate requires at least one file or directory");
    const catalog = await loadActionCatalog(repositoryRoot);
    const knownActions = actionNameSet(catalog);
    const actionTraits = actionTraitsByName(catalog);
    const settingsSchema = await loadSettingsSchema(path.join(repositoryRoot, "Scripts", "settings_schema.conf"));
    const configPaths = [...new Set((await Promise.all(inputs.map((input) => collectConfigPaths(input)))).flat())].sort();
    if (!configPaths.length) throw new Error("No supported .conf, .txt, .zon, or .snippet files were found");

    const documents: AnyDocument[] = [];
    for (const configPath of configPaths) documents.push(parseByPath(await readFile(configPath, "utf8"), configPath, knownActions, settingsSchema, actionTraits));
    const diagnostics = documents.flatMap((document) => document.diagnostics).concat(validateDocumentSet(documents, { actionTraits, settingsSchema }));
    if (jsonOutput) console.log(JSON.stringify({ diagnostics, files: configPaths.length }, null, 2));
    else {
        for (const diagnostic of diagnostics) printDiagnostic(diagnostic);
        const errorCount = diagnostics.filter((diagnostic) => diagnostic.severity === "error").length;
        const warningCount = diagnostics.length - errorCount;
        console.log(`Validated ${configPaths.length} files: ${errorCount} errors, ${warningCount} warnings`);
    }
    return diagnostics.some((diagnostic) => diagnostic.severity === "error") ? 1 : 0;
}

async function actionsCommand(args: string[]): Promise<number> {
    const catalog = await loadActionCatalog(repositoryRoot);
    const outputIndex = args.indexOf("--output");
    if (outputIndex >= 0) {
        const outputPath = args[outputIndex + 1];
        if (!outputPath) throw new Error("--output requires a JSON file path");
        await writeActionCatalog(path.resolve(outputPath), catalog);
        console.log(`Wrote ${catalog.length} runtime actions to ${path.resolve(outputPath)}`);
    } else {
        console.log(JSON.stringify({ actions: catalog, version: 1 }, null, 2));
    }
    return 0;
}

interface LegacyActionUsage {
    action: string;
    count: number;
    examples: string[];
    replacement?: string;
    status: "conflict" | "current" | "ignored" | "renamed" | "unknown";
}

async function legacyActionsCommand(args: string[]): Promise<number> {
    const showAll = args.includes("--all");
    const inputs = args.filter((arg) => arg !== "--all" && arg !== "--");
    if (inputs.length > 1) throw new Error("legacy-actions accepts zero or one legacy Surfaces directory");
    const legacySurfacesRoot = path.resolve(inputs[0] ?? path.join(repositoryRoot, "CSI", "Surfaces"));
    const knownActions = actionNameSet(await loadActionCatalog(repositoryRoot));
    const zonePaths = (await collectConfigPaths(legacySurfacesRoot)).filter((configPath) => configPath.toLowerCase().endsWith(".zon"));
    const widgetsBySurface = new Map<string, Set<string>>();
    const usages = new Map<string, LegacyActionUsage>();
    for (const zonePath of zonePaths) {
        const relativePath = path.relative(legacySurfacesRoot, zonePath);
        if (relativePath.split(path.sep).some((segment) => segment.toLowerCase() === "learnzones") || path.basename(zonePath).toLowerCase() === "gozones.zon") continue;
        const surfaceDirectory = relativePath.split(path.sep)[0];
        let surfaceWidgets = widgetsBySurface.get(surfaceDirectory);
        if (!surfaceWidgets) {
            const surfacePath = path.join(legacySurfacesRoot, surfaceDirectory, "Surface.txt");
            surfaceWidgets = new Set(parseSurface(await readFile(surfacePath, "utf8"), surfacePath).semantic.widgets.map((widget) => widget.name));
            widgetsBySurface.set(surfaceDirectory, surfaceWidgets);
        }
        const source = (await readFile(zonePath, "utf8")).replace(/^(\s*)\/(?!\/)/gm, "$1//").replace(/^(\s*)#/gm, "$1//");
        const document = parseZone(source, zonePath, knownActions);
        const targetContext = legacyMainTargetContext(document.semantic.name ?? path.basename(zonePath, path.extname(zonePath)));
        for (const binding of document.semantic.bindings) {
            const renameContext = { ...targetContext, isLayer: true };
            const renamed = renameLegacyAction(binding.action, binding.params, renameContext);
            const conflict = conflictingLegacyActionRename(binding.action, binding.params, renameContext);
            const replacement = renamed.action === binding.action ? undefined : renamed.action;
            const status = isIgnoredLegacyAction(binding.action) ? "ignored" : replacement ? "renamed" : conflict ? "conflict" : knownActions.has(binding.action) ? "current" : "unknown";
            if (status === "unknown" && (!/^[A-Za-z][A-Za-z0-9_]*$/.test(binding.action) || binding.action.includes("="))) continue;
            const possibleWidget = binding.action.split("+").at(-1)?.replace(/[|#]$/, "") ?? "";
            if (status === "unknown" && [...surfaceWidgets].some((widget) => widget === possibleWidget || widget.startsWith(possibleWidget) && /^\d+$/.test(widget.slice(possibleWidget.length)))) continue;
            const key = `${status}\0${binding.action}\0${replacement ?? ""}`;
            const usage = usages.get(key) ?? { action: binding.action, count: 0, examples: [], replacement, status };
            usage.count++;
            if (usage.examples.length < 3) usage.examples.push(`${relativePath.split(path.sep).join("/")}:${binding.line}`);
            usages.set(key, usage);
        }
    }
    const entries = [...usages.values()].sort((left, right) => left.status.localeCompare(right.status) || right.count - left.count || left.action.localeCompare(right.action));
    for (const entry of entries) {
        if (!showAll && entry.status === "current") continue;
        const replacement = entry.replacement ? ` -> ${entry.replacement}` : "";
        console.log(`${entry.status.padEnd(9)} ${String(entry.count).padStart(5)}  ${entry.action}${replacement}  ${entry.examples.join(", ")}`);
    }
    const currentCount = entries.filter((entry) => entry.status === "current").reduce((sum, entry) => sum + entry.count, 0);
    const ignoredCount = entries.filter((entry) => entry.status === "ignored").reduce((sum, entry) => sum + entry.count, 0);
    const renamedCount = entries.filter((entry) => entry.status === "renamed").reduce((sum, entry) => sum + entry.count, 0);
    const conflictCount = entries.filter((entry) => entry.status === "conflict").reduce((sum, entry) => sum + entry.count, 0);
    const unknownCount = entries.filter((entry) => entry.status === "unknown").reduce((sum, entry) => sum + entry.count, 0);
    console.log(`Legacy Zone actions: ${currentCount} current occurrences, ${renamedCount} renamed occurrences, ${conflictCount} context conflicts, ${ignoredCount} ignored compatibility occurrences, ${unknownCount} unknown occurrences in ${zonePaths.length} files`);
    return conflictCount || unknownCount ? 1 : 0;
}

async function surfaceCoverageCommand(args: string[]): Promise<number> {
    if (args.length > 1) throw new Error("surface-coverage accepts zero or one legacy Surfaces directory");
    const legacySurfacesRoot = path.resolve(args[0] ?? path.join(repositoryRoot, "CSI", "Surfaces"));
    const schema = await loadSurfaceIoSchema(path.join(repositoryRoot, "Scripts", "surface_io_schema.conf"));
    const report = await analyzeLegacySurfaceCoverage(legacySurfacesRoot, schema);
    for (const entry of report.processors) console.log(`${entry.status.padEnd(14)} ${String(entry.count).padStart(5)}  ${entry.processor}${entry.target ? ` -> ${entry.target}` : ""}${entry.note ? ` - ${entry.note}` : ""}`);
    for (const diagnostic of report.diagnostics) printDiagnostic(diagnostic);
    const supportedCount = report.processors.filter((entry) => entry.status === "supported").reduce((sum, entry) => sum + entry.count, 0);
    const plannedCount = report.processors.filter((entry) => entry.status === "planned").reduce((sum, entry) => sum + entry.count, 0);
    const unsupportedCount = report.processors.filter((entry) => entry.status === "unsupported" || entry.status === "invalid-target").reduce((sum, entry) => sum + entry.count, 0);
    console.log(`Legacy Surface processors: ${supportedCount} supported occurrences, ${plannedCount} planned occurrences, ${unsupportedCount} unsupported occurrences, ${report.diagnostics.length} source errors`);
    return plannedCount > 0 || unsupportedCount > 0 || report.diagnostics.length > 0 ? 1 : 0;
}

async function main(): Promise<number> {
    const command = process.argv[2];
    const args = process.argv.slice(3);
    if (command === "validate") return validateCommand(args);
    if (command === "actions") return actionsCommand(args);
    if (command === "legacy-actions") return legacyActionsCommand(args);
    if (command === "surface-coverage") return surfaceCoverageCommand(args);
    printUsage();
    return command ? 1 : 0;
}

try {
    process.exitCode = await main();
} catch (error) {
    console.error(`error: ${error instanceof Error ? error.message : String(error)}`);
    process.exitCode = 1;
}
