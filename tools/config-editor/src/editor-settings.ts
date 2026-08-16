import { mkdir, readFile, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

interface StoredEditorSettings {
    lastDataPath: string;
    version: 1;
}

function defaultSettingsPath(productId: string): string {
    const basePath = process.platform === "darwin"
        ? path.join(os.homedir(), "Library", "Application Support")
        : process.platform === "win32"
            ? process.env.APPDATA ?? path.join(os.homedir(), "AppData", "Roaming")
            : process.env.XDG_CONFIG_HOME ?? path.join(os.homedir(), ".config");
    return path.join(basePath, productId, "conf-editor.json");
}

export class EditorSettingsStore {
    constructor(productId: string, private readonly settingsPath = defaultSettingsPath(productId)) {}

    async readLastDataPath(): Promise<string | undefined> {
        try {
            const settings = JSON.parse(await readFile(this.settingsPath, "utf8")) as Partial<StoredEditorSettings>;
            return settings.version === 1 && typeof settings.lastDataPath === "string" && path.isAbsolute(settings.lastDataPath) ? settings.lastDataPath : undefined;
        } catch {
            return undefined;
        }
    }

    async writeLastDataPath(dataPath: string): Promise<void> {
        if (!path.isAbsolute(dataPath)) throw new Error("Remembered REAPER data path must be absolute");
        const settings: StoredEditorSettings = { lastDataPath: path.resolve(dataPath), version: 1 };
        await mkdir(path.dirname(this.settingsPath), { recursive: true, mode: 0o700 });
        await writeFile(this.settingsPath, JSON.stringify(settings, null, 2) + "\n", { encoding: "utf8", mode: 0o600 });
    }
}
