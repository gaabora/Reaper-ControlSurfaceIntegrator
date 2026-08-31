import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, realpath, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { t } from "../src/i18n.ts";
import { EditorSettingsStore } from "../src/editor-settings.ts";
import { discoverReaperDataPaths, ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.conf",
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
    await writeFile(path.join(configurationPath, identity.configFilename), "Device test {\n  Type=MIDI\n  Input=0\n  Output=0\n}\n\nPage Home {\n  Surface test {\n    Device=test\n    Template=test\n  }\n}\n", "utf8");
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

    test("uses the last selected data path before automatic defaults", async () => {
        const settings = new EditorSettingsStore(identity.productId, path.join(temporaryRoot, "settings", "conf-editor.json"));
        await settings.writeLastDataPath(reaperDataPath);
        const candidates = await discoverReaperDataPaths(undefined, await settings.readLastDataPath());
        expect(candidates[0]).toEqual({ exists: true, path: path.resolve(reaperDataPath), source: "candidate.remembered" });
    });

    test("adds the product directory internally", async () => {
        const guard = await ProductRootGuard.createFromReaperDataPath(reaperDataPath, identity);
        expect(guard.getReaperDataPath()).toBe(path.resolve(reaperDataPath));
        expect(guard.getRoot()).toBe(path.resolve(reaperDataPath, identity.resourceDirectory));
    });

    test("follows linked configuration directories and stops link cycles", async () => {
        if (process.platform === "win32") return;
        const configurationPath = path.join(reaperDataPath, identity.resourceDirectory);
        const linkedSurfaces = path.join(temporaryRoot, "linked-surfaces");
        const linkedUser = path.join(linkedSurfaces, "User");
        await mkdir(linkedUser, { recursive: true });
        await writeFile(path.join(linkedUser, "linked.txt"), "// @format surface 1\nWidget Linked\nWidgetEnd\n", "utf8");
        await symlink(linkedUser, path.join(linkedUser, "loop"));
        await symlink(linkedSurfaces, path.join(configurationPath, "Surfaces"));

        const guard = await ProductRootGuard.createFromReaperDataPath(reaperDataPath, identity);
        expect(await guard.resolveExisting("Surfaces/User/linked.txt")).toBe(await realpath(path.join(linkedUser, "linked.txt")));
        const surfaces = (await guard.listTree()).find((entry) => entry.path === "Surfaces");
        const user = surfaces?.children?.find((entry) => entry.path === "Surfaces/User");
        expect(surfaces?.kind).toBe("directory");
        expect(user?.children?.find((entry) => entry.path === "Surfaces/User/linked.txt")?.kind).toBe("file");
        expect(user?.children?.find((entry) => entry.path === "Surfaces/User/loop")?.reason).toBe("Directory link cycle");
    });
});

describe("English UI text", () => {
    test("interpolates typed translation parameters", () => {
        expect(t("app.title", { product: identity.displayName })).toBe("Test Product Conf Editor");
        expect(t("pending.count", { count: 2 })).toBe("Files ready to save: 2");
    });
});
