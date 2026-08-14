import { createConfigurationEditor } from "./code-editor.js";

let translations = {};
const sessionTokenElement = document.querySelector('meta[name="config-editor-session-token"]');
const token = sessionTokenElement?.getAttribute("content") || "";

function requiredElement(id) {
    const element = document.getElementById(id);
    if (!element) throw new Error("Missing UI element: " + id);
    return element;
}

const elements = {
    backToTasks: requiredElement("back-to-tasks"),
    bottomPanelContent: requiredElement("bottom-panel-content"),
    clone: requiredElement("clone"),
    dataPath: requiredElement("data-path"),
    dataPathCandidates: requiredElement("data-path-candidates"),
    dataPathFeedback: requiredElement("data-path-feedback"),
    detailsPanel: requiredElement("details-panel"),
    diagnostics: requiredElement("diagnostics"),
    documentMode: requiredElement("document-mode"),
    documentPath: requiredElement("document-path"),
    draftConflict: requiredElement("draft-conflict"),
    draftDiscard: requiredElement("draft-discard"),
    draftRestore: requiredElement("draft-restore"),
    editorBody: requiredElement("editor-body"),
    editorMain: requiredElement("editor-main"),
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
    legacyFeedback: requiredElement("legacy-feedback"),
    legacyOperationReport: requiredElement("legacy-operation-report"),
    legacyPath: requiredElement("legacy-path"),
    legacyPathFeedback: requiredElement("legacy-path-feedback"),
    legacyPreview: requiredElement("legacy-preview"),
    legacyRefresh: requiredElement("legacy-refresh"),
    legacySelectAll: requiredElement("legacy-select-all"),
    legacySelectNone: requiredElement("legacy-select-none"),
    legacyStatus: requiredElement("legacy-status"),
    legacySurface: requiredElement("legacy-surface"),
    legacyTargetFeedback: requiredElement("legacy-target-feedback"),
    legacyTargetProfile: requiredElement("legacy-target-profile"),
    legacyWidgetMappings: requiredElement("legacy-widget-mappings"),
    legacyZones: requiredElement("legacy-zones"),
    homeHeader: requiredElement("home-header"),
    openDataPath: requiredElement("open-data-path"),
    openLegacy: requiredElement("open-legacy"),
    problemCount: requiredElement("problem-count"),
    problemsPanel: requiredElement("problems-panel"),
    rawEditor: requiredElement("raw-editor"),
    save: requiredElement("save"),
    saveAll: requiredElement("save-all"),
    semantic: requiredElement("semantic"),
    snippetApplicationId: requiredElement("snippet-application-id"),
    snippetBindings: requiredElement("snippet-bindings"),
    snippetCancel: requiredElement("snippet-cancel"),
    snippetClose: requiredElement("snippet-close"),
    snippetConflict: requiredElement("snippet-conflict"),
    snippetConflictAction: requiredElement("snippet-conflict-action"),
    snippetDialog: requiredElement("snippet-dialog"),
    snippetDiagnostics: requiredElement("snippet-diagnostics"),
    snippetInsert: requiredElement("snippet-insert"),
    snippetOpen: requiredElement("snippet-open"),
    snippetRenameId: requiredElement("snippet-rename-id"),
    snippetSource: requiredElement("snippet-source"),
    snippetSurface: requiredElement("snippet-surface"),
    snippetSurfaceFeedback: requiredElement("snippet-surface-feedback"),
    snippetSurfaceField: requiredElement("snippet-surface-field"),
    snippetTargetPath: requiredElement("snippet-target-path"),
    snippetToolbar: requiredElement("snippet-toolbar"),
    taskHome: requiredElement("task-home"),
    title: requiredElement("title"),
    tree: requiredElement("tree"),
    validate: requiredElement("validate"),
    workflowDescription: requiredElement("workflow-description"),
    workflowEdit: requiredElement("workflow-edit"),
    workflowFeedback: requiredElement("workflow-feedback"),
    workflowLegacy: requiredElement("workflow-legacy"),
    workflowTitle: requiredElement("workflow-title"),
    workspace: requiredElement("workspace"),
};
const state = {
    batch: new Map(),
    bottomPanel: "problems",
    current: null,
    draftConflicts: new Set(),
    legacy: { activeDraftPath: "", drafts: new Map(), preview: null, resolutions: new Map(), selectedZonePaths: new Set(), targetPaths: new Map(), targetProfileId: "", widgetMappings: new Map() },
    snippet: { choices: new Map(), conflictAction: "", insertionLine: 1, preview: null, treeEntries: [] },
    renderedDocumentPath: "",
    task: "",
};
let draftTimer = 0;
let pendingDraft = null;
let draftWriteActive = false;
let draftWritePromise = Promise.resolve();
const codeEditor = createConfigurationEditor(elements.rawEditor, handleEditorChange);
const legacyDraftEditor = createConfigurationEditor(elements.legacyDraftEditor, (source) => {
    const item = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === state.legacy.activeDraftPath);
    if (item) state.legacy.drafts.set(item.sourcePath, { originalSourceHash: item.originalSourceHash, source });
});
legacyDraftEditor.setReadOnly(false);
legacyDraftEditor.setVisible(false);

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
        error.code = payload.error?.code;
        error.details = payload.error?.details;
        throw error;
    }
    return payload;
}

function setFeedback(element, value, tone = "info") {
    element.hidden = !value;
    element.className = `feedback ${tone}`;
    element.textContent = value || "";
}

function showReport(value, target = elements.workflowFeedback) {
    setFeedback(target, typeof value === "string" ? value : JSON.stringify(value, null, 2), "info");
}

function showError(error, target = elements.workflowFeedback) {
    setFeedback(target, error.details ? error.message + "\n" + JSON.stringify(error.details, null, 2) : error.message, "danger");
}

function setTaskAvailability(available) {
    for (const button of document.querySelectorAll(".task-card")) button.disabled = !available;
}

function showTask(task) {
    const workflows = { edit: elements.workflowEdit, legacy: elements.workflowLegacy };
    state.task = task;
    elements.homeHeader.hidden = true;
    elements.taskHome.hidden = true;
    elements.editorMain.hidden = false;
    elements.filePanel.hidden = task !== "edit";
    elements.saveAll.hidden = task !== "edit";
    elements.editorBody.classList.toggle("file-task", task === "edit");
    elements.workspace.classList.toggle("document-task", task === "edit");
    for (const workflow of Object.values(workflows)) workflow.hidden = workflow !== workflows[task];
    elements.workflowTitle.textContent = translate(`task.${task}.title`);
    elements.workflowDescription.textContent = translate(`task.${task}.description`);
    setFeedback(elements.workflowFeedback, "");
    codeEditor.setVisible(task === "edit");
}

function showTaskHome() {
    state.task = "";
    elements.homeHeader.hidden = false;
    elements.editorMain.hidden = true;
    elements.saveAll.hidden = true;
    elements.taskHome.hidden = false;
}

function renderDiagnosticsIn(container, diagnostics = [], navigate = navigateDiagnostic) {
    container.replaceChildren();
    if (!diagnostics.length) {
        container.className = "secondary";
        container.textContent = translate("problems.none");
        return;
    }
    container.className = "";
    for (const diagnostic of diagnostics) {
        const actionable = Boolean(diagnostic.path || diagnostic.line);
        const row = document.createElement(actionable ? "button" : "div");
        row.className = "diagnostic " + (diagnostic.severity === "error" ? "danger" : diagnostic.severity);
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
    elements.problemCount.textContent = String(diagnostics.length);
}

function renderBottomPanel() {
    const activePanel = state.bottomPanel;
    elements.bottomPanelContent.hidden = !activePanel;
    elements.problemsPanel.hidden = activePanel !== "problems";
    elements.detailsPanel.hidden = activePanel !== "details";
    for (const button of document.querySelectorAll(".bottom-tab")) button.classList.toggle("active", button.dataset.bottomTab === activePanel);
}

function renderDraftConflict() {
    elements.draftConflict.hidden = !state.current?.draftConflict;
}

function handleEditorChange(source) {
    if (!state.current?.writable) return;
    state.current.source = source;
    const change = { originalHash: state.current.hash, path: state.current.path, source };
    if (source === state.current.diskSource) state.batch.delete(state.current.path);
    else state.batch.set(state.current.path, change);
    pendingDraft = { ...change, discard: source === state.current.diskSource };
    window.clearTimeout(draftTimer);
    draftTimer = window.setTimeout(() => { void flushPendingDraft(); }, 300);
    updateBatch();
    updateTreeDraftState();
}

async function flushPendingDraft() {
    window.clearTimeout(draftTimer);
    draftTimer = 0;
    const draft = pendingDraft;
    pendingDraft = null;
    if (!draft) return draftWritePromise;
    draftWritePromise = draftWritePromise.then(async () => {
        draftWriteActive = true;
        try {
            if (draft.discard) await api("/api/draft/discard", { method: "POST", body: JSON.stringify({ path: draft.path }) });
            else await api("/api/draft", { method: "POST", body: JSON.stringify({ originalHash: draft.originalHash, path: draft.path, source: draft.source }) });
        } finally {
            draftWriteActive = false;
        }
    }).catch((error) => {
        if (error.code === "conflict.draft") {
            state.draftConflicts.add(draft.path);
            if (state.current?.path === draft.path) {
                state.current.draftConflict = { ...draft, conflict: true, document: state.current.document };
                renderDraftConflict();
            }
            updateBatch();
        }
        showError(error);
    });
    return draftWritePromise;
}

async function refreshDrafts() {
    const result = await api("/api/drafts");
    state.batch = new Map(result.drafts.map((draft) => [draft.path, { originalHash: draft.originalHash, path: draft.path, source: draft.source }]));
    state.draftConflicts = new Set(result.drafts.filter((draft) => draft.conflict).map((draft) => draft.path));
    updateBatch();
}

function currentIsEditableZone() {
    return Boolean(state.current?.writable && state.current.document.format === "zone");
}

function renderSnippetToolbar() {
    elements.snippetToolbar.hidden = !currentIsEditableZone();
    elements.snippetOpen.disabled = !currentIsEditableZone() || !elements.snippetSource.value;
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
    elements.save.disabled = !current || !current.writable || !state.batch.has(current.path);
    elements.clone.hidden = !current || current.writable || !["surface", "zone", "snippet"].includes(current.document.format);
    renderDiagnostics(current?.document.diagnostics);
    elements.semantic.textContent = JSON.stringify(current?.document.semantic || {}, null, 2);
    renderDraftConflict();
    renderSnippetToolbar();
    renderBottomPanel();
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
        await flushPendingDraft();
        const opened = await api("/api/file?path=" + encodeURIComponent(path));
        const memoryDraft = state.batch.get(path);
        const usableDraft = memoryDraft?.originalHash === opened.hash ? { ...opened.draft, ...memoryDraft } : opened.draft && !opened.draft.conflict ? opened.draft : null;
        if (usableDraft && (!usableDraft.document || Boolean(memoryDraft && memoryDraft.source !== opened.draft?.source))) {
            const validation = await api("/api/validate", { method: "POST", body: JSON.stringify({ path, source: usableDraft.source }) });
            usableDraft.document = validation.document;
        }
        let draftConflict = !usableDraft && memoryDraft ? { ...memoryDraft, conflict: true } : !usableDraft && opened.draft?.conflict ? opened.draft : null;
        if (draftConflict && !draftConflict.document) {
            const validation = await api("/api/validate", { method: "POST", body: JSON.stringify({ path, source: draftConflict.source }) });
            draftConflict = { ...draftConflict, document: validation.document };
        }
        state.current = { ...opened, diskSource: opened.source, document: usableDraft?.document || opened.document, draftConflict, source: usableDraft?.source || opened.source };
        if (usableDraft) state.batch.set(path, { originalHash: usableDraft.originalHash, path, source: usableDraft.source });
        if (draftConflict) state.draftConflicts.add(path);
        else state.draftConflicts.delete(path);
        updateBatch();
        updateTreeSelection(path);
        updateTreeDraftState();
        renderDocument();
        if (line) requestAnimationFrame(() => codeEditor.goToLine(line));
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
    codeEditor.setVisible(true);
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
            button.dataset.name = entry.name;
            button.dataset.path = entry.path;
            button.textContent = entry.name + (state.batch.has(entry.path) ? " *" : "");
            button.classList.toggle("dirty", state.batch.has(entry.path));
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

function updateTreeDraftState() {
    for (const button of elements.tree.querySelectorAll("button[data-path]")) {
        const dirty = state.batch.has(button.dataset.path);
        button.classList.toggle("dirty", dirty);
        button.textContent = (button.dataset.name || "") + (dirty ? " *" : "");
    }
}

function updateBatch() {
    elements.saveAll.disabled = state.batch.size === 0 || state.draftConflicts.size > 0;
    elements.saveAll.title = state.draftConflicts.size ? translate("draft.conflictsSave") : translate("pending.count", { count: state.batch.size });
    if (state.current) elements.save.disabled = !state.current.writable || !state.batch.has(state.current.path) || Boolean(state.current.draftConflict);
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
    fillPathSelect(elements.snippetSource, files.filter((file) => file.type === "snippet"), "snippet.source.choose");
    renderSnippetToolbar();
}

function snippetChoicesForRequest() {
    return [...state.snippet.choices].map(([bindingId, choice]) => ({ bindingId, ...choice }));
}

function snippetApplicationBody() {
    return {
        applicationId: elements.snippetApplicationId.value,
        bindingChoices: snippetChoicesForRequest(),
        conflictAction: state.snippet.conflictAction,
        insertionLine: state.snippet.insertionLine,
        renamedApplicationId: elements.snippetRenameId.value,
        snippetPath: elements.snippetSource.value,
        surfacePath: elements.snippetSurface.value,
        targetSource: state.current.source,
        targetZonePath: state.current.path,
    };
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
    const bindings = (state.snippet.preview?.bindings || []).filter((binding) => !binding.automatic);
    elements.snippetBindings.replaceChildren();
    elements.snippetBindings.hidden = bindings.length === 0;
    if (!bindings.length) {
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
            confirm.addEventListener("change", async () => { choice.confirmed = confirm.checked; await refreshSnippetPreviewSafely(); });
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
            override.addEventListener("change", async () => { choice.allowIncompatible = override.checked; await refreshSnippetPreviewSafely(); });
            overrideLabel.append(override, document.createTextNode(" " + translate("snippet.binding.allowIncompatible")));
            options.append(mismatch, overrideLabel);
        }
        select.addEventListener("change", async () => {
            choice.widgetName = select.value;
            const selectedCandidate = selectedSnippetCandidate(binding, choice.widgetName);
            choice.confirmed = Boolean(selectedCandidate?.compatible && selectedCandidate.name.toLowerCase() === binding.id.toLowerCase());
            choice.allowIncompatible = false;
            await refreshSnippetPreviewSafely();
        });
        row.append(identity, select, options);
        elements.snippetBindings.append(row);
    }
}

function renderSnippetDialog() {
    const preview = state.snippet.preview;
    renderSnippetBindings();
    renderSnippetConflict();
    renderDiagnosticsIn(elements.snippetDiagnostics, preview?.diagnostics);
    elements.snippetInsert.disabled = !preview?.valid;
    elements.snippetInsert.textContent = translate(preview?.conflict.action === "skip" ? "action.skip" : "action.insertSnippet");
}

async function refreshSnippetPreview() {
    const result = await api("/api/snippet/preview", { method: "POST", body: JSON.stringify(snippetApplicationBody()) });
    state.snippet.preview = result.preview;
    if (!elements.snippetApplicationId.value) elements.snippetApplicationId.value = result.preview.applicationId;
    state.snippet.conflictAction = result.preview.conflict.action;
    renderSnippetDialog();
}

async function refreshSnippetPreviewSafely() {
    try { await refreshSnippetPreview(); } catch (error) { showError(error, elements.snippetDiagnostics); }
}

function fillSnippetSurfaces(candidates, automatic) {
    setFeedback(elements.snippetSurfaceFeedback, "");
    elements.snippetSurface.replaceChildren();
    const placeholder = document.createElement("option");
    placeholder.value = "";
    placeholder.textContent = translate("snippet.surface.choose");
    elements.snippetSurface.append(placeholder);
    for (const candidate of candidates) {
        const option = document.createElement("option");
        option.value = candidate.path;
        option.textContent = candidate.path;
        elements.snippetSurface.append(option);
    }
    elements.snippetSurface.value = candidates.length === 1 ? candidates[0].path : "";
    elements.snippetSurface.disabled = candidates.length === 0;
    elements.snippetSurfaceField.hidden = automatic;
}

async function applySnippetPreview() {
    await refreshSnippetPreview();
    const preview = state.snippet.preview;
    if (!preview?.valid) return;
    if (preview.conflict.action === "skip") {
        if (elements.snippetDialog.open) elements.snippetDialog.close();
        return;
    }
    codeEditor.setValue(preview.source);
    handleEditorChange(preview.source);
    await validateCurrent();
    if (elements.snippetDialog.open) elements.snippetDialog.close();
    showReport(translate("status.appliedSnippetDraft", { path: state.current.path }));
}

async function openSnippetDialog() {
    if (!currentIsEditableZone() || !elements.snippetSource.value) return;
    await flushPendingDraft();
    state.snippet.choices.clear();
    state.snippet.conflictAction = "";
    state.snippet.insertionLine = codeEditor.getCursorLine();
    state.snippet.preview = null;
    elements.snippetApplicationId.value = "";
    elements.snippetRenameId.value = "";
    elements.snippetTargetPath.textContent = state.current.path;
    const result = await api("/api/snippet/context?zonePath=" + encodeURIComponent(state.current.path));
    fillSnippetSurfaces(result.surfaces, result.automatic);
    if (!result.automatic) {
        renderSnippetDialog();
        elements.snippetDialog.showModal();
        if (!result.surfaces.length) showError(new Error(translate("snippet.surface.none")), elements.snippetSurfaceFeedback);
        else if (result.surfaces.length === 1) await refreshSnippetPreview();
        return;
    }
    await refreshSnippetPreview();
    const needsInput = state.snippet.preview.bindings.some((binding) => !binding.automatic) || Boolean(state.snippet.preview.conflict.existingApplicationId) || !state.snippet.preview.valid;
    if (needsInput) elements.snippetDialog.showModal();
    else await applySnippetPreview();
}

function renameSuggestion(targetPath) {
    const extensionPosition = targetPath.lastIndexOf(".");
    if (extensionPosition < 0) return targetPath + "-imported";
    return targetPath.slice(0, extensionPosition) + "-imported" + targetPath.slice(extensionPosition);
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
        elements.legacyZones.className = "legacy-list secondary";
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
        elements.legacyDependencies.className = "legacy-list secondary";
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
        elements.legacyWidgetMappings.className = "legacy-widget-mappings secondary";
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
        elements.legacyPreview.className = "legacy-preview secondary";
        elements.legacyPreview.textContent = translate("legacy.preview.empty");
        renderDiagnosticsIn(elements.legacyDiagnostics);
        closeLegacyDraft();
        updateLegacyImportButton();
        return;
    }
    elements.legacyStatus.className = preview.valid ? "" : "diagnostic danger";
    elements.legacyStatus.textContent = translate(preview.valid ? "legacy.preview.valid" : "legacy.preview.invalid");
    renderDiagnosticsIn(elements.legacyDiagnostics, preview.diagnostics);
    const selectedItems = selectedLegacyItems();
    if (!selectedItems.length) {
        elements.legacyPreview.className = "legacy-preview secondary";
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
    setFeedback(elements.legacyFeedback, "");
    elements.legacyOperationReport.hidden = true;
    elements.legacyOperationReport.textContent = "";
    state.legacy.drafts.clear();
    state.legacy.preview = null;
    state.legacy.resolutions.clear();
    state.legacy.selectedZonePaths.clear();
    state.legacy.targetPaths.clear();
    state.legacy.targetProfileId = "";
    state.legacy.widgetMappings.clear();
    elements.legacyPath.value = selection?.path || "";
    elements.legacyTargetProfile.value = "";
    setFeedback(elements.legacyTargetFeedback, "");
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
    elements.legacyStatus.className = "secondary";
    elements.legacyStatus.textContent = selection?.root ? translate("legacy.status.opened", { path: selection.root }) : translate("legacy.status.notFound", { path: selection?.path || "CSI" });
    renderLegacyPreview();
}

async function applyDataPath(dataPath) {
    await flushPendingDraft();
    const result = await api("/api/select-data-path", { method: "POST", body: JSON.stringify({ path: dataPath }) });
    elements.dataPath.value = result.dataPath;
    setFeedback(elements.dataPathFeedback, "");
    state.current = null;
    state.snippet.choices.clear();
    state.snippet.conflictAction = "";
    state.snippet.preview = null;
    await refreshDrafts();
    renderDocument();
    renderLegacySelection(result.legacy);
    await refreshTree();
    setTaskAvailability(true);
    showTaskHome();
}

async function initialize() {
    try {
        const translationsResponse = await fetch("/app-translations.json");
        if (!translationsResponse.ok) throw new Error(translationsResponse.statusText);
        translations = await translationsResponse.json();
        setTaskAvailability(false);
        if (!token) {
            showError(new Error(translate("error.missingToken")), elements.dataPathFeedback);
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
            elements.dataPath.value = status.dataPath;
            renderLegacySelection(status.legacy);
            await refreshDrafts();
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
                    await applyDataPath(candidate.path);
                    return;
                } catch (error) {
                    lastError = error;
                }
            }
            if (lastError) showError(new Error(translate("error.configLoad") + "\n" + lastError.message), elements.dataPathFeedback);
        } else if (status.candidates.length) {
            elements.dataPath.value = status.candidates[0].path;
            showError(new Error(translate("tasks.openDataFirst")), elements.dataPathFeedback);
        } else showError(new Error(translate("tasks.openDataFirst")), elements.dataPathFeedback);
    } catch (error) {
        showError(new Error(translate("error.configLoad") + "\n" + error.message), elements.dataPathFeedback);
    }
}

elements.openDataPath.addEventListener("click", async () => {
    try {
        await applyDataPath(elements.dataPath.value);
    } catch (error) { showError(error, elements.dataPathFeedback); }
});

elements.openLegacy.addEventListener("click", async () => {
    try {
        const result = await api("/api/legacy/select", { method: "POST", body: JSON.stringify({ path: elements.legacyPath.value }) });
        renderLegacySelection(result);
        setFeedback(elements.legacyPathFeedback, "");
    } catch (error) { showError(error, elements.legacyPathFeedback); }
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
        setFeedback(elements.legacyTargetFeedback, "");
        state.legacy.targetProfileId = elements.legacyTargetProfile.value.trim();
        state.legacy.targetPaths.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
    } catch (error) { showError(error, elements.legacyTargetFeedback); }
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
        elements.legacyOperationReport.hidden = false;
        elements.legacyOperationReport.textContent = JSON.stringify({ message: translate("status.importedLegacy", { count: result.report.changed.length + result.report.created.length }), ...result.report }, null, 2);
    } catch (error) { showError(error, elements.legacyFeedback); }
});

elements.validate.addEventListener("click", async () => {
    try { await validateCurrent(); showReport(translate("status.checked")); } catch (error) { showError(error); }
});

elements.save.addEventListener("click", async () => {
    try {
        await flushPendingDraft();
        const documentView = await validateCurrent();
        if (documentView.diagnostics.some((diagnostic) => diagnostic.severity === "error")) {
            state.bottomPanel = "problems";
            renderBottomPanel();
            return;
        }
        const result = await api("/api/save", { method: "POST", body: JSON.stringify({ originalHash: state.current.hash, path: state.current.path, source: state.current.source }) });
        state.current.hash = result.hash;
        state.current.diskSource = state.current.source;
        state.current.draftConflict = null;
        state.batch.delete(state.current.path);
        state.draftConflicts.delete(state.current.path);
        updateBatch();
        updateTreeDraftState();
        renderDocument();
        showReport(translate("status.savedFile", { path: state.current.path }));
    } catch (error) { showError(error); }
});

elements.saveAll.addEventListener("click", async () => {
    try {
        await flushPendingDraft();
        const currentPath = state.current?.path;
        const result = await api("/api/transaction", { method: "POST", body: JSON.stringify({ changes: [...state.batch.values()] }) });
        state.batch.clear();
        state.draftConflicts.clear();
        updateBatch();
        await refreshTree();
        if (currentPath) await openDocument(currentPath);
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

elements.draftRestore.addEventListener("click", async () => {
    try {
        const draft = state.current?.draftConflict;
        if (!draft) return;
        state.current.source = draft.source;
        state.current.document = draft.document;
        state.current.draftConflict = null;
        state.draftConflicts.delete(state.current.path);
        codeEditor.setValue(draft.source);
        handleEditorChange(draft.source);
        await flushPendingDraft();
        renderDocument();
    } catch (error) { showError(error); }
});

elements.draftDiscard.addEventListener("click", async () => {
    try {
        if (!state.current?.draftConflict) return;
        await api("/api/draft/discard", { method: "POST", body: JSON.stringify({ path: state.current.path }) });
        const validation = await api("/api/validate", { method: "POST", body: JSON.stringify({ path: state.current.path, source: state.current.diskSource }) });
        state.batch.delete(state.current.path);
        state.draftConflicts.delete(state.current.path);
        state.current.draftConflict = null;
        state.current.document = validation.document;
        state.current.source = state.current.diskSource;
        codeEditor.setValue(state.current.diskSource);
        updateBatch();
        updateTreeDraftState();
        renderDocument();
    } catch (error) { showError(error); }
});

for (const button of document.querySelectorAll(".bottom-tab")) button.addEventListener("click", () => {
    state.bottomPanel = state.bottomPanel === button.dataset.bottomTab ? "" : button.dataset.bottomTab;
    renderBottomPanel();
});

elements.snippetSource.addEventListener("change", () => {
    state.snippet.preview = null;
    renderSnippetToolbar();
});
elements.snippetOpen.addEventListener("click", async () => {
    try { await openSnippetDialog(); } catch (error) { showError(error); }
});
elements.snippetSurface.addEventListener("change", async () => {
    setFeedback(elements.snippetSurfaceFeedback, "");
    state.snippet.choices.clear();
    state.snippet.preview = null;
    if (elements.snippetSurface.value) await refreshSnippetPreviewSafely();
});
elements.snippetApplicationId.addEventListener("change", refreshSnippetPreviewSafely);
elements.snippetRenameId.addEventListener("change", refreshSnippetPreviewSafely);
elements.snippetConflictAction.addEventListener("change", async () => {
    state.snippet.conflictAction = elements.snippetConflictAction.value;
    elements.snippetRenameId.hidden = state.snippet.conflictAction !== "rename";
    if (state.snippet.conflictAction === "rename" && !elements.snippetRenameId.value) elements.snippetRenameId.value = elements.snippetApplicationId.value + "-copy";
    await refreshSnippetPreviewSafely();
});
elements.snippetInsert.addEventListener("click", async () => {
    try { await applySnippetPreview(); } catch (error) { showError(error, elements.snippetDiagnostics); }
});
for (const button of [elements.snippetCancel, elements.snippetClose]) button.addEventListener("click", () => elements.snippetDialog.close());

for (const taskButton of document.querySelectorAll(".task-card")) taskButton.addEventListener("click", () => showTask(taskButton.dataset.task));
elements.backToTasks.addEventListener("click", async () => {
    await flushPendingDraft();
    showTaskHome();
});
elements.dataPath.addEventListener("input", () => setFeedback(elements.dataPathFeedback, ""));
elements.legacyPath.addEventListener("input", () => setFeedback(elements.legacyPathFeedback, ""));
elements.legacyTargetProfile.addEventListener("input", () => setFeedback(elements.legacyTargetFeedback, ""));
window.addEventListener("beforeunload", (event) => {
    if (!pendingDraft && !draftWriteActive) return;
    event.preventDefault();
    event.returnValue = "";
});
setTaskAvailability(false);
showTaskHome();
initialize();
