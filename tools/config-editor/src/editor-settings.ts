import { mkdir, readFile, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";

interface StoredEditorSettings {
    activeServerUrl?: string;
    lastDataPath?: string;
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

    private async readSettings(): Promise<Partial<StoredEditorSettings>> {
        try {
            const settings = JSON.parse(await readFile(this.settingsPath, "utf8")) as Partial<StoredEditorSettings>;
            return settings.version === 1 ? settings : {};
        } catch {
            return {};
        }
    }

    private async writeSettings(settings: StoredEditorSettings): Promise<void> {
        await mkdir(path.dirname(this.settingsPath), { recursive: true, mode: 0o700 });
        await writeFile(this.settingsPath, JSON.stringify(settings, null, 2) + "\n", { encoding: "utf8", mode: 0o600 });
    }

    async readActiveServerUrl(): Promise<string | undefined> {
        const activeServerUrl = (await this.readSettings()).activeServerUrl;
        if (typeof activeServerUrl !== "string") return undefined;
        try {
            const parsedUrl = new URL(activeServerUrl);
            return parsedUrl.protocol === "http:" && parsedUrl.hostname === "127.0.0.1" ? parsedUrl.href : undefined;
        } catch {
            return undefined;
        }
    }

    async readLastDataPath(): Promise<string | undefined> {
        const lastDataPath = (await this.readSettings()).lastDataPath;
        return typeof lastDataPath === "string" && path.isAbsolute(lastDataPath) ? lastDataPath : undefined;
    }

    async writeActiveServerUrl(activeServerUrl: string): Promise<void> {
        const settings = await this.readSettings();
        await this.writeSettings({ ...settings, activeServerUrl, version: 1 });
    }

    async writeLastDataPath(dataPath: string): Promise<void> {
        if (!path.isAbsolute(dataPath)) throw new Error("Remembered REAPER data path must be absolute");
        const settings = await this.readSettings();
        await this.writeSettings({ ...settings, lastDataPath: path.resolve(dataPath), version: 1 });
    }
}
