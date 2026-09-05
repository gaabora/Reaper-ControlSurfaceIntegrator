#!/usr/bin/env bun

import { readFile, readdir, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { actionNameSet, actionTraitsByName, loadActionCatalog, writeActionCatalog } from "./action-catalog.ts";
import { isSupportedConfigPath, parseByPath, type AnyDocument } from "./formats.ts";
import { analyzeLegacySurfaceCoverage } from "./legacy-surface-coverage.ts";
import type { Diagnostic } from "./model.ts";
import { loadSettingsSchema } from "./settings-schema.ts";
import { loadSurfaceIoSchema } from "./surface-io-schema.ts";
import { validateDocumentSet } from "./validation.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));

function printUsage(): void {
    console.log("Usage:");
    console.log("  bun run src/cli.ts validate [--json] <file-or-directory> [...]");
    console.log("  bun run src/cli.ts actions [--output <catalog.json>]");
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
