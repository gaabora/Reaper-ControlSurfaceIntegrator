import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, realpath, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { t } from "../src/i18n.ts";
import { discoverReaperDataPaths, ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.ini",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};

let temporaryRoot = "";
let reaperDataPath = "";

beforeEach(async () => {
    temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-paths-"));
    reaperDataPath = path.join(temporaryRoot, "Data");
    const configurationPath = path.join(reaperDataPath, identity.resourceDirectory);
    await mkdir(configurationPath, { recursive: true });
    await writeFile(path.join(configurationPath, identity.configFilename), "Version=7.0\n", "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-paths-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("REAPER data path", () => {
    test("keeps an explicit data path as the first prefill candidate", async () => {
        const candidates = await discoverReaperDataPaths(reaperDataPath);
        expect(candidates[0]).toEqual({ exists: true, path: path.resolve(reaperDataPath), source: "candidate.commandLine" });
        expect(candidates.every((candidate) => !candidate.path.endsWith(identity.resourceDirectory))).toBeTrue();
    });

    test("adds the product directory internally", async () => {
        const guard = await ProductRootGuard.createFromReaperDataPath(reaperDataPath, identity);
        expect(guard.getReaperDataPath()).toBe(await realpath(reaperDataPath));
        expect(guard.getRoot()).toBe(await realpath(path.join(reaperDataPath, identity.resourceDirectory)));
    });
});

describe("English UI text", () => {
    test("interpolates typed translation parameters", () => {
        expect(t("app.title", { product: identity.displayName })).toBe("Test Product Conf Editor");
        expect(t("pending.count", { count: 2 })).toBe("Files ready to save: 2");
    });
});
