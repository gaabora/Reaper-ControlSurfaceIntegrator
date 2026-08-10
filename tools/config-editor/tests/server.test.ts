import { describe, expect, test } from "bun:test";
import type { ActionCatalogEntry } from "../src/action-catalog.ts";
import type { EditorProductIdentity } from "../src/product-identity.ts";
import { startEditorServer } from "../src/server.ts";

const identity: EditorProductIdentity = {
    configFilename: "TestProduct.ini",
    displayName: "Test Product",
    packagePrefix: "TestProduct",
    productId: "test-product",
    resourceDirectory: "TestProduct",
};
const actions: ActionCatalogEntry[] = [{ category: "Transport", enumName: "Play", name: "Play" }];

describe("local editor server", () => {
    test("binds to loopback and protects API calls with its session token", async () => {
        const running = startEditorServer({ actions, candidates: [], identity });
        try {
            expect(running.url.startsWith("http://127.0.0.1:")).toBeTrue();
            const page = await fetch(new URL("/", running.url));
            expect(await page.text()).toContain("Test Product Conf Editor");
            const translations = await fetch(new URL("/app-translations.json", running.url));
            expect(translations.status).toBe(200);
            expect((await translations.json())["app.title"]).toBe("{product} Conf Editor");
            expect((await fetch(new URL("/api/status", running.url))).status).toBe(401);
            const response = await fetch(new URL("/api/status", running.url), { headers: { "X-Session-Token": running.token } });
            expect(response.status).toBe(200);
            expect((await response.json()).identity.productId).toBe(identity.productId);
        } finally {
            running.server.stop(true);
        }
    });
});
