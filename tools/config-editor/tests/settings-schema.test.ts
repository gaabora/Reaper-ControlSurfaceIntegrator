import { describe, expect, test } from "bun:test";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { loadSettingsSchema, parseSettingsSchema, settingDefinition } from "../src/settings-schema.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const schemaPath = path.join(repositoryRoot, "Scripts", "settings_schema.conf");

describe("settings schema", () => {
    test("loads the canonical defaults and constraints", async () => {
        const schema = await loadSettingsSchema(schemaPath);
        expect(schema.version).toBe(1);
        expect(settingDefinition(schema, "DefaultModifierMode")?.defaultValue).toBe("Latch");
        expect(settingDefinition(schema, "DefaultPseudoModifierMode")?.defaultValue).toBe("Latch");
        expect(settingDefinition(schema, "LongHoldDelayMs")?.greaterThan).toBe("HoldDelayMs");
        expect(settingDefinition(schema, "HoldRepeatIntervalMs")?.defaultValue).toBe(100);
        expect(settingDefinition(schema, "SurfaceRawInDisplay")?.type).toBe("boolean");
        expect(settingDefinition(schema, "SurfaceRawInDisplay")?.defaultValue).toBe(0);
    });

    test("rejects invalid defaults and cross-setting relationships", () => {
        expect(() => parseSettingsSchema("Version=1\nSetting=Mode Type=Enum Default=Bad Values=Latch,Hybrid Scopes=Product,Surface Category=Behavior\n")).toThrow("default is not in Values");
        expect(() => parseSettingsSchema("Version=1\nSetting=HoldDelayMs Type=Integer Default=1000 Min=50 Max=10000 Scopes=Product,Surface Category=Timing Unit=Milliseconds\nSetting=LongHoldDelayMs Type=Integer Default=500 Min=100 Max=30000 Scopes=Product,Surface Category=Timing Unit=Milliseconds GreaterThan=HoldDelayMs\n")).toThrow("default must be greater than HoldDelayMs");
        expect(() => parseSettingsSchema("Version=1\nSetting=SurfaceDisplay Type=Boolean Default=2 Scopes=Product Category=Logging\n")).toThrow("default must be 0 or 1");
    });
});
