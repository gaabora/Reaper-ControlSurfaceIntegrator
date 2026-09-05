import { describe, expect, test } from "bun:test";
import { readdir, readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { actionNameSet, loadActionCatalog } from "../src/action-catalog.ts";
import { parseByPath } from "../src/formats.ts";
import { serializeDocument } from "../src/model.ts";
import { parseProductIdentity } from "../src/product-identity.ts";
import { loadSettingsSchema } from "../src/settings-schema.ts";
import { tokenizeLine } from "../src/text.ts";
import { validateDocumentSet } from "../src/validation.ts";

const editorRoot = fileURLToPath(new URL("../", import.meta.url));
const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));

async function fixturePaths(group: "invalid" | "valid"): Promise<string[]> {
    const root = path.join(editorRoot, "fixtures", group);
    return (await readdir(root)).map((name) => path.join(root, name)).sort();
}

describe("configuration formats", () => {
    test("valid fixtures round-trip without text changes", async () => {
        const catalog = await loadActionCatalog(repositoryRoot);
        const settingsSchema = await loadSettingsSchema(path.join(repositoryRoot, "Scripts", "settings_schema.conf"));
        for (const fixturePath of await fixturePaths("valid")) {
            const source = await readFile(fixturePath, "utf8");
            const document = parseByPath(source, fixturePath, actionNameSet(catalog), settingsSchema);
            expect(serializeDocument(document)).toBe(source);
            expect(document.diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
        }
    });

    test("malformed fixtures report errors without losing text", async () => {
        const catalog = await loadActionCatalog(repositoryRoot);
        const settingsSchema = await loadSettingsSchema(path.join(repositoryRoot, "Scripts", "settings_schema.conf"));
        const expectedErrorCodes = new Map([
            ["home.zon", "zone.end.missing"],
            ["hash-comment.zon", "comment.hash.unsupported"],
            ["product.conf", "product.property.integer"],
            ["single-slash.zon", "comment.single-slash.unsupported"],
            ["surface.txt", "surface.format.version"],
            ["transport.snippet", "snippet.id"],
        ]);
        for (const fixturePath of await fixturePaths("invalid")) {
            const source = await readFile(fixturePath, "utf8");
            const document = parseByPath(source, fixturePath, actionNameSet(catalog), settingsSchema);
            expect(serializeDocument(document)).toBe(source);
            expect(document.diagnostics.some((diagnostic) => diagnostic.code === expectedErrorCodes.get(path.basename(fixturePath)) && diagnostic.severity === "error")).toBeTrue();
        }
    });

    test("mixed line endings round-trip without normalization", () => {
        const source = "// @format zone 1\r\nZone Home\r\n  // keep this comment\n  Play Play\r\nZoneEnd";
        expect(serializeDocument(parseByPath(source, "Home.zon"))).toBe(source);
    });

    test("keeps a spaced property list in one token without joining positional step values", () => {
        expect(tokenizeLine("Rotary1 TrackPan RingColors=[ #003F00, #0000FF ] [ 0.5 ]")).toEqual(["Rotary1", "TrackPan", "RingColors=[ #003F00, #0000FF ]", "[", "0.5", "]"]);
        expect(tokenizeLine("Rotary1 TrackPan [0.5] RingColors=[#003F00,#0000FF]")).toEqual(["Rotary1", "TrackPan", "[", "0.5", "]", "RingColors=[ #003F00, #0000FF ]"]);
    });

    test("product identity accepts hash comments", async () => {
        const source = await readFile(path.join(repositoryRoot, "Scripts", "product_identity.conf"), "utf8");
        expect(parseProductIdentity(`# identity comment\n${source}`).productId).toBe("reacontrolsurface");
        expect(() => parseProductIdentity(`// identity comment\n${source}`)).toThrow("Invalid product identity line 1");
    });

    test("keeps OSC address tokens and rejects legacy single-slash comments", () => {
        const source = "// @format surface 1\nWidget Fader\n  X32Fader /ch/01/mix/fader\n  / disabled mapping\nWidgetEnd\n";
        const document = parseByPath(source, "surface.txt");
        expect(document.lines[2].tokens).toEqual(["X32Fader", "/ch/01/mix/fader"]);
        expect(document.diagnostics.some((diagnostic) => diagnostic.code === "comment.single-slash.unsupported" && diagnostic.line === 4 && diagnostic.severity === "error")).toBeTrue();
    });

    test("rejects hash comments in current Surface files", () => {
        const source = "// @format surface 1\n# disabled widget\nWidget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n";
        const document = parseByPath(source, "surface.txt");
        expect(document.diagnostics.some((diagnostic) => diagnostic.code === "comment.hash.unsupported" && diagnostic.line === 2 && diagnostic.severity === "error")).toBeTrue();
    });

    test("requires a positive channel count in format 2 Surface metadata", () => {
        const document = parseByPath('@Meta { Version=2 Protocol=MIDI Channels=0 Name="Invalid channels" }\n\nWidget Play {\n  Input Press { Encoding=MIDIExact On=[ 0x90, 0x5E, 0x7F ] Off=[ 0x90, 0x5E, 0x00 ] }\n}\n', "surface.txt");
        expect(document.diagnostics.some((diagnostic) => diagnostic.code === "surface.channels.value" && diagnostic.severity === "error")).toBeTrue();
    });

    test("rejects hash comments in the product configuration", () => {
        const source = "# product config comment\n";
        const document = parseByPath(source, "product.conf");
        expect(document.diagnostics.some((diagnostic) => diagnostic.code === "comment.hash.unsupported")).toBeTrue();
    });

    test("keeps exact Learn FX hash directives as zone metadata", () => {
        const source = "// @format zone 1\nZone FXWidgetLayout\nZoneEnd\n#WidgetType Fader\n#DisplayRow DisplayUpper\n#RingStyle Dot\n#DisplayFont Arial\n#SupportsColor\n";
        const document = parseByPath(source, "FXWidgetLayout.zon");
        expect(document.diagnostics.some((diagnostic) => diagnostic.code === "comment.hash.unsupported")).toBeFalse();
        expect(document.diagnostics.some((diagnostic) => diagnostic.code === "zone.line.outside")).toBeFalse();
    });

    test("OSK targets must name widgets with the required input", () => {
        const missingTarget = parseByPath("// @format surface 1\nWidget Rotary\n  Encoder b0 10 7f\nWidgetEnd\nOSKLayout Version=1\n  Row\n    Widget Rotary PressTarget=Missing\n  RowEnd\nOSKLayoutEnd\n", "surface.txt");
        expect(missingTarget.diagnostics.some((diagnostic) => diagnostic.code === "surface.layout.target.missing" && diagnostic.severity === "error")).toBeTrue();

        const wrongCapability = parseByPath("// @format surface 1\nWidget Rotary\n  Encoder b0 10 7f\nWidgetEnd\nOSKLayout Version=1\n  Row\n    Widget Rotary PressTarget=Rotary\n  RowEnd\nOSKLayoutEnd\n", "surface.txt");
        expect(wrongCapability.diagnostics.some((diagnostic) => diagnostic.code === "surface.layout.target.capability" && diagnostic.severity === "error")).toBeTrue();
    });

    test("functional snippet bindings require explicit semantic fields and valid modifiers", () => {
        const document = parseByPath("Snippet Version=1 Id=test Name=Test\n  Binding Id=play Role=Button Input=Press\n    Action NoMod+Shift Play\n  BindingEnd\nSnippetEnd\n", "test.snippet", new Set(["Play"]));
        const errorCodes = document.diagnostics.filter((diagnostic) => diagnostic.severity === "error").map((diagnostic) => diagnostic.code);
        expect(errorCodes).toContain("snippet.binding.feedback");
        expect(errorCodes).toContain("snippet.binding.required.missing");
        expect(errorCodes).toContain("snippet.action.modifiers");
    });

    test("GoZone navigation may return to an earlier zone", () => {
        const alpha = parseByPath("// @format zone 1\nZone alpha\n  Play GoZone beta\nZoneEnd\n", "/zones/alpha.zon");
        const beta = parseByPath("// @format zone 1\nZone beta\n  Play GoZone alpha\nZoneEnd\n", "/zones/beta.zon");
        expect(validateDocumentSet([alpha, beta]).some((diagnostic) => diagnostic.code === "zones.dependency.cycle")).toBeFalse();
    });

    test("structural zone dependency cycles report a stable error", () => {
        const alpha = parseByPath("// @format zone 1\nZone alpha\nIncludedZones\n  beta\nIncludedZonesEnd\nZoneEnd\n", "/zones/alpha.zon");
        const beta = parseByPath("// @format zone 1\nZone beta\nIncludedZones\n  alpha\nIncludedZonesEnd\nZoneEnd\n", "/zones/beta.zon");
        expect(validateDocumentSet([alpha, beta]).some((diagnostic) => diagnostic.code === "zones.dependency.cycle" && diagnostic.severity === "error" && diagnostic.path === "/zones/beta.zon" && diagnostic.line === 4)).toBeTrue();
    });

    test("User FX zones override same-name Vendor FX zones", () => {
        const vendor = parseByPath("// @format zone 1\nZone Compressor\n  Play GoZone MissingVendorDependency\nZoneEnd\n", "/config/Zones/Vendor/faderportv2/FX/Compressor.zon");
        const user = parseByPath("// @format zone 1\nZone Compressor\n  Play Play\nZoneEnd\n", "/config/Zones/User/faderportv2/FX/Compressor.zon");
        const diagnostics = validateDocumentSet([user, vendor]);
        expect(diagnostics.some((diagnostic) => diagnostic.code === "zones.name.duplicate")).toBeFalse();
        expect(diagnostics.some((diagnostic) => diagnostic.code === "zones.dependency.missing" && diagnostic.path === vendor.path)).toBeFalse();
    });

    test("User Main selects the whole Main layer", () => {
        const vendor = parseByPath("// @format zone 1\nZone Home\n  Play GoZone MissingVendorDependency\nZoneEnd\n", "/config/Zones/Vendor/faderportv2/Main/Home.zon");
        const user = parseByPath("// @format zone 1\nZone Home\n  Play Play\nZoneEnd\n", "/config/Zones/User/faderportv2/Main/Home.zon");
        const diagnostics = validateDocumentSet([vendor, user]);
        expect(diagnostics.some((diagnostic) => diagnostic.code === "zones.name.duplicate")).toBeFalse();
        expect(diagnostics.some((diagnostic) => diagnostic.code === "zones.dependency.missing" && diagnostic.path === vendor.path)).toBeFalse();
    });

    test("same-layer FX zone names remain duplicates", () => {
        const override = parseByPath("// @format zone 1\nZone Compressor\nZoneEnd\n", "/config/Zones/User/faderportv2/FX/Compressor.zon");
        const first = parseByPath("// @format zone 1\nZone Compressor\nZoneEnd\n", "/config/Zones/Vendor/faderportv2/FX/Compressor.zon");
        const second = parseByPath("// @format zone 1\nZone Compressor\nZoneEnd\n", "/config/Zones/Vendor/faderportv2/FX/Other.zon");
        const duplicate = validateDocumentSet([override, first, second]).find((diagnostic) => diagnostic.code === "zones.name.duplicate" && diagnostic.severity === "error");
        expect(duplicate?.line).toBe(2);
        expect(duplicate?.message).toContain(first.path);
        expect(duplicate?.related).toEqual([{ line: 2, path: first.path! }]);
    });

    test("zone IDs are independent between profiles", () => {
        const first = parseByPath("// @format zone 1\nZone Home\nZoneEnd\n", "/config/Zones/User/first/Main/Home.zon");
        const second = parseByPath("// @format zone 1\nZone Home\nZoneEnd\n", "/config/Zones/User/second/Main/Home.zon");
        expect(validateDocumentSet([first, second]).some((diagnostic) => diagnostic.code === "zones.name.duplicate")).toBeFalse();
    });

    test("runtime action catalog comes from ACTION_TYPE_LIST", async () => {
        const catalog = await loadActionCatalog(repositoryRoot);
        const names = actionNameSet(catalog);
        expect(names.has("Play")).toBeTrue();
        expect(names.has("Reaper")).toBeTrue();
        expect(catalog.find((entry) => entry.name === "TrackVolume")?.feedbackShape).toBe("Level");
        expect(catalog.find((entry) => entry.name === "TrackPan")?.feedbackShape).toBe("Centered");
        expect(catalog.find((entry) => entry.name === "TrackPanWidth")?.feedbackShape).toBe("Spread");
        expect(catalog.find((entry) => entry.name === "FXParam")?.feedbackShape).toBeUndefined();
        expect(catalog.find((entry) => entry.name === "TrackPanAutoRight")?.feedbackShape).toBeUndefined();
        expect(names.size).toBe(catalog.length);
    });
});
