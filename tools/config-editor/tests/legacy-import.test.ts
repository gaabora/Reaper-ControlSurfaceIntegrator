import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { LegacyCsiSource } from "../src/legacy-import.ts";
import { ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { ConfigurationStore, EditorOperationError } from "../src/store.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.ini",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};
const knownActions = new Set(["GoZone", "Play"]);
const surfaceSource = "Widget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n";
const homeSource = "Zone Home\n  Play Play\n  Shift+Play GoZone Transport\nZoneEnd\n";
const transportSource = "Zone Transport\n  Play Play\nZoneEnd\n";
let temporaryRoot = "";
let legacyRoot = "";
let productRoot = "";

async function createStore(): Promise<ConfigurationStore> {
    const guard = await ProductRootGuard.create(productRoot, identity);
    return new ConfigurationStore(guard, knownActions);
}

beforeEach(async () => {
    temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-legacy-"));
    legacyRoot = path.join(temporaryRoot, "CSI");
    const surfaceRoot = path.join(legacyRoot, "Surfaces", "FaderPortV2");
    await mkdir(path.join(surfaceRoot, "Zones", "HomeZones"), { recursive: true });
    await mkdir(path.join(surfaceRoot, "Zones", "GoZones"), { recursive: true });
    await writeFile(path.join(surfaceRoot, "Surface.txt"), surfaceSource, "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "HomeZones", "Home.zon"), homeSource, "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "HomeZones", "Home.zon~20260101"), "backup\n", "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "GoZones", "Transport.zon"), transportSource, "utf8");

    productRoot = path.join(temporaryRoot, "Data", identity.resourceDirectory);
    await mkdir(path.join(productRoot, "Surfaces", "User"), { recursive: true });
    await mkdir(path.join(productRoot, "Zones", "User"), { recursive: true });
    await writeFile(path.join(productRoot, identity.configFilename), "Version=7.0\n", "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-legacy-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("legacy CSI import", () => {
    test("discovers a surface from a parent path and prepares a complete preview", async () => {
        const source = await LegacyCsiSource.create(temporaryRoot);
        expect(await source.listSurfaces()).toEqual([{ name: "FaderPortV2", stableId: "faderportv2", zoneCount: 2 }]);

        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        expect(preview.valid).toBeTrue();
        expect(preview.selectedZonePaths).toEqual(["GoZones/Transport.zon", "HomeZones/Home.zon"]);
        expect(preview.items.map((item) => item.targetPath)).toEqual([
            "Surfaces/User/faderportv2.txt",
            "Zones/User/faderportv2/GoZones/Transport.zon",
            "Zones/User/faderportv2/HomeZones/Home.zon",
        ]);
        expect(preview.items.every((item) => item.source.startsWith(`// @format ${item.kind} 1\n`))).toBeTrue();
        expect(preview.dependencies).toContainEqual({ from: "HomeZones/Home.zon", matches: ["GoZones/Transport.zon"], name: "Transport", selected: true, type: "GoZone" });
    });

    test("writes selected files in one transaction and requires conflict decisions on repeat", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const preview = await source.preview(store, knownActions, "FaderPortV2", true);
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));
        const report = await source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2" });

        expect(report.created).toHaveLength(3);
        expect(await readFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "utf8")).toStartWith("// @format surface 1\n");
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "HomeZones", "Home.zon"), "utf8")).toStartWith("// @format zone 1\n");
        expect(await readFile(path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt"), "utf8")).toBe(surfaceSource);

        try {
            await source.import(store, knownActions, { includeSurface: true, resolutions: [], selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2" });
            throw new Error("Expected a required conflict resolution");
        } catch (error) {
            expect(error).toBeInstanceOf(EditorOperationError);
            expect((error as EditorOperationError).code).toBe("legacy.resolution.required");
        }
    });
});
