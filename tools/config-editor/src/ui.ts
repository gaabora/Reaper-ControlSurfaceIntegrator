import editorCss from "./ui/app.css" with { type: "text" };
import editorHtml from "./ui/index.html" with { type: "text" };
import editorJavascript from "./ui/app.js" with { type: "text" };
import { DISPLAY_LOCALE, t, translationCatalog, type TranslationKey } from "./i18n.ts";

const HTML_TRANSLATION_KEYS = [
    "action.addToSaveAll",
    "action.check",
    "action.import",
    "action.makeEditable",
    "action.open",
    "action.preview",
    "action.save",
    "action.saveAll",
    "details.title",
    "document.none",
    "files.empty",
    "files.title",
    "legacy.dependencies.empty",
    "legacy.dependencies.title",
    "legacy.includeSurface",
    "legacy.path.placeholder",
    "legacy.preview.empty",
    "legacy.preview.title",
    "legacy.selectAll",
    "legacy.selectNone",
    "legacy.surface.placeholder",
    "legacy.surface.title",
    "legacy.title",
    "legacy.widgetMappings.empty",
    "legacy.widgetMappings.title",
    "legacy.zones.empty",
    "legacy.zones.title",
    "problems.none",
    "problems.title",
    "reaperDataPath.help",
    "reaperDataPath.placeholder",
    "status.ready",
    "tab.guided",
    "tab.text",
] as const satisfies readonly TranslationKey[];

export const EDITOR_CSS = editorCss;
export const EDITOR_JAVASCRIPT = editorJavascript;

function escapeHtml(value: string): string {
    return value.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&#39;");
}

export function createEditorHtml(productName: string): string {
    let html = editorHtml.replaceAll("{{displayLocale}}", escapeHtml(DISPLAY_LOCALE));
    html = html.replaceAll("{{title}}", escapeHtml(t("app.title", { product: productName })));
    html = html.replaceAll("{{pending.count}}", escapeHtml(t("pending.count", { count: 0 })));
    for (const key of HTML_TRANSLATION_KEYS) html = html.replaceAll("{{" + key + "}}", escapeHtml(t(key)));
    return html;
}

export function createEditorTranslationsJson(): string {
    return JSON.stringify(translationCatalog());
}
