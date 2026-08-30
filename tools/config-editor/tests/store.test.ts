import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { lstat, mkdir, mkdtemp, readFile, readdir, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { ConfigurationStore, EditorOperationError } from "../src/store.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.conf",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};

const alphaSource = "// @format zone 1\nZone alpha\n  Play Play\nZoneEnd\n";
const betaSource = "// @format zone 1\nZone beta\n  Play Stop\nZoneEnd\n";
let temporaryRoot = "";
let productRoot = "";

async function createStore(beforeCommit?: (relativePath: string, commitIndex: number) => void): Promise<ConfigurationStore> {
    const guard = await ProductRootGuard.create(productRoot, identity);
    return new ConfigurationStore(guard, new Set(["GoZone", "Play", "Stop"]), { beforeCommit });
}

beforeEach(async () => {
    temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-store-"));
    productRoot = path.join(temporaryRoot, identity.resourceDirectory);
    await mkdir(path.join(productRoot, "Surfaces", "Vendor"), { recursive: true });
    await mkdir(path.join(productRoot, "Surfaces", "User"), { recursive: true });
    await mkdir(path.join(productRoot, "Zones", "Vendor", "faderportv2", "Main"), { recursive: true });
    await mkdir(path.join(productRoot, "Zones", "User"), { recursive: true });
    await mkdir(path.join(productRoot, "Snippets", "BuiltIn"), { recursive: true });
    await mkdir(path.join(productRoot, "Snippets", "User"), { recursive: true });
    await writeFile(path.join(productRoot, identity.configFilename), "Device test {\n  Type=MIDI\n  Channels=1\n  Input=0\n  Output=0\n}\n\nPage Home {\n  Surface test {\n    Device=test\n    Template=test\n  }\n}\n", "utf8");
    await writeFile(path.join(productRoot, "Zones", "Vendor", "faderportv2", "Main", "Home.zon"), "// @format zone 1\nZone Home\n  Play Play\nZoneEnd\n", "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-store-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("configuration store", () => {
    test("checks every file, uses draft sources, and keeps cross-file diagnostics separate", async () => {
        const relativePath = "Zones/User/profile/Main/alpha.zon";
        const absolutePath = path.join(productRoot, ...relativePath.split("/"));
        await mkdir(path.dirname(absolutePath), { recursive: true });
        await writeFile(absolutePath, "Zone alpha\n  Play UnknownAction\n  Next GoZone missing\nZoneEnd\n", "utf8");
        const store = await createStore();
        const opened = await store.openDocument(relativePath);
        const draftSource = "Zone alpha\n  Play Stop\n  Next GoZone missing\nZoneEnd\n";
        const result = await store.validateAll([{ originalHash: opened.hash, path: relativePath, source: draftSource }]);
        const fileResult = result.files.find((file) => file.path === relativePath);

        expect(fileResult?.diagnostics.some((diagnostic) => diagnostic.code === "zone.action.unknown")).toBeFalse();
        expect(fileResult?.diagnostics.find((diagnostic) => diagnostic.code === "zone.format.missing")?.fixes?.[0].id).toBe("zone.format.add");
        expect(result.diagnostics.some((diagnostic) => diagnostic.code === "zones.dependency.missing" && diagnostic.path === relativePath)).toBeTrue();
    });

    test("single save uses the opened hash and rejects an external change", async () => {
        const relativePath = "Zones/User/alpha/Main/alpha.zon";
        const absolutePath = path.join(productRoot, ...relativePath.split("/"));
        await mkdir(path.dirname(absolutePath), { recursive: true });
        await writeFile(absolutePath, alphaSource, "utf8");
        const store = await createStore();
        const opened = await store.openDocument(relativePath);
        const changedSource = alphaSource.replace("Play Play", "Play Stop");
        const saved = await store.saveOne({ originalHash: opened.hash, path: relativePath, source: changedSource });
        expect(saved.report.changed).toEqual([relativePath]);
        expect(await readFile(absolutePath, "utf8")).toBe(changedSource);
        await expect(lstat(path.join(productRoot, "Backups"))).rejects.toThrow();

        await writeFile(absolutePath, alphaSource, "utf8");
        try {
            await store.saveOne({ originalHash: saved.hash, path: relativePath, source: changedSource });
            throw new Error("Expected a hash conflict");
        } catch (error) {
            expect(error).toBeInstanceOf(EditorOperationError);
            expect((error as EditorOperationError).code).toBe("conflict.hash");
        }
    });

    test("multi-file failure restores every committed file", async () => {
        const alphaPath = "Zones/User/profile/Main/alpha.zon";
        const betaPath = "Zones/User/profile/Main/beta.zon";
        const alphaAbsolutePath = path.join(productRoot, ...alphaPath.split("/"));
        const betaAbsolutePath = path.join(productRoot, ...betaPath.split("/"));
        await mkdir(path.dirname(alphaAbsolutePath), { recursive: true });
        await writeFile(alphaAbsolutePath, alphaSource, "utf8");
        await writeFile(betaAbsolutePath, betaSource, "utf8");
        const store = await createStore((_relativePath, commitIndex) => { if (commitIndex === 1) throw new Error("forced commit failure"); });
        const alpha = await store.openDocument(alphaPath);
        const beta = await store.openDocument(betaPath);
        try {
            await store.saveTransaction([
                { originalHash: alpha.hash, path: alphaPath, source: alphaSource.replace("Play Play", "Play Stop") },
                { originalHash: beta.hash, path: betaPath, source: betaSource.replace("Play Stop", "Play Play") },
            ]);
            throw new Error("Expected transaction failure");
        } catch (error) {
            expect(error).toBeInstanceOf(EditorOperationError);
            expect((error as EditorOperationError).code).toBe("transaction.failed");
            expect(((error as EditorOperationError).details as { restored: string[] }).restored).toEqual([alphaPath]);
        }
        expect(await readFile(alphaAbsolutePath, "utf8")).toBe(alphaSource);
        expect(await readFile(betaAbsolutePath, "utf8")).toBe(betaSource);
        const operationDirectories = await readdir(path.join(productRoot, "Backups"));
        const manifest = JSON.parse(await readFile(path.join(productRoot, "Backups", operationDirectories[0], "manifest.json"), "utf8"));
        expect(manifest.status).toBe("rolled-back");
    });

    test("vendor Main clone preserves User FX and does not copy other vendor content", async () => {
        await writeFile(path.join(productRoot, "Zones", "Vendor", "faderportv2", "notes.txt"), "profile notes\n", "utf8");
        await mkdir(path.join(productRoot, "Zones", "Vendor", "faderportv2", "FX"), { recursive: true });
        await writeFile(path.join(productRoot, "Zones", "Vendor", "faderportv2", "FX", "VendorFx.zon"), alphaSource, "utf8");
        await mkdir(path.join(productRoot, "Zones", "User", "faderportv2", "FX"), { recursive: true });
        await writeFile(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "UserFx.zon"), betaSource, "utf8");
        const store = await createStore();
        const report = await store.cloneForEditing("Zones/Vendor/faderportv2/Main/Home.zon");
        expect(report.created).toContain("Zones/User/faderportv2/Main/Home.zon");
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "UserFx.zon"), "utf8")).toBe(betaSource);
        await expect(lstat(path.join(productRoot, "Zones", "User", "faderportv2", "notes.txt"))).rejects.toThrow();
        await expect(lstat(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "VendorFx.zon"))).rejects.toThrow();
    });

    test("vendor FX clone copies only the selected file", async () => {
        const vendorFxRoot = path.join(productRoot, "Zones", "Vendor", "faderportv2", "FX", "Nested");
        await mkdir(vendorFxRoot, { recursive: true });
        await writeFile(path.join(vendorFxRoot, "Selected.zon"), alphaSource, "utf8");
        await writeFile(path.join(vendorFxRoot, "Other.zon"), betaSource, "utf8");
        const store = await createStore();
        const report = await store.cloneForEditing("Zones/Vendor/faderportv2/FX/Nested/Selected.zon");
        expect(report.created).toContain("Zones/User/faderportv2/FX/Nested/Selected.zon");
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "Nested", "Selected.zon"), "utf8")).toBe(alphaSource);
        await expect(lstat(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "Nested", "Other.zon"))).rejects.toThrow();
    });

    test("symbolic link files are followed", async () => {
        if (process.platform === "win32") return;
        const outsidePath = path.join(temporaryRoot, "outside.txt");
        const linkPath = path.join(productRoot, "Surfaces", "User", "linked.txt");
        const originalSource = "// @format surface 1\nWidget Escape\nWidgetEnd\n";
        const changedSource = originalSource.replace("Escape", "Linked");
        await writeFile(outsidePath, originalSource, "utf8");
        await symlink(outsidePath, linkPath);
        const store = await createStore();
        const opened = await store.openDocument("Surfaces/User/linked.txt");
        await store.saveOne({ originalHash: opened.hash, path: opened.path, source: changedSource });
        expect(await readFile(outsidePath, "utf8")).toBe(changedSource);
        expect((await lstat(linkPath)).isSymbolicLink()).toBeTrue();
    });

    test("case-only path conflicts are rejected", async () => {
        await mkdir(path.join(productRoot, "Zones", "User", "Profile"));
        const store = await createStore();
        await expect(store.saveOne({ originalHash: null, path: "Zones/User/profile/Main/alpha.zon", source: alphaSource })).rejects.toThrow("differs only by letter case");
    });
});
