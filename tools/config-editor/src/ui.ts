import editorCss from "./ui/app.css" with { type: "text" };
import editorHtml from "./ui/index.html" with { type: "text" };
import editorJavascript from "./ui/app.js" with { type: "text" };
import { DISPLAY_LOCALE, t, translationCatalog, type TranslationKey } from "./i18n.ts";

const HTML_TRANSLATION_KEYS = [
    "action.addToSaveAll",
    "action.applySnippet",
    "action.backToTasks",
    "action.check",
    "action.export",
    "action.import",
    "action.importSnippet",
    "action.makeEditable",
    "action.open",
    "action.preview",
    "action.save",
    "action.saveAll",
    "advanced.title",
    "details.title",
    "document.none",
    "edit.help",
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
    "legacy.step.source.description",
    "legacy.step.source.title",
    "legacy.surface.placeholder",
    "legacy.surface.title",
    "legacy.widgetMappings.empty",
    "legacy.widgetMappings.title",
    "legacy.zones.empty",
    "legacy.zones.title",
    "problems.none",
    "problems.title",
    "reaperDataPath.help",
    "reaperDataPath.placeholder",
    "snippet.applicationId",
    "snippet.bindings.empty",
    "snippet.conflict.renameId",
    "snippet.conflict.message",
    "snippet.conflict.title",
    "snippet.import.empty",
    "snippet.import.conflict",
    "snippet.import.description",
    "snippet.import.target",
    "snippet.import.title",
    "snippet.export.description",
    "snippet.export.title",
    "snippet.preview.empty",
    "snippet.source",
    "snippet.source.choose",
    "snippet.step.apply.description",
    "snippet.step.apply.title",
    "snippet.step.choose.description",
    "snippet.step.choose.title",
    "snippet.step.map.description",
    "snippet.step.map.title",
    "snippet.surface",
    "snippet.surface.choose",
    "snippet.zone",
    "snippet.zone.choose",
    "status.ready",
    "task.apply.description",
    "task.apply.title",
    "task.edit.description",
    "task.edit.title",
    "task.legacy.description",
    "task.legacy.title",
    "task.share.description",
    "task.share.title",
    "tasks.description",
    "tasks.openDataFirst",
    "tasks.title",
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
