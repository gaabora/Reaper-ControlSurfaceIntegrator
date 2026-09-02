import { describe, expect, test } from "bun:test";
import { mkdtemp, mkdir, rm, writeFile } from "node:fs/promises";
import os from "node:os";
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
        expect(findSurfaceIoRepresentation(schema, "Feedback", "Color", "OSC", "OSCString")?.required).toEqual(["Address", "Format"]);
    });

    test("rejects a representation whose primitive is not declared", () => {
        expect(() => parseSurfaceIoSchema("Version=1\nPrimitive=Press Direction=Input Capabilities=Press\nRepresentation=Missing Direction=Input Protocol=MIDI Encoding=MIDIExact Required=On Optional=None Nested=None Rules=On:MIDIMessage3\n")).toThrow("unknown primitive");
    });

    test("reports supported and unresolved public legacy processors", async () => {
        const schema = await loadSurfaceIoSchema(schemaPath);
        const report = await analyzeLegacySurfaceCoverage(path.join(repositoryRoot, "CSI", "Surfaces"), schema);

        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_encoder")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_processor")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "control")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_faderporttwostatergb")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_faderportvaluebar")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mft_rgb")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcudisplayupper")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcudisplaylower")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcuxtdisplayupper")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcuxtdisplaylower")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_c4displayupper")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_c4displaylower")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcuvumeter")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_mcuxtvumeter")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_sce24encodertext")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_sce24oledbutton")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_sce24encoder")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_sce24ledbutton")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_fp16scribbleline1")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_fp16scribbleline4")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_fpvumeter")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_icondisplay2upper")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_icondisplay2lower")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_asparionrgb")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_asparionencoder")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_aspariondisplayupper")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_aspariondisplaylower")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_aspariondisplayencoder")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_asparionvumeterl")?.status).toBe("supported");
        expect(report.processors.find((entry) => entry.processor.toLowerCase() === "fb_asparionvumeterr")?.status).toBe("supported");
        expect(report.processors.some((entry) => entry.processor.toLowerCase() === "widget")).toBeFalse();
        expect(report.processors.some((entry) => entry.status === "invalid-target")).toBeFalse();
        expect(report.diagnostics).toEqual([]);
    });

    test("reports malformed Widget boundaries separately from processors", async () => {
        const temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "surface-coverage-"));
        try {
            const surfaceRoot = path.join(temporaryRoot, "Broken");
            await mkdir(surfaceRoot);
            await writeFile(path.join(surfaceRoot, "Surface.txt"), "Widget Broken\n  Press 90 10 7f\nWidgetEndWidget\n", "utf8");
            const report = await analyzeLegacySurfaceCoverage(temporaryRoot, await loadSurfaceIoSchema(schemaPath));

            expect(report.processors.map((entry) => entry.processor)).toEqual(["Press"]);
            expect(report.diagnostics).toContainEqual(expect.objectContaining({ code: "legacy.surface.widget.end.invalid", line: 3, path: "Broken/Surface.txt" }));
        } finally {
            await rm(temporaryRoot, { force: true, recursive: true });
        }
    });
});
