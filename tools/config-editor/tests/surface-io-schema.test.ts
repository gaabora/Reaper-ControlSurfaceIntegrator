import { describe, expect, test } from "bun:test";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { analyzeLegacySurfaceCoverage } from "../src/legacy-surface-coverage.ts";
import { findSurfaceIoRepresentation, loadSurfaceIoSchema, parseSurfaceIoSchema } from "../src/surface-io-schema.ts";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const schemaPath = path.join(repositoryRoot, "Scripts", "surface_io_schema.conf");

describe("Surface I/O schema", () => {
    test("loads the canonical primitive and representation catalog", async () => {
        const schema = await loadSurfaceIoSchema(schemaPath);

        expect(schema.version).toBe(1);
        expect(findSurfaceIoRepresentation(schema, "Feedback", "Ring", "MIDI", "MIDI7")?.required).toEqual(["Message", "RingProfile", "StyleTarget"]);
        expect(findSurfaceIoRepresentation(schema, "Input", "Encoder", "OSC", "OSCFloat")?.nested).toEqual(["Acknowledge"]);
    });

    test("rejects a representation whose primitive is not declared", () => {
        expect(() => parseSurfaceIoSchema("Version=1\nPrimitive=Press Direction=Input Capabilities=Press\nRepresentation=Missing Direction=Input Protocol=MIDI Encoding=MIDIExact Required=On Optional=None Nested=None Rules=On:MIDIMessage3\n")).toThrow("unknown primitive");
    });

    test("reports supported and unresolved public legacy processors", async () => {
        const schema = await loadSurfaceIoSchema(schemaPath);
        const report = await analyzeLegacySurfaceCoverage(path.join(repositoryRoot, "CSI", "Surfaces"), schema);

        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_encoder")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_processor")?.status).toBe("planned");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcudisplayupper")?.status).toBe("unsupported");
        expect(report.processors.some((entry) => entry.processor.toLowerCase() === "widget")).toBeFalse();
        expect(report.processors.some((entry) => entry.status === "invalid-target")).toBeFalse();
        expect(report.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.surface.widget.end.invalid", line: 842, path: "SCE24/Surface.txt" }));
    });
});
