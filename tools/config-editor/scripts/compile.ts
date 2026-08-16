#!/usr/bin/env bun

import { mkdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadActionCatalog } from "../src/action-catalog.ts";
import { loadSettingsSchema } from "../src/settings-schema.ts";
import { loadProductIdentity } from "../src/product-identity.ts";
import { bundleEditorJavascript } from "../src/ui.ts";

const editorRoot = fileURLToPath(new URL("../", import.meta.url));
const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const supportedTargets = new Set(["bun-darwin-arm64", "bun-darwin-x64", "bun-linux-arm64", "bun-linux-x64", "bun-windows-x64"]);
const target = process.argv[2];

if (process.argv.length > 3) throw new Error("compile accepts zero or one Bun target");
if (target && !supportedTargets.has(target)) throw new Error(`Unsupported Bun compile target: ${target}`);

const identity = await loadProductIdentity(path.join(repositoryRoot, "Scripts", "product_identity.conf"));
const settingsSchema = await loadSettingsSchema(path.join(repositoryRoot, "Scripts", "settings_schema.conf"));
const actions = await loadActionCatalog(repositoryRoot);
const editorJavascript = await bundleEditorJavascript();
const generatedRoot = path.join(editorRoot, "generated");
const distributionRoot = path.join(editorRoot, "dist");
await mkdir(generatedRoot, { recursive: true });
await mkdir(distributionRoot, { recursive: true });

const entryPath = path.join(generatedRoot, "standalone-entry.ts");
const entrySource = [
    "import { launchEditor } from '../src/launch.ts';",
    `const identity = ${JSON.stringify(identity)};`,
    `const settingsSchema = ${JSON.stringify(settingsSchema)};`,
    `const actions = ${JSON.stringify(actions)};`,
    `const editorJavascript = ${JSON.stringify(editorJavascript)};`,
    "await launchEditor({ actions, args: process.argv.slice(2), editorJavascript, identity, settingsSchema });",
    "",
].join("\n");
await Bun.write(entryPath, entrySource);

const targetSuffix = target ? `-${target.replace(/^bun-/, "")}` : "";
const executableSuffix = target?.includes("windows") || (!target && process.platform === "win32") ? ".exe" : "";
const outputPath = path.join(distributionRoot, `config-editor-${identity.productId}${targetSuffix}${executableSuffix}`);
const command = [process.execPath, "build", entryPath, "--compile", "--outfile", outputPath];
if (target) command.push(`--target=${target}`);
const subprocess = Bun.spawn(command, { cwd: editorRoot, stderr: "inherit", stdout: "inherit" });
const exitCode = await subprocess.exited;
if (exitCode !== 0) process.exit(exitCode);
console.log(`Created standalone editor: ${outputPath}`);
