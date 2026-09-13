import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { ProductRootGuard } from "../src/paths.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { previewSnippetApplication, type SnippetApplicationRequest } from "../src/snippet-workflow.ts";
import { ConfigurationStore } from "../src/store.ts";

const identity: EditorProductIdentity = { configFilename: "TestProduct.conf", displayName: "Test Product", packagePrefix: "TestProduct", productId: "test-product", resourceDirectory: "TestProduct" };
const knownActions = new Set(["Play", "Record", "Stop"]);
const snippetSource = "@Meta { Version=2 Name=Transport }\n\nplay Play\nstop Stop\n";
const surfaceSource = "@Meta { Version=2 Protocol=MIDI Channels=1 }\n\nWidget Play {\n  Input Press { Encoding=MIDIExact On=[0x90,0x5e,0x7f] Off=[0x90,0x5e,0x00] }\n}\nWidget Stop {\n  Input Press { Encoding=MIDIExact On=[0x90,0x5d,0x7f] Off=[0x90,0x5d,0x00] }\n}\nWidget Record {\n  Input Press { Encoding=MIDIExact On=[0x90,0x5c,0x7f] Off=[0x90,0x5c,0x00] }\n}\nWidget Rotary {\n  Input Encoder { Encoding=MIDI7 Message=[0xb0,0x10] Mode=Relative7Bit }\n}\n";
const zoneSource = "@Meta { Version=2 Role=Home }\n\nRecord Record\n";
const snippetPath = "Snippets/BuiltIn/transport.snippet";
const surfacePath = "Surfaces/Vendor/test-surface.txt";
const targetZonePath = "Zones/User/test-profile/Main/Home.zon";
let temporaryRoot = "";
let productRoot = "";

function applicationRequest(bindingChoices: SnippetApplicationRequest["bindingChoices"] = [], insertionLine = 1): SnippetApplicationRequest {
    return { bindingChoices, insertionLine, snippetPath, surfacePath, targetSource: zoneSource, targetZonePath };
}

async function createStore(): Promise<ConfigurationStore> {
    return new ConfigurationStore(await ProductRootGuard.create(productRoot, identity), knownActions);
}

beforeEach(async () => {
    temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-snippet-"));
    productRoot = path.join(temporaryRoot, identity.resourceDirectory);
    for (const directory of [path.dirname(path.join(productRoot, snippetPath)), path.dirname(path.join(productRoot, surfacePath)), path.dirname(path.join(productRoot, targetZonePath))]) await mkdir(directory, { recursive: true });
    await writeFile(path.join(productRoot, identity.configFilename), "Device test {\n  Type=MIDI\n  Input=0\n  Output=0\n}\n\nPage Home {\n  Surface test {\n    Device=test\n    Template=test-surface\n    MainProfile=test-profile\n  }\n}\n", "utf8");
    await writeFile(path.join(productRoot, snippetPath), snippetSource, "utf8");
    await writeFile(path.join(productRoot, surfacePath), surfaceSource, "utf8");
    await writeFile(path.join(productRoot, targetZonePath), zoneSource, "utf8");
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-snippet-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("format 2 snippet workflow", () => {
    test("inserts direct Zone bindings without hidden application markers", async () => {
        const preview = await previewSnippetApplication(await createStore(), knownActions, applicationRequest());
        expect(preview.valid).toBeTrue();
        expect(preview.bindings.every((binding) => binding.automatic)).toBeTrue();
        expect(preview.source).toContain("Play Play\nStop Stop");
        expect(preview.source).not.toContain("@snippet");
        expect(await readFile(path.join(productRoot, targetZonePath), "utf8")).toBe(zoneSource);
    });

    test("requires confirmation when a source Widget maps to another Widget", async () => {
        const bindingChoices = [{ bindingId: "play", confirmed: false, widgetName: "Record" }, { bindingId: "stop", confirmed: true, widgetName: "Stop" }];
        const blocked = await previewSnippetApplication(await createStore(), knownActions, applicationRequest(bindingChoices));
        expect(blocked.valid).toBeFalse();
        bindingChoices[0].confirmed = true;
        const accepted = await previewSnippetApplication(await createStore(), knownActions, applicationRequest(bindingChoices));
        expect(accepted.valid).toBeTrue();
        expect(accepted.source).toContain("Record Play");
    });

    test("does not automatically map an exact Widget with incompatible input", async () => {
        await writeFile(path.join(productRoot, snippetPath), "@Meta { Version=2 }\n\n(Increase)+Play Play\n", "utf8");
        const preview = await previewSnippetApplication(await createStore(), knownActions, applicationRequest());
        expect(preview.valid).toBeFalse();
        expect(preview.bindings[0].automatic).toBeFalse();
        expect(preview.bindings[0].candidates.map((candidate) => candidate.name)).toEqual(["Rotary"]);
    });

    test("inserts after the selected line", async () => {
        const preview = await previewSnippetApplication(await createStore(), knownActions, applicationRequest([], 3));
        expect(preview.source.indexOf("Record Record")).toBeLessThan(preview.source.indexOf("Play Play"));
    });
});
