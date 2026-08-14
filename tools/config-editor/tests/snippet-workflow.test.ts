import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { previewSnippetApplication, type SnippetApplicationRequest } from "../src/snippet-workflow.ts";
import { ConfigurationStore } from "../src/store.ts";

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

function applicationRequest(bindingChoices = choices(), targetSource = zoneSource): SnippetApplicationRequest {
    return { applicationId: "transport", bindingChoices, conflictAction: "", insertionLine: 1, snippetPath, surfacePath, targetSource, targetZonePath };
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
    test("previews confirmed semantic bindings without writing the target", async () => {
        const store = await createStore();
        const unresolved = await previewSnippetApplication(store, knownActions, applicationRequest([]));
        expect(unresolved.valid).toBeTrue();
        expect(unresolved.bindings.every((binding) => binding.automatic)).toBeTrue();
        expect(unresolved.bindings.map((binding) => [binding.id, binding.recommendedWidgetName])).toEqual([["play", "Play"], ["stop", "Stop"]]);

        const preview = await previewSnippetApplication(store, knownActions, applicationRequest());
        expect(preview.valid).toBeTrue();
        expect(preview.source).toContain("// @snippet Application=transport Source=transport\n  Play Play\n  Stop Stop\n  // @snippet-end Application=transport");
        expect(await readFile(path.join(productRoot, ...targetZonePath.split("/")), "utf8")).toBe(zoneSource);
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
        const existingRequest = applicationRequest(choices(), firstPreview.source);

        const conflict = await previewSnippetApplication(store, knownActions, existingRequest);
        expect(conflict.valid).toBeFalse();
        expect(conflict.conflict).toEqual({ action: "", existingApplicationId: "transport" });

        const replaceRequest = { ...existingRequest, conflictAction: "replace" as const };
        const replacement = await previewSnippetApplication(store, knownActions, replaceRequest);
        expect(replacement.valid).toBeTrue();
        expect(replacement.source.match(/@snippet Application=transport/g) ?? []).toHaveLength(1);

        const renameRequest = { ...existingRequest, conflictAction: "rename" as const, renamedApplicationId: "transport-copy" };
        const renamed = await previewSnippetApplication(store, knownActions, renameRequest);
        expect(renamed.valid).toBeTrue();
        expect(renamed.source).toContain("@snippet Application=transport-copy Source=transport");

        const skipRequest = { ...applicationRequest([], firstPreview.source), conflictAction: "skip" as const };
        const skipped = await previewSnippetApplication(store, knownActions, skipRequest);
        expect(skipped.valid).toBeTrue();
        expect(skipped.source).toBe(firstPreview.source);
    });

    test("inserts after the cursor line", async () => {
        const store = await createStore();
        const request = { ...applicationRequest(), insertionLine: 3 };
        const preview = await previewSnippetApplication(store, knownActions, request);
        expect(preview.source.indexOf("Record Record")).toBeLessThan(preview.source.indexOf("@snippet Application=transport"));
        expect(preview.source.indexOf("@snippet-end Application=transport")).toBeLessThan(preview.source.indexOf("ZoneEnd"));
    });
});
