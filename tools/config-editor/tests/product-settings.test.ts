import { describe, expect, test } from "bun:test";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { parseProductConfig } from "../src/product-config.ts";
import { loadSettingsSchema } from "../src/settings-schema.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const schema = await loadSettingsSchema(path.join(repositoryRoot, "Scripts", "settings_schema.conf"));

function errorCodes(source: string): string[] {
    return parseProductConfig(source, "ReaControlSurface.ini", schema).diagnostics.filter((diagnostic) => diagnostic.severity === "error").map((diagnostic) => diagnostic.code);
}

describe("product settings", () => {
    test("accepts Product settings and Surface overrides", () => {
        const source = "Version=7.0\nSettings DefaultModifierMode=Latch HoldDelayMs=1000 LongHoldDelayMs=2000 SurfaceInDisplay=1\nPageName=Home PageFollowsMCP=No SynchPages=No ScrollLink=No ScrollSynch=No\nSurface=fp2 SurfaceFolder=faderportv2 ZoneFolder=faderportv2 FXZoneFolder=faderportv2 StartChannel=0 HoldDelayMs=750 LongHoldDelayMs=1500\n";
        expect(errorCodes(source)).toEqual([]);
    });

    test("rejects duplicate, invalid, and contradictory values", () => {
        const duplicate = errorCodes("Version=7.0\nSettings HoldDelayMs=1000\nSettings HoldDelayMs=1200\n");
        expect(duplicate).toContain("product.setting.duplicate");
        const invalidEnum = errorCodes("Version=7.0\nSettings DefaultModifierMode=Sticky\n");
        expect(invalidEnum).toContain("product.setting.enum");
        const invalidRange = errorCodes("Version=7.0\nSettings HoldDelayMs=10\n");
        expect(invalidRange).toContain("product.setting.range");
        const invalidBoolean = errorCodes("Version=7.0\nSettings SurfaceInDisplay=Yes\n");
        expect(invalidBoolean).toContain("product.setting.boolean");
        const invalidRelationship = errorCodes("Version=7.0\nSettings HoldDelayMs=1000 LongHoldDelayMs=500\n");
        expect(invalidRelationship).toContain("product.setting.relationship");
    });

    test("validates a Surface relationship against inherited Product values", () => {
        const source = "Version=7.0\nSettings HoldDelayMs=1500 LongHoldDelayMs=2500\nPageName=Home PageFollowsMCP=No SynchPages=No ScrollLink=No ScrollSynch=No\nSurface=fp2 SurfaceFolder=faderportv2 ZoneFolder=faderportv2 FXZoneFolder=faderportv2 StartChannel=0 LongHoldDelayMs=1200\n";
        expect(errorCodes(source)).toContain("product.setting.relationship");
    });

    test("uses compiled defaults after an invalid Product scope", () => {
        const source = "Version=7.0\nSettings UnknownSetting=1 HoldDelayMs=1500 LongHoldDelayMs=2500\nPageName=Home PageFollowsMCP=No SynchPages=No ScrollLink=No ScrollSynch=No\nSurface=fp2 SurfaceFolder=faderportv2 ZoneFolder=faderportv2 FXZoneFolder=faderportv2 StartChannel=0 LongHoldDelayMs=1200\n";
        const codes = errorCodes(source);
        expect(codes).toContain("product.setting.unknown");
        expect(codes).not.toContain("product.setting.relationship");
    });

    test("reports an unknown Surface override as a setting error", () => {
        const source = "Version=7.0\nPageName=Home PageFollowsMCP=No SynchPages=No ScrollLink=No ScrollSynch=No\nSurface=fp2 SurfaceFolder=faderportv2 ZoneFolder=faderportv2 FXZoneFolder=faderportv2 StartChannel=0 UnknownDelayMs=500\n";
        const codes = errorCodes(source);
        expect(codes).toContain("product.setting.unknown");
        expect(codes).not.toContain("product.surface.properties");
    });
});
