import { describe, expect, test } from "bun:test";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { parseByPath } from "../src/formats.ts";
import { loadSurfaceIoSchema } from "../src/surface-io-schema.ts";
import { surfaceDesignerModel } from "../src/ui/surface-designer.js";

const repositoryRoot = fileURLToPath(new URL("../../../", import.meta.url));
const schema = await loadSurfaceIoSchema(path.join(repositoryRoot, "Scripts", "surface_io_schema.conf"));
const schemaRecord = (lineType: "Spacer" | "Widget") => {
    const record = schema.records.find((candidate) => candidate.kind === "SurfaceLine" && candidate.properties.get("ParentBlock") === "OSKRow" && candidate.properties.get("SurfaceLine") === lineType);
    if (!record) throw new Error(`Missing ${lineType} OSKRow schema record`);
    return { ...record, properties: Object.fromEntries(record.properties) };
};

const source = `@Meta { Version=2 Protocol=MIDI Channels=1 Name="Designer test" }
// Definition comment
Widget Fader {
  Input Value { Encoding=MIDI14 Status=0xE0 }
  Input Touch { Encoding=MIDIExact On=[ 0x90, 0x68, 0x7F ] Off=[ 0x90, 0x68, 0x00 ] }
}
Widget Encoder {
  Input Encoder { Encoding=MIDI7 Message=[ 0xB0, 0x10 ] Mode=Relative7Bit }
}
Widget Push {
  Input Press { Encoding=MIDIExact On=[ 0x90, 0x20, 0x7F ] Off=[ 0x90, 0x20, 0x00 ] }
}
Widget Stop {
  Input Press { Encoding=MIDIExact On=[ 0x90, 0x5D, 0x7F ] Off=[ 0x90, 0x5D, 0x00 ] }
}
OSKLayout {
  Row {
    Widget Fader Height=7.5 ValueTarget=Fader TouchTarget=Fader // keep fader comment
    Widget Encoder Shape=Round Group=Main PressTarget=Push ScrollTarget=Encoder RotaryStyle=wiper
    Spacer
    Widget Stop Shape=Round
  }
}
`;

function browserDocument() {
    const parsed = parseByPath(source, "Surfaces/User/designer-test.txt");
    return { ...parsed, semantic: JSON.parse(JSON.stringify(parsed.semantic, (_key, value) => value instanceof Map ? Object.fromEntries(value) : value)) } as any;
}

describe("Surface Designer model", () => {
    test("maps Surface text to OSK rows, groups, roles, and Lua geometry", () => {
        const document = browserDocument();
        const row = document.semantic.oskLayout.rows[0];
        const [fader, encoder, spacer, roundButton] = row.cells;

        expect(row.cells).toHaveLength(4);
        expect(surfaceDesignerModel.layoutCellRole(document, fader)).toBe("fader");
        expect(surfaceDesignerModel.layoutCellRole(document, encoder)).toBe("rotary");
        expect(surfaceDesignerModel.layoutCellRole(document, roundButton)).toBe("round");
        expect(surfaceDesignerModel.layoutCellMetrics(document, fader)).toMatchObject({ heightFactor: 1, rowSpan: 8, widthFactor: 1 });
        expect(surfaceDesignerModel.layoutCellMetrics(document, spacer)).toMatchObject({ rowSpan: 1, widthFactor: 0.5 });
        expect(surfaceDesignerModel.groupedRowCells({ cells: [encoder, { ...roundButton, properties: { ...roundButton.properties, Group: "main" } }] })).toHaveLength(1);
    });

    test("offers only applicable schema fields and compatible target widgets", () => {
        const document = browserDocument();
        const [fader, encoder, spacer, roundButton] = document.semantic.oskLayout.rows[0].cells;
        const propertiesFor = (cell: any, lineType: "Spacer" | "Widget" = "Widget") => surfaceDesignerModel.inspectorProperties(document, cell, schemaRecord(lineType));
        const byName = (properties: Array<{ choices?: string[]; propertyName: string }>) => new Map(properties.map((property) => [property.propertyName, property]));

        const faderProperties = byName(propertiesFor(fader));
        expect(faderProperties.has("RotaryStyle")).toBeFalse();
        expect(faderProperties.get("ValueTarget")?.choices).toEqual(["Fader"]);
        expect(faderProperties.get("TouchTarget")?.choices).toEqual(["Fader"]);

        const rotaryProperties = byName(propertiesFor(encoder));
        expect(rotaryProperties.get("RotaryStyle")?.choices).toEqual(["wiper", "dot"]);
        expect(rotaryProperties.get("PressTarget")?.choices).toEqual(["Push", "Stop"]);
        expect(rotaryProperties.get("ScrollTarget")?.choices).toEqual(["Encoder"]);
        expect(rotaryProperties.has("ValueTarget")).toBeFalse();

        const buttonProperties = byName(propertiesFor(roundButton));
        expect(buttonProperties.get("Shape")?.choices).toContain("Round");
        expect(buttonProperties.has("RotaryStyle")).toBeFalse();
        expect(propertiesFor(spacer, "Spacer").map((property: { propertyName: string }) => property.propertyName)).toEqual(["Width"]);
    });

    test("changes one layout line without losing comments, unknown properties, or surrounding source", () => {
        const document = browserDocument();
        const fader = document.semantic.oskLayout.rows[0].cells[0];
        const current = { document, source, surfaceIoSchema: { records: [schemaRecord("Widget")] } };
        const lineText = surfaceDesignerModel.cellLineText(current, fader, { ...fader.properties, FutureVisualOption: "KeepMe", Width: "1.25" });
        const changed = surfaceDesignerModel.replaceLine(source, fader.line, lineText);

        expect(lineText).toContain("Width=1.25");
        expect(lineText).toContain("FutureVisualOption=KeepMe");
        expect(lineText).toEndWith(" // keep fader comment");
        expect(changed.replace(lineText, source.split("\n")[fader.line - 1])).toBe(source);
    });

    test("moves grouped source lines without regenerating other text", () => {
        const original = "Row {\n  // before\n  Widget A Group=Pair\n  Widget APush Group=Pair // push\n  Widget B\n}\n";
        const changed = surfaceDesignerModel.moveLines(original, [3, 4], 5);

        expect(changed).toBe("Row {\n  // before\n  Widget A Group=Pair\n  Widget APush Group=Pair // push\n  Widget B\n}\n");
        expect(surfaceDesignerModel.moveLines(original, [3, 4], 6)).toBe("Row {\n  // before\n  Widget B\n  Widget A Group=Pair\n  Widget APush Group=Pair // push\n}\n");
    });

    test("blocks visual edits for Vendor and invalid documents", () => {
        const document = browserDocument();
        expect(surfaceDesignerModel.surfaceDesignerCanEdit({ document, writable: true })).toBeTrue();
        expect(surfaceDesignerModel.surfaceDesignerCanEdit({ document, writable: false })).toBeFalse();
        expect(surfaceDesignerModel.surfaceDesignerCanEdit({ document: { ...document, diagnostics: [{ severity: "error" }] }, writable: true })).toBeFalse();
    });
});
