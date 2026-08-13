import { createConfigurationEditor } from "./code-editor.js";

let translations = {};
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
    backToTasks: requiredElement("back-to-tasks"),
    batchControls: requiredElement("batch-controls"),
    batchCount: requiredElement("batch-count"),
    clone: requiredElement("clone"),
    commitBatch: requiredElement("commit-batch"),
    dataPath: requiredElement("data-path"),
    dataPathCandidates: requiredElement("data-path-candidates"),
    dataPathStatus: requiredElement("data-path-status"),
    diagnostics: requiredElement("diagnostics"),
    documentMode: requiredElement("document-mode"),
    documentPath: requiredElement("document-path"),
    editorBody: requiredElement("editor-body"),
    editorMain: requiredElement("editor-main"),
    exportSnippet: requiredElement("export-snippet"),
    filePanel: requiredElement("file-panel"),
    legacyDependencies: requiredElement("legacy-dependencies"),
    legacyDiagnostics: requiredElement("legacy-diagnostics"),
    legacyDraftCheck: requiredElement("legacy-draft-check"),
    legacyDraftDiscard: requiredElement("legacy-draft-discard"),
    legacyDraftEditor: requiredElement("legacy-draft-editor"),
    legacyDraftPanel: requiredElement("legacy-draft-panel"),
    legacyDraftPath: requiredElement("legacy-draft-path"),
    legacyImport: requiredElement("legacy-import"),
    legacyIncludeSurface: requiredElement("legacy-include-surface"),
    legacyPath: requiredElement("legacy-path"),
    legacyPreview: requiredElement("legacy-preview"),
    legacyRefresh: requiredElement("legacy-refresh"),
    legacySelectAll: requiredElement("legacy-select-all"),
    legacySelectNone: requiredElement("legacy-select-none"),
    legacyStatus: requiredElement("legacy-status"),
    legacySurface: requiredElement("legacy-surface"),
    legacyTargetProfile: requiredElement("legacy-target-profile"),
    legacyWidgetMappings: requiredElement("legacy-widget-mappings"),
    legacyZones: requiredElement("legacy-zones"),
    homeHeader: requiredElement("home-header"),
    openDataPath: requiredElement("open-data-path"),
    openLegacy: requiredElement("open-legacy"),
    rawEditor: requiredElement("raw-editor"),
    report: requiredElement("report"),
    save: requiredElement("save"),
    semantic: requiredElement("semantic"),
    snippetApplicationId: requiredElement("snippet-application-id"),
    snippetApply: requiredElement("snippet-apply"),
    snippetBindings: requiredElement("snippet-bindings"),
    snippetConflict: requiredElement("snippet-conflict"),
    snippetConflictAction: requiredElement("snippet-conflict-action"),
    snippetDiagnostics: requiredElement("snippet-diagnostics"),
    snippetImportAction: requiredElement("snippet-import-action"),
    snippetImportApply: requiredElement("snippet-import-apply"),
    snippetImportConflict: requiredElement("snippet-import-conflict"),
    snippetImportDiagnostics: requiredElement("snippet-import-diagnostics"),
    snippetImportFile: requiredElement("snippet-import-file"),
    snippetImportSource: requiredElement("snippet-import-source"),
    snippetImportTarget: requiredElement("snippet-import-target"),
    snippetExportDownload: requiredElement("snippet-export-download"),
    snippetExportSource: requiredElement("snippet-export-source"),
    snippetMappingStep: requiredElement("snippet-mapping-step"),
    snippetRenameId: requiredElement("snippet-rename-id"),
    snippetSource: requiredElement("snippet-source"),
    snippetStatus: requiredElement("snippet-status"),
    snippetSurface: requiredElement("snippet-surface"),
    snippetTargetDiagnostics: requiredElement("snippet-target-diagnostics"),
    snippetTargetEditor: requiredElement("snippet-target-editor"),
    snippetTargetPath: requiredElement("snippet-target-path"),
    snippetTargetSave: requiredElement("snippet-target-save"),
    snippetZone: requiredElement("snippet-zone"),
    structuredEditor: requiredElement("structured-editor"),
    taskHome: requiredElement("task-home"),
    taskHomeStatus: requiredElement("task-home-status"),
    title: requiredElement("title"),
    tree: requiredElement("tree"),
    validate: requiredElement("validate"),
    workflowDescription: requiredElement("workflow-description"),
    workflowEdit: requiredElement("workflow-edit"),
    workflowLegacy: requiredElement("workflow-legacy"),
    workflowShare: requiredElement("workflow-share"),
    workflowSnippet: requiredElement("workflow-snippet"),
    workflowTitle: requiredElement("workflow-title"),
    workspace: requiredElement("workspace"),
};
const state = {
    batch: new Map(),
    current: null,
    legacy: { activeDraftPath: "", drafts: new Map(), preview: null, resolutions: new Map(), selectedZonePaths: new Set(), targetPaths: new Map(), targetProfileId: "", widgetMappings: new Map() },
    snippet: { choices: new Map(), conflictAction: "", importAction: "", importFile: null, importPreview: null, preview: null, targetDiagnostics: [], targetDirty: false, targetHash: "", targetPath: "", treeEntries: [] },
    renderedDocumentPath: "",
    tab: "raw",
    task: "",
};
const codeEditor = createConfigurationEditor(elements.rawEditor, (source) => { if (state.current) state.current.source = source; });
const legacyDraftEditor = createConfigurationEditor(elements.legacyDraftEditor, (source) => {
    const item = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === state.legacy.activeDraftPath);
    if (item) state.legacy.drafts.set(item.sourcePath, { originalSourceHash: item.originalSourceHash, source });
});
const snippetTargetEditor = createConfigurationEditor(elements.snippetTargetEditor, () => {
    if (!state.snippet.targetPath) return;
    state.snippet.targetDiagnostics = [];
    state.snippet.targetDirty = true;
    renderSnippetTarget();
});
legacyDraftEditor.setReadOnly(false);
legacyDraftEditor.setVisible(false);
snippetTargetEditor.setReadOnly(false);
snippetTargetEditor.setVisible(false);

function translate(key, params = {}) {
    let text = translations[key] ?? key;
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

function setTaskAvailability(available) {
    for (const button of document.querySelectorAll(".task-card")) button.disabled = !available;
    elements.taskHomeStatus.textContent = translate(available ? "tasks.ready" : "tasks.openDataFirst");
}

function showTask(task) {
    const workflows = { edit: elements.workflowEdit, legacy: elements.workflowLegacy, share: elements.workflowShare, snippet: elements.workflowSnippet };
    const translationTask = task === "snippet" ? "apply" : task;
    state.task = task;
    elements.homeHeader.hidden = true;
    elements.taskHome.hidden = true;
    elements.editorMain.hidden = false;
    elements.filePanel.hidden = task !== "edit";
    elements.batchControls.hidden = task !== "edit";
    elements.editorBody.classList.toggle("file-task", task === "edit");
    elements.workspace.classList.toggle("document-task", task === "edit");
    elements.workspace.classList.toggle("snippet-task", task === "snippet");
    for (const workflow of Object.values(workflows)) workflow.hidden = workflow !== workflows[task];
    elements.workflowTitle.textContent = translate(`task.${translationTask}.title`);
    elements.workflowDescription.textContent = translate(`task.${translationTask}.description`);
    if (task === "edit") codeEditor.setVisible(state.tab === "raw");
    snippetTargetEditor.setVisible(task === "snippet");
}

function showTaskHome() {
    state.task = "";
    elements.homeHeader.hidden = false;
    elements.editorMain.hidden = true;
    elements.batchControls.hidden = true;
    elements.taskHome.hidden = false;
}

function renderDiagnosticsIn(container, diagnostics = [], navigate = navigateDiagnostic) {
    container.replaceChildren();
    if (!diagnostics.length) {
        container.className = "muted";
        container.textContent = translate("problems.none");
        return;
    }
    container.className = "";
    for (const diagnostic of diagnostics) {
        const actionable = Boolean(diagnostic.path || diagnostic.line);
        const row = document.createElement(actionable ? "button" : "div");
        row.className = "diagnostic " + diagnostic.severity;
        if (actionable) {
            row.classList.add("diagnostic-link");
            row.addEventListener("click", () => navigate(diagnostic));
        }
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
        input.addEventListener("input", () => { codeEditor.setValue(rebuildSourceFromStructured()); });
        row.append(lineNumber, input);
        elements.structuredEditor.append(row);
    }
}

function renderDocument() {
    const current = state.current;
    const documentPath = current?.path || "";
    elements.documentPath.textContent = current?.path || translate("document.none");
    elements.documentMode.textContent = current ? (current.writable ? translate("document.editable") : translate("document.readOnly")) : "";
    codeEditor.setReadOnly(!current || !current.writable);
    codeEditor.setValue(current?.source || "", documentPath !== state.renderedDocumentPath);
    state.renderedDocumentPath = documentPath;
    elements.validate.disabled = !current;
    elements.addBatch.disabled = !current || !current.writable;
    elements.save.disabled = !current || !current.writable;
    elements.clone.hidden = !current || current.writable || !["surface", "zone", "snippet"].includes(current.document.format);
    elements.exportSnippet.hidden = !current || current.document.format !== "snippet";
    renderDiagnostics(current?.document.diagnostics);
    elements.semantic.textContent = JSON.stringify(current?.document.semantic || {}, null, 2);
    renderStructured();
}

async function validateCurrent() {
    if (!state.current) return null;
    const source = codeEditor.getValue();
    const result = await api("/api/validate", { method: "POST", body: JSON.stringify({ path: state.current.path, source }) });
    state.current = { ...state.current, document: result.document, source };
    renderDocument();
    return result.document;
}

async function openDocument(path, line) {
    try {
        state.current = await api("/api/file?path=" + encodeURIComponent(path));
        updateTreeSelection(path);
        renderDocument();
        if (line) requestAnimationFrame(() => codeEditor.goToLine(line));
        showReport(translate("status.openedFile", { path }));
    } catch (error) {
        showError(error);
    }
}

async function navigateDiagnostic(diagnostic) {
    if (state.task === "legacy") {
        const item = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === diagnostic.path || candidate.targetPath === diagnostic.path);
        if (item) {
            openLegacyDraft(item, diagnostic.line);
            return;
        }
    }
    if (!diagnostic.path && !state.current) return;
    showTask("edit");
    state.tab = "raw";
    for (const tab of document.querySelectorAll(".tab")) tab.classList.toggle("active", tab.dataset.tab === "raw");
    codeEditor.setVisible(true);
    elements.structuredEditor.hidden = true;
    if (diagnostic.path && diagnostic.path !== state.current?.path) await openDocument(diagnostic.path, diagnostic.line);
    else if (diagnostic.line) requestAnimationFrame(() => codeEditor.goToLine(diagnostic.line));
}

function hasWritableTreeEntry(entry) {
    if (entry.kind === "file") return entry.writable;
    if (entry.kind === "directory") return (entry.children || []).some(hasWritableTreeEntry);
    return false;
}

function treeList(entries) {
    const list = document.createElement("ul");
    for (const entry of entries) {
        const item = document.createElement("li");
        if (entry.kind === "directory") {
            const details = document.createElement("details");
            details.open = true;
            details.classList.toggle("read-only", !hasWritableTreeEntry(entry));
            const summary = document.createElement("summary");
            const folderIcon = document.createElement("span");
            folderIcon.className = "folder-icon";
            folderIcon.setAttribute("aria-hidden", "true");
            summary.append(folderIcon, document.createTextNode(entry.name));
            details.append(summary, treeList(entry.children || []));
            item.append(details);
        } else if (entry.kind === "blocked") {
            item.className = "blocked";
            item.title = entry.reason || translate("files.blocked");
            item.textContent = entry.name + " (" + translate("files.blocked") + ")";
        } else {
            const button = document.createElement("button");
            button.dataset.path = entry.path;
            button.textContent = entry.name;
            button.classList.toggle("read-only", !entry.writable);
            button.classList.toggle("selected", entry.path === state.current?.path);
            if (entry.path === state.current?.path) button.setAttribute("aria-current", "page");
            if (!entry.writable) button.title = translate("files.readOnly");
            button.addEventListener("click", () => openDocument(entry.path));
            item.append(button);
        }
        list.append(item);
    }
    return list;
}

function updateTreeSelection(selectedPath) {
    for (const button of elements.tree.querySelectorAll("button[data-path]")) {
        const selected = button.dataset.path === selectedPath;
        button.classList.toggle("selected", selected);
        if (selected) button.setAttribute("aria-current", "page");
        else button.removeAttribute("aria-current");
    }
}

async function refreshTree() {
    const result = await api("/api/tree");
    state.snippet.treeEntries = result.entries;
    elements.tree.className = "tree";
    elements.tree.replaceChildren(treeList(result.entries));
    renderSnippetPathOptions();
}

function updateBatch() {
    elements.batchCount.textContent = translate("pending.count", { count: state.batch.size });
    elements.commitBatch.disabled = state.batch.size === 0;
}

function flattenConfigFiles(entries, result = []) {
    for (const entry of entries) {
        if (entry.kind === "file") result.push(entry);
        else if (entry.kind === "directory") flattenConfigFiles(entry.children || [], result);
    }
    return result;
}

function fillPathSelect(select, files, placeholderKey) {
    const selectedPath = select.value;
    select.replaceChildren();
    const placeholder = document.createElement("option");
    placeholder.value = "";
    placeholder.textContent = translate(placeholderKey);
    select.append(placeholder);
    for (const file of files.sort((left, right) => left.path.localeCompare(right.path))) {
        const option = document.createElement("option");
        option.value = file.path;
        option.textContent = file.path;
        select.append(option);
    }
    select.value = files.some((file) => file.path === selectedPath) ? selectedPath : "";
    select.disabled = files.length === 0;
}

function renderSnippetPathOptions() {
    const files = flattenConfigFiles(state.snippet.treeEntries);
    const snippets = files.filter((file) => file.type === "snippet");
    fillPathSelect(elements.snippetSource, snippets, "snippet.source.choose");
    fillPathSelect(elements.snippetExportSource, snippets, "snippet.source.choose");
    fillPathSelect(elements.snippetSurface, files.filter((file) => file.type === "surface"), "snippet.surface.choose");
    fillPathSelect(elements.snippetZone, files.filter((file) => file.type === "zone" && file.writable), "snippet.zone.choose");
    elements.snippetApply.disabled = !snippetPathsReady();
    elements.snippetExportDownload.disabled = !elements.snippetExportSource.value;
}

function snippetPathsReady() {
    return Boolean(elements.snippetSource.value && elements.snippetSurface.value && elements.snippetZone.value);
}

function snippetChoicesForRequest() {
    return [...state.snippet.choices].map(([bindingId, choice]) => ({ bindingId, ...choice }));
}

function snippetApplicationBody() {
    return {
        applicationId: elements.snippetApplicationId.value,
        bindingChoices: snippetChoicesForRequest(),
        conflictAction: state.snippet.conflictAction,
        renamedApplicationId: elements.snippetRenameId.value,
        snippetPath: elements.snippetSource.value,
        surfacePath: elements.snippetSurface.value,
        targetZonePath: elements.snippetZone.value,
    };
}

function invalidateSnippetPreview() {
    if (state.snippet.preview) state.snippet.preview.valid = false;
    elements.snippetApply.disabled = !snippetPathsReady();
    elements.snippetStatus.className = "muted";
    elements.snippetStatus.textContent = translate("snippet.preview.invalid");
}

function renderSnippetConflict() {
    const preview = state.snippet.preview;
    const existingApplicationId = preview?.conflict.existingApplicationId;
    elements.snippetConflict.hidden = !existingApplicationId;
    elements.snippetConflictAction.replaceChildren();
    if (!existingApplicationId) return;
    for (const [value, key] of [["", "legacy.conflict.choose"], ["replace", "legacy.conflict.replace"], ["rename", "legacy.conflict.rename"], ["skip", "legacy.conflict.skip"]]) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = translate(key);
        elements.snippetConflictAction.append(option);
    }
    elements.snippetConflictAction.value = state.snippet.conflictAction;
    elements.snippetRenameId.hidden = state.snippet.conflictAction !== "rename";
}

function selectedSnippetCandidate(binding, widgetName) {
    return binding.candidates.find((candidate) => candidate.name.toLowerCase() === widgetName.toLowerCase());
}

function renderSnippetBindings() {
    const bindings = state.snippet.preview?.bindings || [];
    elements.snippetBindings.replaceChildren();
    if (!bindings.length) {
        elements.snippetBindings.className = "snippet-bindings muted";
        elements.snippetBindings.textContent = translate("snippet.bindings.empty");
        return;
    }
    elements.snippetBindings.className = "snippet-bindings";
    for (const binding of bindings) {
        let choice = state.snippet.choices.get(binding.id);
        if (!choice) {
            choice = { allowIncompatible: false, confirmed: binding.automatic, widgetName: binding.selectedWidgetName || binding.recommendedWidgetName || "" };
            state.snippet.choices.set(binding.id, choice);
        }
        const row = document.createElement("div");
        row.className = "snippet-binding";
        const identity = document.createElement("div");
        const name = document.createElement("strong");
        name.textContent = binding.id;
        const requirements = document.createElement("small");
        requirements.textContent = translate("snippet.binding.requirements", { capabilities: translatedCapabilities(binding.requiredCapabilities), role: binding.requiredRole });
        identity.append(name, document.createElement("br"), requirements);
        const select = document.createElement("select");
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = binding.required ? translate("snippet.binding.choose") : translate("snippet.binding.optionalSkip");
        select.append(placeholder);
        for (const candidate of binding.candidates) {
            const option = document.createElement("option");
            option.value = candidate.name;
            option.textContent = candidate.name + " [" + candidate.role + "; " + translatedCapabilities(candidate.capabilities) + "]" + (candidate.compatible ? "" : " ⚠");
            select.append(option);
        }
        select.value = choice.widgetName;
        const options = document.createElement("div");
        options.className = "snippet-binding-options";
        const candidate = selectedSnippetCandidate(binding, choice.widgetName);
        const automatic = Boolean(candidate?.compatible && candidate.name.toLowerCase() === binding.id.toLowerCase());
        if (automatic) {
            const automaticStatus = document.createElement("span");
            automaticStatus.className = "snippet-automatic";
            automaticStatus.textContent = "✓ " + translate("snippet.binding.automatic");
            options.append(automaticStatus);
            choice.confirmed = true;
        } else {
            const confirmLabel = document.createElement("label");
            const confirm = document.createElement("input");
            confirm.type = "checkbox";
            confirm.checked = choice.confirmed;
            confirm.addEventListener("change", () => { choice.confirmed = confirm.checked; invalidateSnippetPreview(); });
            confirmLabel.append(confirm, document.createTextNode(" " + translate("snippet.binding.confirm")));
            options.append(confirmLabel);
        }
        if (candidate && !candidate.compatible) {
            const mismatch = document.createElement("div");
            mismatch.className = "snippet-mismatch";
            mismatch.textContent = candidate.mismatchReasons.join("; ");
            const overrideLabel = document.createElement("label");
            const override = document.createElement("input");
            override.type = "checkbox";
            override.checked = choice.allowIncompatible;
            override.addEventListener("change", () => { choice.allowIncompatible = override.checked; invalidateSnippetPreview(); });
            overrideLabel.append(override, document.createTextNode(" " + translate("snippet.binding.allowIncompatible")));
            options.append(mismatch, overrideLabel);
        }
        select.addEventListener("change", () => {
            choice.widgetName = select.value;
            const selectedCandidate = selectedSnippetCandidate(binding, choice.widgetName);
            choice.confirmed = Boolean(selectedCandidate?.compatible && selectedCandidate.name.toLowerCase() === binding.id.toLowerCase());
            choice.allowIncompatible = false;
            invalidateSnippetPreview();
            renderSnippetBindings();
        });
        row.append(identity, select, options);
        elements.snippetBindings.append(row);
    }
}

function renderSnippetApplication() {
    const preview = state.snippet.preview;
    elements.snippetMappingStep.hidden = !preview?.bindings.length;
    renderSnippetBindings();
    renderSnippetConflict();
    renderDiagnosticsIn(elements.snippetDiagnostics, preview?.diagnostics);
    elements.snippetStatus.className = preview ? (preview.valid ? "" : "diagnostic error") : "muted";
    elements.snippetStatus.textContent = preview ? translate(preview.valid ? "snippet.preview.valid" : "snippet.preview.invalid") : translate("snippet.preview.empty");
    elements.snippetApply.disabled = !preview?.valid || preview.conflict.action === "skip";
    renderSnippetTarget();
}

function renderSnippetTarget() {
    elements.snippetTargetPath.textContent = state.snippet.targetPath || translate("snippet.target.empty");
    elements.snippetTargetSave.disabled = !state.snippet.targetPath || !state.snippet.targetDirty;
    snippetTargetEditor.setReadOnly(!state.snippet.targetPath);
    renderDiagnosticsIn(elements.snippetTargetDiagnostics, state.snippet.targetDiagnostics, (diagnostic) => {
        snippetTargetEditor.goToLine(diagnostic.line || 1);
    });
}

function resetSnippetTarget() {
    state.snippet.targetDiagnostics = [];
    state.snippet.targetDirty = false;
    state.snippet.targetHash = "";
    state.snippet.targetPath = "";
    snippetTargetEditor.setValue("", true);
    renderSnippetTarget();
}

async function loadSnippetTarget(targetPath) {
    resetSnippetTarget();
    if (!targetPath) return;
    const opened = await api("/api/file?path=" + encodeURIComponent(targetPath));
    state.snippet.targetHash = opened.hash;
    state.snippet.targetPath = opened.path;
    state.snippet.targetDiagnostics = opened.document.diagnostics;
    snippetTargetEditor.setValue(opened.source, true);
    renderSnippetTarget();
}

function syncSnippetTarget(preview) {
    const changedTarget = preview.targetZonePath !== state.snippet.targetPath;
    if (!changedTarget && state.snippet.targetDirty) return;
    if (changedTarget) state.snippet.targetDiagnostics = [];
    state.snippet.targetDirty = false;
    state.snippet.targetHash = preview.targetHash;
    state.snippet.targetPath = preview.targetZonePath;
    snippetTargetEditor.setValue(preview.targetSource, changedTarget);
}

async function refreshSnippetPreview() {
    const result = await api("/api/snippet/apply-preview", { method: "POST", body: JSON.stringify(snippetApplicationBody()) });
    state.snippet.preview = result.preview;
    if (!elements.snippetApplicationId.value) elements.snippetApplicationId.value = result.preview.applicationId;
    state.snippet.conflictAction = result.preview.conflict.action;
    syncSnippetTarget(result.preview);
    renderSnippetApplication();
}

function renderSnippetImport() {
    const preview = state.snippet.importPreview;
    renderDiagnosticsIn(elements.snippetImportDiagnostics, preview?.diagnostics);
    elements.snippetImportSource.textContent = preview?.source || translate("snippet.import.empty");
    elements.snippetImportAction.replaceChildren();
    elements.snippetImportConflict.hidden = !preview?.targetExists;
    if (!preview) {
        elements.snippetImportAction.disabled = true;
        elements.snippetImportApply.disabled = true;
        return;
    }
    const actions = preview.targetExists
        ? [["", "legacy.conflict.choose"], ["replace", "legacy.conflict.replace"], ["rename", "legacy.conflict.rename"], ["skip", "legacy.conflict.skip"]]
        : preview.targetPath === preview.defaultTargetPath ? [["create", "legacy.conflict.create"]] : [["rename", "legacy.conflict.rename"]];
    for (const [value, key] of actions) {
        const option = document.createElement("option");
        option.value = value;
        option.textContent = translate(key);
        elements.snippetImportAction.append(option);
    }
    if (!actions.some(([value]) => value === state.snippet.importAction)) state.snippet.importAction = actions.length === 1 ? actions[0][0] : "";
    elements.snippetImportAction.value = state.snippet.importAction;
    elements.snippetImportAction.disabled = actions.length === 1;
    elements.snippetImportApply.disabled = !preview.valid || !state.snippet.importAction || preview.targetPath !== elements.snippetImportTarget.value;
}

async function refreshSnippetImportPreview() {
    if (!state.snippet.importFile) return;
    const result = await api("/api/snippet/import-preview", { method: "POST", body: JSON.stringify({ fileName: state.snippet.importFile.name, source: state.snippet.importFile.source, targetPath: elements.snippetImportTarget.value }) });
    state.snippet.importPreview = result.preview;
    elements.snippetImportTarget.value = result.preview.targetPath;
    renderSnippetImport();
}

function renameSuggestion(targetPath) {
    const extensionPosition = targetPath.lastIndexOf(".");
    if (extensionPosition < 0) return targetPath + "-imported";
    return targetPath.slice(0, extensionPosition) + "-imported" + targetPath.slice(extensionPosition);
}

function downloadSource(fileName, source) {
    const downloadUrl = URL.createObjectURL(new Blob([source], { type: "text/plain;charset=utf-8" }));
    const link = document.createElement("a");
    link.download = fileName;
    link.href = downloadUrl;
    link.click();
    URL.revokeObjectURL(downloadUrl);
}

async function downloadSnippetPath(relativePath) {
    const opened = await api("/api/file?path=" + encodeURIComponent(relativePath));
    if (opened.document.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new Error(translate("error.fixBeforeSave"));
    const fileName = relativePath.split("/").at(-1);
    downloadSource(fileName, opened.source);
    showReport(translate("status.exportedSnippet", { name: fileName }));
}

function legacyDraftsForRequest() {
    return [...state.legacy.drafts].map(([sourcePath, draft]) => ({ originalSourceHash: draft.originalSourceHash, source: draft.source, sourcePath }));
}

function legacyTargetPathsForRequest() {
    return [...state.legacy.targetPaths].map(([sourcePath, targetPath]) => ({ sourcePath, targetPath }));
}

function closeLegacyDraft() {
    state.legacy.activeDraftPath = "";
    legacyDraftEditor.setVisible(false);
    elements.legacyDraftPanel.hidden = true;
}

function openLegacyDraft(item, line) {
    const changedItem = state.legacy.activeDraftPath !== item.sourcePath;
    state.legacy.activeDraftPath = item.sourcePath;
    elements.legacyDraftPanel.hidden = false;
    legacyDraftEditor.setVisible(true);
    elements.legacyDraftPath.textContent = item.sourcePath + " → " + item.targetPath;
    const draft = state.legacy.drafts.get(item.sourcePath);
    legacyDraftEditor.setValue(draft?.source ?? item.source, changedItem);
    if (line) requestAnimationFrame(() => legacyDraftEditor.goToLine(line));
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

function translatedCapabilities(capabilities) {
    if (!capabilities.length) return "-";
    return capabilities.map((capability) => translate("legacy.widget.capability." + capability)).join(", ");
}

function widgetMappingsForRequest() {
    return [...state.legacy.widgetMappings].map(([sourceWidget, targetWidget]) => ({ sourceWidget, targetWidget }));
}

function usesExistingLegacySurface() {
    const surfaceItem = state.legacy.preview?.items.find((item) => item.kind === "surface");
    const resolution = surfaceItem ? state.legacy.resolutions.get(surfaceItem.id) : undefined;
    return elements.legacyIncludeSurface.checked && surfaceItem?.targetExists && (resolution?.action === "rename" || resolution?.action === "skip");
}

function renderLegacyWidgetMappings() {
    const issues = state.legacy.preview?.widgetMappings || [];
    elements.legacyWidgetMappings.replaceChildren();
    if (!issues.length) {
        elements.legacyWidgetMappings.className = "legacy-widget-mappings muted";
        elements.legacyWidgetMappings.textContent = translate("legacy.widgetMappings.empty");
        return;
    }
    elements.legacyWidgetMappings.className = "legacy-widget-mappings";
    for (const issue of issues) {
        const row = document.createElement("div");
        row.className = "legacy-widget-mapping";
        const source = document.createElement("strong");
        source.textContent = issue.sourceWidget;
        const details = document.createElement("small");
        const reason = translate("legacy.widgetMappings." + issue.reason);
        const required = translate("legacy.widgetMappings.required", { capabilities: translatedCapabilities(issue.requiredCapabilities) });
        const occurrences = translate("legacy.widgetMappings.occurrences", { count: issue.occurrences.length });
        details.textContent = reason + " " + required + " " + occurrences;
        const select = document.createElement("select");
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = translate("legacy.widgetMappings.choose");
        select.append(placeholder);
        for (const candidate of issue.candidates) {
            const option = document.createElement("option");
            option.value = candidate.name;
            option.textContent = candidate.name + " [" + translatedCapabilities(candidate.capabilities) + "]";
            select.append(option);
        }
        select.value = state.legacy.widgetMappings.get(issue.sourceWidget) || issue.selectedTarget || "";
        select.addEventListener("change", async () => {
            try {
                if (select.value) state.legacy.widgetMappings.set(issue.sourceWidget, select.value);
                else state.legacy.widgetMappings.delete(issue.sourceWidget);
                await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
            } catch (error) { showError(error); }
        });
        row.append(source, select, details);
        elements.legacyWidgetMappings.append(row);
    }
}

function renderLegacyPreview() {
    const preview = state.legacy.preview;
    renderLegacyZones();
    renderLegacyDependencies();
    renderLegacyWidgetMappings();
    elements.legacyPreview.replaceChildren();
    if (!preview) {
        elements.legacyPreview.className = "legacy-preview muted";
        elements.legacyPreview.textContent = translate("legacy.preview.empty");
        renderDiagnosticsIn(elements.legacyDiagnostics);
        closeLegacyDraft();
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
        const sourcePath = document.createElement("button");
        sourcePath.className = "legacy-source-link";
        sourcePath.textContent = translate("legacy.source") + ": " + item.sourcePath;
        sourcePath.addEventListener("click", () => openLegacyDraft(item));
        const targetPath = document.createElement("input");
        targetPath.className = "legacy-target-input";
        targetPath.value = item.targetPath;
        targetPath.title = translate("legacy.target");
        targetPath.addEventListener("change", async () => {
            try {
                state.legacy.targetPaths.set(item.sourcePath, targetPath.value);
                await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
            } catch (error) { showError(error); }
        });
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
            actionControl.addEventListener("change", async () => {
                resolution.action = actionControl.value;
                if (resolution.action === "rename" && !resolution.targetPath) resolution.targetPath = renameInput.value;
                renameInput.hidden = resolution.action !== "rename";
                if (item.kind === "surface") {
                    try { await refreshLegacyPreview([...state.legacy.selectedZonePaths], usesExistingLegacySurface()); } catch (error) { showError(error); }
                } else updateLegacyImportButton();
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
    const activeItem = preview.items.find((item) => item.sourcePath === state.legacy.activeDraftPath);
    if (activeItem) {
        const draft = state.legacy.drafts.get(activeItem.sourcePath);
        if (draft) draft.source = activeItem.source;
        openLegacyDraft(activeItem);
    } else if (state.legacy.activeDraftPath) closeLegacyDraft();
    updateLegacyImportButton();
}

async function refreshLegacyPreview(selectedZonePaths, useExistingSurface = usesExistingLegacySurface()) {
    if (!elements.legacySurface.value) return;
    const body = { drafts: legacyDraftsForRequest(), includeSurface: elements.legacyIncludeSurface.checked, surfaceName: elements.legacySurface.value, targetPaths: legacyTargetPathsForRequest(), targetProfileId: state.legacy.targetProfileId || undefined, useExistingSurface, widgetMappings: widgetMappingsForRequest() };
    if (selectedZonePaths !== undefined) body.selectedZonePaths = selectedZonePaths;
    const result = await api("/api/legacy/preview", { method: "POST", body: JSON.stringify(body) });
    state.legacy.preview = result.preview;
    state.legacy.selectedZonePaths = new Set(result.preview.selectedZonePaths);
    state.legacy.targetProfileId = result.preview.targetProfileId;
    elements.legacyTargetProfile.value = result.preview.targetProfileId;
    state.legacy.widgetMappings = new Map(result.preview.widgetMappings.filter((issue) => issue.selectedTarget).map((issue) => [issue.sourceWidget, issue.selectedTarget]));
    elements.legacySelectAll.disabled = false;
    elements.legacySelectNone.disabled = false;
    elements.legacyRefresh.disabled = false;
    renderLegacyPreview();
}

function renderLegacySelection(selection) {
    closeLegacyDraft();
    state.legacy.drafts.clear();
    state.legacy.preview = null;
    state.legacy.resolutions.clear();
    state.legacy.selectedZonePaths.clear();
    state.legacy.targetPaths.clear();
    state.legacy.targetProfileId = "";
    state.legacy.widgetMappings.clear();
    elements.legacyPath.value = selection?.path || "";
    elements.legacyTargetProfile.value = "";
    elements.legacySurface.replaceChildren();
    const placeholder = document.createElement("option");
    placeholder.value = "";
    placeholder.textContent = translate("legacy.surface.placeholder");
    elements.legacySurface.append(placeholder);
    for (const surface of selection?.surfaces || []) {
        const option = document.createElement("option");
        option.value = surface.name;
        option.textContent = translate("legacy.surface.option", { count: surface.zoneCount, fxCount: surface.fxZoneCount, name: surface.name });
        elements.legacySurface.append(option);
    }
    elements.legacySurface.disabled = !selection?.surfaces?.length;
    elements.legacySelectAll.disabled = true;
    elements.legacySelectNone.disabled = true;
    elements.legacyRefresh.disabled = true;
    elements.legacyStatus.className = "muted";
    elements.legacyStatus.textContent = selection?.root ? translate("legacy.status.opened", { path: selection.root }) : translate("legacy.status.notFound", { path: selection?.path || "CSI" });
    renderLegacyPreview();
}

async function applyDataPath(dataPath, showOpenedReport) {
    const result = await api("/api/select-data-path", { method: "POST", body: JSON.stringify({ path: dataPath }) });
    elements.dataPathStatus.textContent = result.dataPath;
    elements.dataPath.value = result.dataPath;
    state.current = null;
    state.batch.clear();
    state.snippet.choices.clear();
    state.snippet.conflictAction = "";
    state.snippet.importAction = "";
    state.snippet.importPreview = null;
    state.snippet.preview = null;
    resetSnippetTarget();
    updateBatch();
    renderDocument();
    renderSnippetApplication();
    renderSnippetImport();
    renderLegacySelection(result.legacy);
    await refreshTree();
    setTaskAvailability(true);
    showTaskHome();
    if (showOpenedReport) showReport(translate("status.openedDataPath", { path: result.dataPath }));
}

async function initialize() {
    try {
        const translationsResponse = await fetch("/app-translations.json");
        if (!translationsResponse.ok) throw new Error(translationsResponse.statusText);
        translations = await translationsResponse.json();
        setTaskAvailability(false);
        if (!token) {
            showReport(translate("error.missingToken"));
            return;
        }
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
        if (status.dataPath) {
            elements.dataPathStatus.textContent = status.dataPath;
            elements.dataPath.value = status.dataPath;
            renderLegacySelection(status.legacy);
            await refreshTree();
            setTaskAvailability(true);
            return;
        }
        const existingCandidates = status.candidates.filter((candidate) => candidate.exists);
        if (existingCandidates.length) {
            elements.dataPath.value = existingCandidates[0].path;
            let lastError;
            for (const candidate of existingCandidates) {
                try {
                    await applyDataPath(candidate.path, false);
                    return;
                } catch (error) {
                    lastError = error;
                }
            }
            if (lastError) showReport(translate("error.configLoad") + "\n" + lastError.message);
        } else if (status.candidates.length) {
            elements.dataPath.value = status.candidates[0].path;
        }
    } catch (error) {
        showReport(translate("error.configLoad") + "\n" + error.message);
    }
}

elements.openDataPath.addEventListener("click", async () => {
    try {
        await applyDataPath(elements.dataPath.value, true);
    } catch (error) { showError(error); }
});

elements.openLegacy.addEventListener("click", async () => {
    try {
        const result = await api("/api/legacy/select", { method: "POST", body: JSON.stringify({ path: elements.legacyPath.value }) });
        renderLegacySelection(result);
    } catch (error) { showError(error); }
});

elements.legacySurface.addEventListener("change", async () => {
    try {
        closeLegacyDraft();
        state.legacy.drafts.clear();
        state.legacy.resolutions.clear();
        state.legacy.selectedZonePaths.clear();
        state.legacy.targetPaths.clear();
        state.legacy.targetProfileId = "";
        elements.legacyTargetProfile.value = "";
        state.legacy.widgetMappings.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview();
        else {
            state.legacy.preview = null;
            renderLegacyPreview();
        }
    } catch (error) { showError(error); }
});

elements.legacyTargetProfile.addEventListener("change", async () => {
    try {
        state.legacy.targetProfileId = elements.legacyTargetProfile.value.trim();
        state.legacy.targetPaths.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
    } catch (error) { showError(error); }
});

elements.legacyDraftCheck.addEventListener("click", async () => {
    try { await refreshLegacyPreview([...state.legacy.selectedZonePaths]); } catch (error) { showError(error); }
});

elements.legacyDraftDiscard.addEventListener("click", async () => {
    try {
        const sourcePath = state.legacy.activeDraftPath;
        state.legacy.drafts.delete(sourcePath);
        await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
        const item = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === sourcePath);
        if (item) openLegacyDraft(item);
    } catch (error) { showError(error); }
});

elements.legacyIncludeSurface.addEventListener("change", async () => {
    try {
        state.legacy.widgetMappings.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
    } catch (error) { showError(error); }
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
                drafts: legacyDraftsForRequest(),
                includeSurface: elements.legacyIncludeSurface.checked,
                resolutions,
                selectedZonePaths: [...state.legacy.selectedZonePaths],
                surfaceName: elements.legacySurface.value,
                targetPaths: legacyTargetPathsForRequest(),
                targetProfileId: state.legacy.targetProfileId,
                widgetMappings: widgetMappingsForRequest(),
            }),
        });
        await refreshTree();
        await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
        showReport({ message: translate("status.importedLegacy", { count: result.report.changed.length + result.report.created.length }), ...result.report });
    } catch (error) { showError(error); }
});

for (const select of [elements.snippetSource, elements.snippetSurface, elements.snippetZone]) select.addEventListener("change", async () => {
    try {
        if (select === elements.snippetZone) await loadSnippetTarget(elements.snippetZone.value);
        state.snippet.choices.clear();
        state.snippet.conflictAction = "";
        state.snippet.preview = null;
        elements.snippetApplicationId.value = "";
        elements.snippetRenameId.value = "";
        renderSnippetPathOptions();
        renderSnippetApplication();
        if (snippetPathsReady()) await refreshSnippetPreview();
    } catch (error) { showError(error); }
});

elements.snippetApplicationId.addEventListener("input", invalidateSnippetPreview);
elements.snippetRenameId.addEventListener("input", invalidateSnippetPreview);

elements.snippetConflictAction.addEventListener("change", () => {
    state.snippet.conflictAction = elements.snippetConflictAction.value;
    elements.snippetRenameId.hidden = state.snippet.conflictAction !== "rename";
    if (state.snippet.conflictAction === "rename" && !elements.snippetRenameId.value) elements.snippetRenameId.value = elements.snippetApplicationId.value + "-copy";
    invalidateSnippetPreview();
});

elements.snippetApply.addEventListener("click", async () => {
    try {
        await refreshSnippetPreview();
        const preview = state.snippet.preview;
        if (!preview?.valid || preview.conflict.action === "skip") return;
        state.snippet.targetDiagnostics = [];
        state.snippet.targetDirty = preview.source !== preview.targetSource;
        state.snippet.targetHash = preview.targetHash;
        state.snippet.targetPath = preview.targetZonePath;
        snippetTargetEditor.setValue(preview.source);
        renderSnippetTarget();
        showReport(translate("status.appliedSnippetDraft", { path: preview.targetZonePath }));
    } catch (error) { showError(error); }
});

elements.snippetTargetSave.addEventListener("click", async () => {
    try {
        if (!state.snippet.targetPath) return;
        const source = snippetTargetEditor.getValue();
        const validation = await api("/api/validate", { method: "POST", body: JSON.stringify({ path: state.snippet.targetPath, source }) });
        state.snippet.targetDiagnostics = validation.document.diagnostics;
        renderSnippetTarget();
        if (state.snippet.targetDiagnostics.some((diagnostic) => diagnostic.severity === "error")) {
            showReport(translate("error.fixBeforeSave"));
            return;
        }
        const result = await api("/api/save", { method: "POST", body: JSON.stringify({ originalHash: state.snippet.targetHash, path: state.snippet.targetPath, source }) });
        state.snippet.targetDirty = false;
        state.snippet.targetHash = result.hash;
        if (state.snippet.preview) state.snippet.preview.valid = false;
        renderSnippetApplication();
        await refreshTree();
        showReport(translate("status.savedFile", { path: state.snippet.targetPath }));
    } catch (error) { showError(error); }
});

elements.snippetImportFile.addEventListener("change", async () => {
    try {
        const file = elements.snippetImportFile.files?.[0];
        state.snippet.importAction = "";
        state.snippet.importPreview = null;
        state.snippet.importFile = file ? { name: file.name, source: await file.text() } : null;
        elements.snippetImportTarget.value = "";
        renderSnippetImport();
        if (file) await refreshSnippetImportPreview();
    } catch (error) { showError(error); }
});

elements.snippetImportTarget.addEventListener("input", () => {
    state.snippet.importPreview = null;
    elements.snippetImportApply.disabled = true;
    renderSnippetImport();
});
elements.snippetImportTarget.addEventListener("change", async () => {
    try { await refreshSnippetImportPreview(); } catch (error) { showError(error); }
});

elements.snippetImportAction.addEventListener("change", async () => {
    try {
        state.snippet.importAction = elements.snippetImportAction.value;
        if (state.snippet.importAction === "rename" && state.snippet.importPreview) {
            elements.snippetImportTarget.value = renameSuggestion(state.snippet.importPreview.defaultTargetPath);
            state.snippet.importPreview = null;
            await refreshSnippetImportPreview();
        } else renderSnippetImport();
    } catch (error) { showError(error); }
});

elements.snippetImportApply.addEventListener("click", async () => {
    try {
        const importFile = state.snippet.importFile;
        if (!importFile) return;
        await refreshSnippetImportPreview();
        const preview = state.snippet.importPreview;
        if (!preview?.valid || !importFile) throw new Error(translate("snippet.preview.invalid"));
        if (!state.snippet.importAction) return;
        const result = await api("/api/snippet/import", { method: "POST", body: JSON.stringify({ action: state.snippet.importAction, fileName: importFile.name, source: importFile.source, sourceHash: preview.sourceHash, targetHash: preview.targetHash, targetPath: preview.targetPath }) });
        await refreshTree();
        state.snippet.importAction = "";
        state.snippet.importFile = null;
        state.snippet.importPreview = null;
        elements.snippetImportFile.value = "";
        elements.snippetImportTarget.value = "";
        renderSnippetImport();
        showReport({ message: translate("status.importedSnippet", { count: result.report.changed.length + result.report.created.length }), ...result.report });
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

elements.exportSnippet.addEventListener("click", async () => {
    try {
        const documentView = await validateCurrent();
        if (!state.current || documentView.diagnostics.some((diagnostic) => diagnostic.severity === "error")) throw new Error(translate("error.fixBeforeSave"));
        const fileName = state.current.path.split("/").at(-1);
        downloadSource(fileName, state.current.source);
        showReport(translate("status.exportedSnippet", { name: fileName }));
    } catch (error) { showError(error); }
});

for (const tab of document.querySelectorAll(".tab")) tab.addEventListener("click", async () => {
    try {
        if (state.current) await validateCurrent();
        state.tab = tab.dataset.tab;
        for (const candidate of document.querySelectorAll(".tab")) candidate.classList.toggle("active", candidate === tab);
        codeEditor.setVisible(state.tab === "raw");
        elements.structuredEditor.hidden = state.tab !== "structured";
    } catch (error) { showError(error); }
});

for (const taskButton of document.querySelectorAll(".task-card")) taskButton.addEventListener("click", () => showTask(taskButton.dataset.task));
elements.backToTasks.addEventListener("click", showTaskHome);
elements.snippetExportSource.addEventListener("change", () => { elements.snippetExportDownload.disabled = !elements.snippetExportSource.value; });
elements.snippetExportDownload.addEventListener("click", async () => {
    try { await downloadSnippetPath(elements.snippetExportSource.value); } catch (error) { showError(error); }
});
setTaskAvailability(false);
showTaskHome();
initialize();
