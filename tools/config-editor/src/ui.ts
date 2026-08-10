import { DISPLAY_LOCALE, t, translationCatalog } from "./i18n.ts";

function escapeHtml(value: string): string {
    return value.replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&#39;");
}

export function createEditorHtml(productName: string): string {
    const title = escapeHtml(t("app.title", { product: productName }));
    return `<!doctype html>
<html lang="${DISPLAY_LOCALE}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>${title}</title>
  <link rel="stylesheet" href="/app.css">
</head>
<body>
  <header>
    <div>
      <h1 id="title">${title}</h1>
      <p id="data-path-status">${t("reaperDataPath.help")}</p>
    </div>
    <div class="data-path-controls">
      <input id="data-path" list="data-path-candidates" placeholder="${t("reaperDataPath.placeholder")}">
      <datalist id="data-path-candidates"></datalist>
      <button id="open-data-path">${t("action.open")}</button>
    </div>
  </header>
  <main>
    <aside>
      <div class="aside-title">${t("files.title")}</div>
      <div id="tree" class="tree muted">${t("files.empty")}</div>
    </aside>
    <section class="workspace">
      <details class="import-panel">
        <summary>${t("legacy.title")}</summary>
        <div class="legacy-controls">
          <input id="legacy-path" placeholder="${t("legacy.path.placeholder")}">
          <button id="open-legacy">${t("action.open")}</button>
          <select id="legacy-surface" aria-label="${t("legacy.surface.title")}" disabled><option value="">${t("legacy.surface.placeholder")}</option></select>
          <label><input id="legacy-include-surface" type="checkbox" checked> ${t("legacy.includeSurface")}</label>
          <button id="legacy-select-all" disabled>${t("legacy.selectAll")}</button>
          <button id="legacy-select-none" disabled>${t("legacy.selectNone")}</button>
          <button id="legacy-refresh" disabled>${t("action.preview")}</button>
        </div>
        <p id="legacy-status" class="muted">${t("legacy.preview.empty")}</p>
        <div class="legacy-columns">
          <div><h2>${t("legacy.zones.title")}</h2><div id="legacy-zones" class="legacy-list muted">${t("legacy.zones.empty")}</div></div>
          <div><h2>${t("legacy.dependencies.title")}</h2><div id="legacy-dependencies" class="legacy-list muted">${t("legacy.dependencies.empty")}</div></div>
        </div>
        <h2>${t("legacy.preview.title")}</h2>
        <div id="legacy-diagnostics" class="muted">${t("problems.none")}</div>
        <div id="legacy-preview" class="legacy-preview muted">${t("legacy.preview.empty")}</div>
        <button id="legacy-import" disabled>${t("action.import")}</button>
      </details>
      <div class="document-header">
        <div>
          <strong id="document-path">${t("document.none")}</strong>
          <span id="document-mode"></span>
        </div>
        <div class="actions">
          <button id="clone" hidden>${t("action.makeEditable")}</button>
          <button id="validate" disabled>${t("action.check")}</button>
          <button id="add-batch" disabled>${t("action.addToSaveAll")}</button>
          <button id="save" disabled>${t("action.save")}</button>
        </div>
      </div>
      <div class="tabs">
        <button class="tab active" data-tab="raw">${t("tab.text")}</button>
        <button class="tab" data-tab="structured">${t("tab.guided")}</button>
      </div>
      <textarea id="raw-editor" spellcheck="false" disabled></textarea>
      <div id="structured-editor" hidden></div>
      <div class="details">
        <div>
          <h2>${t("problems.title")}</h2>
          <div id="diagnostics" class="muted">${t("problems.none")}</div>
        </div>
        <div>
          <h2>${t("details.title")}</h2>
          <pre id="semantic">{}</pre>
        </div>
      </div>
    </section>
  </main>
  <footer>
    <div><strong id="batch-count">${t("pending.count", { count: 0 })}</strong> <button id="commit-batch" disabled>${t("action.saveAll")}</button></div>
    <pre id="report">${t("status.ready")}</pre>
  </footer>
  <script src="/app.js"></script>
</body>
</html>`;
}

export const EDITOR_CSS = `
:root { color-scheme: light dark; font-family: system-ui, sans-serif; }
* { box-sizing: border-box; }
body { margin: 0; min-height: 100vh; display: grid; grid-template-rows: auto 1fr auto; background: #16181d; color: #e7e9ee; }
header { display: flex; gap: 24px; align-items: center; justify-content: space-between; padding: 14px 18px; border-bottom: 1px solid #343842; background: #20232a; }
h1 { margin: 0; font-size: 18px; } h2 { margin: 0 0 8px; font-size: 14px; }
p { margin: 4px 0 0; }
button, input, textarea { font: inherit; }
button { border: 1px solid #4a5060; border-radius: 4px; padding: 6px 10px; background: #303641; color: inherit; cursor: pointer; }
button:hover:not(:disabled) { background: #3b4351; } button:disabled { cursor: default; opacity: .45; }
input, textarea { border: 1px solid #424854; border-radius: 4px; background: #111318; color: inherit; }
.data-path-controls { display: flex; gap: 8px; min-width: min(620px, 55vw); } .data-path-controls input { flex: 1; padding: 7px 9px; }
main { min-height: 0; display: grid; grid-template-columns: minmax(230px, 25vw) 1fr; }
aside { overflow: auto; border-right: 1px solid #343842; padding: 12px; background: #1d2026; }
.aside-title { margin-bottom: 10px; font-weight: 650; }
.tree ul { list-style: none; margin: 2px 0; padding-left: 15px; }
.tree button { width: 100%; border: 0; padding: 4px 5px; text-align: left; background: transparent; }
.tree button:hover { background: #2c313b; } .tree .blocked { color: #e99b9b; padding: 4px 5px; }
.workspace { min-width: 0; min-height: 0; display: grid; grid-template-rows: auto auto auto minmax(250px, 1fr) minmax(140px, .45fr); }
.import-panel { max-height: 46vh; overflow: auto; padding: 9px 12px; border-bottom: 1px solid #343842; background: #1a1d23; }
.import-panel > summary { cursor: pointer; font-weight: 650; }
.legacy-controls { display: grid; grid-template-columns: minmax(240px, 1fr) auto minmax(170px, .55fr) auto auto auto auto; align-items: center; gap: 7px; margin-top: 10px; }
.legacy-controls input:not([type="checkbox"]), .legacy-controls select { min-width: 0; padding: 6px 8px; border: 1px solid #424854; border-radius: 4px; background: #111318; color: inherit; }
.legacy-columns { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 8px 0; }
.legacy-columns > div { min-width: 0; max-height: 150px; overflow: auto; border: 1px solid #343842; border-radius: 4px; padding: 8px; }
.legacy-list label, .legacy-dependency { display: block; padding: 3px 2px; overflow-wrap: anywhere; }
.legacy-dependency.warning { color: #e7c36e; }
.legacy-preview { display: grid; gap: 6px; margin: 7px 0; }
.legacy-item { border: 1px solid #343842; border-radius: 4px; padding: 7px; background: #20232a; }
.legacy-item-header { display: grid; grid-template-columns: minmax(220px, 1fr) minmax(190px, 1fr) auto minmax(180px, .55fr); gap: 8px; align-items: center; }
.legacy-item-header select, .legacy-item-header input { min-width: 0; padding: 5px 7px; }
.legacy-item details { margin-top: 6px; }.legacy-item pre { max-height: 180px; overflow: auto; white-space: pre; font-size: 12px; }
.document-header { display: flex; align-items: center; justify-content: space-between; gap: 12px; padding: 10px 12px; border-bottom: 1px solid #343842; }
#document-mode { margin-left: 8px; color: #aeb6c5; }.actions { display: flex; gap: 7px; }
.tabs { display: flex; gap: 4px; padding: 7px 12px 0; } .tab.active { border-bottom-color: #6ca3ff; color: #9fc1ff; }
#raw-editor { width: calc(100% - 24px); height: calc(100% - 12px); margin: 6px 12px; padding: 12px; resize: none; font-family: ui-monospace, monospace; line-height: 1.45; }
#structured-editor { overflow: auto; margin: 6px 12px; border: 1px solid #343842; border-radius: 4px; }
.structured-line { display: grid; grid-template-columns: 55px 1fr; align-items: center; gap: 8px; padding: 4px 7px; border-bottom: 1px solid #292d35; }
.structured-line input { width: 100%; padding: 5px 7px; font-family: ui-monospace, monospace; }
.line-number { color: #969ead; font-size: 12px; }
.details { min-height: 0; display: grid; grid-template-columns: 1fr 1fr; gap: 10px; padding: 8px 12px 12px; }
.details > div { min-width: 0; overflow: auto; border: 1px solid #343842; border-radius: 4px; padding: 9px; }
#semantic, #report { margin: 0; white-space: pre-wrap; overflow-wrap: anywhere; font-size: 12px; }
.diagnostic { margin: 3px 0; padding: 5px 7px; border-left: 3px solid #c7a34a; background: #292820; }
.diagnostic.error { border-color: #e46f6f; background: #302224; }
.muted { color: #969ead; }
footer { display: flex; align-items: start; justify-content: space-between; gap: 20px; min-height: 54px; padding: 9px 14px; border-top: 1px solid #343842; background: #20232a; }
#report { max-width: 65vw; max-height: 90px; overflow: auto; text-align: right; }
@media (max-width: 1100px) { .legacy-controls { grid-template-columns: 1fr auto; } .legacy-item-header { grid-template-columns: 1fr; } }
@media (max-width: 850px) { header { align-items: stretch; flex-direction: column; } .data-path-controls { min-width: 0; } main { grid-template-columns: 1fr; grid-template-rows: 210px 1fr; } aside { border-right: 0; border-bottom: 1px solid #343842; } .details, .legacy-columns { grid-template-columns: 1fr; } }
`;

const CLIENT_TRANSLATIONS = JSON.stringify(translationCatalog());

export const EDITOR_JAVASCRIPT = `const translations = ${CLIENT_TRANSLATIONS};\n` + String.raw`
const hashToken = new URLSearchParams(location.hash.slice(1)).get("token") || "";
if (hashToken) sessionStorage.setItem("config-editor-token", hashToken);
const token = hashToken || sessionStorage.getItem("config-editor-token") || "";
history.replaceState(null, "", location.pathname);

function requiredElement(id) {
    const element = document.getElementById(id);
    if (!element) throw new Error("Missing UI element: " + id);
    return element;
}

const elements = {
    addBatch: requiredElement("add-batch"),
    batchCount: requiredElement("batch-count"),
    clone: requiredElement("clone"),
    commitBatch: requiredElement("commit-batch"),
    dataPath: requiredElement("data-path"),
    dataPathCandidates: requiredElement("data-path-candidates"),
    dataPathStatus: requiredElement("data-path-status"),
    diagnostics: requiredElement("diagnostics"),
    documentMode: requiredElement("document-mode"),
    documentPath: requiredElement("document-path"),
    legacyDependencies: requiredElement("legacy-dependencies"),
    legacyDiagnostics: requiredElement("legacy-diagnostics"),
    legacyImport: requiredElement("legacy-import"),
    legacyIncludeSurface: requiredElement("legacy-include-surface"),
    legacyPath: requiredElement("legacy-path"),
    legacyPreview: requiredElement("legacy-preview"),
    legacyRefresh: requiredElement("legacy-refresh"),
    legacySelectAll: requiredElement("legacy-select-all"),
    legacySelectNone: requiredElement("legacy-select-none"),
    legacyStatus: requiredElement("legacy-status"),
    legacySurface: requiredElement("legacy-surface"),
    legacyZones: requiredElement("legacy-zones"),
    openDataPath: requiredElement("open-data-path"),
    openLegacy: requiredElement("open-legacy"),
    rawEditor: requiredElement("raw-editor"),
    report: requiredElement("report"),
    save: requiredElement("save"),
    semantic: requiredElement("semantic"),
    structuredEditor: requiredElement("structured-editor"),
    title: requiredElement("title"),
    tree: requiredElement("tree"),
    validate: requiredElement("validate"),
};
const state = { batch: new Map(), current: null, legacy: { preview: null, resolutions: new Map(), selectedZonePaths: new Set() }, tab: "raw" };

function translate(key, params = {}) {
    let text = translations[key];
    for (const [paramName, paramValue] of Object.entries(params)) text = text.replaceAll("{" + paramName + "}", String(paramValue));
    return text;
}

async function api(url, options = {}) {
    const headers = { "X-Session-Token": token, ...(options.body ? { "Content-Type": "application/json" } : {}), ...(options.headers || {}) };
    const response = await fetch(url, { ...options, headers });
    const payload = await response.json().catch(() => ({ error: { message: response.statusText } }));
    if (!response.ok) {
        const error = new Error(payload.error?.message || translate("error.request"));
        error.details = payload.error?.details;
        throw error;
    }
    return payload;
}

function showReport(value) {
    elements.report.textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
}

function showError(error) {
    showReport(error.details ? error.message + "\n" + JSON.stringify(error.details, null, 2) : error.message);
}

function renderDiagnosticsIn(container, diagnostics = []) {
    container.replaceChildren();
    if (!diagnostics.length) {
        container.className = "muted";
        container.textContent = translate("problems.none");
        return;
    }
    container.className = "";
    for (const diagnostic of diagnostics) {
        const row = document.createElement("div");
        row.className = "diagnostic " + diagnostic.severity;
        row.textContent = (diagnostic.path ? diagnostic.path + ": " : "") + (diagnostic.line ? translate("diagnostic.line", { line: diagnostic.line }) : "") + diagnostic.severity.toUpperCase() + " " + diagnostic.code + ": " + diagnostic.message;
        container.append(row);
    }
}

function renderDiagnostics(diagnostics = []) {
    renderDiagnosticsIn(elements.diagnostics, diagnostics);
}

function rebuildSourceFromStructured() {
    if (!state.current) return "";
    const replacements = new Map([...elements.structuredEditor.querySelectorAll("input[data-line]")].map((input) => [Number(input.dataset.line), input.value]));
    return state.current.document.lines.map((line) => (replacements.get(line.lineNumber) ?? line.text) + line.ending).join("");
}

function renderStructured() {
    elements.structuredEditor.replaceChildren();
    if (!state.current) return;
    for (const line of state.current.document.lines.filter((candidate) => !["blank", "comment"].includes(candidate.kind))) {
        const row = document.createElement("label");
        row.className = "structured-line";
        const lineNumber = document.createElement("span");
        lineNumber.className = "line-number";
        lineNumber.textContent = String(line.lineNumber);
        const input = document.createElement("input");
        input.dataset.line = String(line.lineNumber);
        input.disabled = !state.current.writable;
        input.value = line.text;
        input.addEventListener("input", () => { elements.rawEditor.value = rebuildSourceFromStructured(); });
        row.append(lineNumber, input);
        elements.structuredEditor.append(row);
    }
}

function renderDocument() {
    const current = state.current;
    elements.documentPath.textContent = current?.path || translate("document.none");
    elements.documentMode.textContent = current ? (current.writable ? translate("document.editable") : translate("document.readOnly")) : "";
    elements.rawEditor.disabled = !current || !current.writable;
    elements.rawEditor.value = current?.source || "";
    elements.validate.disabled = !current;
    elements.addBatch.disabled = !current || !current.writable;
    elements.save.disabled = !current || !current.writable;
    elements.clone.hidden = !current || current.writable || !["surface", "zone", "snippet"].includes(current.document.format);
    renderDiagnostics(current?.document.diagnostics);
    elements.semantic.textContent = JSON.stringify(current?.document.semantic || {}, null, 2);
    renderStructured();
}

async function validateCurrent() {
    if (!state.current) return null;
    const source = elements.rawEditor.value;
    const result = await api("/api/validate", { method: "POST", body: JSON.stringify({ path: state.current.path, source }) });
    state.current = { ...state.current, document: result.document, source };
    renderDocument();
    return result.document;
}

async function openDocument(path) {
    try {
        state.current = await api("/api/file?path=" + encodeURIComponent(path));
        renderDocument();
        showReport(translate("status.openedFile", { path }));
    } catch (error) {
        showError(error);
    }
}

function treeList(entries) {
    const list = document.createElement("ul");
    for (const entry of entries) {
        const item = document.createElement("li");
        if (entry.kind === "directory") {
            const details = document.createElement("details");
            details.open = true;
            const summary = document.createElement("summary");
            summary.textContent = entry.name;
            details.append(summary, treeList(entry.children || []));
            item.append(details);
        } else if (entry.kind === "blocked") {
            item.className = "blocked";
            item.title = entry.reason || translate("files.blocked");
            item.textContent = entry.name + " (" + translate("files.blocked") + ")";
        } else {
            const button = document.createElement("button");
            button.textContent = entry.name + (entry.writable ? "" : " (" + translate("files.readOnly") + ")");
            button.addEventListener("click", () => openDocument(entry.path));
            item.append(button);
        }
        list.append(item);
    }
    return list;
}

async function refreshTree() {
    const result = await api("/api/tree");
    elements.tree.className = "tree";
    elements.tree.replaceChildren(treeList(result.entries));
}

function updateBatch() {
    elements.batchCount.textContent = translate("pending.count", { count: state.batch.size });
    elements.commitBatch.disabled = state.batch.size === 0;
}

function renameSuggestion(targetPath) {
    const extensionPosition = targetPath.lastIndexOf(".");
    if (extensionPosition < 0) return targetPath + "-imported";
    return targetPath.slice(0, extensionPosition) + "-imported" + targetPath.slice(extensionPosition);
}

function selectedLegacyItems() {
    return (state.legacy.preview?.items || []).filter((item) => item.selected);
}

function resolutionFor(item) {
    let resolution = state.legacy.resolutions.get(item.id);
    if (!resolution || resolution.sourceHash !== item.sourceHash || resolution.targetHash !== item.targetHash) {
        resolution = { action: item.targetExists ? "" : "create", id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash };
        state.legacy.resolutions.set(item.id, resolution);
    }
    return resolution;
}

function updateLegacyImportButton() {
    const preview = state.legacy.preview;
    const selectedItems = selectedLegacyItems();
    elements.legacyImport.disabled = !preview || !preview.valid || !selectedItems.length || selectedItems.some((item) => {
        const resolution = resolutionFor(item);
        if (!item.targetExists) return resolution.action !== "create";
        if (!["rename", "replace", "skip"].includes(resolution.action)) return true;
        return resolution.action === "rename" && !resolution.targetPath;
    });
}

function renderLegacyZones() {
    const preview = state.legacy.preview;
    const zones = (preview?.items || []).filter((item) => item.kind === "zone");
    elements.legacyZones.replaceChildren();
    if (!zones.length) {
        elements.legacyZones.className = "legacy-list muted";
        elements.legacyZones.textContent = translate("legacy.zones.empty");
        return;
    }
    elements.legacyZones.className = "legacy-list";
    for (const zone of zones) {
        const label = document.createElement("label");
        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.checked = state.legacy.selectedZonePaths.has(zone.sourcePath);
        checkbox.addEventListener("change", async () => {
            try {
                if (checkbox.checked) {
                    state.legacy.selectedZonePaths.add(zone.sourcePath);
                    const pendingPaths = [zone.sourcePath];
                    const visitedPaths = new Set();
                    while (pendingPaths.length) {
                        const sourcePath = pendingPaths.shift();
                        if (!sourcePath || visitedPaths.has(sourcePath)) continue;
                        visitedPaths.add(sourcePath);
                        for (const dependency of preview.dependencies.filter((candidate) => candidate.from === sourcePath && candidate.matches.length === 1)) {
                            const dependencyPath = dependency.matches[0];
                            if (!state.legacy.selectedZonePaths.has(dependencyPath)) {
                                state.legacy.selectedZonePaths.add(dependencyPath);
                                pendingPaths.push(dependencyPath);
                            }
                        }
                    }
                } else state.legacy.selectedZonePaths.delete(zone.sourcePath);
                await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
            } catch (error) { showError(error); }
        });
        label.append(checkbox, document.createTextNode(" " + zone.sourcePath + (zone.zoneName ? " [" + zone.zoneName + "]" : "")));
        elements.legacyZones.append(label);
    }
}

function renderLegacyDependencies() {
    const preview = state.legacy.preview;
    const dependencies = (preview?.dependencies || []).filter((dependency) => dependency.selected);
    elements.legacyDependencies.replaceChildren();
    if (!dependencies.length) {
        elements.legacyDependencies.className = "legacy-list muted";
        elements.legacyDependencies.textContent = translate("legacy.dependencies.empty");
        return;
    }
    elements.legacyDependencies.className = "legacy-list";
    for (const dependency of dependencies) {
        const selectedMatches = dependency.matches.filter((match) => state.legacy.selectedZonePaths.has(match));
        const row = document.createElement("div");
        row.className = "legacy-dependency" + (dependency.matches.length !== 1 || selectedMatches.length !== 1 ? " warning" : "");
        row.textContent = dependency.from + ": " + dependency.type + " " + dependency.name + " -> " + (dependency.matches.length ? dependency.matches.join(", ") : "?");
        elements.legacyDependencies.append(row);
    }
}

function renderLegacyPreview() {
    const preview = state.legacy.preview;
    renderLegacyZones();
    renderLegacyDependencies();
    elements.legacyPreview.replaceChildren();
    if (!preview) {
        elements.legacyPreview.className = "legacy-preview muted";
        elements.legacyPreview.textContent = translate("legacy.preview.empty");
        renderDiagnosticsIn(elements.legacyDiagnostics);
        updateLegacyImportButton();
        return;
    }
    elements.legacyStatus.className = preview.valid ? "" : "diagnostic error";
    elements.legacyStatus.textContent = translate(preview.valid ? "legacy.preview.valid" : "legacy.preview.invalid");
    renderDiagnosticsIn(elements.legacyDiagnostics, preview.diagnostics);
    const selectedItems = selectedLegacyItems();
    if (!selectedItems.length) {
        elements.legacyPreview.className = "legacy-preview muted";
        elements.legacyPreview.textContent = translate("legacy.preview.empty");
        updateLegacyImportButton();
        return;
    }
    elements.legacyPreview.className = "legacy-preview";
    for (const item of selectedItems) {
        const container = document.createElement("div");
        container.className = "legacy-item";
        const header = document.createElement("div");
        header.className = "legacy-item-header";
        const sourcePath = document.createElement("span");
        sourcePath.textContent = translate("legacy.source") + ": " + item.sourcePath;
        const targetPath = document.createElement("span");
        targetPath.textContent = translate("legacy.target") + ": " + item.targetPath;
        const resolution = resolutionFor(item);
        let actionControl;
        let renameInput;
        if (item.targetExists) {
            actionControl = document.createElement("select");
            for (const [value, key] of [["", "legacy.conflict.choose"], ["replace", "legacy.conflict.replace"], ["rename", "legacy.conflict.rename"], ["skip", "legacy.conflict.skip"]]) {
                const option = document.createElement("option");
                option.value = value;
                option.textContent = translate(key);
                actionControl.append(option);
            }
            actionControl.value = resolution.action;
            renameInput = document.createElement("input");
            renameInput.value = resolution.targetPath || renameSuggestion(item.targetPath);
            renameInput.hidden = resolution.action !== "rename";
            renameInput.addEventListener("input", () => { resolution.targetPath = renameInput.value; updateLegacyImportButton(); });
            actionControl.addEventListener("change", () => {
                resolution.action = actionControl.value;
                if (resolution.action === "rename" && !resolution.targetPath) resolution.targetPath = renameInput.value;
                renameInput.hidden = resolution.action !== "rename";
                updateLegacyImportButton();
            });
        } else {
            actionControl = document.createElement("span");
            actionControl.textContent = translate("legacy.conflict.create");
            renameInput = document.createElement("span");
        }
        header.append(sourcePath, targetPath, actionControl, renameInput);
        const details = document.createElement("details");
        const summary = document.createElement("summary");
        summary.textContent = item.sourcePath;
        const source = document.createElement("pre");
        source.textContent = item.source;
        details.append(summary, source);
        container.append(header, details);
        elements.legacyPreview.append(container);
    }
    updateLegacyImportButton();
}

async function refreshLegacyPreview(selectedZonePaths) {
    if (!elements.legacySurface.value) return;
    const body = { includeSurface: elements.legacyIncludeSurface.checked, surfaceName: elements.legacySurface.value };
    if (selectedZonePaths !== undefined) body.selectedZonePaths = selectedZonePaths;
    const result = await api("/api/legacy/preview", { method: "POST", body: JSON.stringify(body) });
    state.legacy.preview = result.preview;
    state.legacy.selectedZonePaths = new Set(result.preview.selectedZonePaths);
    elements.legacySelectAll.disabled = false;
    elements.legacySelectNone.disabled = false;
    elements.legacyRefresh.disabled = false;
    renderLegacyPreview();
}

async function initialize() {
    if (!token) {
        showReport(translate("error.missingToken"));
        return;
    }
    try {
        const status = await api("/api/status");
        const title = translate("app.title", { product: status.identity.displayName });
        elements.title.textContent = title;
        document.title = title;
        for (const candidate of status.candidates) {
            const option = document.createElement("option");
            option.value = candidate.path;
            const source = translate(candidate.source);
            option.label = candidate.exists ? source : translate("candidate.notFound", { source });
            elements.dataPathCandidates.append(option);
        }
        if (status.candidates.length) elements.dataPath.value = status.candidates[0].path;
        if (status.dataPath) {
            elements.dataPathStatus.textContent = status.dataPath;
            elements.dataPath.value = status.dataPath;
            await refreshTree();
        }
    } catch (error) {
        showReport(translate("error.configLoad") + "\n" + error.message);
    }
}

elements.openDataPath.addEventListener("click", async () => {
    try {
        const result = await api("/api/select-data-path", { method: "POST", body: JSON.stringify({ path: elements.dataPath.value }) });
        elements.dataPathStatus.textContent = result.dataPath;
        state.current = null;
        state.batch.clear();
        updateBatch();
        renderDocument();
        await refreshTree();
        if (elements.legacySurface.value) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
        showReport(translate("status.openedDataPath", { path: result.dataPath }));
    } catch (error) { showError(error); }
});

elements.openLegacy.addEventListener("click", async () => {
    try {
        const result = await api("/api/legacy/select", { method: "POST", body: JSON.stringify({ path: elements.legacyPath.value }) });
        state.legacy.preview = null;
        state.legacy.resolutions.clear();
        state.legacy.selectedZonePaths.clear();
        elements.legacySurface.replaceChildren();
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = translate("legacy.surface.placeholder");
        elements.legacySurface.append(placeholder);
        for (const surface of result.surfaces) {
            const option = document.createElement("option");
            option.value = surface.name;
            option.textContent = translate("legacy.surface.option", { count: surface.zoneCount, name: surface.name });
            elements.legacySurface.append(option);
        }
        elements.legacySurface.disabled = result.surfaces.length === 0;
        elements.legacySelectAll.disabled = true;
        elements.legacySelectNone.disabled = true;
        elements.legacyRefresh.disabled = true;
        elements.legacyStatus.className = "muted";
        elements.legacyStatus.textContent = translate("legacy.status.opened", { path: result.root });
        renderLegacyPreview();
    } catch (error) { showError(error); }
});

elements.legacySurface.addEventListener("change", async () => {
    try {
        state.legacy.resolutions.clear();
        state.legacy.selectedZonePaths.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview();
        else {
            state.legacy.preview = null;
            renderLegacyPreview();
        }
    } catch (error) { showError(error); }
});

elements.legacyIncludeSurface.addEventListener("change", async () => {
    try { if (elements.legacySurface.value) await refreshLegacyPreview([...state.legacy.selectedZonePaths]); } catch (error) { showError(error); }
});

elements.legacySelectAll.addEventListener("click", async () => {
    try {
        const zonePaths = state.legacy.preview.items.filter((item) => item.kind === "zone").map((item) => item.sourcePath);
        await refreshLegacyPreview(zonePaths);
    } catch (error) { showError(error); }
});

elements.legacySelectNone.addEventListener("click", async () => {
    try { await refreshLegacyPreview([]); } catch (error) { showError(error); }
});

elements.legacyRefresh.addEventListener("click", async () => {
    try { await refreshLegacyPreview([...state.legacy.selectedZonePaths]); } catch (error) { showError(error); }
});

elements.legacyImport.addEventListener("click", async () => {
    try {
        const selectedItems = selectedLegacyItems();
        const resolutions = selectedItems.map((item) => resolutionFor(item));
        const result = await api("/api/legacy/import", {
            method: "POST",
            body: JSON.stringify({
                includeSurface: elements.legacyIncludeSurface.checked,
                resolutions,
                selectedZonePaths: [...state.legacy.selectedZonePaths],
                surfaceName: elements.legacySurface.value,
            }),
        });
        await refreshTree();
        await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
        showReport(translate("status.importedLegacy", { count: result.report.changed.length + result.report.created.length }));
    } catch (error) { showError(error); }
});

elements.validate.addEventListener("click", async () => {
    try { await validateCurrent(); showReport(translate("status.checked")); } catch (error) { showError(error); }
});

elements.save.addEventListener("click", async () => {
    try {
        const documentView = await validateCurrent();
        if (documentView.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new Error(translate("error.fixBeforeSave"));
        const result = await api("/api/save", { method: "POST", body: JSON.stringify({ originalHash: state.current.hash, path: state.current.path, source: state.current.source }) });
        state.current.hash = result.hash;
        state.batch.delete(state.current.path);
        updateBatch();
        showReport(translate("status.savedFile", { path: state.current.path }));
    } catch (error) { showError(error); }
});

elements.addBatch.addEventListener("click", async () => {
    try {
        const documentView = await validateCurrent();
        if (documentView.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new Error(translate("error.fixBeforeSaveAll"));
        state.batch.set(state.current.path, { originalHash: state.current.hash, path: state.current.path, source: state.current.source });
        updateBatch();
        showReport(translate("status.addedToSaveAll", { path: state.current.path }));
    } catch (error) { showError(error); }
});

elements.commitBatch.addEventListener("click", async () => {
    try {
        const result = await api("/api/transaction", { method: "POST", body: JSON.stringify({ changes: [...state.batch.values()] }) });
        state.batch.clear();
        state.current = null;
        updateBatch();
        renderDocument();
        await refreshTree();
        showReport(translate("status.savedAll", { count: result.report.changed.length + result.report.created.length }));
    } catch (error) { showError(error); }
});

elements.clone.addEventListener("click", async () => {
    try {
        await api("/api/clone", { method: "POST", body: JSON.stringify({ path: state.current.path }) });
        await refreshTree();
        showReport(translate("status.userCopyCreated"));
    } catch (error) { showError(error); }
});

for (const tab of document.querySelectorAll(".tab")) tab.addEventListener("click", async () => {
    try {
        if (state.current) await validateCurrent();
        state.tab = tab.dataset.tab;
        for (const candidate of document.querySelectorAll(".tab")) candidate.classList.toggle("active", candidate === tab);
        elements.rawEditor.hidden = state.tab !== "raw";
        elements.structuredEditor.hidden = state.tab !== "structured";
    } catch (error) { showError(error); }
});

elements.rawEditor.addEventListener("input", () => { if (state.current) state.current.source = elements.rawEditor.value; });
initialize();
`;
