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
.workspace { min-width: 0; min-height: 0; display: grid; grid-template-rows: auto auto minmax(250px, 1fr) minmax(140px, .45fr); }
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
@media (max-width: 850px) { header { align-items: stretch; flex-direction: column; } .data-path-controls { min-width: 0; } main { grid-template-columns: 1fr; grid-template-rows: 210px 1fr; } aside { border-right: 0; border-bottom: 1px solid #343842; } .details { grid-template-columns: 1fr; } }
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
    openDataPath: requiredElement("open-data-path"),
    rawEditor: requiredElement("raw-editor"),
    report: requiredElement("report"),
    save: requiredElement("save"),
    semantic: requiredElement("semantic"),
    structuredEditor: requiredElement("structured-editor"),
    title: requiredElement("title"),
    tree: requiredElement("tree"),
    validate: requiredElement("validate"),
};
const state = { batch: new Map(), current: null, tab: "raw" };

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

function renderDiagnostics(diagnostics = []) {
    elements.diagnostics.replaceChildren();
    if (!diagnostics.length) {
        elements.diagnostics.className = "muted";
        elements.diagnostics.textContent = translate("problems.none");
        return;
    }
    elements.diagnostics.className = "";
    for (const diagnostic of diagnostics) {
        const row = document.createElement("div");
        row.className = "diagnostic " + diagnostic.severity;
        row.textContent = (diagnostic.line ? translate("diagnostic.line", { line: diagnostic.line }) : "") + diagnostic.severity.toUpperCase() + " " + diagnostic.code + ": " + diagnostic.message;
        elements.diagnostics.append(row);
    }
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
        showReport(translate("status.openedDataPath", { path: result.dataPath }));
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
