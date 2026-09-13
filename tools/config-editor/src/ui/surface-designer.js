const OSK_BUTTON_HEIGHT = 64 * 0.9;
const OSK_BUTTON_ASPECT = 1.4;
const OSK_BUTTON_GAP = 6 * 0.9;

function splitSource(source) {
    const lines = [];
    const pattern = /([^\r\n]*)(\r\n|\n|\r|$)/g;
    let match;
    while ((match = pattern.exec(source)) && (match[0] || match.index < source.length)) {
        lines.push({ ending: match[2], text: match[1] });
        if (!match[2]) break;
    }
    return lines;
}

function joinSource(lines) {
    return lines.map((line) => line.text + line.ending).join("");
}

function sourceEnding(source) {
    return source.match(/\r\n|\n|\r/)?.[0] || "\n";
}

function braceDelta(text) {
    let delta = 0;
    let insideQuote = false;
    let escaped = false;
    for (const character of text) {
        if (character === '"' && !escaped) insideQuote = !insideQuote;
        else if (!insideQuote && character === "{") delta++;
        else if (!insideQuote && character === "}") delta--;
        escaped = character === "\\" && !escaped;
        if (character !== "\\") escaped = false;
    }
    return delta;
}

function blockEndLine(document, startLine) {
    let depth = 0;
    for (const line of document.lines.filter((candidate) => candidate.lineNumber >= startLine)) {
        depth += braceDelta(line.text);
        if (depth <= 0) return line.lineNumber;
    }
    return undefined;
}

function replaceLine(source, lineNumber, text) {
    const lines = splitSource(source);
    if (!lines[lineNumber - 1]) return source;
    lines[lineNumber - 1].text = text;
    return joinSource(lines);
}

function removeLine(source, lineNumber) {
    const lines = splitSource(source);
    if (!lines[lineNumber - 1]) return source;
    lines.splice(lineNumber - 1, 1);
    return joinSource(lines);
}

function insertBeforeLine(source, lineNumber, text) {
    const lines = splitSource(source);
    lines.splice(Math.max(0, lineNumber - 1), 0, ...splitSource(text));
    return joinSource(lines);
}

function schemaList(value) {
    return !value || value === "None" ? [] : value.split(",");
}

function layoutSchemaRecord(schema, cellType = "widget") {
    const lineType = cellType === "widget" ? "Widget" : "Spacer";
    return schema?.records?.find((record) => record.kind === "SurfaceLine" && record.properties.SurfaceLine === lineType && record.properties.ParentBlock === "OSKRow");
}

function serializePropertyValue(propertyName, value, rules = "") {
    const rule = rules.split(",").find((candidate) => candidate.startsWith(propertyName + ":")) || "";
    if (rule.includes("QuotedString")) return JSON.stringify(value.replace(/^"|"$/g, ""));
    if (/\s/.test(value) && !/^\[.*\]$/.test(value) && !/^".*"$/.test(value)) return JSON.stringify(value);
    return value;
}

function serializeProperties(properties, propertyOrder, rules = "") {
    const names = [...propertyOrder, ...Object.keys(properties).filter((name) => !propertyOrder.includes(name))];
    return names.filter((name) => properties[name] !== undefined && properties[name] !== "").map((name) => `${name}=${serializePropertyValue(name, String(properties[name]), rules)}`).join(" ");
}

function cellLineText(current, cell, nextProperties) {
    const sourceLine = current.document.lines.find((line) => line.lineNumber === cell.line)?.text || "";
    const indentation = sourceLine.match(/^\s*/)?.[0] || "    ";
    const prefix = cell.type === "widget" ? `Widget ${cell.widgetName}` : "Spacer";
    const record = layoutSchemaRecord(current.surfaceIoSchema, cell.type);
    const propertyOrder = schemaList(record?.properties.Optional);
    const serialized = serializeProperties(nextProperties, propertyOrder, record?.properties.Rules || "");
    return indentation + prefix + (serialized ? " " + serialized : "");
}

function rowEndLine(current, row) {
    return blockEndLine(current.document, row.line);
}

function moveLines(source, sourceLines, targetLine) {
    if (sourceLines.includes(targetLine)) return source;
    const lines = splitSource(source);
    const sortedLines = [...new Set(sourceLines)].sort((left, right) => left - right);
    const moved = sortedLines.map((lineNumber) => lines[lineNumber - 1]).filter(Boolean);
    if (!moved.length) return source;
    for (const lineNumber of [...sortedLines].reverse()) lines.splice(lineNumber - 1, 1);
    const adjustedTarget = targetLine - sortedLines.filter((lineNumber) => lineNumber < targetLine).length;
    lines.splice(Math.max(0, adjustedTarget - 1), 0, ...moved);
    return joinSource(lines);
}

function groupedRowCells(row) {
    const result = [];
    const groups = new Map();
    for (const cell of row.cells) {
        const group = cell.type === "widget" ? (cell.properties.Group || "").toLowerCase() : "";
        const existing = group ? groups.get(group) : undefined;
        if (existing) existing.push(cell);
        else {
            const cells = [cell];
            result.push(cells);
            if (group) groups.set(group, cells);
        }
    }
    return result;
}

function groupedCellsForLayoutCell(layout, cell) {
    const row = layout?.rows.find((candidate) => candidate.cells.some((rowCell) => rowCell.line === cell?.line));
    const group = (cell?.properties.Group || "").toLowerCase();
    return row && group ? row.cells.filter((candidate) => (candidate.properties.Group || "").toLowerCase() === group) : cell ? [cell] : [];
}

function widgetInputNames(widget) {
    return (widget?.body || []).map((line) => (line.tokens[0] || "").toLowerCase() === "input" ? (line.tokens[1] || "").toLowerCase() : (line.tokens[0] || "").toLowerCase());
}

function widgetSupportsInput(widget, inputName) {
    const names = widgetInputNames(widget);
    if (inputName === "Press") return names.some((name) => name === "press" || name === "anypress");
    if (inputName === "Encoder") return names.some((name) => name === "relative" || name.includes("encoder"));
    if (inputName === "Value") return names.some((name) => name === "value" || name === "absolute" || name.includes("fader"));
    if (inputName === "Touch") return names.some((name) => name === "touch");
    return false;
}

function layoutCellRole(document, cell) {
    if (cell.type === "spacer") return "spacer";
    const shapeName = (cell.properties.Shape || "").toLowerCase();
    const explicitRole = (cell.properties.Role || "").toLowerCase();
    if (explicitRole === "fader" || shapeName === "fader") return "fader";
    if (explicitRole === "rotary") return "rotary";
    const widget = document.semantic.widgets.find((candidate) => candidate.name.toLowerCase() === (cell.widgetName || "").toLowerCase());
    const inputNames = widgetInputNames(widget);
    if (inputNames.some((name) => name.includes("fader") || name === "value" || name === "absolute")) return "fader";
    if (inputNames.some((name) => name.includes("encoder") || name === "relative")) return "rotary";
    const fallbackName = (cell.widgetName || "").toLowerCase();
    const fallbackGroup = (cell.properties.Group || "").toLowerCase();
    const buttonLike = fallbackName.includes("push") || fallbackName.includes("press") || fallbackName.includes("touch") || fallbackName.includes("button");
    if (!buttonLike && (fallbackName.includes("rotary") || fallbackName.includes("encoder") || fallbackGroup.includes("rotary") || fallbackGroup.includes("encoder"))) return "rotary";
    return shapeName === "round" ? "round" : "button";
}

function layoutCellMetrics(document, cell) {
    const role = layoutCellRole(document, cell);
    const widthFactor = Math.max(0.1, Number(cell.properties.Width) || (cell.type === "spacer" ? 0.5 : 1));
    let heightFactor = Math.max(0.1, Number(cell.properties.Height) || 1);
    let rowSpan = Math.max(1, Math.round(Number(cell.properties.RowSpan) || 1));
    if (role === "fader") {
        rowSpan = Math.max(1, Math.round(Number(cell.properties.Height) || 1));
        heightFactor = 1;
    }
    return { heightFactor, role, rowSpan, widthFactor };
}

function fieldChoices(propertyName, rules) {
    const escapedName = escapePattern(propertyName);
    const enumMatch = rules.match(new RegExp(`(?:^|,)${escapedName}:Enum\\(([^)]+)\\)`));
    if (enumMatch) return enumMatch[1].split("|");
    return rules.match(new RegExp(`(?:^|,)${escapedName}:Boolean(?:,|$)`)) ? ["true", "false"] : [];
}

function createField(label, value, disabled, onChange, rules = "", choiceOverride, defaultLabel = "") {
    const field = document.createElement("label");
    field.className = "surface-inspector-field";
    const title = document.createElement("span");
    title.textContent = label;
    const choices = choiceOverride ?? fieldChoices(label, rules);
    const usesChoices = choiceOverride !== undefined || choices.length > 0;
    const control = document.createElement(usesChoices ? "select" : "input");
    if (usesChoices) {
        const empty = document.createElement("option");
        empty.value = "";
        empty.textContent = defaultLabel;
        control.append(empty);
        for (const choice of choices) {
            const option = document.createElement("option");
            option.value = choice;
            option.textContent = choice;
            control.append(option);
        }
        if (value && !choices.includes(value)) {
            const unsupported = document.createElement("option");
            unsupported.value = value;
            unsupported.textContent = value;
            control.append(unsupported);
        }
    } else {
        const rule = rules.split(",").find((candidate) => candidate.startsWith(label + ":")) || "";
        if (rule.includes("Finite")) {
            control.type = "number";
            control.step = "any";
            if (rule.includes("PositiveFinite")) control.min = "0.01";
        }
    }
    control.value = value ?? "";
    control.disabled = disabled;
    control.addEventListener("change", () => onChange(control.value.trim()));
    field.append(title, control);
    return field;
}

function escapePattern(value) {
    return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function editorPropertyMap(record, metadataName) {
    const result = {};
    for (const entry of (record?.properties[metadataName] || "").split(";")) {
        const separatorIdx = entry.indexOf(":");
        if (separatorIdx > 0) result[entry.slice(0, separatorIdx)] = entry.slice(separatorIdx + 1);
    }
    return result;
}

export function createSurfaceDesigner(elements, translate, callbacks) {
    let current;
    let selectedPalette = "";
    let selectedCellLine = 0;

    const canEdit = () => Boolean(current?.writable && !current.document.diagnostics.some((diagnostic) => diagnostic.severity === "error"));

    const applySource = async (source, preserveSelection = false) => {
        if (!canEdit() || source === current.source) return;
        if (!preserveSelection) selectedCellLine = 0;
        await callbacks.applySource(source);
    };

    const selectWidget = (widgetName, cellLine = 0) => {
        selectedPalette = widgetName ? `widget:${widgetName}` : "";
        selectedCellLine = cellLine;
        render();
    };

    const renderInspector = () => {
        elements.inspector.replaceChildren();
        elements.inspector.className = "surface-inspector";
        const cell = current?.document.semantic.oskLayout?.rows.flatMap((row) => row.cells).find((candidate) => candidate.line === selectedCellLine);
        if (!cell) {
            elements.inspector.className = "surface-inspector secondary";
            elements.inspector.textContent = translate("surfaceDesigner.select");
            return;
        }
        if (cell) {
            const section = document.createElement("section");
            section.className = "surface-inspector-section";
            const heading = document.createElement("strong");
            heading.textContent = cell.type === "widget" ? cell.widgetName : translate("surfaceDesigner.spacer");
            section.append(heading);
            const groupedCells = groupedCellsForLayoutCell(current.document.semantic.oskLayout, cell);
            if (groupedCells.length > 1) {
                const groupMembers = document.createElement("div");
                groupMembers.className = "surface-group-members";
                const groupLabel = document.createElement("span");
                groupLabel.textContent = translate("surfaceDesigner.groupedControl");
                groupMembers.append(groupLabel);
                for (const groupedCell of groupedCells) {
                    const memberButton = document.createElement("button");
                    memberButton.classList.toggle("selected", groupedCell.line === cell.line);
                    memberButton.textContent = groupedCell.widgetName;
                    memberButton.addEventListener("click", () => selectWidget(groupedCell.widgetName || "", groupedCell.line));
                    groupMembers.append(memberButton);
                }
                section.append(groupMembers);
            }
            const record = layoutSchemaRecord(current.surfaceIoSchema, cell.type);
            const editorChoices = editorPropertyMap(record, "EditorChoices");
            const editorApplies = editorPropertyMap(record, "EditorApplies");
            const editorTargets = editorPropertyMap(record, "EditorTargets");
            const role = layoutCellRole(current.document, cell);
            const propertyNames = schemaList(record?.properties.Optional).filter((propertyName) => {
                if (cell.properties[propertyName]) return true;
                const roles = editorApplies[propertyName]?.split("|") || [];
                return !roles.length || roles.includes("all") || roles.includes(role);
            });
            for (const propertyName of propertyNames) {
                const targetInput = editorTargets[propertyName];
                const choices = targetInput ? current.document.semantic.widgets.filter((widget) => widgetSupportsInput(widget, targetInput)).map((widget) => widget.name) : editorChoices[propertyName]?.split("|");
                section.append(createField(propertyName, cell.properties[propertyName] || "", !canEdit(), async (value) => {
                    const nextProperties = { ...cell.properties };
                    if (value) nextProperties[propertyName] = value;
                    else delete nextProperties[propertyName];
                    await applySource(replaceLine(current.source, cell.line, cellLineText(current, cell, nextProperties)), true);
                }, record?.properties.Rules || "", choices, translate("surfaceDesigner.defaultValue")));
            }
            const remove = document.createElement("button");
            remove.textContent = translate("surfaceDesigner.remove");
            remove.disabled = !canEdit();
            remove.addEventListener("click", () => { void applySource(removeLine(current.source, cell.line)); });
            section.append(remove);
            elements.inspector.append(section);
        }
    };

    const dropOnRow = (event, row) => {
        event.preventDefault();
        event.currentTarget.classList.remove("drag-target");
        if (!canEdit()) return;
        const targetLine = rowEndLine(current, row);
        if (!targetLine) return;
        const sourceLine = Number(event.dataTransfer.getData("application/x-surface-cell-line"));
        if (sourceLine) {
            const sourceCell = current.document.semantic.oskLayout?.rows.flatMap((candidate) => candidate.cells).find((candidate) => candidate.line === sourceLine);
            const sourceLines = groupedCellsForLayoutCell(current.document.semantic.oskLayout, sourceCell).map((candidate) => candidate.line);
            void applySource(moveLines(current.source, sourceLines, targetLine));
            return;
        }
        const widgetName = event.dataTransfer.getData("application/x-surface-widget");
        const cellType = event.dataTransfer.getData("application/x-surface-cell-type");
        if (!widgetName && cellType !== "spacer") return;
        const rowLine = current.document.lines.find((line) => line.lineNumber === row.line)?.text || "  Row {";
        const indentation = (rowLine.match(/^\s*/)?.[0] || "  ") + "  ";
        const text = indentation + (cellType === "spacer" ? "Spacer" : `Widget ${widgetName}`) + sourceEnding(current.source);
        void applySource(insertBeforeLine(current.source, targetLine, text));
    };

    const moveCellByKeyboard = (event, rowIdx, cellIdx, groupedCells) => {
        if (!event.altKey || !canEdit()) return;
        const rows = current.document.semantic.oskLayout?.rows || [];
        const visibleRows = rows.map(groupedRowCells);
        let targetLine;
        if (event.key === "ArrowLeft" && cellIdx > 0) targetLine = visibleRows[rowIdx][cellIdx - 1][0].line;
        if (event.key === "ArrowRight" && cellIdx + 1 < visibleRows[rowIdx].length) targetLine = Math.max(...visibleRows[rowIdx][cellIdx + 1].map((cell) => cell.line)) + 1;
        if (event.key === "ArrowUp" && rowIdx > 0) targetLine = rowEndLine(current, rows[rowIdx - 1]);
        if (event.key === "ArrowDown" && rowIdx + 1 < rows.length) targetLine = rowEndLine(current, rows[rowIdx + 1]);
        if (!targetLine) return;
        event.preventDefault();
        void applySource(moveLines(current.source, groupedCells.map((cell) => cell.line), targetLine));
    };

    const renderLayout = () => {
        elements.layout.replaceChildren();
        const layout = current?.document.semantic.oskLayout;
        if (!layout?.rows.length) {
            elements.layout.className = "surface-layout secondary";
            elements.layout.textContent = translate("surfaceDesigner.emptyLayout");
            return;
        }
        elements.layout.className = "surface-layout";
        const activeRowSpans = new Map();
        layout.rows.forEach((row, rowIdx) => {
            const visibleCells = groupedRowCells(row);
            const rowHeight = visibleCells.reduce((maximumHeight, groupedCells) => {
                const cell = groupedCells[0];
                if (cell.type === "spacer") return maximumHeight;
                const metrics = layoutCellMetrics(current.document, cell);
                const topPadding = Math.max(0, OSK_BUTTON_HEIGHT * (Number(cell.properties.Top) || 0));
                return Math.max(maximumHeight, OSK_BUTTON_HEIGHT * metrics.heightFactor + topPadding);
            }, OSK_BUTTON_HEIGHT);
            const renderItems = [];
            let columnNumber = 1;
            for (const [visibleCellIdx, groupedCells] of visibleCells.entries()) {
                while (activeRowSpans.get(columnNumber)?.remaining > 0) {
                    renderItems.push({ occupied: activeRowSpans.get(columnNumber) });
                    columnNumber++;
                }
                const cell = groupedCells[0];
                const metrics = layoutCellMetrics(current.document, cell);
                const width = OSK_BUTTON_HEIGHT * OSK_BUTTON_ASPECT * metrics.widthFactor;
                renderItems.push({ cell, groupedCells, metrics, visibleCellIdx, width });
                if (metrics.rowSpan > 1) activeRowSpans.set(columnNumber, { newlyAdded: true, remaining: metrics.rowSpan - 1, width });
                columnNumber++;
            }
            const lastOccupiedColumn = Math.max(0, ...[...activeRowSpans.entries()].filter(([, span]) => span.remaining > 0).map(([occupiedColumn]) => occupiedColumn));
            while (columnNumber <= lastOccupiedColumn) {
                renderItems.push({ occupied: activeRowSpans.get(columnNumber) || { width: OSK_BUTTON_HEIGHT * OSK_BUTTON_ASPECT } });
                columnNumber++;
            }
            const rowElement = document.createElement("div");
            rowElement.className = "surface-layout-row";
            rowElement.style.height = `${rowHeight}px`;
            rowElement.addEventListener("dragover", (event) => { event.preventDefault(); rowElement.classList.add("drag-target"); });
            rowElement.addEventListener("dragleave", () => rowElement.classList.remove("drag-target"));
            rowElement.addEventListener("drop", (event) => dropOnRow(event, row));
            renderItems.forEach((item) => {
                const slotElement = document.createElement("div");
                slotElement.className = "surface-cell-slot" + (item.occupied ? " occupied" : "");
                slotElement.style.flex = `0 0 ${item.occupied?.width || item.width}px`;
                slotElement.style.width = `${item.occupied?.width || item.width}px`;
                slotElement.style.height = `${rowHeight}px`;
                if (item.occupied) {
                    rowElement.append(slotElement);
                    return;
                }
                const { cell, groupedCells, metrics, visibleCellIdx } = item;
                const shapeName = (cell.properties.Shape || "").toLowerCase();
                const topPadding = Math.max(0, OSK_BUTTON_HEIGHT * (Number(cell.properties.Top) || 0));
                const cellHeight = metrics.role === "fader" && metrics.rowSpan > 1 ? OSK_BUTTON_HEIGHT * metrics.rowSpan + OSK_BUTTON_GAP * (metrics.rowSpan - 1) : OSK_BUTTON_HEIGHT * metrics.heightFactor;
                const roundVisualWidth = OSK_BUTTON_HEIGHT * metrics.widthFactor;
                const cellElement = document.createElement("button");
                cellElement.className = `surface-cell ${metrics.role}` + (shapeName ? ` ${shapeName}` : "") + (groupedCells.length > 1 ? " grouped" : "") + (groupedCells.some((candidate) => candidate.line === selectedCellLine) ? " selected" : "");
                cellElement.draggable = canEdit();
                cellElement.style.height = `${cellHeight}px`;
                cellElement.style.top = `${topPadding}px`;
                cellElement.style.setProperty("--surface-round-width", `${roundVisualWidth}px`);
                cellElement.style.setProperty("--surface-rotary-size", `${Math.max(12, Math.min(item.width, cellHeight) - 8)}px`);
                if (/^#[0-9a-f]{6}$/i.test(cell.properties.Color || "")) cellElement.style.setProperty("--surface-cell-color", cell.properties.Color);
                const groupNames = groupedCells.filter((candidate) => candidate.type === "widget").map((candidate) => candidate.widgetName).join(" + ");
                cellElement.title = translate("surfaceDesigner.keyboardMove") + (groupedCells.length > 1 ? `. ${translate("surfaceDesigner.groupedNames", { names: groupNames })}` : "");
                const face = document.createElement("span");
                face.className = "surface-cell-face";
                const label = document.createElement("span");
                label.className = "surface-cell-label";
                label.textContent = cell.type === "widget" ? cell.properties.Label?.replace(/^"|"$/g, "") || cell.widgetName : translate("surfaceDesigner.spacer");
                cellElement.append(face, label);
                cellElement.addEventListener("click", () => selectWidget(cell.widgetName || "", cell.line));
                cellElement.addEventListener("keydown", (event) => moveCellByKeyboard(event, rowIdx, visibleCellIdx, groupedCells));
                cellElement.addEventListener("dragstart", (event) => event.dataTransfer.setData("application/x-surface-cell-line", String(cell.line)));
                cellElement.addEventListener("dragover", (event) => event.preventDefault());
                cellElement.addEventListener("drop", (event) => {
                    event.preventDefault();
                    event.stopPropagation();
                    const sourceLine = Number(event.dataTransfer.getData("application/x-surface-cell-line"));
                    if (sourceLine && sourceLine !== cell.line) {
                        const sourceCell = layout.rows.flatMap((candidate) => candidate.cells).find((candidate) => candidate.line === sourceLine);
                        const sourceLines = groupedCellsForLayoutCell(layout, sourceCell).map((candidate) => candidate.line);
                        void applySource(moveLines(current.source, sourceLines, cell.line));
                        return;
                    }
                    const widgetName = event.dataTransfer.getData("application/x-surface-widget");
                    const cellType = event.dataTransfer.getData("application/x-surface-cell-type");
                    if (!widgetName && cellType !== "spacer") return;
                    const targetText = current.document.lines.find((line) => line.lineNumber === cell.line)?.text || "";
                    const indentation = targetText.match(/^\s*/)?.[0] || "    ";
                    const text = indentation + (cellType === "spacer" ? "Spacer" : `Widget ${widgetName}`) + sourceEnding(current.source);
                    void applySource(insertBeforeLine(current.source, cell.line, text));
                });
                slotElement.append(cellElement);
                rowElement.append(slotElement);
            });
            elements.layout.append(rowElement);
            for (const [occupiedColumn, span] of activeRowSpans) {
                if (span.newlyAdded) span.newlyAdded = false;
                else if (--span.remaining <= 0) activeRowSpans.delete(occupiedColumn);
            }
        });
    };

    const renderWidgets = () => {
        elements.widgetList.replaceChildren();
        const spacer = document.createElement("button");
        spacer.className = "surface-widget-button";
        spacer.textContent = translate("surfaceDesigner.spacer");
        spacer.classList.toggle("selected", selectedPalette === "spacer");
        spacer.draggable = canEdit();
        spacer.addEventListener("click", () => { selectedCellLine = 0; selectedPalette = "spacer"; render(); });
        spacer.addEventListener("dragstart", (event) => event.dataTransfer.setData("application/x-surface-cell-type", "spacer"));
        elements.widgetList.append(spacer);
        const usedWidgetNames = new Set((current?.document.semantic.oskLayout?.rows || []).flatMap((row) => row.cells).filter((cell) => cell.type === "widget").map((cell) => cell.widgetName?.toLowerCase()));
        for (const widget of current?.document.semantic.widgets || []) {
            if (usedWidgetNames.has(widget.name.toLowerCase())) continue;
            const button = document.createElement("button");
            button.className = "surface-widget-button" + (selectedPalette === `widget:${widget.name}` && !selectedCellLine ? " selected" : "");
            button.textContent = widget.name;
            button.draggable = canEdit();
            button.addEventListener("click", () => selectWidget(widget.name));
            button.addEventListener("dragstart", (event) => event.dataTransfer.setData("application/x-surface-widget", widget.name));
            elements.widgetList.append(button);
        }
    };

    const renderStatus = () => {
        const message = !current?.writable ? translate("surfaceDesigner.readOnly") : current.document.diagnostics.some((diagnostic) => diagnostic.severity === "error") ? translate("surfaceDesigner.invalid") : "";
        elements.status.textContent = message;
        elements.status.className = message ? "surface-designer-blocked" : "feedback info";
        elements.status.hidden = !message;
        elements.addRow.disabled = !canEdit();
    };

    const render = () => {
        if (!current) return;
        renderStatus();
        renderWidgets();
        renderLayout();
        renderInspector();
    };

    elements.addRow.addEventListener("click", () => {
        if (!canEdit()) return;
        const layout = current.document.semantic.oskLayout;
        const ending = sourceEnding(current.source);
        const itemText = selectedPalette === "spacer" || !selectedPalette ? "Spacer" : `Widget ${selectedPalette.slice("widget:".length)}`;
        if (!layout) {
            const separator = current.source && !/[\r\n]$/.test(current.source) ? ending : "";
            void applySource(current.source + separator + `OSKLayout {${ending}  Row {${ending}    ${itemText}${ending}  }${ending}}${ending}`);
            return;
        }
        const endLine = blockEndLine(current.document, layout.line);
        if (endLine) void applySource(insertBeforeLine(current.source, endLine, `  Row {${ending}    ${itemText}${ending}  }${ending}`));
    });
    elements.undo.addEventListener("click", callbacks.undo);
    elements.redo.addEventListener("click", callbacks.redo);

    return {
        render(nextCurrent, surfaceIoSchema) {
            current = nextCurrent ? { ...nextCurrent, surfaceIoSchema } : undefined;
            if (!current) {
                selectedCellLine = 0;
                selectedPalette = "";
                elements.widgetList.replaceChildren();
                elements.layout.replaceChildren();
                elements.status.className = "feedback info";
                elements.status.hidden = false;
                elements.status.textContent = translate("surfaceDesigner.chooseSurface");
                elements.addRow.disabled = true;
                elements.inspector.textContent = translate("surfaceDesigner.select");
                return;
            }
            render();
        },
    };
}
