import { createConfigurationEditor } from "./code-editor.js";
import { editorRouteUrl, onEditorRouteChange, readEditorRoute, updateEditorRoute } from "./router.js";

let translations = {};
const sessionTokenElement = document.querySelector('meta[name="config-editor-session-token"]');
const token = sessionTokenElement?.getAttribute("content") || "";

function requiredElement(id) {
    const element = document.getElementById(id);
    if (!element) throw new Error("Missing UI element: " + id);
    return element;
}

const elements = {
    allDiagnostics: requiredElement("all-diagnostics"),
    allProblemCount: requiredElement("all-problem-count"),
    allProblemsGroup: requiredElement("all-problems-group"),
    backToTasks: requiredElement("back-to-tasks"),
    bottomPanelContent: requiredElement("bottom-panel-content"),
    clone: requiredElement("clone"),
    checkAll: requiredElement("check-all"),
    currentProblemCount: requiredElement("current-problem-count"),
    dataPath: requiredElement("data-path"),
    dataPathCandidates: requiredElement("data-path-candidates"),
    dataPathFeedback: requiredElement("data-path-feedback"),
    taskLoadStatus: requiredElement("task-load-status"),
    detailsPanel: requiredElement("details-panel"),
    diagnostics: requiredElement("diagnostics"),
    discardCancel: requiredElement("discard-cancel"),
    discardChanges: requiredElement("discard-changes"),
    discardConfirm: requiredElement("discard-confirm"),
    discardDialog: requiredElement("discard-dialog"),
    discardDialogMessage: requiredElement("discard-dialog-message"),
    discardDialogTitle: requiredElement("discard-dialog-title"),
    documentMode: requiredElement("document-mode"),
    documentPath: requiredElement("document-path"),
    draftConflict: requiredElement("draft-conflict"),
    draftDiscard: requiredElement("draft-discard"),
    draftRestore: requiredElement("draft-restore"),
    editorBody: requiredElement("editor-body"),
    editorMain: requiredElement("editor-main"),
    filePanel: requiredElement("file-panel"),
    helpClose: requiredElement("help-close"),
    helpDialog: requiredElement("help-dialog"),
    legacyDependencies: requiredElement("legacy-dependencies"),
    legacyDependenciesSection: requiredElement("legacy-dependencies-section"),
    legacyDiagnostics: requiredElement("legacy-diagnostics"),
    legacyConflictAll: requiredElement("legacy-conflict-all"),
    legacyCheckAll: requiredElement("legacy-check-all"),
    legacyDraftDiscard: requiredElement("legacy-draft-discard"),
    legacyDraftDiscardAll: requiredElement("legacy-draft-discard-all"),
    legacyDraftEmpty: requiredElement("legacy-draft-empty"),
    legacyDraftEditor: requiredElement("legacy-draft-editor"),
    legacyDraftPanel: requiredElement("legacy-draft-panel"),
    legacyDraftSource: requiredElement("legacy-draft-source"),
    legacyDraftTarget: requiredElement("legacy-draft-target"),
    legacyDraftTargetFeedback: requiredElement("legacy-draft-target-feedback"),
    legacyImport: requiredElement("legacy-import"),
    legacyImportReason: requiredElement("legacy-import-reason"),
    legacyIncludeSurface: requiredElement("legacy-include-surface"),
    legacyOperationReport: requiredElement("legacy-operation-report"),
    legacyPath: requiredElement("legacy-path"),
    legacyPathFeedback: requiredElement("legacy-path-feedback"),
    legacyPreview: requiredElement("legacy-preview"),
    legacyProblemsPanel: requiredElement("legacy-problems-panel"),
    legacyProblemsResizer: requiredElement("legacy-problems-resizer"),
    legacyReload: requiredElement("legacy-reload"),
    legacySelectAll: requiredElement("legacy-select-all"),
    legacySelectNone: requiredElement("legacy-select-none"),
    legacyStatus: requiredElement("legacy-status"),
    legacySourceStep: requiredElement("legacy-source-step"),
    legacySurface: requiredElement("legacy-surface"),
    legacyTargetFeedback: requiredElement("legacy-target-feedback"),
    legacyTargetProfile: requiredElement("legacy-target-profile"),
    legacyWidgetMappings: requiredElement("legacy-widget-mappings"),
    legacyWidgetMappingsSection: requiredElement("legacy-widget-mappings-section"),
    legacyZones: requiredElement("legacy-zones"),
    homeHeader: requiredElement("home-header"),
    openDataPath: requiredElement("open-data-path"),
    openLegacy: requiredElement("open-legacy"),
    notifications: requiredElement("notifications"),
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
    workflowDescription: requiredElement("workflow-description"),
    workflowEdit: requiredElement("workflow-edit"),
    workflowLegacy: requiredElement("workflow-legacy"),
    workflowTitle: requiredElement("workflow-title"),
    workspace: requiredElement("workspace"),
};
const state = {
    batch: new Map(),
    bottomPanel: "problems",
    current: null,
    draftConflicts: new Set(),
    globalProblems: [],
    legacy: { activeDraftPath: "", collapsedFolders: new Set(), drafts: new Map(), originalSources: new Map(), originalTargetPaths: new Map(), preview: null, profileAuthor: "user", resolutions: new Map(), selectedZonePaths: new Set(), surfaceName: "", targetPaths: new Map(), targetProfileId: "", widgetMappings: new Map() },
    problemFiles: new Map(),
    snippet: { choices: new Map(), conflictAction: "", insertionLine: 1, preview: null, treeEntries: [] },
    renderedDocumentPath: "",
    task: "",
};
let draftTimer = 0;
let validationTimer = 0;
let discardConfirmedAction = null;
let pendingDraft = null;
let draftWriteActive = false;
let draftWritePromise = Promise.resolve();
let legacyDraftTimer = 0;
let quickFixActive = false;
const codeEditor = createConfigurationEditor(elements.rawEditor, handleEditorChange);
const legacyDraftEditor = createConfigurationEditor(elements.legacyDraftEditor, (source) => {
    const item = legacySourceForPath(state.legacy.activeDraftPath);
    if (item) {
        state.legacy.drafts.set(item.sourcePath, { originalSourceHash: item.originalSourceHash, source });
        updateLegacyZoneTreeSelection();
        updateLegacyDraftState(item);
        scheduleLegacyDraftUpdate();
    }
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

function showNotification(value, tone) {
    const notification = document.createElement("div");
    notification.className = `notification ${tone}`;
    notification.setAttribute("role", tone === "danger" ? "alert" : "status");
    const message = document.createElement("span");
    message.textContent = typeof value === "string" ? value : JSON.stringify(value, null, 2);
    const close = document.createElement("button");
    close.className = "notification-close";
    close.title = translate("notification.close");
    close.setAttribute("aria-label", translate("notification.close"));
    close.textContent = "×";
    close.addEventListener("click", () => notification.remove());
    notification.append(message, close);
    elements.notifications.append(notification);
    if (tone === "success") window.setTimeout(() => notification.remove(), 5000);
}

function showReport(value, tone = "success") {
    showNotification(value, tone);
}

function diagnosticDetails(value) {
    if (!Array.isArray(value)) return [];
    return value.filter((item) => item && typeof item === "object" && typeof item.code === "string" && typeof item.message === "string" && ["error", "warning"].includes(item.severity));
}

function diagnosticIdentity(diagnostic) {
    return [diagnostic.path || "", diagnostic.line || "", diagnostic.severity, diagnostic.code, diagnostic.message].join("\u0000");
}

function mergeErrorDiagnostics(diagnostics) {
    const grouped = new Map();
    for (const diagnostic of diagnostics) {
        const relativePath = diagnostic.path || state.current?.path || "";
        const group = grouped.get(relativePath) || [];
        group.push(relativePath && !diagnostic.path ? { ...diagnostic, path: relativePath } : diagnostic);
        grouped.set(relativePath, group);
    }
    for (const [relativePath, newDiagnostics] of grouped) {
        if (!relativePath) {
            const existing = new Map(state.globalProblems.map((diagnostic) => [diagnosticIdentity(diagnostic), diagnostic]));
            for (const diagnostic of newDiagnostics) existing.set(diagnosticIdentity(diagnostic), diagnostic);
            state.globalProblems = [...existing.values()];
            continue;
        }
        const existing = new Map((state.problemFiles.get(relativePath) || []).map((diagnostic) => [diagnosticIdentity(diagnostic), diagnostic]));
        for (const diagnostic of newDiagnostics) existing.set(diagnosticIdentity(diagnostic), diagnostic);
        state.problemFiles.set(relativePath, [...existing.values()]);
    }
    state.bottomPanel = "problems";
    elements.allProblemsGroup.open = true;
    renderProblems();
    renderBottomPanel();
    if (state.task === "edit") writeEditorRoute({}, true);
}

function showError(error, target) {
    const diagnostics = diagnosticDetails(error.details);
    if (diagnostics.length) mergeErrorDiagnostics(diagnostics);
    const message = diagnostics.length ? translate("error.validationProblems", { count: diagnostics.length, message: error.message }) : error.message;
    if (target) setFeedback(target, message, "danger");
    else showNotification(message, "danger");
}

function setTaskAvailability(available, status = "", tone = "info") {
    for (const button of document.querySelectorAll(".task-card")) button.disabled = !available;
    setFeedback(elements.taskLoadStatus, status, tone);
}

function currentEditorRoute(overrides = {}) {
    const view = state.task || "home";
    const file = view === "edit" ? state.current?.path || "" : "";
    const browserRoute = readEditorRoute();
    const line = browserRoute.view === "edit" && browserRoute.file === file ? browserRoute.line : undefined;
    return { file, line, panel: state.bottomPanel, view, ...overrides };
}

function writeEditorRoute(overrides = {}, replace = false) {
    updateEditorRoute(currentEditorRoute(overrides), replace);
}

function showTask(task, updateRoute = true) {
    const workflows = { edit: elements.workflowEdit, legacy: elements.workflowLegacy };
    state.task = task;
    elements.homeHeader.hidden = true;
    elements.taskHome.hidden = true;
    elements.editorMain.hidden = false;
    elements.filePanel.hidden = task !== "edit";
    elements.checkAll.hidden = task !== "edit";
    elements.legacyCheckAll.hidden = task !== "legacy";
    elements.legacyDraftDiscardAll.hidden = task !== "legacy";
    elements.saveAll.hidden = task !== "edit";
    elements.editorBody.classList.toggle("file-task", task === "edit");
    elements.workspace.classList.toggle("document-task", task === "edit");
    elements.workspace.classList.toggle("legacy-task", task === "legacy");
    for (const workflow of Object.values(workflows)) workflow.hidden = workflow !== workflows[task];
    elements.workflowTitle.textContent = translate(`task.${task}.title`);
    elements.workflowDescription.textContent = translate(`task.${task}.description`);
    codeEditor.setVisible(task === "edit");
    if (updateRoute) writeEditorRoute({ line: undefined, view: task });
}

function showTaskHome(updateRoute = true) {
    state.task = "";
    elements.homeHeader.hidden = false;
    elements.editorMain.hidden = true;
    elements.checkAll.hidden = true;
    elements.legacyCheckAll.hidden = true;
    elements.legacyDraftDiscardAll.hidden = true;
    elements.saveAll.hidden = true;
    elements.taskHome.hidden = false;
    if (updateRoute) writeEditorRoute({ file: "", line: undefined, view: "home" });
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
        const row = document.createElement("div");
        row.className = "diagnostic " + (diagnostic.severity === "error" ? "danger" : diagnostic.severity);
        if (actionable) {
            const location = document.createElement("a");
            location.className = "diagnostic-location";
            const targetPath = diagnostic.path || state.current?.path || "";
            location.href = editorRouteUrl(state.task === "legacy" ? { view: "legacy" } : { file: targetPath, line: diagnostic.line, panel: state.bottomPanel, view: "edit" }).href;
            location.textContent = (diagnostic.path || "") + (diagnostic.path && diagnostic.line ? ": " : "") + (diagnostic.line ? translate("diagnostic.line", { line: diagnostic.line }).replace(/:\s*$/, "") : "");
            location.addEventListener("click", (event) => {
                event.preventDefault();
                void navigate(diagnostic);
            });
            row.append(location, document.createTextNode(" "));
        }
        const message = document.createElement("span");
        message.textContent = diagnostic.severity.toUpperCase() + " " + diagnostic.code + ": " + diagnostic.message;
        row.append(message);
        for (const related of diagnostic.related || []) {
            const relatedLink = document.createElement("a");
            relatedLink.className = "diagnostic-location";
            relatedLink.href = editorRouteUrl(state.task === "legacy" ? { view: "legacy" } : { file: related.path, line: related.line, panel: state.bottomPanel, view: "edit" }).href;
            relatedLink.textContent = related.path + (related.line ? ": " + translate("diagnostic.line", { line: related.line }).replace(/:\s*$/, "") : "");
            relatedLink.addEventListener("click", (event) => {
                event.preventDefault();
                void navigate({ ...diagnostic, fixes: undefined, line: related.line, path: related.path, related: undefined });
            });
            row.append(document.createTextNode(" "), relatedLink);
        }
        if (diagnostic.fixes?.length) {
            const fixes = document.createElement("span");
            fixes.className = "diagnostic-fixes";
            for (const fix of diagnostic.fixes) {
                const button = document.createElement("button");
                button.className = "diagnostic-fix";
                button.textContent = fix.label;
                button.addEventListener("click", () => { void applyDiagnosticQuickFix(diagnostic, fix); });
                fixes.append(button);
            }
            row.append(fixes);
        }
        container.append(row);
    }
}

function legacySourceForPath(sourcePath) {
    return state.legacy.preview?.items.find((candidate) => candidate.sourcePath === sourcePath) || state.legacy.preview?.sources?.find((candidate) => candidate.sourcePath === sourcePath);
}

function diagnosticsForAllFiles() {
    return [...state.problemFiles.values()].flat().concat(state.globalProblems);
}

function recordFileProblems(relativePath, diagnostics) {
    state.problemFiles.set(relativePath, diagnostics.map((diagnostic) => diagnostic.path ? diagnostic : { ...diagnostic, path: relativePath }));
}

function renderProblems() {
    const currentDiagnostics = state.current?.document.diagnostics || [];
    const allDiagnostics = diagnosticsForAllFiles();
    renderDiagnosticsIn(elements.diagnostics, currentDiagnostics);
    renderDiagnosticsIn(elements.allDiagnostics, allDiagnostics);
    elements.currentProblemCount.textContent = String(currentDiagnostics.length);
    elements.allProblemCount.textContent = String(allDiagnostics.length);
    elements.problemCount.textContent = String(allDiagnostics.length);
}

function replaceAllProblems(files, diagnostics) {
    state.problemFiles.clear();
    for (const file of files) recordFileProblems(file.path, file.diagnostics);
    state.globalProblems = diagnostics;
    renderProblems();
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
    window.clearTimeout(validationTimer);
    validationTimer = window.setTimeout(() => { void validateCurrent().catch((error) => showError(error)); }, 450);
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
    elements.save.disabled = !current || !current.writable || !state.batch.has(current.path);
    elements.discardChanges.disabled = !current || !current.writable || !state.batch.has(current.path) || Boolean(current.draftConflict);
    elements.clone.hidden = !current || current.writable || !["surface", "zone", "snippet"].includes(current.document.format);
    if (current) recordFileProblems(current.path, current.document.diagnostics);
    renderProblems();
    elements.semantic.textContent = JSON.stringify(current?.document.semantic || {}, null, 2);
    renderDraftConflict();
    renderSnippetToolbar();
    renderBottomPanel();
}

async function validateCurrent() {
    if (!state.current) return null;
    const path = state.current.path;
    const source = codeEditor.getValue();
    const result = await api("/api/validate", { method: "POST", body: JSON.stringify({ path, source }) });
    if (state.current?.path !== path || codeEditor.getValue() !== source) return result.document;
    state.current = { ...state.current, document: result.document, source };
    renderDocument();
    return result.document;
}

async function validateAllConfigurations() {
    await flushPendingDraft();
    const result = await api("/api/validate-all", { method: "POST", body: JSON.stringify({ changes: [...state.batch.values()] }) });
    const currentResult = result.files.find((file) => file.path === state.current?.path);
    if (currentResult && state.current) state.current.document = { ...state.current.document, diagnostics: currentResult.diagnostics };
    replaceAllProblems(result.files, result.diagnostics);
    state.bottomPanel = "problems";
    elements.allProblemsGroup.open = true;
    renderBottomPanel();
    if (state.task === "edit") writeEditorRoute({}, true);
    showReport(translate("status.checkedAll", { files: result.checkedPaths.length, problems: diagnosticsForAllFiles().length }), "info");
}

async function discardCurrentChanges() {
    if (!state.current?.writable || !state.batch.has(state.current.path)) return;
    await flushPendingDraft();
    const relativePath = state.current.path;
    await api("/api/draft/discard", { method: "POST", body: JSON.stringify({ path: relativePath }) });
    state.batch.delete(relativePath);
    state.draftConflicts.delete(relativePath);
    await openDocument(relativePath);
    showReport(translate("status.discardedChanges", { path: relativePath }));
}

async function openDocument(path, line, updateRoute = true) {
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
        if (updateRoute) updateEditorRoute(currentEditorRoute({ file: path, line, view: "edit" }));
        return true;
    } catch (error) {
        showError(error);
        return false;
    }
}

function clearCurrentDocument() {
    state.current = null;
    updateTreeSelection("");
    renderDocument();
}

async function restoreEditorRoute(route) {
    await flushPendingDraft();
    state.bottomPanel = route.panel;
    if (route.view === "legacy") {
        showTask("legacy", false);
        renderBottomPanel();
        updateEditorRoute({ file: "", line: undefined, panel: state.bottomPanel, view: "legacy" }, true);
        return;
    }
    if (route.view === "edit") {
        showTask("edit", false);
        renderBottomPanel();
        if (!route.file) {
            clearCurrentDocument();
            updateEditorRoute({ file: "", line: undefined, panel: state.bottomPanel, view: "edit" }, true);
            return;
        }
        const opened = await openDocument(route.file, route.line, false);
        if (!opened) {
            clearCurrentDocument();
            updateEditorRoute({ file: "", line: undefined, panel: state.bottomPanel, view: "edit" }, true);
            return;
        }
        updateEditorRoute({ file: route.file, line: route.line, panel: state.bottomPanel, view: "edit" }, true);
        return;
    }
    showTaskHome(false);
    renderBottomPanel();
    updateEditorRoute({ file: "", line: undefined, panel: state.bottomPanel, view: "home" }, true);
}

async function navigateDiagnostic(diagnostic) {
    if (state.task === "legacy") {
        const item = legacySourceForPath(diagnostic.path) || state.legacy.preview?.items.find((candidate) => candidate.targetPath === diagnostic.path);
        if (item) {
            openLegacyDraft(item, diagnostic.line);
            return;
        }
        showNotification(translate("legacy.source.unavailable", { path: diagnostic.path || "" }), "warning");
        return;
    }
    if (!diagnostic.path && !state.current) return;
    showTask("edit", false);
    codeEditor.setVisible(true);
    if (diagnostic.path && diagnostic.path !== state.current?.path) await openDocument(diagnostic.path, diagnostic.line);
    else {
        if (diagnostic.line) requestAnimationFrame(() => codeEditor.goToLine(diagnostic.line));
        writeEditorRoute({ file: state.current?.path || "", line: diagnostic.line, view: "edit" });
    }
}

async function applyDiagnosticQuickFix(diagnostic, fix) {
    if (quickFixActive) return;
    quickFixActive = true;
    try {
        if (state.task === "legacy") {
            const item = legacySourceForPath(diagnostic.path) || state.legacy.preview?.items.find((candidate) => candidate.targetPath === diagnostic.path);
            if (!item) throw new Error(translate("error.quickFixEditable"));
            const currentSource = state.legacy.drafts.get(item.sourcePath)?.source ?? item.source;
            if (["zone.bank.move-to-context", "zone.relationship.make-layer"].includes(fix.id)) {
                const documentItems = state.legacy.preview?.items.filter((candidate) => candidate.selected && candidate.kind === "zone") ?? [];
                const documents = documentItems.map((candidate) => ({ path: candidate.targetPath, source: state.legacy.drafts.get(candidate.sourcePath)?.source ?? candidate.source }));
                const result = await api("/api/quick-fix-set", { method: "POST", body: JSON.stringify({ diagnostic: { code: diagnostic.code, line: diagnostic.line, message: diagnostic.message }, documents, fix: { data: fix.data, id: fix.id } }) });
                for (const change of result.changes) {
                    const changedItem = state.legacy.preview?.items.find((candidate) => candidate.targetPath === change.path);
                    if (!changedItem) throw new Error(translate("error.quickFixEditable"));
                    state.legacy.drafts.set(changedItem.sourcePath, { originalSourceHash: changedItem.originalSourceHash, source: change.source });
                    await persistLegacyDraft(changedItem.sourcePath, false);
                }
                await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
                const refreshedItem = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === item.sourcePath);
                if (refreshedItem) openLegacyDraft(refreshedItem, diagnostic.line);
                showReport(translate("status.appliedQuickFix", { fix: fix.label }));
                return;
            }
            const result = await api("/api/quick-fix", { method: "POST", body: JSON.stringify({ diagnostic: { code: diagnostic.code, line: diagnostic.line, message: diagnostic.message }, fix: { data: fix.data, id: fix.id }, path: item.targetPath, source: currentSource }) });
            state.legacy.drafts.set(item.sourcePath, { originalSourceHash: item.originalSourceHash, source: result.source });
            await persistLegacyDraft(item.sourcePath);
            const refreshedItem = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === item.sourcePath);
            if (refreshedItem) openLegacyDraft(refreshedItem, diagnostic.line);
            showReport(translate("status.appliedQuickFix", { fix: fix.label }));
            return;
        }
        if (diagnostic.path && diagnostic.path !== state.current?.path) await openDocument(diagnostic.path, diagnostic.line);
        if (!state.current?.writable || diagnostic.path && diagnostic.path !== state.current.path) throw new Error(translate("error.quickFixEditable"));
        const result = await api("/api/quick-fix", { method: "POST", body: JSON.stringify({ diagnostic: { code: diagnostic.code, line: diagnostic.line, message: diagnostic.message }, fix: { data: fix.data, id: fix.id }, path: state.current.path, source: state.current.source }) });
        state.current.document = result.document;
        state.current.source = result.source;
        codeEditor.setValue(result.source);
        handleEditorChange(result.source);
        state.globalProblems = state.globalProblems.filter((candidate) => diagnosticIdentity(candidate) !== diagnosticIdentity(diagnostic));
        renderDocument();
        showReport(translate("status.appliedQuickFix", { fix: fix.label }));
    } catch (error) { showError(error); }
    finally { quickFixActive = false; }
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
    if (state.current) elements.discardChanges.disabled = !state.current.writable || !state.batch.has(state.current.path) || Boolean(state.current.draftConflict);
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

function legacyDraftTargetPath(item) {
    return state.legacy.targetPaths.get(item.sourcePath) ?? item.targetPath;
}

function legacyDraftIsDirty(item) {
    const originalSource = state.legacy.originalSources.get(item.sourcePath) ?? item.source;
    const source = state.legacy.drafts.get(item.sourcePath)?.source ?? item.source;
    const originalTargetPath = state.legacy.originalTargetPaths.get(item.sourcePath) ?? item.targetPath;
    return source !== originalSource || legacyDraftTargetPath(item) !== originalTargetPath;
}

function updateLegacyDraftState(item) {
    elements.legacyDraftDiscard.disabled = !legacyDraftIsDirty(item);
    elements.legacyDraftDiscardAll.disabled = state.legacy.drafts.size === 0 && state.legacy.targetPaths.size === 0;
    setFeedback(elements.legacyDraftTargetFeedback, item.targetExists && !legacyItemAlreadyImported(item) ? translate("legacy.conflict.unresolved") : "", "danger");
}

async function persistLegacyDraft(sourcePath, refresh = true) {
    const item = legacySourceForPath(sourcePath);
    if (!item) return;
    const originalSource = state.legacy.originalSources.get(sourcePath) ?? item.source;
    const source = state.legacy.drafts.get(sourcePath)?.source ?? item.source;
    const originalTargetPath = state.legacy.originalTargetPaths.get(sourcePath) ?? item.targetPath;
    const targetPath = legacyDraftTargetPath(item);
    await api("/api/legacy/draft", { method: "POST", body: JSON.stringify({ originalSource, originalSourceHash: item.originalSourceHash, originalTargetPath, source, sourcePath, surfaceName: state.legacy.surfaceName, targetPath, targetProfileId: state.legacy.targetProfileId }) });
    if (source === originalSource && targetPath === originalTargetPath) {
        state.legacy.drafts.delete(sourcePath);
        state.legacy.targetPaths.delete(sourcePath);
    }
    if (refresh) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
}

function scheduleLegacyDraftUpdate() {
    window.clearTimeout(legacyDraftTimer);
    const sourcePath = state.legacy.activeDraftPath;
    legacyDraftTimer = window.setTimeout(() => { void persistLegacyDraft(sourcePath).catch((error) => showError(error, elements.legacyDraftTargetFeedback)); }, 450);
}

async function restoreLegacyDrafts() {
    const query = new URLSearchParams({ surfaceName: elements.legacySurface.value, targetProfileId: state.legacy.targetProfileId });
    const result = await api(`/api/legacy/drafts?${query}`);
    let restored = false;
    for (const draft of result.drafts) {
        const source = legacySourceForPath(draft.sourcePath);
        if (!source || source.originalSourceHash !== draft.originalSourceHash) continue;
        state.legacy.originalSources.set(draft.sourcePath, state.legacy.originalSources.get(draft.sourcePath) ?? source.source);
        state.legacy.originalTargetPaths.set(draft.sourcePath, draft.originalTargetPath);
        state.legacy.drafts.set(draft.sourcePath, { originalSourceHash: draft.originalSourceHash, source: draft.source });
        state.legacy.targetPaths.set(draft.sourcePath, draft.targetPath);
        if (source.kind === "zone" || source.kind === "learn-fx") state.legacy.selectedZonePaths.add(draft.sourcePath);
        restored = true;
    }
    if (restored) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
}

function closeLegacyDraft() {
    state.legacy.activeDraftPath = "";
    legacyDraftEditor.setVisible(false);
    elements.legacyDraftPanel.hidden = true;
    elements.legacyDraftEmpty.hidden = false;
    setFeedback(elements.legacyDraftTargetFeedback, "");
    elements.legacyDraftDiscardAll.disabled = state.legacy.drafts.size === 0 && state.legacy.targetPaths.size === 0;
    updateLegacyZoneTreeSelection();
}

function openLegacyDraft(item, line) {
    const changedItem = state.legacy.activeDraftPath !== item.sourcePath;
    state.legacy.activeDraftPath = item.sourcePath;
    elements.legacyDraftEmpty.hidden = true;
    elements.legacyDraftPanel.hidden = false;
    legacyDraftEditor.setVisible(true);
    elements.legacyDraftSource.textContent = translate("legacy.source") + ": " + item.sourcePath;
    elements.legacyDraftTarget.value = legacyDraftTargetPath(item);
    elements.legacyDraftTarget.readOnly = item.kind === "learn-fx";
    const draft = state.legacy.drafts.get(item.sourcePath);
    legacyDraftEditor.setValue(draft?.source ?? item.source, changedItem);
    updateLegacyDraftState(item);
    updateLegacyZoneTreeSelection();
    if (line) requestAnimationFrame(() => legacyDraftEditor.goToLine(line));
}

function selectedLegacyItems() {
    return (state.legacy.preview?.items || []).filter((item) => item.selected);
}

function legacyItemAlreadyImported(item) {
    return item.targetExists && Boolean(item.targetHash) && item.targetHash === item.sourceHash;
}

function resolutionFor(item) {
    let resolution = state.legacy.resolutions.get(item.id);
    if (!resolution || resolution.sourceHash !== item.sourceHash || resolution.targetHash !== item.targetHash) {
        resolution = { action: legacyItemAlreadyImported(item) ? "skip" : item.targetExists ? "" : "create", id: item.id, sourceHash: item.sourceHash, targetHash: item.targetHash };
        state.legacy.resolutions.set(item.id, resolution);
    }
    return resolution;
}

function updateLegacyImportButton() {
    const preview = state.legacy.preview;
    const selectedItems = selectedLegacyItems();
    const pendingItems = selectedItems.filter((item) => !legacyItemAlreadyImported(item));
    const unresolvedConflictCount = pendingItems.filter((item) => {
        const resolution = resolutionFor(item);
        if (!item.targetExists) return resolution.action !== "create";
        if (!["rename", "replace", "skip"].includes(resolution.action)) return true;
        return resolution.action === "rename" && !resolution.targetPath;
    }).length;
    const errorCount = preview?.diagnostics.filter((diagnostic) => diagnostic.severity === "error").length || 0;
    const mappingErrorCount = preview?.diagnostics.filter((diagnostic) => diagnostic.severity === "error" && diagnostic.code === "legacy.widget.mapping.required").length || 0;
    let message = translate("legacy.import.selectSurface");
    let ready = false;
    let statusClass = "danger";
    if (preview && !selectedItems.length) message = translate("legacy.import.selectFiles");
    else if (preview && mappingErrorCount) message = translate("legacy.import.resolveMappings", { count: mappingErrorCount });
    else if (preview && errorCount) message = translate("legacy.import.fixErrors", { count: errorCount });
    else if (preview && unresolvedConflictCount) message = translate("legacy.import.resolveConflicts", { count: unresolvedConflictCount });
    else if (preview && !pendingItems.length) {
        message = translate("legacy.import.identical");
        statusClass = "success";
    }
    else if (preview) {
        message = translate("legacy.import.ready", { count: pendingItems.length });
        ready = true;
        statusClass = "success";
    }
    elements.legacyImport.disabled = !ready;
    elements.legacyImportReason.className = statusClass;
    elements.legacyImportReason.textContent = message;
    if (preview && !mappingErrorCount && !errorCount && unresolvedConflictCount) elements.legacyImportReason.setAttribute("href", "#legacy-preview");
    else elements.legacyImportReason.removeAttribute("href");
}

function addLegacyZoneWithDependencies(sourcePath, selectedPaths, preview) {
    selectedPaths.add(sourcePath);
    const pendingPaths = [sourcePath];
    const visitedPaths = new Set();
    while (pendingPaths.length) {
        const pendingPath = pendingPaths.shift();
        if (!pendingPath || visitedPaths.has(pendingPath)) continue;
        visitedPaths.add(pendingPath);
        for (const dependency of preview.dependencies.filter((candidate) => candidate.from === pendingPath && candidate.matches.length === 1)) {
            const dependencyPath = dependency.matches[0];
            if (selectedPaths.has(dependencyPath)) continue;
            selectedPaths.add(dependencyPath);
            pendingPaths.push(dependencyPath);
        }
    }
}

async function setLegacyZonesSelected(zones, selected) {
    const preview = state.legacy.preview;
    if (!preview) return;
    const selectedPaths = new Set(state.legacy.selectedZonePaths);
    if (selected) {
        for (const zone of zones) addLegacyZoneWithDependencies(zone.sourcePath, selectedPaths, preview);
    } else for (const zone of zones) selectedPaths.delete(zone.sourcePath);
    await refreshLegacyPreview([...selectedPaths]);
}

async function setLegacyZoneSelected(zone, selected) {
    await setLegacyZonesSelected([zone], selected);
}

function updateLegacyZoneTreeSelection() {
    for (const button of elements.legacyZones.querySelectorAll("button[data-path]")) {
        const dirty = state.legacy.drafts.has(button.dataset.path);
        button.classList.toggle("selected", button.dataset.path === state.legacy.activeDraftPath);
        button.classList.toggle("dirty", dirty);
        button.textContent = button.dataset.label + (dirty ? " *" : "");
    }
}

function renderLegacyZones() {
    const preview = state.legacy.preview;
    const importItemsByPath = new Map((preview?.items || []).filter((item) => item.kind === "zone" || item.kind === "learn-fx").map((item) => [item.sourcePath, item]));
    const zones = (preview?.sources || []).filter((source) => /^(?:Zones|FXZones)\/.+\.zon$/i.test(source.sourcePath)).map((source) => importItemsByPath.get(source.sourcePath) || { ...source, auxiliary: true });
    elements.legacyZones.replaceChildren();
    if (!zones.length) {
        elements.legacyZones.className = "legacy-zone-tree secondary";
        elements.legacyZones.textContent = translate("legacy.zones.empty");
        return;
    }
    elements.legacyZones.className = "legacy-zone-tree";
    const root = { directories: new Map(), files: [] };
    for (const zone of zones) {
        const pathParts = zone.sourcePath.split("/");
        const fileName = pathParts.pop();
        let directory = root;
        for (const pathPart of pathParts) {
            if (!directory.directories.has(pathPart)) directory.directories.set(pathPart, { directories: new Map(), files: [] });
            directory = directory.directories.get(pathPart);
        }
        directory.files.push({ fileName, zone });
    }
    const selectableZones = (directory) => [...directory.files.map((file) => file.zone), ...[...directory.directories.values()].flatMap(selectableZones)].filter((zone) => !zone.auxiliary);
    const renderDirectory = (directory, parentPath = "") => {
        const list = document.createElement("ul");
        for (const [directoryName, childDirectory] of [...directory.directories].sort(([leftName], [rightName]) => leftName.localeCompare(rightName))) {
            const directoryPath = parentPath ? parentPath + "/" + directoryName : directoryName;
            const directoryZones = selectableZones(childDirectory);
            const selectedCount = directoryZones.filter((zone) => state.legacy.selectedZonePaths.has(zone.sourcePath)).length;
            const item = document.createElement("li");
            const details = document.createElement("details");
            details.dataset.path = directoryPath;
            details.open = !state.legacy.collapsedFolders.has(directoryPath);
            const summary = document.createElement("summary");
            const checkbox = document.createElement("input");
            checkbox.type = "checkbox";
            checkbox.disabled = !directoryZones.length;
            checkbox.checked = Boolean(directoryZones.length) && selectedCount === directoryZones.length;
            checkbox.indeterminate = selectedCount > 0 && selectedCount < directoryZones.length;
            checkbox.title = translate("legacy.folder.select", { folder: directoryPath });
            checkbox.addEventListener("click", (event) => event.stopPropagation());
            checkbox.addEventListener("change", () => {
                details.open = checkbox.checked;
                if (checkbox.checked) state.legacy.collapsedFolders.delete(directoryPath);
                else state.legacy.collapsedFolders.add(directoryPath);
                void setLegacyZonesSelected(directoryZones, checkbox.checked).catch(showError);
            });
            const icon = document.createElement("span");
            icon.className = "folder-icon";
            summary.append(checkbox, icon, document.createTextNode(directoryName));
            details.addEventListener("toggle", () => {
                if (details.open) state.legacy.collapsedFolders.delete(directoryPath);
                else state.legacy.collapsedFolders.add(directoryPath);
            });
            details.append(summary, renderDirectory(childDirectory, directoryPath));
            item.append(details);
            list.append(item);
        }
        for (const { fileName, zone } of directory.files.sort((left, right) => left.fileName.localeCompare(right.fileName))) {
            const item = document.createElement("li");
            const row = document.createElement("div");
            row.className = "legacy-zone-row";
            const button = document.createElement("button");
            button.dataset.path = zone.sourcePath;
            button.dataset.label = fileName + (zone.zoneName ? " [" + zone.zoneName + "]" : zone.auxiliary ? " [" + translate("legacy.source.learnFx") + "]" : "");
            button.title = zone.sourcePath;
            button.addEventListener("click", () => openLegacyDraft(zone));
            if (!zone.auxiliary) {
                const checkbox = document.createElement("input");
                checkbox.type = "checkbox";
                checkbox.checked = state.legacy.selectedZonePaths.has(zone.sourcePath);
                checkbox.title = zone.sourcePath;
                checkbox.addEventListener("change", () => { void setLegacyZoneSelected(zone, checkbox.checked).catch(showError); });
                row.append(checkbox);
            } else row.append(document.createElement("span"));
            row.append(button);
            item.append(row);
            list.append(item);
        }
        return list;
    };
    elements.legacyZones.append(renderDirectory(root));
    updateLegacyZoneTreeSelection();
}

function renderLegacyDependencies() {
    const preview = state.legacy.preview;
    const dependencies = (preview?.dependencies || []).filter((dependency) => dependency.selected);
    elements.legacyDependenciesSection.hidden = !dependencies.length;
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
    elements.legacyWidgetMappingsSection.hidden = !issues.length;
    elements.legacyWidgetMappings.replaceChildren();
    if (!issues.length) {
        elements.legacyWidgetMappings.className = "legacy-widget-mappings secondary";
        elements.legacyWidgetMappings.textContent = translate("legacy.widgetMappings.empty");
        return;
    }
    elements.legacyWidgetMappings.className = "legacy-widget-mappings";
    for (const [issueIdx, issue] of issues.entries()) {
        const row = document.createElement("div");
        row.className = "legacy-widget-mapping";
        const source = document.createElement("strong");
        source.textContent = issue.sourceWidget;
        const details = document.createElement("small");
        const reason = translate("legacy.widgetMappings." + issue.reason);
        const required = translate("legacy.widgetMappings.required", { capabilities: translatedCapabilities(issue.requiredCapabilities) });
        const occurrences = translate("legacy.widgetMappings.occurrences", { count: issue.occurrences.length });
        details.textContent = reason + " " + required + " " + occurrences;
        const occurrenceLinks = document.createElement("div");
        occurrenceLinks.className = "legacy-widget-occurrences";
        for (const occurrence of issue.occurrences) {
            const link = document.createElement("a");
            link.href = editorRouteUrl({ view: "legacy" }).href;
            link.textContent = translate("legacy.widgetMappings.openOccurrence", { line: occurrence.line, path: occurrence.path });
            link.addEventListener("click", (event) => {
                event.preventDefault();
                const item = state.legacy.preview?.items.find((candidate) => candidate.sourcePath === occurrence.path);
                if (item) openLegacyDraft(item, occurrence.line);
            });
            occurrenceLinks.append(link);
        }
        const input = document.createElement("input");
        const suggestions = document.createElement("datalist");
        suggestions.id = "legacy-widget-mapping-" + issueIdx;
        input.setAttribute("list", suggestions.id);
        input.placeholder = translate("legacy.widgetMappings.choose");
        for (const candidate of issue.candidates) {
            const option = document.createElement("option");
            option.value = candidate.name;
            option.label = "[" + translatedCapabilities(candidate.capabilities) + "]";
            suggestions.append(option);
        }
        input.value = state.legacy.widgetMappings.get(issue.sourceWidget) || issue.selectedTarget || "";
        input.addEventListener("change", async () => {
            try {
                const targetExpression = input.value.trim();
                if (targetExpression) state.legacy.widgetMappings.set(issue.sourceWidget, targetExpression);
                else state.legacy.widgetMappings.delete(issue.sourceWidget);
                await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
            } catch (error) { showError(error); }
        });
        row.append(source, input, suggestions, details, occurrenceLinks);
        elements.legacyWidgetMappings.append(row);
    }
}

function renderLegacyPreview() {
    const preview = state.legacy.preview;
    elements.legacyCheckAll.disabled = !preview;
    elements.legacyDraftDiscardAll.disabled = state.legacy.drafts.size === 0 && state.legacy.targetPaths.size === 0;
    renderLegacyZones();
    renderLegacyDependencies();
    renderLegacyWidgetMappings();
    elements.legacyPreview.replaceChildren();
    if (!preview) {
        elements.legacyConflictAll.disabled = true;
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
    elements.legacyConflictAll.disabled = !selectedItems.some((item) => item.targetExists && !legacyItemAlreadyImported(item));
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
        const conflictMessage = document.createElement("p");
        conflictMessage.className = "legacy-conflict-message";
        conflictMessage.textContent = translate("legacy.conflict.unresolved");
        const header = document.createElement("div");
        header.className = "legacy-item-header";
        const sourcePath = document.createElement("button");
        sourcePath.className = "legacy-source-link";
        sourcePath.textContent = translate("legacy.source") + ": " + item.sourcePath;
        sourcePath.addEventListener("click", () => openLegacyDraft(item));
        const zoneTargetPrefix = `Zones/User/${state.legacy.targetProfileId}/`;
        const surfaceTargetPrefix = "Surfaces/User/";
        const resolution = resolutionFor(item);
        const targetPath = document.createElement(resolution.action === "rename" ? "label" : "span");
        targetPath.className = resolution.action === "rename" ? "legacy-rename-target" : "legacy-target-path";
        const resolvedTargetPath = resolution.action === "rename" ? resolution.targetPath || "" : item.targetPath;
        const compactTargetPath = resolvedTargetPath.startsWith(zoneTargetPrefix) ? resolvedTargetPath.slice(zoneTargetPrefix.length) : resolvedTargetPath.startsWith(surfaceTargetPrefix) ? resolvedTargetPath.slice(surfaceTargetPrefix.length) : resolvedTargetPath;
        if (resolution.action === "rename") {
            const targetLabel = document.createElement("span");
            targetLabel.textContent = translate("legacy.target") + ":";
            const targetInput = document.createElement("input");
            targetInput.value = compactTargetPath;
            targetInput.addEventListener("input", () => {
                const requestedTargetPath = targetInput.value.trim().replaceAll("\\", "/");
                resolution.targetPath = requestedTargetPath ? requestedTargetPath.startsWith(zoneTargetPrefix) ? requestedTargetPath : zoneTargetPrefix + requestedTargetPath.replace(/^\/+/, "") : "";
                renderConflictState();
                updateLegacyImportButton();
            });
            targetPath.append(targetLabel, targetInput);
        } else {
            targetPath.textContent = translate("legacy.target") + ": " + compactTargetPath;
        }
        const renderConflictState = () => {
            const unresolved = item.targetExists && (!["rename", "replace", "skip"].includes(resolution.action) || (resolution.action === "rename" && !resolution.targetPath));
            container.classList.toggle("unresolved-conflict", unresolved);
        };
        let actionControl;
        if (legacyItemAlreadyImported(item)) {
            actionControl = document.createElement("span");
            actionControl.className = "success";
            actionControl.textContent = translate("legacy.conflict.identical");
        } else if (item.targetExists) {
            actionControl = document.createElement("select");
            actionControl.className = "legacy-conflict-action";
            const conflictActions = item.kind === "zone" ? [["", "legacy.conflict.choose"], ["replace", "legacy.conflict.replace"], ["rename", "legacy.conflict.rename"], ["skip", "legacy.conflict.skip"]] : [["", "legacy.conflict.choose"], ["replace", "legacy.conflict.replace"], ["skip", "legacy.conflict.skip"]];
            for (const [value, key] of conflictActions) {
                const option = document.createElement("option");
                option.value = value;
                option.textContent = translate(key);
                actionControl.append(option);
            }
            actionControl.value = resolution.action;
            actionControl.addEventListener("change", async () => {
                resolution.action = actionControl.value;
                if (resolution.action === "rename" && !resolution.targetPath) resolution.targetPath = renameSuggestion(item.targetPath);
                if (item.kind === "surface") {
                    try { await refreshLegacyPreview([...state.legacy.selectedZonePaths], usesExistingLegacySurface()); } catch (error) { showError(error); }
                } else {
                    renderLegacyPreview();
                }
            });
        } else {
            actionControl = document.createElement("span");
            actionControl.textContent = translate("legacy.conflict.create");
        }
        renderConflictState();
        const statusRow = document.createElement("div");
        statusRow.className = "legacy-item-status";
        if (item.targetExists && !legacyItemAlreadyImported(item)) statusRow.append(conflictMessage, actionControl);
        else statusRow.append(actionControl);
        header.append(sourcePath, targetPath);
        container.append(header, statusRow);
        elements.legacyPreview.append(container);
    }
    const activeItem = legacySourceForPath(state.legacy.activeDraftPath);
    if (activeItem) {
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
    state.legacy.surfaceName = elements.legacySurface.value;
    for (const source of result.preview.sources || []) if (!state.legacy.originalSources.has(source.sourcePath)) state.legacy.originalSources.set(source.sourcePath, source.source);
    for (const item of result.preview.items) if (!state.legacy.originalTargetPaths.has(item.sourcePath) && !state.legacy.targetPaths.has(item.sourcePath)) state.legacy.originalTargetPaths.set(item.sourcePath, item.targetPath);
    state.legacy.selectedZonePaths = new Set(result.preview.selectedZonePaths);
    state.legacy.targetProfileId = result.preview.targetProfileId;
    elements.legacyTargetProfile.value = result.preview.targetProfileId;
    elements.legacyStatus.textContent = translate("legacy.status.opened", { path: result.preview.root }) + (result.preview.recommendedSourceMode === "User" ? translate("legacy.status.userSource") : "");
    state.legacy.widgetMappings = new Map(result.preview.widgetMappings.filter((issue) => issue.selectedTarget).map((issue) => [issue.sourceWidget, issue.selectedTarget]));
    elements.legacySelectAll.disabled = false;
    elements.legacySelectNone.disabled = false;
    renderLegacyPreview();
}

function renderLegacySource(selection, selectedSurfaceName = "") {
    elements.legacyPath.value = selection?.path || "";
    elements.legacySurface.replaceChildren();
    const placeholder = document.createElement("option");
    placeholder.value = "";
    placeholder.textContent = translate("legacy.surface.placeholder");
    elements.legacySurface.append(placeholder);
    let selectedSurfaceAvailable = false;
    for (const surface of selection?.surfaces || []) {
        const option = document.createElement("option");
        option.value = surface.name;
        option.textContent = translate("legacy.surface.option", { count: surface.zoneCount, fxCount: surface.fxZoneCount, name: surface.name });
        option.dataset.targetProfileId = surface.stableId + "-by-" + state.legacy.profileAuthor;
        elements.legacySurface.append(option);
        if (surface.name === selectedSurfaceName) selectedSurfaceAvailable = true;
    }
    elements.legacySurface.value = selectedSurfaceAvailable ? selectedSurfaceName : "";
    elements.legacySurface.disabled = !selection?.surfaces?.length;
    elements.legacyReload.disabled = !selection?.path;
    elements.legacyStatus.className = "secondary";
    elements.legacyStatus.textContent = selection?.root ? translate("legacy.status.opened", { path: selection.root }) : translate("legacy.status.notFound", { path: selection?.path || "CSI" });
    return selectedSurfaceAvailable;
}

function renderLegacySelection(selection) {
    closeLegacyDraft();
    elements.legacyOperationReport.hidden = true;
    elements.legacyOperationReport.textContent = "";
    state.legacy.drafts.clear();
    state.legacy.originalSources.clear();
    state.legacy.originalTargetPaths.clear();
    state.legacy.collapsedFolders.clear();
    state.legacy.preview = null;
    state.legacy.resolutions.clear();
    state.legacy.selectedZonePaths.clear();
    state.legacy.targetPaths.clear();
    state.legacy.targetProfileId = "";
    state.legacy.surfaceName = "";
    state.legacy.widgetMappings.clear();
    elements.legacyTargetProfile.value = "";
    setFeedback(elements.legacyTargetFeedback, "");
    renderLegacySource(selection);
    elements.legacySelectAll.disabled = true;
    elements.legacySelectNone.disabled = true;
    elements.legacySourceStep.open = true;
    renderLegacyPreview();
}

async function applyDataPath(dataPath, updateRoute = true) {
    await flushPendingDraft();
    const result = await api("/api/select-data-path", { method: "POST", body: JSON.stringify({ path: dataPath }) });
    elements.dataPath.value = result.dataPath;
    setFeedback(elements.dataPathFeedback, "");
    state.current = null;
    state.globalProblems = [];
    state.problemFiles.clear();
    state.snippet.choices.clear();
    state.snippet.conflictAction = "";
    state.snippet.preview = null;
    await refreshDrafts();
    renderDocument();
    renderLegacySelection(result.legacy);
    await refreshTree();
    setTaskAvailability(true);
    showTaskHome(updateRoute);
}

async function initialize(initialRoute) {
    try {
        const translationsResponse = await fetch("/app-translations.json");
        if (!translationsResponse.ok) throw new Error(translationsResponse.statusText);
        translations = await translationsResponse.json();
        setTaskAvailability(false, translate("tasks.loading"));
        if (!token) {
            setTaskAvailability(false, translate("tasks.loadFailed", { message: translate("error.missingToken") }), "danger");
            return;
        }
        const status = await api("/api/status");
        state.legacy.profileAuthor = status.profileAuthor || "user";
        codeEditor.setActionCompletions(status.actions);
        legacyDraftEditor.setActionCompletions(status.actions);
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
            await restoreEditorRoute(initialRoute);
            return;
        }
        const existingCandidates = status.candidates.filter((candidate) => candidate.exists);
        if (existingCandidates.length) {
            elements.dataPath.value = existingCandidates[0].path;
            let lastError;
            for (const candidate of existingCandidates) {
                try {
                    await applyDataPath(candidate.path, false);
                    await restoreEditorRoute(initialRoute);
                    return;
                } catch (error) {
                    lastError = error;
                }
            }
            if (lastError) setTaskAvailability(false, translate("tasks.loadFailed", { message: lastError.message }), "danger");
        } else if (status.candidates.length) {
            elements.dataPath.value = status.candidates[0].path;
            setTaskAvailability(false, translate("tasks.openDataFirst"), "danger");
        } else setTaskAvailability(false, translate("tasks.openDataFirst"), "danger");
    } catch (error) {
        const message = error instanceof Error ? error.message : String(error);
        const failureMessage = translations["tasks.loadFailed"] ? translate("tasks.loadFailed", { message }) : `Configuration loading failed: ${message}`;
        setTaskAvailability(false, failureMessage, "danger");
    }
}

for (const button of document.querySelectorAll("[data-open-help]")) button.addEventListener("click", () => elements.helpDialog.showModal());
elements.helpClose.addEventListener("click", () => elements.helpDialog.close());

elements.legacyProblemsResizer.addEventListener("pointerdown", (event) => {
    const startHeight = elements.legacyProblemsPanel.getBoundingClientRect().height;
    const startPosition = event.clientY;
    const maximumHeight = Math.max(100, elements.legacyProblemsPanel.parentElement.clientHeight - 150);
    const resize = (moveEvent) => { elements.legacyProblemsPanel.style.height = Math.max(100, Math.min(maximumHeight, startHeight + startPosition - moveEvent.clientY)) + "px"; };
    const stop = () => { window.removeEventListener("pointermove", resize); window.removeEventListener("pointerup", stop); };
    window.addEventListener("pointermove", resize);
    window.addEventListener("pointerup", stop);
});

elements.openDataPath.addEventListener("click", async () => {
    try {
        const requestedRoute = readEditorRoute();
        await applyDataPath(elements.dataPath.value, false);
        await restoreEditorRoute(requestedRoute);
    } catch (error) { showError(error, elements.dataPathFeedback); }
});

elements.openLegacy.addEventListener("click", async () => {
    try {
        const result = await api("/api/legacy/select", { method: "POST", body: JSON.stringify({ path: elements.legacyPath.value }) });
        renderLegacySelection(result);
        setFeedback(elements.legacyPathFeedback, "");
    } catch (error) { showError(error, elements.legacyPathFeedback); }
});

elements.legacyImportReason.addEventListener("click", (event) => {
    const firstConflict = elements.legacyPreview.querySelector(".legacy-item.unresolved-conflict");
    if (!firstConflict) return;
    event.preventDefault();
    firstConflict.scrollIntoView({ behavior: "smooth", block: "center" });
    firstConflict.querySelector("select")?.focus({ preventScroll: true });
});

elements.legacyReload.addEventListener("click", async () => {
    try {
        const selectedSurfaceName = elements.legacySurface.value;
        const selectedZonePaths = [...state.legacy.selectedZonePaths];
        const result = await api("/api/legacy/select", { method: "POST", body: JSON.stringify({ path: elements.legacyPath.value }) });
        const selectedSurfaceAvailable = renderLegacySource(result, selectedSurfaceName);
        setFeedback(elements.legacyPathFeedback, "");
        if (selectedSurfaceName && !selectedSurfaceAvailable) {
            elements.legacyImport.disabled = true;
            showReport(translate("legacy.reload.surfaceMissing", { name: selectedSurfaceName }), "warning");
            return;
        }
        if (selectedSurfaceName) {
            elements.legacyImport.disabled = true;
            await refreshLegacyPreview(selectedZonePaths);
        }
        showReport(translate("status.reloadedLegacy"));
    } catch (error) { showError(error); }
});

elements.legacySurface.addEventListener("change", async () => {
    try {
        window.clearTimeout(legacyDraftTimer);
        if (state.legacy.activeDraftPath) await persistLegacyDraft(state.legacy.activeDraftPath, false);
        closeLegacyDraft();
        state.legacy.collapsedFolders.clear();
        state.legacy.drafts.clear();
        state.legacy.originalSources.clear();
        state.legacy.originalTargetPaths.clear();
        state.legacy.resolutions.clear();
        state.legacy.selectedZonePaths.clear();
        state.legacy.targetPaths.clear();
        state.legacy.targetProfileId = "";
        state.legacy.surfaceName = elements.legacySurface.value;
        elements.legacyTargetProfile.value = "";
        state.legacy.widgetMappings.clear();
        if (elements.legacySurface.value) {
            state.legacy.targetProfileId = elements.legacySurface.selectedOptions[0]?.dataset.targetProfileId || "";
            elements.legacyTargetProfile.value = state.legacy.targetProfileId;
            await refreshLegacyPreview();
            await restoreLegacyDrafts();
        }
        else {
            state.legacy.preview = null;
            renderLegacyPreview();
        }
    } catch (error) { showError(error); }
});

elements.legacyTargetProfile.addEventListener("change", async () => {
    try {
        window.clearTimeout(legacyDraftTimer);
        if (state.legacy.activeDraftPath) await persistLegacyDraft(state.legacy.activeDraftPath, false);
        setFeedback(elements.legacyTargetFeedback, "");
        state.legacy.targetProfileId = elements.legacyTargetProfile.value.trim();
        state.legacy.drafts.clear();
        state.legacy.originalSources.clear();
        state.legacy.originalTargetPaths.clear();
        state.legacy.targetPaths.clear();
        if (elements.legacySurface.value) {
            await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
            await restoreLegacyDrafts();
        }
    } catch (error) { showError(error, elements.legacyTargetFeedback); }
});

async function discardLegacyDraft(sourcePath) {
    const discardedItem = legacySourceForPath(sourcePath);
    window.clearTimeout(legacyDraftTimer);
    await api("/api/legacy/draft/discard", { method: "POST", body: JSON.stringify({ sourcePath, surfaceName: elements.legacySurface.value, targetProfileId: state.legacy.targetProfileId }) });
    state.legacy.drafts.delete(sourcePath);
    state.legacy.targetPaths.delete(sourcePath);
    if (discardedItem?.kind === "learn-fx") state.legacy.selectedZonePaths.delete(sourcePath);
    await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
    const item = legacySourceForPath(sourcePath);
    if (item) openLegacyDraft(item);
}

elements.legacyDraftDiscard.addEventListener("click", () => showDiscardConfirmation(translate("discard.importDraft.title"), translate("discard.importDraft.message"), translate("legacy.draft.discard"), async () => discardLegacyDraft(state.legacy.activeDraftPath)));

elements.legacyCheckAll.addEventListener("click", async () => {
    try {
        if (state.legacy.activeDraftPath) await persistLegacyDraft(state.legacy.activeDraftPath, false);
        renderDiagnosticsIn(elements.legacyDiagnostics);
        await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
        showReport(translate("status.checked"), "info");
    } catch (error) { showError(error); }
});

elements.legacyDraftDiscardAll.addEventListener("click", () => showDiscardConfirmation(translate("discard.importDrafts.title"), translate("discard.importDrafts.message"), translate("action.discardAll"), async () => {
    window.clearTimeout(legacyDraftTimer);
    await api("/api/legacy/drafts/discard-all", { method: "POST", body: JSON.stringify({ surfaceName: elements.legacySurface.value, targetProfileId: state.legacy.targetProfileId }) });
    for (const sourcePath of new Set([...state.legacy.drafts.keys(), ...state.legacy.targetPaths.keys()])) if (legacySourceForPath(sourcePath)?.kind === "learn-fx") state.legacy.selectedZonePaths.delete(sourcePath);
    state.legacy.drafts.clear();
    state.legacy.targetPaths.clear();
    await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
}));

elements.legacyIncludeSurface.addEventListener("change", async () => {
    try {
        state.legacy.widgetMappings.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
    } catch (error) { showError(error); }
});

elements.legacySelectAll.addEventListener("click", async () => {
    try {
        state.legacy.collapsedFolders.clear();
        const zonePaths = state.legacy.preview.items.filter((item) => item.kind === "zone" || item.kind === "learn-fx").map((item) => item.sourcePath);
        await refreshLegacyPreview(zonePaths);
    } catch (error) { showError(error); }
});

elements.legacySelectNone.addEventListener("click", async () => {
    try {
        for (const details of elements.legacyZones.querySelectorAll("details[data-path]")) state.legacy.collapsedFolders.add(details.dataset.path);
        await refreshLegacyPreview([]);
    } catch (error) { showError(error); }
});

elements.legacyDraftTarget.addEventListener("input", () => {
    const item = legacySourceForPath(state.legacy.activeDraftPath);
    if (!item) return;
    state.legacy.targetPaths.set(item.sourcePath, elements.legacyDraftTarget.value.trim());
    updateLegacyDraftState(item);
    scheduleLegacyDraftUpdate();
});

elements.legacyConflictAll.addEventListener("change", async () => {
    try {
        const action = elements.legacyConflictAll.value;
        if (!action) return;
        let surfaceChanged = false;
        for (const item of selectedLegacyItems().filter((candidate) => candidate.targetExists && !legacyItemAlreadyImported(candidate) && (action !== "rename" || candidate.kind === "zone"))) {
            const resolution = resolutionFor(item);
            resolution.action = action;
            if (action === "rename") resolution.targetPath = renameSuggestion(item.targetPath);
            if (item.kind === "surface") surfaceChanged = true;
        }
        elements.legacyConflictAll.value = "";
        if (surfaceChanged) await refreshLegacyPreview([...state.legacy.selectedZonePaths], usesExistingLegacySurface());
        else renderLegacyPreview();
    } catch (error) { showError(error); }
});

elements.legacyImport.addEventListener("click", async () => {
    try {
        window.clearTimeout(legacyDraftTimer);
        if (state.legacy.activeDraftPath) await persistLegacyDraft(state.legacy.activeDraftPath);
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
        showReport(translate("status.importedLegacy", { count: result.report.changed.length + result.report.created.length }));
    } catch (error) { showError(error); }
});

elements.checkAll.addEventListener("click", async () => {
    try { await validateAllConfigurations(); } catch (error) { showError(error); }
});

elements.save.addEventListener("click", async () => {
    try {
        await flushPendingDraft();
        const documentView = await validateCurrent();
        if (documentView.diagnostics.some((diagnostic) => diagnostic.severity === "error")) {
            state.bottomPanel = "problems";
            renderBottomPanel();
            writeEditorRoute({}, true);
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

function showDiscardConfirmation(title, message, actionLabel, action) {
    elements.discardDialogTitle.textContent = title;
    elements.discardDialogMessage.textContent = message;
    elements.discardConfirm.textContent = actionLabel;
    discardConfirmedAction = action;
    elements.discardDialog.showModal();
}

elements.draftDiscard.addEventListener("click", () => {
    if (state.current?.draftConflict) showDiscardConfirmation(translate("discard.title"), translate("discard.message"), translate("action.discardChanges"), discardCurrentChanges);
});

elements.discardChanges.addEventListener("click", () => showDiscardConfirmation(translate("discard.title"), translate("discard.message"), translate("action.discardChanges"), discardCurrentChanges));
elements.discardCancel.addEventListener("click", () => {
    discardConfirmedAction = null;
    elements.discardDialog.close();
});
elements.discardConfirm.addEventListener("click", async () => {
    elements.discardDialog.close();
    const action = discardConfirmedAction;
    discardConfirmedAction = null;
    try { if (action) await action(); } catch (error) { showError(error); }
});

for (const button of document.querySelectorAll(".bottom-tab")) button.addEventListener("click", () => {
    state.bottomPanel = state.bottomPanel === button.dataset.bottomTab ? "" : button.dataset.bottomTab;
    renderBottomPanel();
    if (state.task === "edit") writeEditorRoute({}, true);
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
setTaskAvailability(false, "Loading configuration files...");
const initialRoute = readEditorRoute();
showTaskHome(false);
onEditorRouteChange((route) => { void restoreEditorRoute(route); });
initialize(initialRoute);
