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
    legacyWidgetMappings: requiredElement("legacy-widget-mappings"),
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
const state = { batch: new Map(), current: null, legacy: { preview: null, resolutions: new Map(), selectedZonePaths: new Set(), widgetMappings: new Map() }, tab: "raw" };

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
    updateLegacyImportButton();
}

async function refreshLegacyPreview(selectedZonePaths, useExistingSurface = usesExistingLegacySurface()) {
    if (!elements.legacySurface.value) return;
    const body = { includeSurface: elements.legacyIncludeSurface.checked, surfaceName: elements.legacySurface.value, useExistingSurface, widgetMappings: widgetMappingsForRequest() };
    if (selectedZonePaths !== undefined) body.selectedZonePaths = selectedZonePaths;
    const result = await api("/api/legacy/preview", { method: "POST", body: JSON.stringify(body) });
    state.legacy.preview = result.preview;
    state.legacy.selectedZonePaths = new Set(result.preview.selectedZonePaths);
    state.legacy.widgetMappings = new Map(result.preview.widgetMappings.filter((issue) => issue.selectedTarget).map((issue) => [issue.sourceWidget, issue.selectedTarget]));
    elements.legacySelectAll.disabled = false;
    elements.legacySelectNone.disabled = false;
    elements.legacyRefresh.disabled = false;
    renderLegacyPreview();
}

async function initialize() {
    try {
        const translationsResponse = await fetch("/app-translations.json");
        if (!translationsResponse.ok) throw new Error(translationsResponse.statusText);
        translations = await translationsResponse.json();
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
        state.legacy.widgetMappings.clear();
        elements.legacySurface.replaceChildren();
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = translate("legacy.surface.placeholder");
        elements.legacySurface.append(placeholder);
        for (const surface of result.surfaces) {
            const option = document.createElement("option");
            option.value = surface.name;
            option.textContent = translate("legacy.surface.option", { count: surface.zoneCount, fxCount: surface.fxZoneCount, name: surface.name });
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
        state.legacy.widgetMappings.clear();
        if (elements.legacySurface.value) await refreshLegacyPreview();
        else {
            state.legacy.preview = null;
            renderLegacyPreview();
        }
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
                includeSurface: elements.legacyIncludeSurface.checked,
                resolutions,
                selectedZonePaths: [...state.legacy.selectedZonePaths],
                surfaceName: elements.legacySurface.value,
                widgetMappings: widgetMappingsForRequest(),
            }),
        });
        await refreshTree();
        await refreshLegacyPreview([...state.legacy.selectedZonePaths]);
        showReport({ message: translate("status.importedLegacy", { count: result.report.changed.length + result.report.created.length }), ...result.report });
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
