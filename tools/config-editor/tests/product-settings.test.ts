import { describe, expect, test } from "bun:test";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { parseProductConfig } from "../src/product-config.ts";
import { loadSettingsSchema } from "../src/settings-schema.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const schema = await loadSettingsSchema(path.join(repositoryRoot, "Scripts", "settings_schema.conf"));

function errorCodes(source: string): string[] {
    return parseProductConfig(source, "ReaControlSurface.conf", schema).diagnostics.filter((diagnostic) => diagnostic.severity === "error").map((diagnostic) => diagnostic.code);
}

function productConfig(productSettings = "", deviceSettings = ""): string {
    const lines: string[] = [];
    if (productSettings) lines.push("Settings {", productSettings, "}", "");
    lines.push("Device fp2 {", "  Type=MIDI", "  Channels=1", "  Input=0", "  Output=0");
    if (deviceSettings) lines.push("", "  Settings {", deviceSettings, "  }");
    lines.push("}", "", "Page Home {", "  Surface fp2 {", "    Device=fp2", "    Template=faderportv2", "  }", "}", "");
    return lines.join("\n");
}

describe("product settings", () => {
    test("accepts Product settings and Device overrides", () => {
        expect(errorCodes(productConfig("  DefaultModifierMode=Latch\n  HoldDelayMs=1000\n  LongHoldDelayMs=2000\n  SurfaceInDisplay=true", "    HoldDelayMs=750\n    LongHoldDelayMs=1500"))).toEqual([]);
    });

    test("rejects duplicate, invalid, and contradictory values", () => {
        expect(errorCodes(productConfig("  HoldDelayMs=1000\n  HoldDelayMs=1200"))).toContain("product.property.duplicate");
        expect(errorCodes(productConfig("  DefaultModifierMode=Sticky"))).toContain("product.setting.enum");
        expect(errorCodes(productConfig("  HoldDelayMs=10"))).toContain("product.setting.range");
        expect(errorCodes(productConfig("  SurfaceInDisplay=Yes"))).toContain("product.setting.boolean");
        expect(errorCodes(productConfig("  HoldDelayMs=1000\n  LongHoldDelayMs=500"))).toContain("product.setting.relationship");
    });

    test("validates a Device relationship against inherited Product values", () => {
        expect(errorCodes(productConfig("  HoldDelayMs=1500\n  LongHoldDelayMs=2500", "    LongHoldDelayMs=1200"))).toContain("product.setting.relationship");
    });

    test("uses compiled defaults after an invalid Product scope", () => {
        const codes = errorCodes(productConfig("  UnknownSetting=1\n  HoldDelayMs=1500\n  LongHoldDelayMs=2500", "    LongHoldDelayMs=1200"));
        expect(codes).toContain("product.setting.unknown");
        expect(codes).not.toContain("product.setting.relationship");
    });

    test("reports an unknown Device override as a setting error", () => {
        expect(errorCodes(productConfig("", "    UnknownDelayMs=500"))).toContain("product.setting.unknown");
    });
});
