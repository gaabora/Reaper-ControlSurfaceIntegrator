import { describe, expect, test } from "bun:test";
import { readdir, readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { actionNameSet, loadActionCatalog } from "../src/action-catalog.ts";
import { parseByPath } from "../src/formats.ts";
import { serializeDocument } from "../src/model.ts";
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
        for (const fixturePath of await fixturePaths("valid")) {
            const source = await readFile(fixturePath, "utf8");
            const document = parseByPath(source, fixturePath, actionNameSet(catalog));
            expect(serializeDocument(document)).toBe(source);
            expect(document.diagnostics.filter((diagnostic) => diagnostic.severity === "error")).toEqual([]);
        }
    });

    test("malformed fixtures report errors without losing text", async () => {
        const catalog = await loadActionCatalog(repositoryRoot);
        const expectedErrorCodes = new Map([
            ["home.zon", "zone.end.missing"],
            ["product.ini", "product.version.unsupported"],
            ["surface.txt", "surface.format.version"],
            ["transport.snippet", "snippet.id"],
        ]);
        for (const fixturePath of await fixturePaths("invalid")) {
            const source = await readFile(fixturePath, "utf8");
            const document = parseByPath(source, fixturePath, actionNameSet(catalog));
            expect(serializeDocument(document)).toBe(source);
            expect(document.diagnostics.some((diagnostic) => diagnostic.code === expectedErrorCodes.get(path.basename(fixturePath)) && diagnostic.severity === "error")).toBeTrue();
        }
    });

    test("mixed line endings round-trip without normalization", () => {
        const source = "// @format zone 1\r\nZone Home\r\n  // keep this comment\n  Play Play\r\nZoneEnd";
        expect(serializeDocument(parseByPath(source, "Home.zon"))).toBe(source);
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

    test("zone dependency cycles report a stable error", () => {
        const alpha = parseByPath("// @format zone 1\nZone alpha\n  Play GoZone beta\nZoneEnd\n", "/zones/alpha.zon");
        const beta = parseByPath("// @format zone 1\nZone beta\n  Play GoZone alpha\nZoneEnd\n", "/zones/beta.zon");
        expect(validateDocumentSet([alpha, beta]).some((diagnostic) => diagnostic.code === "zones.dependency.cycle" && diagnostic.severity === "error")).toBeTrue();
    });

    test("runtime action catalog comes from ACTION_TYPE_LIST", async () => {
        const catalog = await loadActionCatalog(repositoryRoot);
        const names = actionNameSet(catalog);
        expect(names.has("Play")).toBeTrue();
        expect(names.has("Reaper")).toBeTrue();
        expect(names.size).toBe(catalog.length);
    });
});
