import { describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import type { ActionCatalogEntry } from "../src/action-catalog.ts";
import type { SettingsSchema } from "../src/settings-schema.ts";
import type { SurfaceIoSchema } from "../src/surface-io-schema.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { startEditorServer } from "../src/server.ts";
import { bundleEditorJavascript } from "../src/ui.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.conf",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};
const actions: ActionCatalogEntry[] = [{ category: "Transport", enumName: "Play", name: "Play" }];
const settingsSchema: SettingsSchema = { settings: [{ category: "Behavior", defaultValue: "Latch", enumValues: ["Momentary", "Latch", "Hybrid"], name: "DefaultModifierMode", scopes: ["Product", "Surface"], type: "enum" }], version: 1 };
const surfaceIoSchema: SurfaceIoSchema = { primitives: [{ capabilities: ["Press"], direction: "Input", name: "Press" }], records: [{ kind: "SurfaceLine", line: 1, properties: new Map([["SurfaceLine", "Widget"], ["ParentBlock", "OSKRow"], ["Argument", "Identifier"], ["Required", "None"], ["Optional", "Width"], ["Rules", "Width:PositiveFinite"]]) }], representations: [{ direction: "Input", encoding: "MIDIExact", nested: [], optional: [], primitive: "Press", protocol: "MIDI", required: ["On"] }], version: 1 };
const editorJavascript = await bundleEditorJavascript();

describe("local editor server", () => {
    test("stores legacy source and result drafts separately", async () => {
        const temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-server-legacy-draft-"));
        const dataRoot = path.join(temporaryRoot, "Data");
        const productRoot = path.join(dataRoot, identity.resourceDirectory);
        const legacySurfacePath = path.join(temporaryRoot, "CSI", "Surfaces", "LegacySurface", "Surface.txt");
        await mkdir(productRoot, { recursive: true });
        await mkdir(path.dirname(legacySurfacePath), { recursive: true });
        await writeFile(legacySurfacePath, "Widget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n", "utf8");
        const running = startEditorServer({ actions, candidates: [], editorJavascript, identity, settingsSchema, surfaceIoSchema });
        const headers = { "Content-Type": "application/json", "X-Session-Token": running.token };
        const originalSource = "Widget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n";
        const originalSourceHash = "source-hash";
        try {
            const selected = await fetch(new URL("/api/select-data-path", running.url), { body: JSON.stringify({ path: dataRoot }), headers, method: "POST" });
            expect(selected.status).toBe(200);
            const saved = await fetch(new URL("/api/legacy/draft", running.url), {
                body: JSON.stringify({ generatedResult: "@Meta { Version=2 Protocol=MIDI Channels=1 }\n", originalSource, originalSourceHash, originalTargetPath: "Surfaces/User/legacysurface.txt", resultDraft: "@Meta { Version=2 Protocol=MIDI Channels=1 }\n\nWidget Play {}\n", sourceDraft: originalSource + "// source edit\n", sourcePath: "Surfaces/LegacySurface/Surface.txt", stage: "result", surfaceName: "LegacySurface", targetPath: "Surfaces/User/legacysurface.txt", targetProfileId: "legacysurface" }),
                headers,
                method: "POST",
            });
            expect(saved.status).toBe(200);
            expect((await saved.json()).dirty).toBeTrue();
            const restored = await fetch(new URL("/api/legacy/drafts?surfaceName=LegacySurface&targetProfileId=legacysurface", running.url), { headers });
            expect(restored.status).toBe(200);
            const restoredPayload = await restored.json();
            expect(restoredPayload.draftFolder).toBe(path.join(os.tmpdir(), "test-product-legacy-import-conf-editor", "drafts"));
            expect(restoredPayload.drafts).toEqual([{ generatedResult: "@Meta { Version=2 Protocol=MIDI Channels=1 }\n", originalSourceHash, originalTargetPath: "Surfaces/User/legacysurface.txt", resultDraft: "@Meta { Version=2 Protocol=MIDI Channels=1 }\n\nWidget Play {}\n", sourceDraft: originalSource + "// source edit\n", sourcePath: "Surfaces/LegacySurface/Surface.txt", stage: "result", targetPath: "Surfaces/User/legacysurface.txt" }]);
        } finally {
            running.server.stop(true);
            if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-server-legacy-draft-`)) await rm(temporaryRoot, { force: true, recursive: true });
        }
    });

    test("binds to loopback and protects API calls with its session token", async () => {
        const running = startEditorServer({ actions, candidates: [], editorJavascript, identity, settingsSchema, surfaceIoSchema });
        try {
            expect(running.url.startsWith("http://127.0.0.1:")).toBeTrue();
            const page = await fetch(new URL("/", running.url));
            const pageText = await page.text();
            expect(pageText).toContain("Test Product Conf Editor");
            expect(pageText).toContain(`<meta name="config-editor-session-token" content="${running.token}">`);
            expect(running.url).not.toContain(running.token);
            expect(await (await fetch(new URL("/health", running.url))).json()).toEqual({ editorApiVersion: 12, productId: identity.productId });
            const translations = await fetch(new URL("/app-translations.json", running.url));
            expect(translations.status).toBe(200);
            expect((await translations.json())["app.title"]).toBe("{product} Conf Editor");
            expect((await fetch(new URL("/api/status", running.url))).status).toBe(401);
            const response = await fetch(new URL("/api/status", running.url), { headers: { "X-Session-Token": running.token } });
            expect(response.status).toBe(200);
            const status = await response.json();
            expect(status.identity.productId).toBe(identity.productId);
            expect(status.settingsSchema.settings[0].defaultValue).toBe("Latch");
            expect(status.surfaceIoSchema.records[0].properties.Optional).toBe("Width");
        } finally {
            running.server.stop(true);
        }
    });

    test("serves the inline functional snippet preview API", async () => {
        const temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-server-snippet-"));
        const dataRoot = path.join(temporaryRoot, "Data");
        const productRoot = path.join(dataRoot, identity.resourceDirectory);
        const snippetPath = path.join(productRoot, "Snippets", "BuiltIn", "transport.snippet");
        const surfacePath = path.join(productRoot, "Surfaces", "Vendor", "test-surface.txt");
        const zonePath = path.join(productRoot, "Zones", "User", "test-profile", "Main", "Home.zon");
        const legacySurfacePath = path.join(temporaryRoot, "CSI", "Surfaces", "LegacySurface", "Surface.txt");
        await mkdir(path.dirname(snippetPath), { recursive: true });
        await mkdir(path.dirname(surfacePath), { recursive: true });
        await mkdir(path.dirname(zonePath), { recursive: true });
        await mkdir(path.dirname(legacySurfacePath), { recursive: true });
        await writeFile(path.join(productRoot, identity.configFilename), "Device test {\n  Type=MIDI\n  Input=0\n  Output=0\n}\n\nPage Home {\n  Surface test {\n    Device=test\n    Template=test-surface\n    MainProfile=test-profile\n    FXProfile=test-profile\n  }\n}\n", "utf8");
        await writeFile(snippetPath, "@Meta { Version=2 Name=Transport }\n\nPlay Play\n", "utf8");
        await writeFile(surfacePath, "@Meta { Version=2 Protocol=MIDI Channels=1 }\n\nWidget Play {\n  Input Press { Encoding=MIDIExact On=[0x90,0x5e,0x7f] Off=[0x90,0x5e,0x00] }\n}\n", "utf8");
        await writeFile(zonePath, "@Meta { Version=2 Role=Home }\n\nPlay Play\n", "utf8");
        await writeFile(legacySurfacePath, "Widget Play\n  Press 90 5e 7f 90 5e 00\nWidgetEnd\n", "utf8");
        const running = startEditorServer({ actions, candidates: [], editorJavascript, identity, settingsSchema, surfaceIoSchema });
        const headers = { "Content-Type": "application/json", "X-Session-Token": running.token };
        try {
            const selected = await fetch(new URL("/api/select-data-path", running.url), { body: JSON.stringify({ path: dataRoot }), headers, method: "POST" });
            expect(selected.status).toBe(200);
            const selectedPayload = await selected.json();
            expect(selectedPayload.legacy.path).toBe(path.join(temporaryRoot, "CSI"));
            expect(selectedPayload.legacy.surfaces.map((surface: { name: string }) => surface.name)).toEqual(["LegacySurface"]);

            const previewResponse = await fetch(new URL("/api/snippet/preview", running.url), {
                body: JSON.stringify({ bindingChoices: [{ bindingId: "Play", confirmed: true, widgetName: "Play" }], insertionLine: 1, snippetPath: "Snippets/BuiltIn/transport.snippet", surfacePath: "Surfaces/Vendor/test-surface.txt", targetSource: await Bun.file(zonePath).text(), targetZonePath: "Zones/User/test-profile/Main/Home.zon" }),
                headers,
                method: "POST",
            });
            expect(previewResponse.status).toBe(200);
            expect((await previewResponse.json()).preview.valid).toBeTrue();

            const contextResponse = await fetch(new URL("/api/snippet/context?zonePath=Zones%2FUser%2Ftest-profile%2FMain%2FHome.zon", running.url), { headers });
            expect(contextResponse.status).toBe(200);
            expect(await contextResponse.json()).toEqual({ automatic: true, surfaces: [{ path: "Surfaces/Vendor/test-surface.txt" }] });

            const validateAllResponse = await fetch(new URL("/api/validate-all", running.url), { body: JSON.stringify({ changes: [] }), headers, method: "POST" });
            expect(validateAllResponse.status).toBe(200);
            const validateAllPayload = await validateAllResponse.json();
            expect(validateAllPayload.checkedPaths).toContain("Zones/User/test-profile/Main/Home.zon");
            expect(validateAllPayload.files.find((file: { path: string }) => file.path === "Zones/User/test-profile/Main/Home.zon").diagnostics).toEqual([]);

            const missingMarkerSource = "Zone Home\n  Play Play\nZoneEnd\n";
            const quickFixResponse = await fetch(new URL("/api/quick-fix", running.url), {
                body: JSON.stringify({ diagnostic: { code: "zone.format.missing", message: "Zone has no // @format zone 1 marker" }, fix: { id: "zone.format.add" }, path: "Zones/User/test-profile/Main/Home.zon", source: missingMarkerSource }),
                headers,
                method: "POST",
            });
            expect(quickFixResponse.status).toBe(200);
            expect((await quickFixResponse.json()).source).toStartWith("// @format zone 1\nZone Home");
        } finally {
            running.server.stop(true);
            if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-server-snippet-`)) await rm(temporaryRoot, { force: true, recursive: true });
        }
    });
});
