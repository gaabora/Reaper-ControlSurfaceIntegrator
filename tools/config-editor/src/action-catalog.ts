import { readdir, readFile, writeFile, mkdir } from "node:fs/promises";
import path from "node:path";

export interface ActionCatalogEntry {
    brief?: string;
    category: string;
    enumName: string;
    feedback?: string;
    feedbackShape?: string;
    name: string;
    notes?: string;
    params?: string;
    usage?: string;
}

interface ActionDocumentation {
    brief?: string;
    feedback?: string;
    feedbackShape?: string;
    notes?: string;
    params?: string;
    usage?: string;
}

function parseDocumentationBlock(block: string): Map<string, ActionDocumentation> {
    const result = new Map<string, ActionDocumentation>();
    const tagValues = new Map<string, string>();
    let currentTag = "";
    for (const rawLine of block.split(/\r?\n/)) {
        const line = rawLine.replace(/^\s*\/\/!\s?/, "").trim();
        const tagMatch = line.match(/^@(\w+)\s*(.*)$/);
        if (tagMatch) {
            currentTag = tagMatch[1].toLowerCase();
            tagValues.set(currentTag, tagMatch[2].trim());
        } else if (currentTag && line) {
            tagValues.set(currentTag, `${tagValues.get(currentTag) ?? ""} ${line}`.trim());
        }
    }
    const actionNames = (tagValues.get("action") ?? "").split("/").map((name) => name.trim()).filter((name) => name && !name.startsWith("("));
    for (const actionName of actionNames) {
        result.set(actionName, {
            brief: tagValues.get("brief"),
            feedback: tagValues.get("feedback"),
            feedbackShape: tagValues.get("feedback_shape"),
            notes: tagValues.get("notes"),
            params: tagValues.get("params"),
            usage: tagValues.get("zone_usage"),
        });
    }
    return result;
}

async function loadActionDocumentation(actionsRoot: string): Promise<Map<string, ActionDocumentation>> {
    const documentation = new Map<string, ActionDocumentation>();
    const entries = await readdir(actionsRoot, { withFileTypes: true });
    for (const entry of entries) {
        if (!entry.isFile() || !entry.name.endsWith(".h")) continue;
        const source = await readFile(path.join(actionsRoot, entry.name), "utf8");
        const blocks = source.match(/(?:^\s*\/\/!.*(?:\r?\n|$))+/gm) ?? [];
        for (const block of blocks) for (const [name, metadata] of parseDocumentationBlock(block)) documentation.set(name, metadata);
    }
    return documentation;
}

export async function loadActionCatalog(repositoryRoot: string): Promise<ActionCatalogEntry[]> {
    const typesPath = path.join(repositoryRoot, "src", "shared", "types.h");
    const source = await readFile(typesPath, "utf8");
    const start = source.indexOf("#define ACTION_TYPE_LIST(X)");
    const end = source.indexOf("enum class ActionType", start);
    if (start < 0 || end < 0) throw new Error(`Cannot find ACTION_TYPE_LIST in ${typesPath}`);

    const documentation = await loadActionDocumentation(path.join(repositoryRoot, "src", "actions"));
    const catalog: ActionCatalogEntry[] = [];
    let category = "Uncategorized";
    for (const line of source.slice(start, end).split(/\r?\n/)) {
        const categoryMatch = line.match(/\/\*\s*(.+?)\s*\*\//);
        if (categoryMatch) category = categoryMatch[1];
        const actionMatch = line.match(/X\(\s*([^,]+),\s*"([^"]+)"\s*\)/);
        if (!actionMatch) continue;
        const enumName = actionMatch[1].trim();
        const name = actionMatch[2];
        catalog.push({ category, enumName, name, ...documentation.get(name) });
    }
    if (!catalog.length) throw new Error(`ACTION_TYPE_LIST contains no actions in ${typesPath}`);
    return catalog;
}

export function actionNameSet(catalog: ActionCatalogEntry[]): Set<string> {
    return new Set(catalog.map((entry) => entry.name));
}

export async function writeActionCatalog(outputPath: string, catalog: ActionCatalogEntry[]): Promise<void> {
    await mkdir(path.dirname(outputPath), { recursive: true });
    await writeFile(outputPath, JSON.stringify({ actions: catalog, version: 1 }, null, 2) + "\n", "utf8");
}
