import { describe, expect, test } from "bun:test";
import { createEditorHtml, EDITOR_CSS, EDITOR_JAVASCRIPT } from "../src/ui.ts";

describe("browser UI bindings", () => {
    test("declares every used element and references an existing HTML ID", () => {
        const html = createEditorHtml("Test Product");
        const htmlIds = new Set([...html.matchAll(/\bid="([^"]+)"/g)].map((match) => match[1]));
        const declarations = new Map([...EDITOR_JAVASCRIPT.matchAll(/^\s+([A-Za-z][A-Za-z0-9]*): requiredElement\("([^"]+)"\),$/gm)].map((match) => [match[1], match[2]]));
        const usedNames = new Set([...EDITOR_JAVASCRIPT.matchAll(/\belements\.([A-Za-z][A-Za-z0-9]*)/g)].map((match) => match[1]));

        expect(declarations.size).toBeGreaterThan(0);
        expect([...usedNames].filter((name) => !declarations.has(name))).toEqual([]);
        expect([...declarations.values()].filter((id) => !htmlIds.has(id))).toEqual([]);
        expect(html).not.toContain("{{");
        expect(html).toContain('<link rel="stylesheet" href="/app.css">');
        expect(EDITOR_CSS).toContain(":root");
        expect(EDITOR_JAVASCRIPT).toContain('fetch("/app-translations.json")');
        expect(EDITOR_JAVASCRIPT).toContain('showReport({ message: translate("status.importedLegacy"');
        expect(EDITOR_JAVASCRIPT).toContain('api("/api/snippet/apply"');
        expect(EDITOR_JAVASCRIPT).toContain('api("/api/snippet/import"');
        expect(html).toContain('id="task-home"');
        expect(html).toContain('id="workflow-snippet"');
        expect(html).toContain('id="workflow-edit"');
        expect(html).toContain('id="snippet-bindings"');
        expect(html).toContain('id="export-snippet"');
        expect(html).not.toContain('id="snippet-preview"');
        expect(html).not.toContain('id="snippet-import-preview"');
    });
});
