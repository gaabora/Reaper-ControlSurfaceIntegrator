import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { mkdtemp, rm } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { ConfigurationDraftStore } from "../src/draft-store.ts";

let temporaryRoot = "";

beforeEach(async () => {
    temporaryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-drafts-"));
});

afterEach(async () => {
    if (temporaryRoot.startsWith(`${os.tmpdir()}${path.sep}config-editor-drafts-`)) await rm(temporaryRoot, { force: true, recursive: true });
});

describe("temporary configuration drafts", () => {
    test("stores one recoverable draft per logical file path", async () => {
        const store = new ConfigurationDraftStore("test-product", "/reaper/Data/TestProduct", temporaryRoot);
        await store.write("Zones/User/test/Main/Home.zon", "original-home", "home draft");
        await store.write("Zones/User/test/Main/Selected.zon", "original-selected", "selected draft");

        expect(await store.read("Zones/User/test/Main/Home.zon")).toMatchObject({ originalHash: "original-home", path: "Zones/User/test/Main/Home.zon", source: "home draft" });
        expect((await store.list()).map((draft) => draft.path)).toEqual(["Zones/User/test/Main/Home.zon", "Zones/User/test/Main/Selected.zon"]);

        await store.discard("Zones/User/test/Main/Home.zon");
        expect(await store.read("Zones/User/test/Main/Home.zon")).toBeUndefined();
    });

    test("separates the same relative path in different product roots", async () => {
        const firstStore = new ConfigurationDraftStore("test-product", "/first/Data/TestProduct", temporaryRoot);
        const secondStore = new ConfigurationDraftStore("test-product", "/second/Data/TestProduct", temporaryRoot);
        const relativePath = "Zones/User/test/Main/Home.zon";
        await firstStore.write(relativePath, "first-hash", "first draft");
        await secondStore.write(relativePath, "second-hash", "second draft");

        expect((await firstStore.read(relativePath))?.source).toBe("first draft");
        expect((await secondStore.read(relativePath))?.source).toBe("second draft");
    });
});
