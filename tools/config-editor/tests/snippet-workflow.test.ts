import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { applySnippetApplication, importSnippet, previewSnippetApplication, previewSnippetImport, type SnippetApplicationRequest } from "../src/snippet-workflow.ts";
import { ConfigurationStore, EditorOperationError } from "../src/store.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.ini",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};
const knownActions = new Set(["Play", "Record", "Stop"]);
const snippetSource = `Snippet Version=1 Id=transport Name="Transport controls"
  Binding Id=play Role=Button Input=Press Feedback=Toggle Required=Yes
    Action NoMod Play
  BindingEnd
  Binding Id=stop Role=Button Input=Press Feedback=Toggle Required=Yes
    Action NoMod Stop
  BindingEnd
SnippetEnd
`;
const surfaceSource = `// @format surface 1
Widget Play
  Press 90 5e 7f 90 5e 00
  FB_TwoState 90 5e 7f 90 5e 00
WidgetEnd
Widget Stop
  Press 90 5d 7f 90 5d 00
  FB_TwoState 90 5d 7f 90 5d 00
WidgetEnd
Widget Rotary RotaryWidgetClass
  Encoder b0 10 7f
WidgetEnd
`;
const zoneSource = "// @format zone 1\nZone Home\n  Record Record\nZoneEnd\n";
const snippetPath = "Snippets/BuiltIn/transport.snippet";
const surfacePath = "Surfaces/Vendor/test-surface.txt";
const targetZonePath = "Zones/User/test-profile/Main/Home.zon";
let temporaryRoot = "";
let productRoot = "";

function choices(playWidget = "Play", allowIncompatible = false): SnippetApplicationRequest["bindingChoices"] {
    return [
        { allowIncompatible, bindingId: "play", confirmed: true, widgetName: playWidget },
        { allowIncompatible: false, bindingId: "stop", confirmed: true, widgetName: "Stop" },
    ];
}

function applicationRequest(bindingChoices = choices()): SnippetApplicationRequest {
    return { applicationId: "transport", bindingChoices, conflictAction: "", snippetPath, surfacePath, targetZonePath };
}

async function createStore(): Promise<ConfigurationStore> {
    const guard = await ProductRootGuard.create(productRoot, identity);
    return new ConfigurationStore(guard, knownActions);
}

beforeEach(async () => {
    temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-snippet-"));
    productRoot = path.join(temporaryRoot, identity.resourceDirectory);
    await mkdir(path.join(productRoot, "Snippets", "BuiltIn"), { recursive: true });
    await mkdir(path.join(productRoot, "Snippets", "User"), { recursive: true });
    await mkdir(path.join(productRoot, "Surfaces", "Vendor"), { recursive: true });
    await mkdir(path.join(productRoot, "Zones", "User", "test-profile", "Main"), { recursive: true });
    await writeFile(path.join(productRoot, identity.configFilename), "Version=7.0\n", "utf8");
    await writeFile(path.join(productRoot, ...snippetPath.split("/")), snippetSource, "utf8");
    await writeFile(path.join(productRoot, ...surfacePath.split("/")), surfaceSource, "utf8");
    await writeFile(path.join(productRoot, ...targetZonePath.split("/")), zoneSource, "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-snippet-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("functional snippet workflow", () => {
    test("previews and applies confirmed semantic bindings in one transaction", async () => {
        const store = await createStore();
        const unresolved = await previewSnippetApplication(store, knownActions, applicationRequest([]));
        expect(unresolved.valid).toBeTrue();
        expect(unresolved.bindings.every((binding) => binding.automatic)).toBeTrue();
        expect(unresolved.bindings.map((binding) => [binding.id, binding.recommendedWidgetName])).toEqual([["play", "Play"], ["stop", "Stop"]]);

        const preview = await previewSnippetApplication(store, knownActions, applicationRequest());
        expect(preview.valid).toBeTrue();
        expect(preview.targetSource).toBe(zoneSource);
        expect(preview.source).toContain("// @snippet Application=transport Source=transport\n  Play Play\n  Stop Stop\n  // @snippet-end Application=transport");
        const report = await applySnippetApplication(store, knownActions, { ...applicationRequest(), snippetHash: preview.snippetHash, surfaceHash: preview.surfaceHash, targetHash: preview.targetHash });

        expect(report.changed).toEqual([targetZonePath]);
        expect(await readFile(path.join(productRoot, ...targetZonePath.split("/")), "utf8")).toBe(preview.source);
    });

    test("requires an explained override for an incompatible widget", async () => {
        const store = await createStore();
        const blocked = await previewSnippetApplication(store, knownActions, applicationRequest(choices("Rotary")));
        expect(blocked.valid).toBeFalse();
        expect(blocked.diagnostics.some((diagnostic) => diagnostic.code === "snippet.binding.incompatible" && diagnostic.severity === "error")).toBeTrue();

        const overridden = await previewSnippetApplication(store, knownActions, applicationRequest(choices("Rotary", true)));
        expect(overridden.valid).toBeTrue();
        expect(overridden.diagnostics.some((diagnostic) => diagnostic.code === "snippet.binding.override" && diagnostic.severity === "warning")).toBeTrue();
        expect(overridden.source).toContain("  Rotary Play\n");
    });

    test("requires confirmation for a compatible widget without an exact semantic name", async () => {
        const store = await createStore();
        const unconfirmedChoices = choices("Stop");
        unconfirmedChoices[0].confirmed = false;
        const preview = await previewSnippetApplication(store, knownActions, applicationRequest(unconfirmedChoices));
        expect(preview.valid).toBeFalse();
        expect(preview.diagnostics.some((diagnostic) => diagnostic.code === "snippet.binding.confirm" && diagnostic.severity === "error")).toBeTrue();
    });

    test("requires Replace, Rename, or Skip when an application ID already exists", async () => {
        const store = await createStore();
        const firstPreview = await previewSnippetApplication(store, knownActions, applicationRequest());
        await applySnippetApplication(store, knownActions, { ...applicationRequest(), snippetHash: firstPreview.snippetHash, surfaceHash: firstPreview.surfaceHash, targetHash: firstPreview.targetHash });

        const conflict = await previewSnippetApplication(store, knownActions, applicationRequest());
        expect(conflict.valid).toBeFalse();
        expect(conflict.conflict).toEqual({ action: "", existingApplicationId: "transport" });

        const replaceRequest = { ...applicationRequest(), conflictAction: "replace" as const };
        const replacement = await previewSnippetApplication(store, knownActions, replaceRequest);
        expect(replacement.valid).toBeTrue();
        expect(replacement.source.match(/@snippet Application=transport/g) ?? []).toHaveLength(1);

        const renameRequest = { ...applicationRequest(), conflictAction: "rename" as const, renamedApplicationId: "transport-copy" };
        const renamed = await previewSnippetApplication(store, knownActions, renameRequest);
        expect(renamed.valid).toBeTrue();
        expect(renamed.source).toContain("@snippet Application=transport-copy Source=transport");

        const skipRequest = { ...applicationRequest([]), conflictAction: "skip" as const };
        const skipped = await previewSnippetApplication(store, knownActions, skipRequest);
        expect(skipped.valid).toBeTrue();
        const report = await applySnippetApplication(store, knownActions, { ...skipRequest, snippetHash: skipped.snippetHash, surfaceHash: skipped.surfaceHash, targetHash: skipped.targetHash });
        expect(report.skipped).toEqual([targetZonePath]);
    });

    test("imports user snippets with explicit create, replace, rename, and skip actions", async () => {
        const store = await createStore();
        const firstPreview = await previewSnippetImport(store, knownActions, "transport.snippet", snippetSource);
        const created = await importSnippet(store, knownActions, { action: "create", fileName: "transport.snippet", source: snippetSource, sourceHash: firstPreview.sourceHash, targetHash: firstPreview.targetHash, targetPath: firstPreview.targetPath });
        expect(created.created).toEqual(["Snippets/User/transport.snippet"]);

        const existing = await previewSnippetImport(store, knownActions, "transport.snippet", snippetSource);
        const skipped = await importSnippet(store, knownActions, { action: "skip", fileName: "transport.snippet", source: snippetSource, sourceHash: existing.sourceHash, targetHash: existing.targetHash, targetPath: existing.targetPath });
        expect(skipped.skipped).toEqual(["Snippets/User/transport.snippet"]);

        const renamedPreview = await previewSnippetImport(store, knownActions, "transport.snippet", snippetSource, "Snippets/User/transport-copy.snippet");
        const renamed = await importSnippet(store, knownActions, { action: "rename", fileName: "transport.snippet", source: snippetSource, sourceHash: renamedPreview.sourceHash, targetHash: renamedPreview.targetHash, targetPath: renamedPreview.targetPath });
        expect(renamed.created).toEqual(["Snippets/User/transport-copy.snippet"]);

        const changedSource = snippetSource.replace("Name=\"Transport controls\"", "Name=\"Changed transport\"");
        const replacePreview = await previewSnippetImport(store, knownActions, "transport.snippet", changedSource);
        const replaced = await importSnippet(store, knownActions, { action: "replace", fileName: "transport.snippet", source: changedSource, sourceHash: replacePreview.sourceHash, targetHash: replacePreview.targetHash, targetPath: replacePreview.targetPath });
        expect(replaced.changed).toEqual(["Snippets/User/transport.snippet"]);
    });

    test("rejects apply when a source changes after preview", async () => {
        const store = await createStore();
        const preview = await previewSnippetApplication(store, knownActions, applicationRequest());
        await writeFile(path.join(productRoot, ...snippetPath.split("/")), snippetSource.replace("Transport controls", "Changed transport"), "utf8");
        try {
            await applySnippetApplication(store, knownActions, { ...applicationRequest(), snippetHash: preview.snippetHash, surfaceHash: preview.surfaceHash, targetHash: preview.targetHash });
            throw new Error("Expected a snippet source hash conflict");
        } catch (error) {
            expect(error).toBeInstanceOf(EditorOperationError);
            expect((error as EditorOperationError).code).toBe("conflict.snippet-source");
        }
    });
});
