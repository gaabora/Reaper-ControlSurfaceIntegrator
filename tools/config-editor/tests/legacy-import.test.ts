import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { LegacyCsiSource, migrateLegacyCommentSyntax } from "../src/legacy-import.ts";
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
const knownActions = new Set(["GoZone", "Play", "TrackPan", "TrackPanL", "TrackPanR", "TrackVolumeDisplay"]);
const surfaceSource = "Widget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n";
const homeSource = "Zone Home\n  Play Play\n  Shift+Play GoZone Transport\nZoneEnd\n";
const transportSource = "Zone Transport\n  Play Play\nZoneEnd\n";
const fxSource = "Zone ReaEQ\n  Play Play\nZoneEnd\n";
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
    await mkdir(path.join(surfaceRoot, "FXZones"), { recursive: true });
    await writeFile(path.join(surfaceRoot, "Surface.txt"), surfaceSource, "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "HomeZones", "Home.zon"), homeSource, "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "HomeZones", "Home.zon~20260101"), "backup\n", "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "GoZones", "GoZones.zon"), "Zone GoZones\n  Transport TrackNavigator\nZoneEnd\n", "utf8");
    await writeFile(path.join(surfaceRoot, "Zones", "GoZones", "Transport.zon"), transportSource, "utf8");
    await writeFile(path.join(surfaceRoot, "FXZones", "ReaEQ.zon"), fxSource, "utf8");

    productRoot = path.join(temporaryRoot, "Data", identity.resourceDirectory);
    await mkdir(path.join(productRoot, "Surfaces", "User"), { recursive: true });
    await mkdir(path.join(productRoot, "Zones", "User"), { recursive: true });
    await writeFile(path.join(productRoot, identity.configFilename), "Version=7.0\n", "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-legacy-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("legacy CSI import", () => {
    test("converts legacy comments and Learn directives at the start of a physical line", () => {
        const source = "\uFEFF/ disabled surface line\r\n  /OnZoneActivation NoAction\r\n# disabled hash line\r\n#WidgetType Fader\r\n# OSKRow\r\n  X32Fader /ch/01/mix/fader // inline comment\r\n";
        expect(migrateLegacyCommentSyntax(source)).toBe("\uFEFF// disabled surface line\r\n  //OnZoneActivation NoAction\r\n// disabled hash line\r\n#WidgetType Fader\r\n// OSKRow\r\n  X32Fader /ch/01/mix/fader // inline comment\r\n");
    });

    test("shows migrated comments in the import preview without changing the old file", async () => {
        const sourcePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        const legacySource = "/ disabled binding\n" + homeSource;
        await writeFile(sourcePath, legacySource, "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        expect(preview.valid).toBeTrue();
        expect(preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.source).toStartWith("// @format zone 1\n// disabled binding\n");
        expect(await readFile(sourcePath, "utf8")).toBe(legacySource);
    });

    test("offers similar action fixes in an import draft", async () => {
        const sourcePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(sourcePath, "Zone Home\n  Play MCUTrackPan\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "zone.action.unknown");

        expect(diagnostic?.fixes?.map((fix) => fix.label)).toEqual(["TrackPan", "TrackPanL", "TrackPanR"]);
        expect(preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.diagnostics.find((candidate) => candidate.code === "zone.action.unknown")?.fixes).toEqual(diagnostic?.fixes);
    });

    test("discovers a surface from a parent path and prepares a complete preview", async () => {
        const source = await LegacyCsiSource.create(temporaryRoot);
        expect(await source.listSurfaces()).toEqual([{ fxZoneCount: 1, name: "FaderPortV2", stableId: "faderportv2", zoneCount: 3 }]);

        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        expect(preview.valid).toBeTrue();
        expect(preview.selectedZonePaths).toEqual(["FXZones/ReaEQ.zon", "Zones/GoZones/Transport.zon", "Zones/HomeZones/Home.zon"]);
        expect(preview.items.map((item) => item.targetPath)).toEqual([
            "Surfaces/User/faderportv2.txt",
            "Zones/User/faderportv2/Main/GoZones/Transport.zon",
            "Zones/User/faderportv2/Main/HomeZones/Home.zon",
            "Zones/User/faderportv2/FX/ReaEQ.zon",
        ]);
        expect(preview.items.every((item) => item.source.startsWith(`// @format ${item.kind} 1\n`))).toBeTrue();
        expect(preview.items.some((item) => item.sourcePath.endsWith("GoZones.zon"))).toBeFalse();
        expect(preview.items.find((item) => item.sourcePath === "Zones/GoZones/Transport.zon")?.source).toContain("Zone Transport NavType=TrackNavigator\n");
        expect(preview.dependencies).toContainEqual({ from: "Zones/HomeZones/Home.zon", matches: ["Zones/GoZones/Transport.zon"], name: "Transport", selected: true, type: "GoZone" });
    });

    test("explains when a referenced legacy zone is not selected", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "zones.dependency.missing");

        expect(diagnostic?.message).toContain("matching legacy zone is not selected for import");
        expect(diagnostic?.line).toBe(3);
        expect(diagnostic?.related).toEqual([{ line: 2, path: "Zones/GoZones/Transport.zon" }]);
    });

    test("resolves an import dependency from the active target profile", async () => {
        const targetZonePath = path.join(productRoot, "Zones", "User", "faderportv2", "Main", "GoZones", "Transport.zon");
        await mkdir(path.dirname(targetZonePath), { recursive: true });
        await writeFile(targetZonePath, "// @format zone 1\nZone Transport\n  Play Play\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);

        expect(preview.diagnostics.some((diagnostic) => diagnostic.code === "zones.dependency.missing")).toBeFalse();
    });

    test("writes selected files in one transaction and requires conflict decisions on repeat", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const preview = await source.preview(store, knownActions, "FaderPortV2", true);
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));
        const report = await source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] });

        expect(report.created).toHaveLength(4);
        expect(await readFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "utf8")).toStartWith("// @format surface 1\n");
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "Main", "HomeZones", "Home.zon"), "utf8")).toStartWith("// @format zone 1\n");
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "ReaEQ.zon"), "utf8")).toStartWith("// @format zone 1\n");
        expect(await readFile(path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt"), "utf8")).toBe(surfaceSource);

        try {
            await source.import(store, knownActions, { includeSurface: true, resolutions: [], selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] });
            throw new Error("Expected a required conflict resolution");
        } catch (error) {
            expect(error).toBeInstanceOf(EditorOperationError);
            expect((error as EditorOperationError).code).toBe("legacy.resolution.required");
        }
    });

    test("follows linked legacy zone files", async () => {
        if (process.platform === "win32") return;
        const linkedSource = path.join(temporaryRoot, "Linked.zon");
        await writeFile(linkedSource, "Zone Linked\n  Play Play\nZoneEnd\n", "utf8");
        await symlink(linkedSource, path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "GoZones", "Linked.zon"));
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        expect(preview.selectedZonePaths).toContain("Zones/GoZones/Linked.zon");
        expect(preview.valid).toBeTrue();
    });

    test("requires a compatible widget mapping and rewrites every selected binding", async () => {
        await writeFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "Widget Play\n  Encoder b0 10 7f\nWidgetEnd\nWidget Stop\n  Press 90 5d 7f 90 5d 00\nWidgetEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const selectedZonePaths = ["Zones/HomeZones/Home.zon"];
        const unresolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths);

        expect(unresolved.valid).toBeFalse();
        expect(unresolved.widgetMappings).toContainEqual({
            candidates: [{ capabilities: ["press-input"], name: "Stop" }],
            occurrences: [{ line: 3, path: "Zones/HomeZones/Home.zon" }, { line: 4, path: "Zones/HomeZones/Home.zon" }],
            reason: "incompatible",
            requiredCapabilities: ["press-input"],
            selectedTarget: undefined,
            sourceWidget: "Play",
        });

        const widgetMappings = [{ sourceWidget: "Play", targetWidget: "Stop" }];
        const resolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths, widgetMappings);
        expect(resolved.valid).toBeTrue();
        const zone = resolved.items.find((item) => item.sourcePath === selectedZonePaths[0])!;
        expect(zone.source).toContain("  Stop Play\n");
        expect(zone.source).toContain("  Shift+Stop GoZone Transport\n");
        const manuallyResolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths, [{ sourceWidget: "Play", targetWidget: "Control+Stop" }]);
        const manuallyMappedZone = manuallyResolved.items.find((item) => item.sourcePath === selectedZonePaths[0])!;
        expect(manuallyResolved.valid).toBeTrue();
        expect(manuallyMappedZone.source).toContain("  Control+Stop Play\n");
        expect(manuallyMappedZone.source).toContain("  Shift+Control+Stop GoZone Transport\n");
        const resolutions = [{ action: "create" as const, id: zone.id, sourceHash: zone.sourceHash, targetHash: zone.targetHash }];
        await source.import(store, knownActions, { includeSurface: false, resolutions, selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings });
        const imported = await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "Main", "HomeZones", "Home.zon"), "utf8");
        expect(imported).toContain("  Stop Play\n");
        expect(imported).toContain("  Shift+Stop GoZone Transport\n");
    });

    test("maps channel placeholder widgets only to another channel family", async () => {
        const legacySurfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const legacyZonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(legacySurfacePath, "Widget Fader1\n  Press 90 01 7f 90 01 00\nWidgetEnd\nWidget Fader2\n  Press 90 02 7f 90 02 00\nWidgetEnd\n", "utf8");
        await writeFile(legacyZonePath, "Zone Home\n  Fader| Play\nZoneEnd\n", "utf8");
        await writeFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "Widget RotaryPush1\n  Press 90 11 7f 90 11 00\nWidgetEnd\nWidget RotaryPush2\n  Press 90 12 7f 90 12 00\nWidgetEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const selectedZonePaths = ["Zones/HomeZones/Home.zon"];
        const unresolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths);

        expect(unresolved.widgetMappings[0].sourceWidget).toBe("Fader|");
        expect(unresolved.widgetMappings[0].candidates.map((candidate) => candidate.name)).toEqual(["RotaryPush|"]);
        const resolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths, [{ sourceWidget: "Fader|", targetWidget: "RotaryPush|" }]);
        expect(resolved.valid).toBeTrue();
        expect(resolved.items.find((item) => item.sourcePath === selectedZonePaths[0])?.source).toContain("  RotaryPush| Play\n");
    });

    test("treats Touch as a modifier for a display family", async () => {
        const legacySurfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const legacyZonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(legacySurfacePath, "Widget DisplayLower1\n  FB_MCUDisplayLower 0\nWidgetEnd\nWidget DisplayLower2\n  FB_MCUDisplayLower 1\nWidgetEnd\n", "utf8");
        await writeFile(legacyZonePath, "Zone Home\n  Touch+DisplayLower| TrackVolumeDisplay\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);

        expect(preview.valid).toBeTrue();
        expect(preview.widgetMappings).toEqual([]);
        expect(preview.diagnostics.some((diagnostic) => diagnostic.code === "legacy.widget.mapping.required")).toBeFalse();
    });

    test("imports an edited draft into a custom profile and target without changing the old CSI file", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const selectedZonePaths = ["Zones/HomeZones/Home.zon"];
        const initial = await source.preview(store, knownActions, "FaderPortV2", true, selectedZonePaths);
        const initialZone = initial.items.find((item) => item.sourcePath === selectedZonePaths[0])!;
        const draftSource = initialZone.source.replace("  Play Play\n", "  Play GoZone Transport\n");
        const drafts = [{ originalSourceHash: initialZone.originalSourceHash, source: draftSource, sourcePath: initialZone.sourcePath }];
        const targetPaths = [{ sourcePath: initialZone.sourcePath, targetPath: "Zones/User/custom-profile/Main/Transport/Home.zon" }];
        const preview = await source.preview(store, knownActions, "FaderPortV2", true, selectedZonePaths, [], false, drafts, "custom-profile", targetPaths);
        const importedZone = preview.items.find((item) => item.sourcePath === selectedZonePaths[0])!;
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));

        expect(preview.valid).toBeTrue();
        expect(preview.targetProfileId).toBe("custom-profile");
        expect(importedZone.targetPath).toBe("Zones/User/custom-profile/Main/Transport/Home.zon");
        expect(importedZone.source).toContain("  Play GoZone Transport\n");
        await source.import(store, knownActions, { drafts, includeSurface: true, resolutions, selectedZonePaths, surfaceName: "FaderPortV2", targetPaths, targetProfileId: "custom-profile", widgetMappings: [] });
        expect(await readFile(path.join(productRoot, "Zones", "User", "custom-profile", "Main", "Transport", "Home.zon"), "utf8")).toContain("  Play GoZone Transport\n");
        expect(await readFile(path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon"), "utf8")).toBe(homeSource);
    });

    test("reports when two selected zones use the same import target", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const selectedZonePaths = ["Zones/HomeZones/Home.zon", "Zones/GoZones/Transport.zon"];
        const targetPath = "Zones/User/faderportv2/Main/Shared.zon";
        const targetPaths = selectedZonePaths.map((sourcePath) => ({ sourcePath, targetPath }));
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, selectedZonePaths, [], false, [], undefined, targetPaths);

        expect(preview.valid).toBeFalse();
        expect(preview.diagnostics.some((diagnostic) => diagnostic.code === "legacy.target.duplicate")).toBeTrue();
    });

    test("does not request hardware mapping for a declared modifier alias", async () => {
        const legacyZonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(legacyZonePath, "Zone Home\n  Play Nudge\n  Nudge Play\n  NullDisplay NoAction\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);

        expect(preview.valid).toBeTrue();
        expect(preview.widgetMappings).toEqual([]);
    });

    test("uses the existing surface when the imported surface conflict is skipped", async () => {
        await writeFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "Widget Play\n  Encoder b0 10 7f\nWidgetEnd\nWidget Stop\n  Press 90 5d 7f 90 5d 00\nWidgetEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const selectedZonePaths = ["Zones/HomeZones/Home.zon"];
        const widgetMappings = [{ sourceWidget: "Play", targetWidget: "Stop" }];
        const preview = await source.preview(store, knownActions, "FaderPortV2", true, selectedZonePaths, widgetMappings, true);
        const surface = preview.items.find((item) => item.kind === "surface")!;
        const zone = preview.items.find((item) => item.sourcePath === selectedZonePaths[0])!;
        const resolutions = [
            { action: "skip" as const, id: surface.id, sourceHash: surface.sourceHash, targetHash: surface.targetHash },
            { action: "create" as const, id: zone.id, sourceHash: zone.sourceHash, targetHash: zone.targetHash },
        ];

        expect(preview.widgetTarget).toBe("existing");
        await source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings });
        expect(await readFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "utf8")).toContain("  Encoder b0 10 7f\n");
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "Main", "HomeZones", "Home.zon"), "utf8")).toContain("  Stop Play\n");
    });
});
