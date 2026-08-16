import type { ActionCatalogEntry } from "./action-catalog.ts";
import { EditorSettingsStore } from "./editor-settings.ts";
import { t } from "./i18n.ts";
import { discoverReaperDataPaths } from "./paths.ts";
import type { EditorProductIdentity } from "./product-identity.ts";
import { startEditorServer } from "./server.ts";
import { bundleEditorJavascript } from "./ui.ts";

export interface EditorLaunchOptions {
    actions: ActionCatalogEntry[];
    args: string[];
    identity: EditorProductIdentity;
    editorJavascript?: string;
}

function argumentValue(args: string[], name: string): string | undefined {
    const argumentIdx = args.indexOf(name);
    if (argumentIdx < 0) return undefined;
    const value = args[argumentIdx + 1];
    if (!value || value.startsWith("--")) throw new Error(`${name} requires a value`);
    return value;
}

async function openBrowser(url: string): Promise<void> {
    const command = process.platform === "darwin" ? ["open", url] : process.platform === "win32" ? ["cmd.exe", "/c", "start", "", url] : ["xdg-open", url];
    const subprocess = Bun.spawn(command, { stderr: "ignore", stdout: "ignore" });
    subprocess.unref();
}

export async function launchEditor(options: EditorLaunchOptions): Promise<void> {
    const explicitDataPath = argumentValue(options.args, "--data-path");
    const portValue = argumentValue(options.args, "--port");
    const port = portValue ? Number(portValue) : 0;
    if (!Number.isInteger(port) || port < 0 || port > 65535) throw new Error("--port must be an integer from 0 to 65535");
    for (let argumentIdx = 0; argumentIdx < options.args.length; argumentIdx++) {
        const argument = options.args[argumentIdx];
        if (argument === "--no-open") continue;
        if (argument === "--data-path" || argument === "--port") {
            argumentIdx++;
            continue;
        }
        throw new Error(`Unknown argument: ${argument}`);
    }
    const settings = new EditorSettingsStore(options.identity.productId);
    const candidates = await discoverReaperDataPaths(explicitDataPath, await settings.readLastDataPath());
    const editorJavascript = options.editorJavascript ?? await bundleEditorJavascript();
    const running = startEditorServer({ actions: options.actions, candidates, editorJavascript, identity: options.identity, port, settings });
    console.log(t("app.title", { product: options.identity.displayName }));
    console.log(t("server.local", { url: running.url }));
    console.log(t("server.stop"));
    if (!options.args.includes("--no-open")) {
        try {
            await openBrowser(running.url);
        } catch (error) {
            console.warn(t("error.browserOpen", { message: error instanceof Error ? error.message : String(error) }));
        }
    }
}
