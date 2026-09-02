import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { LegacyCsiSource, migrateLegacyCommentSyntax, migrateLegacyZoneSyntax } from "../src/legacy-import.ts";
import { convertLegacySurfaceToFormat2 } from "../src/legacy-surface-format2.ts";
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

    test("converts legacy comments and Learn directives at the start of a physical line", () => {
        const source = "\uFEFF/ disabled surface line\r\n  /OnZoneActivation NoAction\r\n# disabled hash line\r\n#WidgetType Fader\r\n# OSKRow\r\n  X32Fader /ch/01/mix/fader // inline comment\r\n";
        expect(migrateLegacyCommentSyntax(source)).toBe("\uFEFF// disabled surface line\r\n  //OnZoneActivation NoAction\r\n// disabled hash line\r\n#WidgetType Fader\r\n// OSKRow\r\n  X32Fader /ch/01/mix/fader // inline comment\r\n");
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

    test("converts Icon display variants to universal seven-character text feedback", () => {
        const legacySurface = `Widget Icon1Upper
  FB_IconDisplay1Upper 0
WidgetEnd
Widget Icon1Lower
  FB_IconDisplay1Lower 7
WidgetEnd
Widget Icon2Upper
  FB_IconDisplay2Upper 1
WidgetEnd
Widget Icon2Lower
  FB_IconDisplay2Lower 6
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Icon displays", "Surfaces/User/icon-displays.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source.match(/TextProfile Display7 \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("Widget Icon1Upper {\n  Channel=1\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x12, 0x00, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Widget Icon1Lower {\n  Channel=8\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x12, 0x69, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Widget Icon2Upper {\n  Channel=2\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x02, 0x4E, 0x15, 0x13, 0x07, Text ] TextProfile=Display7 }");
        expect(conversion.source).toContain("Widget Icon2Lower {\n  Channel=7\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x02, 0x4E, 0x15, 0x13, 0x62, Text ] TextProfile=Display7 }");
    });

    test("converts Asparion color, ring, displays, and meters to universal feedback", () => {
        const legacySurface = `Widget TrackColor1
  FB_AsparionRGB 90 20 7f
WidgetEnd
Widget Rotary1
  FB_AsparionEncoder b0 10 7f
WidgetEnd
Widget DisplayUpper1
  FB_AsparionDisplayUpper 0
WidgetEnd
Widget DisplayLower8
  FB_AsparionDisplayLower 7
WidgetEnd
Widget DisplayEncoder2
  FB_AsparionDisplayEncoder 1
WidgetEnd
Widget MeterLeft1
  FB_AsparionVUMeterL 0
WidgetEnd
Widget MeterRight8
  FB_AsparionVUMeterR 7
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Asparion", "Surfaces/User/asparion.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(parseSurface(conversion.source, "Surfaces/User/asparion.txt").diagnostics).toEqual([]);
        expect(conversion.source.match(/RingProfile AsparionRing \{/g)).toHaveLength(1);
        expect(conversion.source.match(/MeterProfile AsparionMeter \{/g)).toHaveLength(1);
        expect(conversion.source.match(/TextProfile Display12 \{/g)).toHaveLength(1);
        expect(conversion.source.match(/TextProfile Display8 \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("Widget TrackColor1 {\n  Channel=1\n  Feedback Color { Encoding=MIDIRGB Red=[ 0x91, 0x20 ] Green=[ 0x92, 0x20 ] Blue=[ 0x93, 0x20 ] TrackColor=true }");
        expect(conversion.source).toContain("Feedback Ring { Encoding=MIDI7 Message=[ 0xB0, 0x30 ] RingProfile=AsparionRing StyleTarget=Status StyleCombine=Add }");
        expect(conversion.source).toContain("Widget DisplayUpper1 {\n  Channel=1\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x1A, 0x00, 0x01, Text ] TextProfile=Display12 }");
        expect(conversion.source).toContain("Widget DisplayLower8 {\n  Channel=8\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x1A, 0x54, 0x02, Text ] TextProfile=Display12 }");
        expect(conversion.source).toContain("Widget DisplayEncoder2 {\n  Channel=2\n  Feedback Text { Encoding=MIDISysEx Payload=[ 0x00, 0x00, 0x66, 0x14, 0x19, 0x08, Text ] TextProfile=Display8 }");
        expect(conversion.source).toContain("Widget MeterLeft1 {\n  Channel=1\n  Feedback Meter { Encoding=MIDI7 Message=[ 0xD0 ] MeterProfile=AsparionMeter ValueBase=0x00 Combine=BitOr }");
        expect(conversion.source).toContain("Widget MeterRight8 {\n  Channel=8\n  Feedback Meter { Encoding=MIDI7 Message=[ 0xD1 ] MeterProfile=AsparionMeter ValueBase=0x70 Combine=BitOr }");
    });

    test("requires an explicit channel for imported Asparion track-color feedback", () => {
        const conversion = convertLegacySurfaceToFormat2("Widget TrackColor\n  FB_AsparionRGB 90 20 7f\nWidgetEnd\n", "Asparion", "Surfaces/User/asparion.txt");

        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.widget.channel.unresolved", line: 2 }));
    });

    test("converts legacy dynamic text and OLED button displays to universal SysEx fields", () => {
        const legacySurface = `Widget Display1
  FB_SCE24EncoderText 90 20 7f 0 15 2
WidgetEnd
Widget ButtonDisplay1
  FB_SCE24OLEDButton 90 0d 7f 1 63 6
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "Dynamic displays", "Surfaces/User/dynamic-displays.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source.match(/TextProfile DynamicText \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx TextProfile=DynamicText TopMargin=0 BottomMargin=15 Font=2 BackgroundColor=#000000 TextColor=#000000 Payload=[ 0x00, 0x02, 0x38, 0x01, 0x20, TopMargin7, BottomMargin7, Font7, BackgroundRed7, BackgroundGreen7, BackgroundBlue7, TextRed7, TextGreen7, TextBlue7, Text ] }");
        expect(conversion.source).toContain("Feedback Text { Encoding=MIDISysEx TextProfile=DynamicText TopMargin=1 BottomMargin=63 Font=6 BackgroundColor=#000000 TextColor=#000000 Payload=[ 0x00, 0x02, 0x38, 0x01, 0x6D, TopMargin7, BottomMargin7, Font7, BackgroundRed7, BackgroundGreen7, BackgroundBlue7, TextRed7, TextGreen7, TextBlue7, Text ] }");
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

    test("converts FaderPort peak meters to valid continuous MIDI data", () => {
        const legacySurface = `Widget VUMeter1
  FB_FPVUMeter 0
WidgetEnd
Widget VUMeter8
  FB_FPVUMeter 7
WidgetEnd
Widget VUMeter9
  FB_FPVUMeter 8
WidgetEnd
Widget VUMeter16
  FB_FPVUMeter 15
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "FaderPort meters", "Surfaces/User/faderport-meters.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source.match(/MeterProfile FaderPortPeakMeter \{/g)).toHaveLength(1);
        expect(conversion.source).toContain("OutputRange=[ 0, 127 ]");
        expect(conversion.source).toContain("Widget VUMeter1 {\n  Channel=1\n  Feedback Meter { Encoding=MIDI7 Message=[ 0xD0 ] MeterProfile=FaderPortPeakMeter Refresh=Continuous RefreshIntervalMs=10 }");
        expect(conversion.source).toContain("Widget VUMeter8 {\n  Channel=8\n  Feedback Meter { Encoding=MIDI7 Message=[ 0xD7 ] MeterProfile=FaderPortPeakMeter Refresh=Continuous RefreshIntervalMs=10 }");
        expect(conversion.source).toContain("Widget VUMeter9 {\n  Channel=9\n  Feedback Meter { Encoding=MIDI7 Message=[ 0xC0 ] MeterProfile=FaderPortPeakMeter Refresh=Continuous RefreshIntervalMs=10 }");
        expect(conversion.source).toContain("Widget VUMeter16 {\n  Channel=16\n  Feedback Meter { Encoding=MIDI7 Message=[ 0xC7 ] MeterProfile=FaderPortPeakMeter Refresh=Continuous RefreshIntervalMs=10 }");
        expect(conversion.source).not.toContain("0xA0");
    });

    test("converts SCE24 encoder rings and explicit segment colors", () => {
        const legacySurface = `Widget Rotary1 RotaryWidgetClass
  Encoder b0 00 7f
  FB_SCE24Encoder b0 00 7f
WidgetEnd
Widget RotaryPush1
  Press 90 20 7f 90 20 00
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "SCE24", "Surfaces/User/sce24.txt");
        const zone = migrateLegacySce24RingColors("Zone Track\n  Rotary1 TrackVolume LEDRingColor=#0000ffff PushColor=#003f00ff\nZoneEnd\n", "Zones/Track.zon");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain("RingProfile SCE24Ring {");
        expect(conversion.source).toContain("Payload=[ 0x00, 0x02, 0x38, 0x01, 0x00, SegmentMasks, SegmentRed7, SegmentGreen7, SegmentBlue7 ]");
        expect(zone.diagnostics).toEqual([]);
        expect(zone.source).toContain("RingColors=[ #003F00, #003F00, #003F00, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF, #0000FF ]");
        expect(zone.source).not.toContain("LEDRingColor=");
        expect(zone.source).not.toContain("PushColor=");
    });

    test("converts SCE24 LED buttons and their binary state colors", () => {
        const conversion = convertLegacySurfaceToFormat2("Widget LEDButton1\n  Press 90 05 7f 90 05 00\n  FB_SCE24LEDButton 90 05 7f\nWidgetEnd\n", "SCE24", "Surfaces/User/sce24.txt");
        const zone = migrateLegacySce24StateColors("Zone Home\n  LEDButton1 Shift OnColor=#2f0f0000 OffColor=#00000000\nZoneEnd\n", "Zones/Home.zon");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain("Feedback State { Encoding=MIDISysEx Payload=[ 0x00, 0x02, 0x38, 0x01, 0x65, Red7, Green7, Blue7 ] }");
        expect(zone.diagnostics).toEqual([]);
        expect(zone.source).toContain("LEDButton1 Shift StateColors=[ #000000, #2F0F00 ]");
        expect(zone.source).not.toContain("OnColor=");
        expect(zone.source).not.toContain("OffColor=");
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

    test("converts legacy MCU meters to a universal meter profile and Surface initialization", () => {
        const legacySurface = `Widget Meter1
  FB_MCUVUMeter 0
WidgetEnd
Widget Meter8
  FB_MCUXTVUMeter 7
WidgetEnd
`;
        const conversion = convertLegacySurfaceToFormat2(legacySurface, "MCU meters", "Surfaces/User/mcu-meters.txt");

        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain("MeterProfile SurfaceMeter {");
        expect(conversion.source).toContain("Step Minimum=-60.3 Output=1");
        expect(conversion.source).toContain("Feedback Meter { Encoding=MIDI7 Message=[ 0xD0 ] MeterProfile=SurfaceMeter ValueBase=0x00 Combine=BitOr Refresh=Continuous RefreshIntervalMs=10 }");
        expect(conversion.source).toContain("Feedback Meter { Encoding=MIDI7 Message=[ 0xD0 ] MeterProfile=SurfaceMeter ValueBase=0x70 Combine=BitOr Refresh=Continuous RefreshIntervalMs=10 }");
        expect(conversion.source.match(/  MIDI Bytes=/g)).toHaveLength(21);
        expect(conversion.source).toContain("MIDI Bytes=[ 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x00, 0x01, 0xF7 ]");
        expect(conversion.source).toContain("MIDI Bytes=[ 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x07, 0x01, 0xF7 ]");
    });

    test("normalizes the legacy value-bar style spelling", () => {
        expect(migrateLegacyZoneSyntax("Zone Track\n  ValueBar| TrackPan BarStyle=BiPolar\nZoneEnd\n")).toContain("BarStyle=Bipolar");
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
        expect(preview.items.find((item) => item.sourcePath === "Zones/HomeZones/Home.zon")?.source).toStartWith("// @format zone 1\n// disabled binding\n");
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
        expect(preview.items.every((item) => item.kind === "surface" ? item.source.startsWith("@Meta { Version=2 Protocol=MIDI") : item.source.startsWith("// @format zone 1\n"))).toBeTrue();
        expect(preview.items.some((item) => item.sourcePath.endsWith("GoZones.zon"))).toBeFalse();
        expect(preview.items.find((item) => item.sourcePath === "Zones/GoZones/Transport.zon")?.source).toContain("Zone Transport NavType=TrackNavigator\n");
        expect(preview.dependencies).toContainEqual({ from: "Zones/HomeZones/Home.zon", matches: ["Zones/GoZones/Transport.zon"], name: "Transport", selected: true, type: "GoZone" });
    });

    test("explains when a referenced legacy zone is not selected", async () => {
        const source = await LegacyCsiSource.create(legacyRoot);
        const preview = await source.preview(await createStore(), knownActions, "FaderPortV2", true, ["Zones/HomeZones/Home.zon"]);
        const diagnostic = preview.diagnostics.find((candidate) => candidate.code === "zones.dependency.missing");

        expect(diagnostic?.message).toContain("matching legacy zone is not selected for import");
        expect(diagnostic?.line).toBe(4);
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
        const importedSurface = await readFile(path.join(productRoot, "Surfaces", "User", "faderportv2.txt"), "utf8");
        expect(importedSurface).toStartWith("@Meta { Version=2 Protocol=MIDI");
        expect(importedSurface).toContain("Widget Play {");
        expect(importedSurface).toContain("OSKLayout {");
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
