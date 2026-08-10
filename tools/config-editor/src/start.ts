#!/usr/bin/env bun

import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadActionCatalog } from "./action-catalog.ts";
import { t } from "./i18n.ts";
import { launchEditor } from "./launch.ts";
import { loadProductIdentity } from "./product-identity.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));

try {
    const identity = await loadProductIdentity(path.join(repositoryRoot, "Scripts", "product_identity.conf"));
    const actions = await loadActionCatalog(repositoryRoot);
    await launchEditor({ actions, args: process.argv.slice(2), identity });
} catch (error) {
    console.error(t("error.start", { message: error instanceof Error ? error.message : String(error) }));
    process.exitCode = 1;
}
