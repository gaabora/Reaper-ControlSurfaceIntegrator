import { describe, expect, test } from "bun:test";
import { convertLegacyProductConfig } from "../src/legacy-product-config.ts";
import { parseByPath } from "../src/formats.ts";

describe("legacy CSI.ini conversion", () => {
    test("converts devices, pages, assignments, and links", () => {
        const legacy = "Version=7.0\n\nSurfaceType=MIDI SurfaceName=main SurfaceChannelCount=8 MidiInput=0 MidiOutput=1 MIDISurfaceRefreshRate=15 MaxMIDIMesssagesPerRun=200\nSurfaceType=MIDI SurfaceName=extender SurfaceChannelCount=8 MidiInput=2 MidiOutput=3 MIDISurfaceRefreshRate=15 MaxMIDIMesssagesPerRun=200\n\nPageName=Home PageFollowsMCP=Yes SynchPages=Yes ScrollLink=No ScrollSynch=No\n  Surface=main SurfaceFolder=main ZoneFolder=main FXZoneFolder=main StartChannel=0\n  Surface=extender SurfaceFolder=extender ZoneFolder=extender FXZoneFolder=extender StartChannel=8\n  Broadcaster=main\n    Listener=extender GoHome=Yes Modifiers=Yes FXMenu=No\n";
        const conversion = convertLegacyProductConfig(legacy, "CSI.ini");
        expect(conversion.source).toContain("Device main {");
        expect(conversion.source).toContain("Page Home {");
        expect(conversion.source).toContain("Share=[Home, Modifiers]");
        expect(parseByPath(conversion.source, "ReaControlSurface.conf").diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
    });

    test("reports unsupported device types", () => {
        const conversion = convertLegacyProductConfig("SurfaceType=Unknown SurfaceName=test\n", "CSI.ini");
        expect(conversion.diagnostics.some((diagnostic) => diagnostic.code === "legacy.config.device.type" && diagnostic.severity === "error")).toBeTrue();
    });

    test("converts an X32 device to the current OSC properties", () => {
        const legacy = "SurfaceType=OSCX32 SurfaceName=x32 ReceiveOnPort=8000 TransmitToPort=9000 TransmitToIPAddress=192.168.1.20 MaxPacketsPerRun=200\nPageName=Home\nSurface=x32 SurfaceFolder=X32 ZoneFolder=X32 FXZoneFolder=X32 StartChannel=0\n";
        const conversion = convertLegacyProductConfig(legacy, "CSI.ini");
        expect(conversion.source).toContain("Protocol=X32");
        expect(conversion.source).toContain("Address=192.168.1.20");
        expect(conversion.source).toContain("Template=x32");
        expect(parseByPath(conversion.source, "ReaControlSurface.conf").diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
    });
});
