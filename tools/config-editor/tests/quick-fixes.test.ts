import { describe, expect, test } from "bun:test";
import { parseByPath } from "../src/formats.ts";
import { applyQuickFix, diagnosticsWithQuickFixes, QuickFixError } from "../src/quick-fixes.ts";

const knownActions = new Set(["Play"]);

describe("diagnostic quick fix registry", () => {
    test("offers and applies the zone format marker fix without saving", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "\uFEFFZone Home\r\n  Play Play\r\nZoneEnd\r\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "zone.format.missing");

        expect(diagnostic?.fixes).toEqual([{ id: "zone.format.add", label: "Add // @format zone 1" }]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: { id: diagnostic!.fixes![0].id } });
        expect(result.source).toStartWith("\uFEFF// @format zone 1\r\nZone Home");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "zone.format.missing")).toBeFalse();
    });

    test("does not offer edits for a read-only document and rejects stale diagnostics", () => {
        const relativePath = "Zones/Vendor/test/Main/Home.zon";
        const source = "Zone Home\n  Play Play\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        expect(diagnosticsWithQuickFixes(document, knownActions, false).some((diagnostic) => diagnostic.fixes?.length)).toBeFalse();
        expect(() => applyQuickFix("// @format zone 1\n" + source, relativePath, knownActions, { diagnostic: { code: "zone.format.missing", message: "Zone has no // @format zone 1 marker" }, fix: { id: "zone.format.add" } })).toThrow(QuickFixError);
    });
});
