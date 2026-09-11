import { describe, expect, test } from "bun:test";
import { parseByPath } from "../src/formats.ts";
import { convertLegacyZoneToFormat2 } from "../src/legacy-zone-format2.ts";
import { validateDocumentSet } from "../src/validation.ts";

const profileRoot = "Zones/User/faderportv2/Main/";

describe("legacy Zone readiness", () => {
    test("converts a matching bank target and preserves amount and feedback", () => {
        const conversion = convertLegacyZoneToFormat2("Zone SelectedTrackSend\nPrev Bank SelectedTrackSend -1 Blink=1000\nNext Bank SelectedTrackSend 1\nZoneEnd\n", { profile: "Main", targetPath: profileRoot + "SelectedTrackSend.zon" });
        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).toContain("Prev Bank -1 Blink=1000");
        expect(conversion.source).toContain("Next Bank 1");
        expect(parseByPath(conversion.source, profileRoot + "SelectedTrackSend.zon").diagnostics).toEqual([]);
    });

    test("blocks a layer bank that would change its target", () => {
        const conversion = convertLegacyZoneToFormat2("Zone LinkLock\nPrev Bank SelectedTrackFXMenu -1\nZoneEnd\n", { bankContexts: ["Home"], isLayer: true, profile: "Main", targetPath: profileRoot + "LinkLock.zon" });
        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.zone.bank.context", severity: "error", line: 3 }));
        expect(conversion.source).toContain("Bank SelectedTrackFXMenu -1");
    });

    test("requires every parent of a reusable layer to have the matching bank context", () => {
        const source = "Zone SendLayer\nPrev Bank SelectedTrackSend -1\nZoneEnd\n";
        const options = { isLayer: true, profile: "Main" as const, targetPath: profileRoot + "SendLayer.zon" };
        expect(convertLegacyZoneToFormat2(source, { ...options, bankContexts: ["SelectedTrackSend"] }).diagnostics).toEqual([]);
        expect(convertLegacyZoneToFormat2(source, { ...options, bankContexts: ["SelectedTrackSend", "Home"] }).diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.zone.bank.context" }));
    });

    test("converts Home navigation and keeps comments from empty relation blocks", () => {
        const conversion = convertLegacyZoneToFormat2("Zone Home\nIncludedZones\n// add zones here\nIncludedZonesEnd\nSubZones\n// add layers here\nSubZonesEnd\nButton GoZone Home\nZoneEnd\n", { profile: "Main", targetPath: profileRoot + "Home.zon" });
        expect(conversion.diagnostics).toEqual([]);
        expect(conversion.source).not.toContain("IncludedZones {");
        expect(conversion.source).not.toContain("ZoneLayers {");
        expect(conversion.source).toContain("// add zones here");
        expect(conversion.source).toContain("// add layers here");
        expect(conversion.source).toContain("Button GoHome");
    });

    test("does not guess how an independent Metronome zone should exit", () => {
        const source = "Zone Metronome\nClick LeaveSubZone Blink=1000\nZoneEnd\n";
        const options = { profile: "Main" as const, targetPath: profileRoot + "Metronome.zon" };
        const independent = convertLegacyZoneToFormat2(source, options);
        expect(independent.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.zone.exit.context", severity: "error" }));
        const layer = convertLegacyZoneToFormat2(source, { ...options, isLayer: true });
        expect(layer.diagnostics).toEqual([]);
        expect(layer.source).toContain("Click ExitZoneLayer Blink=1000");
    });

    test("an unmatched relation end cannot remove the generated metadata", () => {
        const conversion = convertLegacyZoneToFormat2("Zone Home\nIncludedZonesEnd\nZoneEnd\n", { profile: "Main", targetPath: profileRoot + "Home.zon" });
        expect(conversion.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.zone.reference.end", severity: "error" }));
        expect(conversion.source).toStartWith("@Meta { Version=2 Role=Home }");
    });

    test.each(["Bank SelectedTrackSend -1", "Bank", "Bank 1.0", "Bank \"1\"", "Bank 1 2"])("rejects runtime-invalid arguments: %s", (action) => {
        const document = parseByPath(`@Meta { Version=2 Target=SelectedTrack BankTarget=Sends }\nPrev ${action}\n`, profileRoot + "SelectedTrackSend.zon");
        expect(document.diagnostics).toContainEqual(expect.objectContaining({ code: "format2.zone.action.bank-amount", line: 2 }));
    });

    test("checks empty inline and multiline relations and lifecycle actions", () => {
        const document = parseByPath('@Meta { Version=2 Role=Home }\nIncludedZones {}\nZoneLayers {\n// empty\n}\nOn SurfaceInitialization {\nBank "1"\nExitZoneLayer\n}\n', profileRoot + "Home.zon");
        expect(document.diagnostics.filter((diagnostic) => diagnostic.code === "format2.zone.reference.required").map((diagnostic) => diagnostic.line)).toEqual([2, 3]);
        expect(document.diagnostics).toContainEqual(expect.objectContaining({ code: "format2.zone.action.bank-amount", line: 7 }));
        expect(document.diagnostics).toContainEqual(expect.objectContaining({ code: "format2.zone.action.layer-only", line: 8 }));
    });

    test("checks lifecycle navigation and Home targets across files", () => {
        const home = parseByPath("@Meta { Version=2 Role=Home }\nOn SurfaceInitialization {\nGoZone SelectedTracks\n}\n", profileRoot + "Home.zon");
        const tracks = parseByPath("@Meta { Version=2 Target=SelectedTracks }\nPrev GoZone Home\n", profileRoot + "SelectedTracks.zon");
        expect(validateDocumentSet([home, tracks], { completeProfiles: true })).toContainEqual(expect.objectContaining({ code: "format2.zone-profile.navigation.home", path: tracks.path, line: 2 }));
        expect(validateDocumentSet([home], { completeProfiles: true })).toContainEqual(expect.objectContaining({ code: "zones.dependency.missing", severity: "warning", line: 3 }));
    });

    test("a broken destination is unavailable even when its name exists", () => {
        const home = parseByPath("@Meta { Version=2 Role=Home }\nButton GoZone SelectedTracks\n", profileRoot + "Home.zon");
        const tracks = parseByPath("@Meta { Version=2 Target=SelectedTracks }\nPrev Bank SelectedTracks -8\n", profileRoot + "SelectedTracks.zon");
        expect(validateDocumentSet([home, tracks], { completeProfiles: true })).toContainEqual(expect.objectContaining({ code: "zones.dependency.missing", severity: "warning", path: home.path }));
    });

    test("reports a missing Home only when validating a complete profile", () => {
        const transport = parseByPath("@Meta { Version=2 }\nPlay Play\n", profileRoot + "Transport.zon");
        expect(validateDocumentSet([transport])).toEqual([]);
        expect(validateDocumentSet([transport], { completeProfiles: true })).toContainEqual(expect.objectContaining({ code: "format2.zone-profile.home.missing" }));
    });

    test("reports legacy Vendor content instead of approving a mixed runtime profile", () => {
        const home = parseByPath("@Meta { Version=2 Role=Home }\nPlay Play\n", profileRoot + "Home.zon");
        const vendor = parseByPath("Zone Home\nPlay Play\nZoneEnd\n", "Zones/Vendor/faderportv2/Main/Home.zon");
        expect(validateDocumentSet([vendor, home], { completeProfiles: true })).toContainEqual(expect.objectContaining({ code: "format2.zone-profile.legacy", path: vendor.path }));
    });
});
