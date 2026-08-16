import { describe, expect, test } from "bun:test";
import { parseByPath } from "../src/formats.ts";
import { applyQuickFix, diagnosticWithQuickFixes, diagnosticsWithQuickFixes, QuickFixError } from "../src/quick-fixes.ts";
import { validateDocumentSet } from "../src/validation.ts";

const knownActions = new Set(["GoZone", "Play"]);

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

    test("converts an unsupported single-slash comment without saving", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "// @format zone 1\nZone Home\n  / disabled binding\n  Play Play\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "comment.single-slash.unsupported");

        expect(diagnostic?.fixes).toEqual([{ id: "comment.single-slash.convert", label: "Convert to // comment" }]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: { id: diagnostic!.fixes![0].id } });
        expect(result.source).toContain("\n  // disabled binding\n");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "comment.single-slash.unsupported")).toBeFalse();
    });

    test("converts an unsupported hash comment without saving", () => {
        const relativePath = "Zones/User/test/Main/Home.zon";
        const source = "// @format zone 1\nZone Home\n  # disabled binding\n  Play Play\nZoneEnd\n";
        const document = parseByPath(source, relativePath, knownActions);
        const diagnostic = diagnosticsWithQuickFixes(document, knownActions, true).find((candidate) => candidate.code === "comment.hash.unsupported");

        expect(diagnostic?.fixes).toEqual([{ id: "comment.hash.convert", label: "Convert to // comment" }]);
        const result = applyQuickFix(source, relativePath, knownActions, { diagnostic: { code: diagnostic!.code, line: diagnostic!.line, message: diagnostic!.message }, fix: { id: diagnostic!.fixes![0].id } });
        expect(result.source).toContain("\n  // disabled binding\n");
        expect(result.document.diagnostics.some((candidate) => candidate.code === "comment.hash.unsupported")).toBeFalse();
    });

    test("comments out the exact dependency line that closes a zone cycle", () => {
        const alphaPath = "Zones/User/test/Main/Alpha.zon";
        const betaPath = "Zones/User/test/Main/Beta.zon";
        const alpha = parseByPath("// @format zone 1\nZone Alpha\n  Play GoZone Beta\nZoneEnd\n", alphaPath, knownActions);
        const betaSource = "// @format zone 1\nZone Beta\n  Play GoZone Alpha\nZoneEnd\n";
        const beta = parseByPath(betaSource, betaPath, knownActions);
        const cycle = validateDocumentSet([alpha, beta]).find((diagnostic) => diagnostic.code === "zones.dependency.cycle")!;
        const diagnostic = diagnosticWithQuickFixes(beta, cycle, knownActions, true);

        expect(diagnostic.path).toBe(betaPath);
        expect(diagnostic.line).toBe(3);
        expect(diagnostic.fixes).toEqual([{ data: { dependency: "Alpha" }, id: "zones.dependency.cycle.comment-out", label: "Comment out dependency on Alpha" }]);
        const result = applyQuickFix(betaSource, betaPath, knownActions, { diagnostic: { code: diagnostic.code, line: diagnostic.line, message: diagnostic.message }, fix: diagnostic.fixes![0] });
        expect(result.source).toContain("\n  // Play GoZone Alpha\n");
    });
});
