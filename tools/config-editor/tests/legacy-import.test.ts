import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { LegacyCsiSource, migrateLegacyCommentSyntax, migrateLegacyZoneSyntax } from "../src/legacy-import.ts";
import { parseByPath } from "../src/formats.ts";
import { convertLegacyLearnFxToFormat2 } from "../src/legacy-learn-fx.ts";
import { convertLegacySurfaceToFormat2 } from "../src/legacy-surface-format2.ts";
import { convertLegacyZoneToFormat2 } from "../src/legacy-zone-format2.ts";
import { migrateLegacySce24RingColors } from "../src/legacy-sce24-ring.ts";
import { migrateLegacySce24StateColors } from "../src/legacy-sce24-state.ts";
import { ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { ConfigurationStore, EditorOperationError } from "../src/store.ts";
import { parseSurface } from "../src/surface.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.conf",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};
const knownActions = new Set(["FXParam", "GoZone", "Play", "TrackPan", "TrackPanL", "TrackPanR", "TrackSelect", "TrackVolume", "TrackVolumeDisplay"]);
const surfaceSource = "Widget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n";
const homeSource = "Zone Home\n  Play Play\n  Shift+Play GoZone Transport\nZoneEnd\n";
const transportSource = "Zone Transport\n  Play Play\nZoneEnd\n";
const fxSource = "Zone ReaEQ\n  Play Play\nZoneEnd\n";
const goldenFixtureRoot = path.join(import.meta.dir, "..", "fixtures", "format2-spec", "golden");
let temporaryRoot = "";
let legacyRoot = "";
let productRoot = "";

async function readGoldenFixture(scenario: string, kind: "expected" | "legacy", filename: string): Promise<string> {
    return readFile(path.join(goldenFixtureRoot, scenario, kind, filename), "utf8");
}

function normalizeTrimLineEnd(source: string): string {
    return source.replace(/\r\n/g, "\n").trimEnd();
}

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
    await writeFile(path.join(productRoot, identity.configFilename), "Device test {\n  Type=MIDI\n  Input=0\n  Output=0\n}\n\nPage Home {\n  Surface test {\n    Device=test\n    Template=test\n  }\n}\n", "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-legacy-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("legacy CSI import", () => {
    test("converts a legacy MIDI Surface and creates a fader-aware OSK layout", () => {
        const legacySurface = `StepSize
  RotaryWidgetClass 0.003
StepSizeEnd
AccelerationValues
  RotaryWidgetClass Dec 41 42
  RotaryWidgetClass Inc 01 02
AccelerationValuesEnd
Widget Fader # Shape=Fader Height=7
  Fader14Bit e0 7f 7f
  FB_Fader14Bit e0 7f 7f
  Touch 90 68 7f 90 68 00
WidgetEnd
Widget Rotary RotaryWidgetClass # Group=RotaryGroup
  Encoder b0 10 7f
  FB_Encoder b0 10 7f
WidgetEnd
Widget RotaryPush # Group=RotaryGroup OSKHidden
  Press 90 20 7f 90 20 00
WidgetEnd
Widget Play
  Press 90 5e 7f 90 5e 00
  FB_TwoState 90 5e 7f 90 5e 00
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPortV2", "Surfaces/User/faderportv2.txt");
        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toStartWith('@Meta { Version=2 Protocol=MIDI Channels=1 Name="FaderPortV2" }');
        expect(conversion.source).toContain("Input Value { Encoding=MIDI14 Status=0xE0 }");
        expect(conversion.source).toContain("RingProfile RotaryRing {");
        expect(conversion.source).toContain("Feedback Ring { Encoding=MIDI7 Message=[ 0xB0, 0x30 ] RingProfile=RotaryRing StyleTarget=Value StyleShift=4 StyleCombine=BitOr }");
        expect(conversion.source).toContain("Widget Fader Shape=Fader Height=7 TouchTarget=Fader ValueTarget=Fader");
        expect(conversion.source).toContain("Widget Rotary Group=RotaryGroup ScrollTarget=Rotary PressTarget=RotaryPush");
        expect(conversion.source.match(/  Row \{/g)).toHaveLength(7);
    });

    test("keeps the FaderPort fader and its first button row together", () => {
        const legacySurface = migrateLegacyCommentSyntax(`Widget Fader # Shape=Fader Height=7.5
  Fader14Bit e0 7f 7f
  Touch 90 68 7f 90 68 00
WidgetEnd

# OSKRow
Widget Solo # Color=ff9900
  Press 90 08 7f 90 08 00
WidgetEnd
Widget Mute # Color=ff2222
  Press 90 10 7f 90 10 00
WidgetEnd
Widget Arm # Color=ff2222
  Press 90 00 7f 90 00 00
WidgetEnd
Widget Shift # Color=ff9900
  Press 90 46 7f 90 46 00
WidgetEnd
`);
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPortV2", "Surfaces/User/faderportv2.txt");

        expect(conversion.source).toContain("  Row {\n    Widget Fader Shape=Fader Height=7.5 TouchTarget=Fader ValueTarget=Fader\n    Widget Solo Color=#FF9900\n    Widget Mute Color=#FF2222\n    Widget Arm Color=#FF2222\n    Widget Shift Color=#FF9900\n  }");
    });

    test("converts legacy comments and Learn directives at the start of a physical line", () => {
        const source = "\uFEFF/ disabled surface line\r\n  /OnZoneActivation NoAction\r\n# disabled hash line\r\n#WidgetType Fader\r\n# OSKRow\r\n  X32Fader /ch/01/mix/fader // inline comment\r\n";
        expect(migrateLegacyCommentSyntax(source)).toBe("\uFEFF// disabled surface line\r\n  //OnZoneActivation NoAction\r\n// disabled hash line\r\n#WidgetType Fader\r\n// OSKRow\r\n  X32Fader /ch/01/mix/fader // inline comment\r\n");
    });

    test("matches the structural legacy Zone golden files", async () => {
        for (const filename of ["Channel.zon", "Home.zon", "Pan.zon"]) {
            const legacyZone = migrateLegacyCommentSyntax(await readGoldenFixture("zone-structure", "legacy", filename));
            const expectedZone = await readGoldenFixture("zone-structure", "expected", filename);
            const conversion = convertLegacyZoneToFormat2(legacyZone, { isLayer: filename === "Pan.zon", profile: "Main", targetPath: `Zones/User/test/Main/${filename}` });
            expect(conversion.diagnostics).toEqual([]);
            expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedZone));
        }
    });

    test("matches the legacy Zone action and value golden file", async () => {
        const legacyZone = await readGoldenFixture("actions-and-values", "legacy", "Actions.zon");
        const expectedZone = await readGoldenFixture("actions-and-values", "expected", "Actions.zon");
        const conversion = convertLegacyZoneToFormat2(legacyZone, { profile: "Main", targetPath: "Zones/User/test/Main/Track.zon" });
        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedZone));
    });

    test("converts implicit legacy holds and keeps quoted values", () => {
        const conversion = convertLegacyZoneToFormat2("Zone Home\n  Play Reaper 40044 HoldDelay=500 HoldRepeatInterval=100 OSD=\"Repeat action\"\nZoneEnd\n", { profile: "Main", targetPath: "Zones/User/test/Main/Home.zon" });
        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain('(Hold)+Play Reaper 40044 DelayMs=500 RepeatIntervalMs=100 OSD="Repeat action"');
    });

    test("combines legacy Learn FX layout and generated bindings", () => {
        const layout = migrateLegacyCommentSyntax("Zone FXWidgetLayout\n  Fader FXParam\n  RotaryBig FXParam\nZoneEnd\n\n#WidgetType Fader\n#WidgetType RotaryBig RingStyle=Dot\n/ #RingStyle Fill\n");
        const prologue = migrateLegacyCommentSyntax("Zone FXPrologue\n  /OnZoneActivation ToggleUseLocalModifiers\nZoneEnd\n");
        const epilogue = migrateLegacyCommentSyntax("Zone FXEpilogue\n  OnZoneDeactivation HideFXSlot\n  Bypass ClearFXSlot\nZoneEnd\n");
        const conversion = convertLegacyLearnFxToFormat2({
            epilogue: { source: epilogue, sourcePath: "Zones/LearnZones/FXEpilogue.zon" },
            layout: { source: layout, sourcePath: "Zones/FXWidgetLayout.zon" },
            prologue: { source: prologue, sourcePath: "Zones/LearnZones/FXPrologue.zon" },
        });

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toBe("@Meta { Version=2 }\n\nFXWidgets {\n  Parameter Fader\n  Parameter RotaryBig RingStyle=Dot\n}\n\nGeneratedBindings {\n  On ZoneDeactivation {\n    HideFXSlot\n  }\n\n  Bypass ClearFXSlot\n}\n");
    });

    test("shows one LearnFX.fxzon import item instead of legacy Learn pseudo-zones", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const zonesRoot = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones");
        await writeFile(surfacePath, `${surfaceSource}Widget Fader\n  Fader14Bit e0 7f 7f\nWidgetEnd\n`, "utf8");
        await mkdir(path.join(zonesRoot, "LearnZones"), { recursive: true });
        await writeFile(path.join(zonesRoot, "FXWidgetLayout.zon"), "Zone FXWidgetLayout\n  Fader FXParam\nZoneEnd\n\n#WidgetType Fader\n", "utf8");
        await writeFile(path.join(zonesRoot, "LearnZones", "FXPrologue.zon"), "Zone FXPrologue\nZoneEnd\n", "utf8");
        await writeFile(path.join(zonesRoot, "LearnZones", "FXEpilogue.zon"), "Zone FXEpilogue\n  OnZoneDeactivation HideFXSlot\nZoneEnd\n", "utf8");
        await writeFile(path.join(zonesRoot, "LearnZones", "FXRowLayout.zon"), "Zone FXRowLayout\n  \"\" \"\"\nZoneEnd\n", "utf8");

        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const learnFx = preview.items.find((item) => item.kind === "learn-fx");

        expect(learnFx).toMatchObject({ selected: true, sourcePath: "Zones/FXWidgetLayout.zon", targetPath: "Zones/User/faderportv2/LearnFX.fxzon" });
        expect(learnFx?.source).toContain("Parameter Fader");
        expect(learnFx?.source).toContain("On ZoneDeactivation");
        expect(preview.items.some((item) => /FX(?:Prologue|Epilogue|RowLayout)\.zon$/.test(item.sourcePath))).toBeFalse();
        expect(preview.sources.map((item) => item.sourcePath)).toContain("Zones/LearnZones/FXPrologue.zon");

        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));
        await source.import(await createStore(), knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] });
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "LearnFX.fxzon"), "utf8")).toBe(learnFx?.source);
        const importedPreview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        expect(importedPreview.items.filter((item) => item.selected).every((item) => item.targetHash === item.sourceHash)).toBeTrue();
    });

    test("keeps duplicate Learn FX sources available for diagnostic navigation and drafts", async () => {
        const zonesRoot = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones");
        await mkdir(path.join(zonesRoot, "LearnZones"), { recursive: true });
        await writeFile(path.join(zonesRoot, "FXWidgetLayout.zon"), "Zone FXWidgetLayout\n  Fader FXParam\nZoneEnd\n", "utf8");
        await writeFile(path.join(zonesRoot, "LearnZones", "FXWidgetLayout.zon"), "Zone FXWidgetLayout\n  Rotary FXParam\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "legacy.learn-fx.source.duplicate");

        expect(diagnostic?.related?.map((location) => location.path)).toEqual(["Zones/FXWidgetLayout.zon", "Zones/LearnZones/FXWidgetLayout.zon"]);
        const duplicateSource = preview.sources.find((item) => item.sourcePath === "Zones/LearnZones/FXWidgetLayout.zon");
        expect(duplicateSource?.source).toContain("Rotary FXParam");
        expect(duplicateSource).toMatchObject({ kind: "learn-fx", targetPath: "Zones/User/faderportv2/LearnFX.fxzon" });
        expect(diagnostic?.fixes?.map((fix) => fix.label)).toContain("Comment out this duplicate file");
        const fixedPreview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, [...preview.selectedZonePaths, duplicateSource!.sourcePath], [], false, [{ originalSourceHash: duplicateSource!.originalSourceHash, source: "// duplicate disabled\n", sourcePath: duplicateSource!.sourcePath }]);
        expect(fixedPreview.diagnostics.some((candidate) => candidate.code === "legacy.learn-fx.source.duplicate")).toBeFalse();
        expect(fixedPreview.selectedZonePaths).not.toContain(duplicateSource!.sourcePath);
    });

    test("preserves prefix presses, press-only buttons, and seven-bit values", () => {
        const legacySurface = `Widget Any
  AnyPress b0 10 7f
WidgetEnd
Widget PressOnly
  Press 90 20 7f
WidgetEnd
Widget Value
  Fader7Bit b0 30 7f
  FB_Fader7Bit b0 30 7f
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Generic MIDI", "Surfaces/User/generic-midi.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain("Input Press { Encoding=MIDIPrefix Message=[ 0xB0, 0x10 ] }");
        expect(conversion.source).toContain("Input Press { Encoding=MIDIExact On=[ 0x90, 0x20, 0x7F ] }");
        expect(conversion.source).toContain("Input Value { Encoding=MIDI7 Message=[ 0xB0, 0x30 ] }");
        expect(conversion.source).toContain("Feedback Value { Encoding=MIDI7 Message=[ 0xB0, 0x30 ] }");
    });

    test("matches the FaderPort Classic split-fader golden Surface", async () => {
        const legacySurface = await readGoldenFixture("faderport-classic-split", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("faderport-classic-split", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPort Classic", "Surfaces/User/faderport-classic.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("rejects a FaderPort Classic split fader whose two parts have the same prefix", () => {
        const legacySurface = `Widget Fader
  FaderportClassicFader14Bit b0 00 7f b0 00 7f
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPort Classic", "Surfaces/User/faderport-classic.txt");

        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.surface.midi-split.prefix.duplicate", line: 2 }));
    });

    test("converts palette color, state-scaled RGB, and value bars to universal feedback", () => {
        const legacySurface = `Widget ColoredButton
  FB_FaderportTwoStateRGB 90 18 7f
WidgetEnd
Widget ValueBar
  FB_FaderportValueBar 9
WidgetEnd
Widget PaletteButton
  FB_MFT_RGB b1 20 7f
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Feedback surface", "Surfaces/User/feedback.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain("BarProfile StandardBar {");
        expect(conversion.source).toContain("Feedback Bar { Encoding=MIDI7 Message=[ 0xB0, 0x41 ] StyleMessage=[ 0xB0, 0x49 ] BarProfile=StandardBar }");
        expect(conversion.source).toContain("Feedback Color { Encoding=MIDIRGB Enable=[ 0x90, 0x18, 0x7F ] Red=[ 0x91, 0x18 ] Green=[ 0x92, 0x18 ] Blue=[ 0x93, 0x18 ] InactiveBrightness=0.1111111111111111 ActiveBrightness=1 }");
        expect(conversion.source).toContain("Feedback Color { Encoding=MIDIPalette Message=[ 0xB1, 0x20 ] ColorProfile=Palette128 Companion=[ 0xB2, 0x20, 0x2F ] CompanionOrder=After }");
        expect(conversion.source.match(/  Entry Color=#[0-9A-F]{6} Value=\d+/g)).toHaveLength(128);
    });

    test("matches the MFT palette golden Surface", async () => {
        const legacySurface = await readGoldenFixture("mft-palette", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("mft-palette", "expected", "surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported MIDI Fighter Twister Surface", "Surfaces/User/mft.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("converts MCU-family display rows to universal SysEx text feedback", () => {
        const legacySurface = `Widget McuUpper
  FB_MCUDisplayUpper 2
WidgetEnd
Widget McuLower
  FB_MCUDisplayLower 2
WidgetEnd
Widget ExtenderUpper
  FB_MCUXTDisplayUpper 1
WidgetEnd
Widget C4Upper
  FB_C4DisplayUpper 3 7
WidgetEnd
Widget C4Lower
  FB_C4DisplayLower 0 0
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Text displays", "Surfaces/User/text-displays.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source.match(/TextProfile Display7 \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x12, 0x0E, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x12, 0x46, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x15, 0x12, 0x07, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x17, 0x33, 0x31, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x17, 0x30, 0x38, Text ] TextProfile=Display7 }");
    });

    test("matches the MCU character-display golden Surface", async () => {
        const legacySurface = await readGoldenFixture("mcu-character-displays", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("mcu-character-displays", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "MCU displays", "Surfaces/User/mcu-displays.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the X-Touch text and track-color golden Surface", async () => {
        const legacySurface = await readGoldenFixture("xtouch-text", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("xtouch-text", "expected", "surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "X-Touch", "Surfaces/User/x-touch.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the iCON text and track-color golden Surface", async () => {
        const legacySurface = await readGoldenFixture("icon-track-color", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("icon-track-color", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "iCON V1X", "Surfaces/User/icon-v1x.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the Icon display golden Surface", async () => {
        const legacySurface = await readGoldenFixture("icon-displays", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("icon-displays", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Icon displays", "Surfaces/User/icon-displays.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the Asparion feedback golden Surface", async () => {
        const legacySurface = await readGoldenFixture("asparion-feedback", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("asparion-feedback", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Asparion", "Surfaces/User/asparion.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/asparion.txt").diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("requires an explicit channel for imported Asparion track-color feedback", () => {
        const conversion = convertLegacySurfaceToFormat2("Widget TrackColor\n  FB_AsparionRGB 90 20 7f\nWidgetEnd\n", "Asparion", "Surfaces/User/asparion.txt");

        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.widget.channel.unresolved", line: 2 }));
    });

    test("matches the SCE24 dynamic-text golden Surface", async () => {
        const legacySurface = await readGoldenFixture("text-feedback", "legacy", "SCE24Surface.txt");
        const expectedSurface = await readGoldenFixture("text-feedback", "expected", "SCE24Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported SCE24 Surface", "Surfaces/User/sce24.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("converts FaderPort scribble rows to one universal text profile", () => {
        const legacySurface = `Widget ScribbleLine1_1
  FB_FP16ScribbleLine1 "0"
WidgetEnd
Widget ScribbleLine4_16
  FB_FP16ScribbleLine4 "15"
WidgetEnd
Widget FP8Line2
  FB_FP8ScribbleLine2 "3"
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPort displays", "Surfaces/User/faderport-displays.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source.match(/TextProfile FaderPortScribble \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("Width=30");
        expect(conversion.source).toContain("DefaultAlignment=Center");
        expect(conversion.source).toContain("InvertCode=4");
        expect(conversion.source).toContain("Widget ScribbleLine1_1 {\n  Channel=1\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x16, 0x12, 0x00, 0x00, TextPresentationCode, Text ] }");
        expect(conversion.source).toContain("Widget ScribbleLine4_16 {\n  Channel=16\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x16, 0x12, 0x0F, 0x03, TextPresentationCode, Text ] }");
        expect(conversion.source).toContain("Widget FP8Line2 {\n  Channel=4\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x02, 0x12, 0x03, 0x01, TextPresentationCode, Text ] }");
    });

    test("matches the FaderPort scribble-text golden Surface", async () => {
        const legacySurface = await readGoldenFixture("text-feedback", "legacy", "FaderPortSurface.txt");
        const expectedSurface = await readGoldenFixture("text-feedback", "expected", "FaderPortSurface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported FaderPort 16 Surface", "Surfaces/User/faderport16.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the FaderPort scribble-strip mode golden Surface", async () => {
        const legacySurface = await readGoldenFixture("scribble-strip-mode", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("scribble-strip-mode", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPort 16", "Surfaces/User/faderport16.txt", "XTouch", 2);

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("converts removed FaderPort 8 display aliases to the universal scribble profile", () => {
        const legacySurface = `Widget DisplayUpper1
  FB_FP8DisplayUpper 0
WidgetEnd
Widget DisplayUpperMiddle2
  FB_FP8DisplayUpperMiddle 1
WidgetEnd
Widget DisplayLowerMiddle7
  FB_FP8DisplayLowerMiddle 6
WidgetEnd
Widget DisplayLower8
  FB_FP8DisplayLower 7
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Legacy FaderPort 8", "Surfaces/User/faderport8.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/faderport8.txt").diagnostics).toEqual([]);
        expect(conversion.source.match(/TextProfile FaderPortScribble \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("Widget DisplayUpper1 {\n  Channel=1\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x02, 0x12, 0x00, 0x00, TextPresentationCode, Text ] }");
        expect(conversion.source).toContain("Widget DisplayUpperMiddle2 {\n  Channel=2\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x02, 0x12, 0x01, 0x01, TextPresentationCode, Text ] }");
        expect(conversion.source).toContain("Widget DisplayLowerMiddle7 {\n  Channel=7\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x02, 0x12, 0x06, 0x02, TextPresentationCode, Text ] }");
        expect(conversion.source).toContain("Widget DisplayLower8 {\n  Channel=8\n  Feedback Text { Encoding=MIDISysEx TextProfile=FaderPortScribble Payload=[ 0x00, 0x01, 0x06, 0x02, 0x12, 0x07, 0x03, TextPresentationCode, Text ] }");
    });

    test("matches the FaderPort value-bar and peak-meter golden Surface", async () => {
        const legacySurface = await readGoldenFixture("faderport-feedback", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("faderport-feedback", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPort feedback", "Surfaces/User/faderport-feedback.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the QCon master-meter golden Surface", async () => {
        const legacySurface = await readGoldenFixture("qcon-meter", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("qcon-meter", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "QCon master", "Surfaces/User/qcon-master.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/qcon-master.txt").diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the SCE24 ring golden Surface and migrates its zone colors", async () => {
        const legacySurface = await readGoldenFixture("sce24-ring", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("sce24-ring", "expected", "surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported SCE24 Surface", "Surfaces/User/sce24.txt");
        const zone = migrateLegacySce24RingColors("Zone Track\n  Rotary1 TrackVolume LEDRingColor=#0000ffff PushColor=#003f00ff\nZoneEnd\n", "Zones/Track.zon");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
        expect(zone.diagnostics).toEqual([]);
        expect(zone.source).toContain("RingColors=[ #003F00, #003F00, #003F00, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF ]");
        expect(zone.source).not.toContain("LEDRingColor=");
        expect(zone.source).not.toContain("PushColor=");
    });

    test("matches the SCE24 state-button golden Surface and zone colors", async () => {
        const legacySurface = await readGoldenFixture("sce24-state", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("sce24-state", "expected", "Surface.txt");
        const legacyZone = await readGoldenFixture("sce24-state", "legacy", "Home.zon");
        const expectedZone = await readGoldenFixture("sce24-state", "expected", "Home.zon");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "SCE24", "Surfaces/User/sce24.txt");
        const zone = migrateLegacySce24StateColors(legacyZone, "Zones/Home.zon");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
        expect(zone.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(zone.source)).toBe(normalizeTrimLineEnd(expectedZone));
    });

    test("moves a standalone SCE24 push color to its paired ring binding", () => {
        const migration = migrateLegacySce24RingColors("Zone FX\n  RotaryB4 NoAction\n  RotaryPushB4 FXParam 10 PushColor=#003f00ff\nZoneEnd\n", "Zones/FX.zon");

        expect(migration.diagnostics).toEqual([]);
        expect(migration.source).toContain("RotaryB4 NoAction RingColors=[ #003F00, #003F00, #003F00, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000, #000000 ]");
        expect(migration.source).toContain("RotaryPushB4 FXParam 10");
        expect(migration.source).not.toContain("PushColor=");
    });

    test("reports invalid and overlapping SCE24 ring ranges without changing the binding", () => {
        const source = "Zone Track\n  Rotary1 TrackVolume LEDRingColors=3-8-#ff0000ff+8-18-#00ff00ff\nZoneEnd\n";
        const migration = migrateLegacySce24RingColors(source, "Zones/Track.zon");

        expect(migration.diagnostics.map((diagnostic) => diagnostic.code)).toContain("legacy.zone.sce24-ring.overlap");
        expect(migration.diagnostics.map((diagnostic) => diagnostic.code)).toContain("legacy.zone.sce24-ring.range");
        expect(migration.source).toBe(source);
    });

    test("includes SCE24 ring and color migration in the import preview", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const zonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        const legacyZone = "Zone Home\n  Rotary1 TrackPan LEDRingColor=#0000ffff\n  RotaryPush1 Play PushColor=#003f00ff\nZoneEnd\n";
        await writeFile(surfacePath, "Widget Rotary1 RotaryWidgetClass\n  Encoder b0 00 7f\n  FB_SCE24Encoder b0 00 7f\nWidgetEnd\nWidget RotaryPush1\n  Press 90 20 7f 90 20 00\nWidgetEnd\n", "utf8");
        await writeFile(zonePath, legacyZone, "utf8");

        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);
        const convertedSurface = preview.items.find((item) => item.kind === "surface")?.source;
        const convertedZone = preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.source;

        expect(convertedSurface).toContain("RingProfile SCE24Ring {");
        expect(convertedZone).toContain("RingColors=[ #003F00, #003F00, #003F00, #0000FF");
        expect(convertedZone).not.toContain("LEDRingColor=");
        expect(convertedZone).not.toContain("PushColor=");
        expect(await readFile(zonePath, "utf8")).toBe(legacyZone);
    });

    test("includes SCE24 LED state migration in the import preview", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const zonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(surfacePath, "Widget LEDButton1\n  Press 90 05 7f 90 05 00\n  FB_SCE24LEDButton 90 05 7f\nWidgetEnd\n", "utf8");
        await writeFile(zonePath, "Zone Home\n  LEDButton1 Shift OnColor=#2f0f0000 OffColor=#00000000\nZoneEnd\n", "utf8");

        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);
        const convertedSurface = preview.items.find((item) => item.kind === "surface")?.source;
        const convertedZone = preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.source;

        expect(convertedSurface).toContain("Feedback State { Encoding=MIDISysEx Payload=[ 0x00, 0x02, 0x38, 0x01, 0x65, Red7, Green7, Blue7 ] }");
        expect(convertedZone).toContain("StateColors=[ #000000, #2F0F00 ]");
        expect(convertedZone).not.toContain("OnColor=");
        expect(convertedZone).not.toContain("OffColor=");
    });

    test("matches the MCU meter and display golden Surface", async () => {
        const legacySurface = await readGoldenFixture("mcu-meter-display", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("mcu-meter-display", "expected", "Surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "MCU meter and displays", "Surfaces/User/mcu.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("normalizes the legacy value-bar style spelling", () => {
        expect(migrateLegacyZoneSyntax("Zone Track\n  ValueBar| TrackPan BarStyle=BiPolar\nZoneEnd\n")).toContain("BarStyle=Bipolar");
    });

    test("normalizes legacy text presentation properties", () => {
        const source = "Zone Track\n  Upper TrackNameDisplay TextAlign=left TextInvert=YES\n  Lower TrackVolumeDisplay TextAlign=RIGHT TextInvert=no\nZoneEnd\n";

        expect(migrateLegacyZoneSyntax(source)).toBe("Zone Track\n  Upper TrackNameDisplay TextAlign=Left TextInvert=true\n  Lower TrackVolumeDisplay TextAlign=Right TextInvert=false\nZoneEnd\n");
    });

    test("does not guess unsupported legacy text presentation values", () => {
        const source = "Zone Track\n  Upper TrackNameDisplay TextAlign=Justify TextInvert=Maybe\nZoneEnd\n";

        expect(migrateLegacyZoneSyntax(source)).toBe(source);
    });

    test("splits generic legacy OSC input and feedback into typed primitives", () => {
        const legacySurface = `Widget ControlA
  Control /ControlA
  FB_Processor /ControlA
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Generic OSC", "Surfaces/User/generic-osc.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toStartWith('@Meta { Version=2 Protocol=OSC Channels=1 Name="Generic OSC" }');
        expect(conversion.source).toContain('Input Value { Encoding=OSCFloat Address="/ControlA" }');
        expect(conversion.source).toContain('Feedback Value { Encoding=OSCFloat Address="/ControlA" }');
        expect(conversion.source).toContain('Feedback Text { Encoding=OSCString Address="/ControlA" }');
        expect(conversion.source).toContain('Feedback Color { Encoding=OSCString Address="/ControlA/Color" Format=HexRGBA }');
    });

    test("matches the basic OSC control golden Surface", async () => {
        const legacySurface = await readGoldenFixture("osc-basic", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("osc-basic", "expected", "surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported OSC controls", "Surfaces/User/osc-controls.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/osc-controls.txt").diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("blocks conditional X32 integer address rewriting", () => {
        const conversion = convertLegacySurfaceToFormat2("Widget Select1\n  FB_X32IntProcessor /-stat/selidx/01\nWidgetEnd\n", "X32 selection", "Surfaces/User/x32-selection.txt");

        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.surface.x32-int.address-rewrite.unsupported", line: 2 }));
        expect(conversion.source).not.toContain("Feedback Value");
    });

    test("explains ambiguous combined X32 value and color feedback", () => {
        const conversion = convertLegacySurfaceToFormat2("Widget Mute1\n  FB_X32Processor /ch/01/mix/on\nWidgetEnd\n", "X32 feedback", "Surfaces/User/x32-feedback.txt");

        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.surface.x32-feedback.ambiguous", line: 2, message: expect.stringContaining("numeric and palette-color feedback") }));
    });

    test("matches the X32 OSC golden Surface", async () => {
        const legacySurface = await readGoldenFixture("osc-x32", "legacy", "Surface.txt");
        const expectedSurface = await readGoldenFixture("osc-x32", "expected", "surface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported X32 Surface", "Surfaces/User/x32.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/x32.txt").diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("rejects an X32 rotary acknowledgement without its input", () => {
        const conversion = convertLegacySurfaceToFormat2("Widget Pan1\n  FB_X32RotaryToEncoder /ch/01/mix/pan\nWidgetEnd\n", "Broken X32 Surface", "Surfaces/User/x32.txt");

        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.surface.x32-rotary.input.missing", line: 2 }));
    });

    test("matches the MIDI OSK and color-calibration golden Surface", async () => {
        const legacySurface = await readGoldenFixture("osk-color-calibration", "legacy", "MIDISurface.txt");
        const expectedSurface = await readGoldenFixture("osk-color-calibration", "expected", "MIDISurface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported MIDI Surface", "Surfaces/User/midi-osk.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/midi-osk.txt").diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("matches the OSC OSK golden Surface", async () => {
        const legacySurface = await readGoldenFixture("osk-color-calibration", "legacy", "OSCSurface.txt");
        const expectedSurface = await readGoldenFixture("osk-color-calibration", "expected", "OSCSurface.txt");
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Imported OSC Surface", "Surfaces/User/osc-osk.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/osc-osk.txt").diagnostics).toEqual([]);
        expect(normalizeTrimLineEnd(conversion.source)).toBe(normalizeTrimLineEnd(expectedSurface));
    });

    test("reports raw MIDI commands only when an RGB value targets palette feedback", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const zonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(surfacePath, `Widget PaletteButton
  Press b1 20 7f b1 20 00
  FB_MFT_RGB b1 20 7f
WidgetEnd
Widget DirectButton
  Press 90 18 7f 90 18 00
  FB_FaderportRGB 90 18 7f
WidgetEnd
`, "utf8");
        await writeFile(zonePath, "Zone Home\n  PaletteButton Play { 177 31 47 }\n  DirectButton Play { 177 31 47 }\nZoneEnd\n", "utf8");

        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const diagnostics = preview.diagnostics.filter((diagnostic) => diagnostic.code === "legacy.zone.mft-color-command");

        expect(diagnostics).toHaveLength(1);
        expect(diagnostics[0]).toEqual(expect.objectContaining({ line: 3, message: expect.stringContaining("0xB1 0x1F 0x2F") }));
    });

    test("shows migrated comments in the import preview without changing the old file", async () => {
        const sourcePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        const legacySource = "/ disabled binding\n" + homeSource;
        await writeFile(sourcePath, legacySource, "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        expect(preview.valid).toBeTrue();
        expect(preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.source).toStartWith("@Meta { Version=2 Role=Home }\n\n// disabled binding\n");
        expect(await readFile(sourcePath, "utf8")).toBe(legacySource);
    });

    test("moves the legacy zone MeterMode into the converted Surface meter profile", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const zonePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(surfacePath, "Widget Meter\n  FB_MCUVUMeter 0\nWidgetEnd\n", "utf8");
        await writeFile(zonePath, "Zone Home\n  Meter Play MeterMode=IconV1M\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const convertedSurface = preview.items.find((item) => item.kind === "surface")?.source;

        expect(convertedSurface).toContain("InputUnit=Decibels");
        expect(convertedSurface).toContain("Step Minimum=-60.1 Output=1");
        expect(convertedSurface).not.toContain("Step Minimum=-60.3 Output=1");
    });

    test("moves repeated scribble-strip Mode settings into the converted Surface", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const homePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        const transportPath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "GoZones", "Transport.zon");
        await writeFile(surfacePath, "Widget ScribbleStripMode\n  FB_FP8ScribbleStripMode 0\nWidgetEnd\n", "utf8");
        await writeFile(homePath, "Zone Home\n  ScribbleStripMode Mode=2\nZoneEnd\n", "utf8");
        await writeFile(transportPath, "Zone Transport\n  ScribbleStripMode Mode=2\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const convertedSurface = preview.items.find((item) => item.kind === "surface")?.source;
        const convertedHome = preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.source;

        expect(convertedSurface).toContain("InitialValue=2");
        expect(convertedHome).not.toContain("ScribbleStripMode");
    });

    test("reports conflicting legacy meter scales before import", async () => {
        const surfacePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt");
        const homePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        const transportPath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "GoZones", "Transport.zon");
        await writeFile(surfacePath, "Widget Meter\n  FB_MCUVUMeter 0\nWidgetEnd\n", "utf8");
        await writeFile(homePath, "Zone Home\n  Meter Play MeterMode=IconV1M\nZoneEnd\n", "utf8");
        await writeFile(transportPath, "Zone Transport\n  Meter Play MeterMode=XTouch\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "legacy.surface.meter-mode.conflict");
        const locations = [{ line: diagnostic?.line, path: diagnostic?.path }, ...(diagnostic?.related ?? [])];

        expect(diagnostic?.message).toContain("XTouch");
        expect(diagnostic?.message).toContain("IconV1M");
        expect(locations).toContainEqual({ line: 2, path: "Zones/HomeZones/Home.zon" });
        expect(locations).toContainEqual({ line: 2, path: "Zones/GoZones/Transport.zon" });
    });

    test("offers similar action fixes in an import draft", async () => {
        const sourcePath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "HomeZones", "Home.zon");
        await writeFile(sourcePath, "Zone Home\n  Play MCUTrackPan\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "zone.action.unknown");

        expect(diagnostic?.fixes?.map((fix) => fix.label)).toEqual(["TrackPan", "TrackPanL", "TrackPanR", "Comment out this line"]);
        expect(preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.diagnostics.find((candidate) => candidate.code === "zone.action.unknown")?.fixes).toEqual(diagnostic?.fixes);
    });

    test("discovers a surface from a parent path and prepares a complete preview", async () => {
        const zonesRoot = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones");
        await writeFile(path.join(zonesRoot, "Empty.zon"), "", "utf8");
        await writeFile(path.join(zonesRoot, "Comments.zon"), "// placeholder\n/ old placeholder\n# old placeholder\n", "utf8");
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
        expect(preview.items.every((item) => item.source.startsWith("@Meta { Version=2"))).toBeTrue();
        expect(preview.items.some((item) => item.sourcePath.endsWith("GoZones.zon"))).toBeFalse();
        expect(preview.items.some((item) => item.sourcePath.endsWith("Empty.zon") || item.sourcePath.endsWith("Comments.zon"))).toBeFalse();
        expect(preview.items.find((item) => item.sourcePath === "Zones/GoZones/Transport.zon")?.source).toStartWith("@Meta { Version=2 Target=Tracks }");
        expect(preview.dependencies).toContainEqual({ from: "Zones/HomeZones/Home.zon", matches: ["Zones/GoZones/Transport.zon"], name: "Transport", selected: true, type: "GoZone" });
    });

    test("explains when a referenced legacy zone is not selected", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "zones.dependency.missing");

        expect(diagnostic?.message).toContain("matching legacy zone is not selected for import");
        expect(diagnostic?.line).toBe(4);
        expect(diagnostic?.related).toEqual([{ line: 1, path: "Zones/GoZones/Transport.zon" }]);
    });

    test("converts legacy modifier Blink declarations and modifier source bindings", () => {
        const source = "Zone Home\n  Section Nudge Blink\n  Nudge SendMIDIMessage \"90 3c 7f\"\n  Nudge+Touch Reaper 41228\n  DoublePress+Nudge ToggleOSK\nZoneEnd\n";
        const conversion = convertLegacyZoneToFormat2(source, { isLayer: false, profile: "Main", targetPath: "Zones/User/test/Main/Home.zon" });

        expect(conversion.source).toContain("Section Modifier Nudge Blink\n");
        expect(conversion.source).toContain('[Nudge] SendMIDIMessage "90 3c 7f"\n');
        expect(conversion.source).toContain("[Nudge]+Touch Reaper 41228\n");
        expect(conversion.source).toContain("(DoublePress)+[Nudge] ToggleOSK\n");
    });

    test("explains that a selected dependency is invalid and links Bank context zones", async () => {
        const surfaceRoot = path.join(legacyRoot, "Surfaces", "FaderPortV2");
        await writeFile(path.join(surfaceRoot, "Zones", "HomeZones", "Home.zon"), "Zone Home\n  SubZones\n    LinkLock\n  SubZonesEnd\n  Link GoSubZone LinkLock\nZoneEnd\n", "utf8");
        await writeFile(path.join(surfaceRoot, "Zones", "GoZones", "SelectedTrackFXMenu.zon"), "Zone SelectedTrackFXMenu\n  Play Play\nZoneEnd\n", "utf8");
        await writeFile(path.join(surfaceRoot, "Zones", "GoZones", "LinkLock.zon"), "Zone LinkLock\n  Prev Bank SelectedTrackFXMenu -1\n  Link LeaveSubZone\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true);
        const dependency = preview.diagnostics.find((diagnostic) => diagnostic.code === "zones.dependency.missing" && diagnostic.message.includes("LinkLock"));
        const bankContext = preview.diagnostics.find((diagnostic) => diagnostic.code === "legacy.zone.bank.context");

        expect(dependency?.message).toContain("selected but invalid");
        expect(preview.diagnostics.some((diagnostic) => diagnostic.code === "format2.zone.action.bank-amount" && diagnostic.line === bankContext?.line && diagnostic.path === bankContext?.path)).toBeFalse();
        expect(bankContext?.line).toBe(preview.items.find((item) => item.zoneName === "LinkLock")?.source.split("\n").findIndex((line) => line.includes("Bank SelectedTrackFXMenu"))! + 1);
        expect(bankContext?.related?.map((related) => related.path)).toEqual(["Zones/User/faderportv2/Main/GoZones/SelectedTrackFXMenu.zon"]);
        expect(bankContext?.fixes?.map((fix) => fix.label)).toContain("Move this Bank binding to SelectedTrackFXMenu");
        const linkLock = preview.items.find((item) => item.zoneName === "LinkLock")!;
        const draftPreview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, preview.selectedZonePaths, [], false, [{ originalSourceHash: linkLock.originalSourceHash, source: linkLock.source, sourcePath: linkLock.sourcePath }]);
        const draftBankContext = draftPreview.diagnostics.find((diagnostic) => diagnostic.code === "legacy.zone.bank.context" && diagnostic.path === linkLock.targetPath);
        expect(draftBankContext?.fixes?.map((fix) => fix.label)).toContain("Move this Bank binding to SelectedTrackFXMenu");
        expect(draftPreview.diagnostics.some((diagnostic) => diagnostic.code === "format2.zone.action.bank-amount" && diagnostic.line === draftBankContext?.line && diagnostic.path === draftBankContext?.path)).toBeFalse();
    });

    test("resolves an import dependency from the active target profile", async () => {
        const targetZonePath = path.join(productRoot, "Zones", "User", "faderportv2", "Main", "GoZones", "Transport.zon");
        await mkdir(path.dirname(targetZonePath), { recursive: true });
        await writeFile(targetZonePath, "@Meta { Version=2 }\nPlay Play\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);

        expect(preview.diagnostics.some((diagnostic) => diagnostic.code === "zones.dependency.missing")).toBeFalse();
    });

    test("writes a complete FaderPortV2 format 2 set in one transaction and requires conflict decisions on repeat", async () => {
        const learnLayoutPath = path.join(legacyRoot, "Surfaces", "FaderPortV2", "Zones", "FXWidgetLayout.zon");
        await writeFile(learnLayoutPath, "Zone FXWidgetLayout\n  Play FXParam\nZoneEnd\n\n#WidgetType Play\n", "utf8");
        const oldVendorZonePath = path.join(productRoot, "Zones", "Vendor", "faderportv2", "Main", "FXWidgetLayout.zon");
        await mkdir(path.dirname(oldVendorZonePath), { recursive: true });
        await writeFile(oldVendorZonePath, "Zone FXWidgetLayout\n  Play FXParam\nZoneEnd\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const preview = await source.preview(store, knownActions, "FaderPortV2", true);
        expect(preview.recommendedSourceMode).toBe("User");
        expect(preview.valid).toBeTrue();
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));
        const report = await source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] });

        expect(report.created).toHaveLength(5);
        const importedSurface = await readFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "utf8");
        expect(importedSurface).toStartWith("@Meta { Version=2 Protocol=MIDI");
        expect(importedSurface).toContain("Widget Play {");
        expect(importedSurface).toContain("OSKLayout {");
        const importedHome = await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "Main", "HomeZones", "Home.zon"), "utf8");
        const importedFx = await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "FX", "ReaEQ.zon"), "utf8");
        const importedLearnFx = await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "LearnFX.fxzon"), "utf8");
        expect(importedHome).toStartWith("@Meta { Version=2 Role=Home }");
        expect(importedFx).toStartWith('@Meta { Version=2 MatchFX="ReaEQ" }');
        expect(importedLearnFx).toStartWith("@Meta { Version=2 }");
        for (const [targetPath, importedSource] of [["Surfaces/User/faderportv2.txt", importedSurface], ["Zones/User/faderportv2/Main/HomeZones/Home.zon", importedHome], ["Zones/User/faderportv2/FX/ReaEQ.zon", importedFx], ["Zones/User/faderportv2/LearnFX.fxzon", importedLearnFx]]) {
            const document = parseByPath(importedSource, targetPath, knownActions);
            expect(document.version).toBe("2");
            expect(document.diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
        }
        expect(await readFile(path.join(legacyRoot, "Surfaces", "FaderPortV2", "Surface.txt"), "utf8")).toBe(surfaceSource);

        try {
            await source.import(store, knownActions, { includeSurface: true, resolutions: [], selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] });
            throw new Error("Expected a required conflict resolution");
        } catch (error) {
            expect(error).toBeInstanceOf(EditorOperationError);
            expect((error as EditorOperationError).code).toBe("legacy.resolution.required");
        }
    });

    test("writes a complete eight-channel XTouchMiniMC format 2 set", async () => {
        const surfaceRoot = path.join(legacyRoot, "Surfaces", "XTouchMiniMC");
        const zoneRoot = path.join(surfaceRoot, "Zones");
        const fxRoot = path.join(surfaceRoot, "FXZones");
        await mkdir(zoneRoot, { recursive: true });
        await mkdir(fxRoot, { recursive: true });
        const channelWidgets = Array.from({ length: 8 }, (_, channelIdx) => {
            const channel = channelIdx + 1;
            const controller = (0x10 + channelIdx).toString(16);
            const pushNote = (0x20 + channelIdx).toString(16);
            return `Widget Rotary${channel} RotaryWidgetClass\n  Encoder b0 ${controller} 7f\n  FB_Encoder b0 ${controller} 7f\nWidgetEnd\n\nWidget RotaryPush${channel}\n  Press 90 ${pushNote} 7f 90 ${pushNote} 00\nWidgetEnd\n`;
        }).join("\n");
        const layoutWidgets = Array.from({ length: 8 }, (_, channelIdx) => `    Widget Rotary${channelIdx + 1} Shape=Round PressTarget=RotaryPush${channelIdx + 1}`).join("\n");
        const xTouchSurface = `StepSize\n  RotaryWidgetClass 0.003\nStepSizeEnd\n\n${channelWidgets}\nWidget Fader\n  Fader14Bit e8 7f 7f\nWidgetEnd\n\nWidget LayerA\n  Press 90 54 7f 90 54 00\nWidgetEnd\n\nWidget ButtonB7\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n\nOSKLayout Version=1\n  Row\n${layoutWidgets}\n    Widget Fader Shape=Fader Height=3\n  RowEnd\n  Row\n    Widget LayerA\n    Widget ButtonB7\n  RowEnd\nOSKLayoutEnd\n`;
        await writeFile(path.join(surfaceRoot, "Surface.txt"), xTouchSurface, "utf8");
        await writeFile(path.join(zoneRoot, "Home.zon"), "Zone Home\n  IncludedZones\n    Channel\n  IncludedZonesEnd\n  LayerA Control\n  ButtonB7 Play\nZoneEnd\n", "utf8");
        await writeFile(path.join(zoneRoot, "Channel.zon"), "Zone Channel TrackNavigator\n  Rotary| TrackVolume\n  RotaryPush| TrackSelect\nZoneEnd\n", "utf8");
        await writeFile(path.join(fxRoot, "ReaEQ.zon"), "Zone ReaEQ\n  Rotary1 FXParam 0\nZoneEnd\n", "utf8");

        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const preview = await source.preview(store, knownActions, "XTouchMiniMC", true);
        expect(preview.valid).toBeTrue();
        expect(preview.items.find((item) => item.kind === "surface")?.source).toStartWith('@Meta { Version=2 Protocol=MIDI Channels=8 Name="XTouchMiniMC" }');
        expect(preview.items.find((item) => item.sourcePath === "Zones/Channel.zon")?.source).toContain("Rotary# TrackVolume");
        expect(preview.items.find((item) => item.sourcePath === "Zones/Channel.zon")?.source).toContain("RotaryPush# TrackSelect");

        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));
        const report = await source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "XTouchMiniMC", widgetMappings: [] });
        expect(report.created).toHaveLength(4);
        for (const targetPath of report.created) {
            const importedSource = await readFile(path.join(productRoot, targetPath), "utf8");
            const document = parseByPath(importedSource, targetPath, knownActions);
            expect(document.version).toBe("2");
            expect(document.diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
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
        await mkdir(path.join(productRoot, "Zones/User/faderportv2/Main"), { recursive: true });
        await writeFile(path.join(productRoot, "Zones/User/faderportv2/Main/Transport.zon"), "@Meta { Version=2 }\nStop Play\n", "utf8");
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
        expect(zone.source).toContain("Stop Play\n");
        expect(zone.source).toContain("[Shift]+Stop GoZone Transport\n");
        const manuallyResolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths, [{ sourceWidget: "Play", targetWidget: "Control+Stop" }]);
        const manuallyMappedZone = manuallyResolved.items.find((item) => item.sourcePath === selectedZonePaths[0])!;
        expect(manuallyResolved.valid).toBeTrue();
        expect(manuallyMappedZone.source).toContain("[Control]+Stop Play\n");
        expect(manuallyMappedZone.source).toContain("[Shift]+[Control]+Stop GoZone Transport\n");
        const resolutions = [{ action: "create" as const, id: zone.id, sourceHash: zone.sourceHash, targetHash: zone.targetHash }];
        await source.import(store, knownActions, { includeSurface: false, resolutions, selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings });
        const imported = await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "Main", "HomeZones", "Home.zon"), "utf8");
        expect(imported).toContain("Stop Play\n");
        expect(imported).toContain("[Shift]+Stop GoZone Transport\n");
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

        expect(unresolved.widgetMappings[0].sourceWidget).toBe("Fader#");
        expect(unresolved.widgetMappings[0].candidates.map((candidate) => candidate.name)).toEqual(["RotaryPush#"]);
        const resolved = await source.preview(store, knownActions, "FaderPortV2", false, selectedZonePaths, [{ sourceWidget: "Fader|", targetWidget: "RotaryPush|" }]);
        expect(resolved.valid).toBeTrue();
        expect(resolved.items.find((item) => item.sourcePath === selectedZonePaths[0])?.source).toContain("RotaryPush# Play\n");
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
        const homePath = "Zones/HomeZones/Home.zon";
        const selectedZonePaths = [homePath, "Zones/GoZones/Transport.zon"];
        const initial = await source.preview(store, knownActions, "FaderPortV2", true, selectedZonePaths);
        const initialZone = initial.items.find((item) => item.sourcePath === homePath)!;
        const draftSource = initialZone.source.replace("Play Play\n", "Play GoZone Transport\n");
        const drafts = [{ originalSourceHash: initialZone.originalSourceHash, source: draftSource, sourcePath: initialZone.sourcePath }];
        const targetPaths = [{ sourcePath: homePath, targetPath: "Zones/User/custom-profile/Main/Transport/Home.zon" }];
        const preview = await source.preview(store, knownActions, "FaderPortV2", true, selectedZonePaths, [], false, drafts, "custom-profile", targetPaths);
        const importedZone = preview.items.find((item) => item.sourcePath === homePath)!;
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));

        expect(preview.valid).toBeTrue();
        expect(preview.targetProfileId).toBe("custom-profile");
        expect(importedZone.targetPath).toBe("Zones/User/custom-profile/Main/Transport/Home.zon");
        expect(importedZone.source).toContain("Play GoZone Transport\n");
        await source.import(store, knownActions, { drafts, includeSurface: true, resolutions, selectedZonePaths, surfaceName: "FaderPortV2", targetPaths, targetProfileId: "custom-profile", widgetMappings: [] });
        expect(await readFile(path.join(productRoot, "Zones", "User", "custom-profile", "Main", "Transport", "Home.zon"), "utf8")).toContain("Play GoZone Transport\n");
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
        await mkdir(path.join(productRoot, "Zones/User/faderportv2/Main"), { recursive: true });
        await writeFile(path.join(productRoot, "Zones/User/faderportv2/Main/Transport.zon"), "@Meta { Version=2 }\nStop Play\n", "utf8");
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
        expect(await readFile(path.join(productRoot, "Zones", "User", "faderportv2", "Main", "HomeZones", "Home.zon"), "utf8")).toContain("Stop Play\n");
    });

    test("blocks Skip when it retains an invalid destination required by Home", async () => {
        const targetPath = "Zones/User/faderportv2/Main/GoZones/Transport.zon";
        const invalidSource = "@Meta { Version=2 }\nPlay Bank SelectedTracks -1\n";
        await mkdir(path.dirname(path.join(productRoot, targetPath)), { recursive: true });
        await writeFile(path.join(productRoot, targetPath), invalidSource, "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const preview = await source.preview(store, knownActions, "FaderPortV2", true);
        expect(preview.valid).toBeTrue();
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: item.targetPath === targetPath ? "skip" as const : "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash }));
        await expect(source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] })).rejects.toThrow("final import profile contains errors");
        expect((await store.fileState("Surfaces/User/faderportv2.txt")).exists).toBeFalse();
        expect(await readFile(path.join(productRoot, targetPath), "utf8")).toBe(invalidSource);
    });

    test("blocks Rename when the retained destination creates a second Home", async () => {
        const targetPath = "Zones/User/faderportv2/Main/HomeZones/Home.zon";
        await mkdir(path.dirname(path.join(productRoot, targetPath)), { recursive: true });
        await writeFile(path.join(productRoot, targetPath), "@Meta { Version=2 Role=Home }\nPlay Play\n", "utf8");
        const source = await LegacyCsiSource.create(legacyRoot);
        const store = await createStore();
        const preview = await source.preview(store, knownActions, "FaderPortV2", true);
        expect(preview.valid).toBeTrue();
        const resolutions = preview.items.filter((item) => item.selected).map((item) => ({ action: item.targetPath === targetPath ? "rename" as const : "create" as const, id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash, ...(item.targetPath === targetPath ? { targetPath: "Zones/User/faderportv2/Main/OtherHome.zon" } : {}) }));
        await expect(source.import(store, knownActions, { includeSurface: true, resolutions, selectedZonePaths: preview.selectedZonePaths, surfaceName: "FaderPortV2", widgetMappings: [] })).rejects.toThrow("final import profile contains errors");
        expect((await store.fileState("Surfaces/User/faderportv2.txt")).exists).toBeFalse();
        expect((await store.fileState("Zones/User/faderportv2/Main/OtherHome.zon")).exists).toBeFalse();
    });
});
