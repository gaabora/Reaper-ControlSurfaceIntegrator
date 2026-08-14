import { fileURLToPath } from "node:url";
import editorCss from "./ui/app.css" with { type: "text" };
import editorHtml from "./ui/index.html" with { type: "text" };
import editorJavascript from "./ui/app.js" with { type: "text" };
import { DISPLAY_LOCALE, isTranslationKey, t, translationCatalog } from "./i18n.ts";

export const EDITOR_CSS = editorCss;
export const EDITOR_JAVASCRIPT = editorJavascript;

export async function bundleEditorJavascript(): Promise<string> {
    const result = await Bun.build({ entrypoints: [fileURLToPath(new URL("./ui/app.js", import.meta.url))], format: "esm", minify: true, target: "browser", write: false });
    if (!result.success || !result.outputs[0]) throw new Error(`Could not bundle browser editor: ${result.logs.map((entry) => entry.message).join("; ")}`);
    return result.outputs[0].text();
}

function escapeHtml(value: string): string {
    return value.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&#39;");
}

export function createEditorHtml(productName: string, sessionToken: string): string {
    const specialValues = new Map<string, string>([
        ["displayLocale", DISPLAY_LOCALE],
        ["sessionToken", sessionToken],
        ["title", t("app.title", { product: productName })],
        ["pending.count", t("pending.count", { count: 0 })],
    ]);
    return editorHtml.replace(/\{\{([^{}]+)\}\}/g, (_match, key: string) => {
        const specialValue = specialValues.get(key);
        if (specialValue !== undefined) return escapeHtml(specialValue);
        if (isTranslationKey(key)) return escapeHtml(t(key));
        throw new Error(`Unknown HTML translation key: ${key}`);
    });
}

export function createEditorTranslationsJson(): string {
    return JSON.stringify(translationCatalog());
}
