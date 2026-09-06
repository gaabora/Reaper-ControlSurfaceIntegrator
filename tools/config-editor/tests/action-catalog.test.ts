import { expect, test } from "bun:test";
import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { loadActionCatalog } from "../src/action-catalog.ts";

test("action catalog reads C++ macro lists with CRLF line endings", async () => {
    const repositoryRoot = await mkdtemp(path.join(os.tmpdir(), "config-editor-actions-"));
    try {
        await mkdir(path.join(repositoryRoot, "src", "actions"), { recursive: true });
        await mkdir(path.join(repositoryRoot, "src", "controls"), { recursive: true });
        await mkdir(path.join(repositoryRoot, "src", "shared"), { recursive: true });
        await writeFile(path.join(repositoryRoot, "src", "controls", "format2_action_metadata.h"), '#define FORMAT2_CONTEXT_CHANGING_ACTION_LIST(X) \\\r\nX("GoZone")\r\n\r\n#define FORMAT2_MODIFIER_ACTION_LIST(X) \\\r\nX("Shift")\r\n', "utf8");
        await writeFile(path.join(repositoryRoot, "src", "shared", "types.h"), '#define ACTION_TYPE_LIST(X) \\\r\nX(GoZone, "GoZone") \\\r\nX(Shift, "Shift")\r\n\r\nenum class ActionType {};\r\n', "utf8");

        const catalog = await loadActionCatalog(repositoryRoot);
        expect(catalog.find((entry) => entry.name === "GoZone")?.changesContext).toBeTrue();
        expect(catalog.find((entry) => entry.name === "Shift")?.changesModifier).toBeTrue();
    } finally {
        await rm(repositoryRoot, { force: true, recursive: true });
    }
});
