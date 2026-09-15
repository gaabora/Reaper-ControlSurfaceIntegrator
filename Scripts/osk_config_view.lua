local imgui = require "imgui" "0.9.3"

local osk_color_picker = require("osk_color_picker")
local osd_templates = require("osd_templates")
local theme = require("theme_settings")
local ui = require("ui_components")

local M = {}

local function selectBinding(state, bindingIndex)
    state.selectedBinding = bindingIndex
end

local function getLayoutColor(state, deps)
    if not deps.data or not deps.data.GetCellInfo then return nil end
    local cell = deps.data.GetCellInfo(state.surfaceName, state.widgetName)
    return cell and cell.color or nil
end

local function shouldBoostInactiveTableSwatch(state, colors, inactiveColor, layoutColor, deps)
    if layoutColor then return false end
    if not colors or not colors[1] then return false end
    if not theme.IsMeaningfulColor(inactiveColor) then return false end
    return deps.data and deps.data.IsButtonWidget and deps.data.IsButtonWidget(state.surfaceName, state.widgetName)
end

local function renderBindingColors(ctx, state, binding, bindingIndex, deps)
    local parts = deps.action_line.Parse(binding.line)
    local colors = deps.action_line.ParseColors(parts)
    local layoutColor = getLayoutColor(state, deps)
    local inactiveColor = colors and colors[1] or layoutColor or theme.CONFIG.default_inactive_color
    local activeColor = colors and (colors[2] or colors[1]) or layoutColor or theme.CONFIG.default_active_color
    local inactiveDisplayColor = layoutColor and theme.AdjustColorValue(layoutColor, -50) or inactiveColor
    local activeDisplayColor = layoutColor or activeColor
    local pickerOptions = nil
    if layoutColor then
        pickerOptions = { readOnly = true, tooltip = "Fixed surface color from OSK layout" }
    elseif shouldBoostInactiveTableSwatch(state, colors, inactiveColor, layoutColor, deps) then
        inactiveDisplayColor = theme.ApplyInactiveLedBoost(inactiveColor, theme.osk.inactive_led_boost)
    end

    inactiveColor = osk_color_picker.RenderBindingColorPicker(ctx, state, binding, bindingIndex, 1, "Inactive", inactiveColor, deps, inactiveDisplayColor, pickerOptions)
    imgui.SameLine(ctx, 0, 3)
    osk_color_picker.RenderBindingColorPicker(ctx, state, binding, bindingIndex, 2, "Active", activeColor, deps, activeDisplayColor, pickerOptions)
end

local function renderBindingTable(ctx, state, deps)
    if not imgui.BeginTable(ctx, "##bindings_table", 3, deps.tableFlags, -1, theme.CONFIG.bindings_table_height, 0) then return end

    imgui.TableSetupColumn(ctx, "Modifier", imgui.TableColumnFlags_WidthFixed, 95, 0)
    imgui.TableSetupColumn(ctx, "Action", imgui.TableColumnFlags_WidthStretch, 0.52, 1)
    imgui.TableSetupColumn(ctx, "Colors", imgui.TableColumnFlags_WidthFixed, theme.CONFIG.color_column_width, 2)
    imgui.TableHeadersRow(ctx)

    for bindingIndex, binding in ipairs(state.bindings) do
        local selected = bindingIndex == state.selectedBinding
        imgui.TableNextRow(ctx, imgui.TableRowFlags_None, 0)

        imgui.TableSetColumnIndex(ctx, 0)
        if imgui.Selectable(ctx, deps.model.GetModifierLabel(binding, deps.modifierFlags) .. "##modifier_" .. bindingIndex, selected) then
            selectBinding(state, bindingIndex)
        end

        imgui.TableSetColumnIndex(ctx, 1)
        if imgui.Selectable(ctx, deps.model.GetBindingTitle(binding, deps.action_line, deps.data, deps.reaper) .. "##action_" .. bindingIndex, selected) then
            selectBinding(state, bindingIndex)
        end
        ui.ItemTooltip(ctx, binding.line)

        imgui.TableSetColumnIndex(ctx, 2)
        renderBindingColors(ctx, state, binding, bindingIndex, deps)
    end

    imgui.EndTable(ctx)
end

local function saveParts(state, binding, parts, deps)
    binding.line = deps.action_line.Build(parts)
    deps.model.RefreshBindingDerivedFields(binding, deps.action_line)
    deps.model.UpdateDirtyState(state)
end

local function field(ctx, label, extraWidth)
    imgui.AlignTextToFramePadding(ctx)
    imgui.Text(ctx, label)
    imgui.SameLine(ctx, theme.CONFIG.form_label_width)
    imgui.SetNextItemWidth(ctx, -(extraWidth or 1))
end

local function softButton(ctx, label, width, height)
    return ui.ColorButton(ctx, label, imgui.GetStyleColor(ctx, imgui.Col_TableHeaderBg), width, height)
end

local function renderActionPicker(ctx, state, binding, deps)
    field(ctx, "Action")
    imgui.SetNextWindowSizeConstraints(ctx, 340, 0, 800, 360)
    if not imgui.BeginCombo(ctx, "##action_picker", deps.model.GetActionTitle(binding, deps.action_line, deps.reaper)) then return end
    if imgui.IsWindowAppearing(ctx) then
        state.searchQuery = ""
        state.searchSelected = 1
        deps.model.RefreshSearchResults(state, deps.reaper)
        state.focusActionSearch = true
    end
    imgui.SetNextItemWidth(ctx, theme.CONFIG.action_search_mode_width)
    local modeChanged
    modeChanged, state.searchModeIndex = imgui.Combo(ctx, "##action_source", state.searchModeIndex, deps.model.SEARCH_MODE_ITEMS)
    if modeChanged then deps.model.SyncSearchModeFromIndex(state) end
    imgui.SameLine(ctx)
    imgui.SetNextItemWidth(ctx, -1)
    if state.focusActionSearch then imgui.SetKeyboardFocusHere(ctx); state.focusActionSearch = false end
    local queryChanged
    queryChanged, state.searchQuery = imgui.InputTextWithHint(ctx, "##action_search", "Find action or ID...", state.searchQuery)
    local searchFocused = imgui.IsItemFocused(ctx)
    if modeChanged or queryChanged then
        deps.model.RefreshSearchResults(state, deps.reaper)
        state.searchSelected = 1
    end
    if searchFocused then
        if imgui.IsKeyPressed(ctx, imgui.Key_DownArrow) then state.searchSelected = math.min(#state.searchResults, state.searchSelected + 1) end
        if imgui.IsKeyPressed(ctx, imgui.Key_UpArrow) then state.searchSelected = math.max(1, state.searchSelected - 1) end
    end
    local chosenRow = searchFocused and imgui.IsKeyPressed(ctx, imgui.Key_Enter) and state.searchResults[state.searchSelected] or nil
    if imgui.BeginListBox(ctx, "##action_results", -1, theme.CONFIG.action_search_results_height * 2) then
        for idx, row in ipairs(state.searchResults) do
            if imgui.Selectable(ctx, row, idx == state.searchSelected) then chosenRow = row end
            if idx == state.searchSelected and searchFocused and (imgui.IsKeyPressed(ctx, imgui.Key_DownArrow) or imgui.IsKeyPressed(ctx, imgui.Key_UpArrow)) then imgui.SetScrollHereY(ctx) end
        end
        if #state.searchResults == 0 then imgui.TextDisabled(ctx, "No matching actions") end
        imgui.EndListBox(ctx)
    end
    if chosenRow then
        deps.model.ApplySearchSelectionToBinding(state, binding, chosenRow, deps.action_line)
        imgui.CloseCurrentPopup(ctx)
    end
    imgui.EndCombo(ctx)
end

function M.RenderToolbar(ctx, state, deps)
    local availableWidth = imgui.GetContentRegionAvail(ctx)
    local startX, startY = imgui.GetCursorPos(ctx)
    local paddingX = imgui.GetStyleVar(ctx, imgui.StyleVar_FramePadding)
    local spacingX = imgui.GetStyleVar(ctx, imgui.StyleVar_ItemSpacing)
    local buttonHeight = imgui.GetFrameHeight(ctx)
    local leftWidth = imgui.CalcTextSize(ctx, "Save") + imgui.CalcTextSize(ctx, "Apply") + imgui.CalcTextSize(ctx, "Revert") + paddingX * 6 + spacingX * 2
    local rightWidth = buttonHeight * 4 + spacingX * 3
    local sideWidth = math.max(leftWidth, rightWidth)
    if deps.headingFont then imgui.PushFont(ctx, deps.headingFont) end
    local titleWidth, titleHeight = imgui.CalcTextSize(ctx, state.widgetName)
    if deps.headingFont then imgui.PopFont(ctx) end
    local separateTitle = availableWidth < sideWidth * 2 + titleWidth + spacingX * 2
    if separateTitle then
        if deps.headingFont then imgui.PushFont(ctx, deps.headingFont) end
        imgui.SetCursorPosX(ctx, startX + math.max(0, (availableWidth - titleWidth) / 2))
        imgui.Text(ctx, state.widgetName)
        if deps.headingFont then imgui.PopFont(ctx) end
        startX, startY = imgui.GetCursorPos(ctx)
    end
    local operationReady = state.pendingOperation == nil and not state.previewApplyPending
    ui.Disabled(ctx, not operationReady or not state.isDirty, function()
        if ui.ColorButton(ctx, "Save", theme.HexToImCol(theme.osk.primary_button_color)) then
            if state.hasUnappliedEdits then
                state.saveAfterApply = true
                if not deps.protocol.SendApplyLive(state, deps.data, deps.model) then state.saveAfterApply = false end
            else
                deps.protocol.SendSave(state, deps.data, deps.model)
            end
        end
    end)
    imgui.SameLine(ctx)
    ui.Disabled(ctx, not operationReady or not state.hasUnappliedEdits, function()
        if softButton(ctx, "Apply") then
            state.saveAfterApply = false
            deps.protocol.SendApplyLive(state, deps.data, deps.model)
        end
    end)
    imgui.SameLine(ctx)
    ui.Disabled(ctx, not operationReady or not state.isDirty, function()
        if ui.ColorButton(ctx, "Revert", theme.DIRTY_BUTTON_COLORS.button) then deps.protocol.RequestRevert(state, deps.data) end
    end)
    if not separateTitle then
        imgui.SetCursorPos(ctx, startX + (availableWidth - titleWidth) / 2, startY + math.max(0, (buttonHeight - titleHeight) / 2))
        if deps.headingFont then imgui.PushFont(ctx, deps.headingFont) end
        imgui.Text(ctx, state.widgetName)
        if deps.headingFont then imgui.PopFont(ctx) end
    end
    local toolsY = startY
    if availableWidth < leftWidth + rightWidth + spacingX then toolsY = toolsY + buttonHeight + spacingX end
    imgui.SetCursorPos(ctx, startX + math.max(0, availableWidth - rightWidth), toolsY)
    if softButton(ctx, "+", buttonHeight, buttonHeight) then deps.model.AddBinding(state) end
    ui.ItemTooltip(ctx, "Add binding")
    imgui.SameLine(ctx)
    ui.Disabled(ctx, #state.bindings == 0, function()
        if softButton(ctx, "-", buttonHeight, buttonHeight) then deps.model.RemoveSelectedBinding(state) end
    end)
    ui.ItemTooltip(ctx, "Remove selected binding")
    imgui.PushStyleColor(ctx, imgui.Col_Button, imgui.GetStyleColor(ctx, imgui.Col_TableHeaderBg))
    imgui.SameLine(ctx)
    ui.Disabled(ctx, state.selectedBinding <= 1, function()
        if imgui.ArrowButton(ctx, "##move_binding_up", imgui.Dir_Up) then deps.model.MoveSelectedBinding(state, -1) end
    end)
    ui.ItemTooltip(ctx, "Move binding up")
    imgui.SameLine(ctx)
    ui.Disabled(ctx, state.selectedBinding >= #state.bindings, function()
        if imgui.ArrowButton(ctx, "##move_binding_down", imgui.Dir_Down) then deps.model.MoveSelectedBinding(state, 1) end
    end)
    ui.ItemTooltip(ctx, "Move binding down")
    imgui.PopStyleColor(ctx)
    imgui.SetCursorPos(ctx, startX, toolsY + math.max(buttonHeight, titleHeight) + spacingX)
    if state.status:match("^ERR%s*|") or state.status:match("^WARN%s*|") then imgui.TextWrapped(ctx, state.status) end
end

local function modifierColor(state, modifier, deps)
    local cell = deps.data.GetCellInfo(state.surfaceName, modifier.name)
    if cell and cell.color then return theme.AdjustColorValue(cell.color, -50) end
    local widgetState = deps.data.states[state.surfaceName] and deps.data.states[state.surfaceName][modifier.name]
    if widgetState and theme.IsMeaningfulColor(widgetState.color) and widgetState.value == 0 then
        return theme.ApplyInactiveLedBoost(widgetState.color, theme.osk.inactive_led_boost)
    end
    return theme.CONFIG.default_inactive_color
end

local function renderConditions(ctx, state, binding, deps)
    local parts = deps.action_line.Parse(binding.line)
    local colors = deps.action_line.ParseColors(parts)
    local activeColor = getLayoutColor(state, deps) or colors and (colors[2] or colors[1]) or theme.CONFIG.default_active_color
    local items = {}
    local spacingX = imgui.GetStyleVar(ctx, imgui.StyleVar_ItemSpacing)
    local availableWidth = imgui.GetContentRegionAvail(ctx)
    local plusWidth = imgui.CalcTextSize(ctx, "+")
    for _, modifier in ipairs(deps.modifierFlags) do
        if ((binding.mod or 0) & modifier.bit) ~= 0 then
            items[#items + 1] = { label = modifier.name, modifier = modifier, color = modifierColor(state, modifier, deps) }
        end
    end
    items[#items + 1] = { label = state.widgetName, color = activeColor }
    local rows = { { items = {}, width = 0 } }
    for _, item in ipairs(items) do
        item.width = math.min(availableWidth, math.max(theme.CONFIG.preview_button_width, imgui.CalcTextSize(ctx, item.label) + 24))
        local row = rows[#rows]
        local addition = #row.items > 0 and plusWidth + spacingX * 2 or 0
        if #row.items > 0 and row.width + addition + item.width > availableWidth then
            row = { items = {}, width = 0 }
            rows[#rows + 1] = row
            addition = 0
        end
        row.items[#row.items + 1] = item
        row.width = row.width + addition + item.width
    end
    imgui.Spacing(ctx)
    local startX = imgui.GetCursorPosX(ctx)
    imgui.PushStyleVar(ctx, imgui.StyleVar_FrameRounding, theme.WIDGET.rect_corner_radius)
    for _, row in ipairs(rows) do
        local rowY = imgui.GetCursorPosY(ctx)
        imgui.SetCursorPosX(ctx, startX + math.max(0, (availableWidth - row.width) / 2))
        for idx, item in ipairs(row.items) do
            if idx > 1 then
                imgui.SameLine(ctx)
                imgui.SetCursorPosY(ctx, rowY + (theme.CONFIG.preview_button_height - imgui.GetFontSize(ctx)) / 2)
                imgui.Text(ctx, "+")
                imgui.SameLine(ctx)
                imgui.SetCursorPosY(ctx, rowY)
            end
            local clicked = ui.ColorButton(ctx, item.label .. "##condition_" .. idx, item.color, item.width, theme.CONFIG.preview_button_height)
            if item.modifier then
                ui.ItemTooltip(ctx, "Remove " .. item.label .. " from this binding")
                if clicked then
                    binding.mod = (binding.mod or 0) & (~item.modifier.bit)
                    deps.model.UpdateDirtyState(state)
                end
            else
                ui.ItemTooltip(ctx, "Edited widget: " .. state.widgetName .. " (active color)")
            end
        end
    end
    imgui.PopStyleVar(ctx)
    if binding.modifierMode then return end
    imgui.Spacing(ctx)
    local gestureItems = {}
    for _, gesture in ipairs(deps.model.GESTURES) do gestureItems[#gestureItems + 1] = { label = gesture.label, value = gesture.value } end
    gestureItems[#gestureItems + 1] = { label = "Modifiers...", modifiers = true }
    local gestureRows = { { items = {}, width = 0 } }
    local paddingX = imgui.GetStyleVar(ctx, imgui.StyleVar_FramePadding)
    for _, gestureItem in ipairs(gestureItems) do
        gestureItem.width = imgui.CalcTextSize(ctx, gestureItem.label) + paddingX * 2
        local gestureRow = gestureRows[#gestureRows]
        local addition = #gestureRow.items > 0 and spacingX or 0
        if #gestureRow.items > 0 and gestureRow.width + addition + gestureItem.width > availableWidth then
            gestureRow = { items = {}, width = 0 }
            gestureRows[#gestureRows + 1] = gestureRow
            addition = 0
        end
        gestureRow.items[#gestureRow.items + 1] = gestureItem
        gestureRow.width = gestureRow.width + addition + gestureItem.width
    end
    for _, gestureRow in ipairs(gestureRows) do
        imgui.SetCursorPosX(ctx, startX + math.max(0, (availableWidth - gestureRow.width) / 2))
        for idx, gestureItem in ipairs(gestureRow.items) do
            if idx > 1 then imgui.SameLine(ctx) end
            if gestureItem.modifiers then
                if softButton(ctx, gestureItem.label, gestureItem.width) then imgui.OpenPopup(ctx, "##modifiers") end
            else
                local selected = (binding.inputEvent or "Legacy") == gestureItem.value
                local buttonColor = imgui.GetStyleColor(ctx, selected and imgui.Col_HeaderActive or imgui.Col_TableHeaderBg)
                if ui.ColorButton(ctx, gestureItem.label .. "##gesture_" .. gestureItem.value, buttonColor, gestureItem.width) then deps.model.SetInputEvent(state, binding, gestureItem.value, deps.action_line) end
                if gestureItem.value == "Legacy" then ui.ItemTooltip(ctx, "Use the Device Default button trigger: Press or Tap") end
            end
        end
    end
    local toggled, enabled
    if imgui.BeginPopup(ctx, "##modifiers") then
        for idx, modifier in ipairs(deps.modifierFlags) do
            local selected = ((binding.mod or 0) & modifier.bit) ~= 0
            toggled, enabled = ui.ToggleButton(ctx, modifier.name .. "##choose_modifier", selected)
            if toggled then
                binding.mod = enabled and ((binding.mod or 0) | modifier.bit) or ((binding.mod or 0) & (~modifier.bit))
                deps.model.UpdateDirtyState(state)
            end
            if idx % theme.CONFIG.modifier_columns ~= 0 and idx < #deps.modifierFlags then imgui.SameLine(ctx) end
        end
        imgui.EndPopup(ctx)
    end
    if deps.data.IsRelativeWidget(state.surfaceName, state.widgetName) then
        imgui.SameLine(ctx)
        toggled, enabled = ui.ToggleButton(ctx, "Increase", binding.isIncrease == true)
        if toggled then
            binding.isIncrease = enabled
            if enabled then binding.isDecrease = false end
            deps.model.UpdateDirtyState(state)
        end
        imgui.SameLine(ctx)
        toggled, enabled = ui.ToggleButton(ctx, "Decrease", binding.isDecrease == true)
        if toggled then
            binding.isDecrease = enabled
            if enabled then binding.isIncrease = false end
            deps.model.UpdateDirtyState(state)
        end
    end
end

local function renderParameters(ctx, state, binding, parts, deps)
    if parts.actionName == "GoZone" then
        field(ctx, "Zone")
        local currentZone = parts.params[1] ~= "Blink" and parts.params[1] or ""
        if imgui.BeginCombo(ctx, "##target_zone", currentZone ~= "" and currentZone or "Select zone...") then
            local choices = {}
            if currentZone ~= "" then choices[#choices + 1] = currentZone end
            for _, zoneName in ipairs(state.zoneNames or {}) do if zoneName ~= currentZone then choices[#choices + 1] = zoneName end end
            for _, zoneName in ipairs(choices) do
                if imgui.Selectable(ctx, zoneName, zoneName == currentZone) then
                    if parts.params[1] == "Blink" then table.insert(parts.params, 1, zoneName) else parts.params[1] = zoneName end
                    saveParts(state, binding, parts, deps)
                end
            end
            if imgui.Selectable(ctx, "+ create zone") and deps.onCreateZone then deps.onCreateZone() end
            imgui.EndCombo(ctx)
        end
    elseif parts.actionName ~= "Reaper" then
        local params = {}
        for _, param in ipairs(parts.params) do if param ~= "Blink" then params[#params + 1] = deps.action_line.QuoteIfNeeded(param) end end
        field(ctx, "Parameters")
        local changed, value = imgui.InputText(ctx, "##action_parameters", table.concat(params, " "))
        if changed then
            local hasBlink = false
            for _, param in ipairs(parts.params) do if param == "Blink" then hasBlink = true end end
            parts.params = {}
            for _, token in ipairs(deps.action_line.TokenizePreservingQuotes(value)) do parts.params[#parts.params + 1] = deps.action_line.UnquoteValue(token) end
            if hasBlink then parts.params[#parts.params + 1] = "Blink" end
            saveParts(state, binding, parts, deps)
        end
    end
end

local function ensureOsdCallback(ctx, state)
    if state.osdCallback and state.osdCallbackContext == ctx then return end
    state.osdCallbackContext = ctx
    state.osdCallback = imgui.CreateFunctionFromEEL([[
        insertRequested ? (
            insertStart = min(savedStart, strlen(#Buf));
            insertEnd = min(savedEnd, strlen(#Buf));
            InputTextCallback_DeleteChars(insertStart, insertEnd - insertStart);
            InputTextCallback_InsertChars(insertStart, #insertText);
            CursorPos = insertStart + strlen(#insertText);
            SelectionStart = SelectionEnd = CursorPos;
            insertRequested = 0;
        );
        savedStart = min(SelectionStart, SelectionEnd);
        savedEnd = max(SelectionStart, SelectionEnd);
    ]])
    imgui.Attach(ctx, state.osdCallback)
end

local function renderLabels(ctx, state, binding, parts, deps)
    local storedText = tostring(parts.properties.OSD or "")
    local osdText = storedText
    if osdText == "" or osdText == "?" or osdText:lower() == "no" then osdText = deps.model.GetBindingTitle(binding, deps.action_line, deps.data, deps.reaper) end
    field(ctx, "Button label")
    local changed, label = imgui.InputTextWithHint(ctx, "##key_label", osdText, tostring(parts.properties.KeyLabel or ""))
    if changed then parts.properties.KeyLabel = label ~= "" and label or nil; saveParts(state, binding, parts, deps) end
    ensureOsdCallback(ctx, state)
    if state.osdBinding ~= binding then
        state.osdBinding = binding
        state.focusOsd = false
        imgui.Function_SetValue(state.osdCallback, "insertRequested", 0)
        imgui.Function_SetValue(state.osdCallback, "savedStart", #osdText)
        imgui.Function_SetValue(state.osdCallback, "savedEnd", #osdText)
    end
    local paddingX = imgui.GetStyleVar(ctx, imgui.StyleVar_FramePadding)
    local spacingX = imgui.GetStyleVar(ctx, imgui.StyleVar_ItemSpacing)
    local menuWidth = imgui.CalcTextSize(ctx, "{}") + paddingX * 2
    field(ctx, "On-screen text", menuWidth + spacingX)
    if state.focusOsd then imgui.SetKeyboardFocusHere(ctx); state.focusOsd = false end
    changed, osdText = imgui.InputText(ctx, "##osd_text", osdText, imgui.InputTextFlags_CallbackAlways, state.osdCallback)
    if changed then parts.properties.OSD = osdText ~= "" and osdText or nil; saveParts(state, binding, parts, deps) end
    ui.ItemTooltip(ctx, "Use \\n for a new line. Empty text uses the action title.")
    imgui.SameLine(ctx)
    if softButton(ctx, "{}") then imgui.OpenPopup(ctx, "##osd_variables") end
    ui.ItemTooltip(ctx, "Insert variable at the text cursor")
    if imgui.BeginPopup(ctx, "##osd_variables") then
        for _, definition in ipairs(osd_templates.GetDefinitions()) do
            local token = "{" .. definition.name .. "}"
            if imgui.Selectable(ctx, token .. "  " .. definition.description) then
                imgui.Function_SetValue_String(state.osdCallback, "#insertText", token)
                imgui.Function_SetValue(state.osdCallback, "insertRequested", 1)
                state.focusOsd = true
            end
        end
        imgui.EndPopup(ctx)
    end
    local unknown = osd_templates.FindUnknownVariables(osdText)
    if #unknown > 0 then imgui.TextWrapped(ctx, "Unknown variables stay unchanged: " .. table.concat(unknown, ", ")) end
end

local function renderChoiceButtons(ctx, label, choices, selected, disabled)
    imgui.Text(ctx, label)
    local changedChoice = nil
    for _, choice in ipairs(choices) do
        imgui.SameLine(ctx)
        local selectedChoice = selected == choice
        local buttonColor = imgui.GetStyleColor(ctx, selectedChoice and imgui.Col_HeaderActive or imgui.Col_TableHeaderBg)
        local clicked = ui.Disabled(ctx, disabled, function() return ui.ColorButton(ctx, choice .. "##" .. label .. "_" .. choice, buttonColor) end)
        if clicked and not disabled and not selectedChoice then changedChoice = choice end
    end
    return changedChoice
end

local function renderBlink(ctx, state, binding, parts, deps)
    local modes = { "Off", "Active", "Inactive", "Always" }
    local feedbackNone = parts.properties.Feedback == "None"
    if feedbackNone then modes = { "Off", "Always" } end
    local currentMode = parts.properties.Blink or "Off"
    if feedbackNone and currentMode ~= "Always" then currentMode = "Off" end
    local nextMode = renderChoiceButtons(ctx, "Blink", modes, currentMode, false)
    local changed = nextMode ~= nil
    nextMode = nextMode or currentMode
    local custom = parts.properties.BlinkIntervalMs ~= nil
    local interval = tonumber(parts.properties.BlinkIntervalMs) or 500
    if nextMode ~= "Off" then
        imgui.SameLine(ctx)
        local customChanged
        customChanged, custom = imgui.Checkbox(ctx, "Custom interval", custom)
        changed = changed or customChanged
        if custom then
            imgui.SameLine(ctx)
            imgui.SetNextItemWidth(ctx, 120)
            local intervalChanged
            intervalChanged, interval = imgui.DragInt(ctx, "##blink_interval", interval, 10, 1, 10000, "%d ms")
            changed = changed or intervalChanged
        end
    end
    if changed then
        for idx = #parts.params, 1, -1 do if parts.params[idx] == "Blink" then table.remove(parts.params, idx) end end
        parts.properties.Blink = nextMode ~= "Off" and nextMode or nil
        parts.properties.BlinkIntervalMs = nextMode ~= "Off" and custom and tostring(interval) or nil
        saveParts(state, binding, parts, deps)
    end
end

local function renderAdvanced(ctx, state, binding, parts, deps)
    if not imgui.CollapsingHeader(ctx, "Advanced") then return end
    local changed, enabled = imgui.Checkbox(ctx, "Use " .. state.widgetName .. " as modifier", binding.modifierMode ~= nil)
    ui.ItemTooltip(ctx, "This binding declares the edited physical widget as a standard modifier source instead of running a normal action.", true)
    if changed then
        binding.modifierMode = enabled and "Default" or nil
        if enabled then
            binding.mod = 0
            binding.inputEvent = "Legacy"
            binding.isValueInverted = false
            binding.isFeedbackInverted = false
            binding.isIncrease = false
            binding.isDecrease = false
            parts.actionName = deps.modifierFlags[1].name
            parts.params = {}
            parts.properties = {}
            saveParts(state, binding, parts, deps)
        else
            deps.model.UpdateDirtyState(state)
        end
    end
    if binding.modifierMode then
        imgui.SameLine(ctx)
        imgui.SetNextItemWidth(ctx, 180)
        local modeChanged, mode = imgui.Combo(ctx, "##modifier_mode", deps.model.MODIFIER_MODE_INDEX[binding.modifierMode] or 0, deps.model.MODIFIER_MODE_ITEMS)
        if modeChanged then binding.modifierMode = deps.model.MODIFIER_MODE_BY_INDEX[mode + 1]; deps.model.UpdateDirtyState(state) end
        imgui.SameLine(ctx)
        imgui.SetNextItemWidth(ctx, 120)
        if imgui.BeginCombo(ctx, "##modifier_name", parts.actionName) then
            for _, modifier in ipairs(deps.modifierFlags) do
                if imgui.Selectable(ctx, modifier.name, parts.actionName == modifier.name) then parts.actionName = modifier.name; saveParts(state, binding, parts, deps) end
            end
            imgui.EndCombo(ctx)
        end
    else
        local feedbackAvailable = deps.model.IsFeedbackAvailable(binding, deps.action_line, deps.reaper)
        local feedbackModes = { "Default", "Invert", "None" }
        local feedbackMode = feedbackAvailable and (parts.properties.Feedback or "Default") or "None"
        local nextFeedbackMode = renderChoiceButtons(ctx, "Feedback", feedbackModes, feedbackMode, not feedbackAvailable)
        ui.ItemTooltip(ctx, feedbackAvailable and "Select the action value used for feedback." or "This REAPER action does not report a toggle state. Feedback is None.", true)
        if nextFeedbackMode and feedbackAvailable then
            parts.properties.Feedback = nextFeedbackMode
            if parts.properties.Feedback == "None" and parts.properties.Blink and parts.properties.Blink ~= "Always" then parts.properties.Blink = "Always" end
            saveParts(state, binding, parts, deps)
        end
        imgui.SameLine(ctx)
        local invertValue = false
        for _, param in ipairs(parts.params) do if param == "InvertValue" then invertValue = true end end
        changed, enabled = imgui.Checkbox(ctx, "Invert value", invertValue)
        if changed then
            for paramIdx = #parts.params, 1, -1 do if parts.params[paramIdx] == "InvertValue" then table.remove(parts.params, paramIdx) end end
            if enabled then parts.params[#parts.params + 1] = "InvertValue" end
            saveParts(state, binding, parts, deps)
        end
    end
    renderBlink(ctx, state, binding, parts, deps)
    if not binding.modifierMode then
        imgui.SetNextItemWidth(ctx, theme.CONFIG.run_count_width)
        local runCountChanged, runCount = imgui.DragInt(ctx, "##run_count", tonumber(parts.properties.RunCount) or 1, 1, 1, 10, "Run count: %d")
        if runCountChanged then parts.properties.RunCount = runCount > 1 and tostring(runCount) or nil; saveParts(state, binding, parts, deps) end
        if binding.inputEvent == "Hold" or binding.inputEvent == "LongHold" then
            imgui.SetNextItemWidth(ctx, theme.CONFIG.hold_input_width)
            local delayChanged, delay = imgui.DragInt(ctx, "##hold_delay", tonumber(parts.properties.HoldDelay) or 0, 100, 0, 10000, "Hold delay: %d ms")
            ui.ItemTooltip(ctx, "0 uses the Device hold delay")
            if delayChanged then parts.properties.HoldDelay = delay > 0 and tostring(delay) or nil; saveParts(state, binding, parts, deps) end
            imgui.SameLine(ctx)
            imgui.SetNextItemWidth(ctx, theme.CONFIG.hold_input_width)
            local repeatChanged, interval = imgui.DragInt(ctx, "##hold_repeat", tonumber(parts.properties.HoldRepeatInterval) or 0, 10, 0, 1000, "Repeat: %d ms")
            ui.ItemTooltip(ctx, "0 disables hold repeat")
            if repeatChanged then parts.properties.HoldRepeatInterval = interval > 0 and tostring(interval) or nil; saveParts(state, binding, parts, deps) end
        end
    end
    field(ctx, "Action raw")
    local lineChanged, line = imgui.InputText(ctx, "##raw_action", binding.line)
    if lineChanged then
        binding.line = line
        deps.model.RefreshBindingDerivedFields(binding, deps.action_line)
        deps.model.UpdateDirtyState(state)
    end
    field(ctx, "Zone line")
    imgui.InputText(ctx, "##zone_line", deps.model.GetZoneLinePreview(state.widgetName, binding, deps.modifierFlags), imgui.InputTextFlags_ReadOnly)
end

function M.RenderBody(ctx, state, deps)
    renderBindingTable(ctx, state, deps)
    local binding = deps.model.GetSelectedBinding(state)
    if not binding then
        imgui.Text(ctx, "No binding for this widget. Use + to add one.")
        return
    end
    renderConditions(ctx, state, binding, deps)
    imgui.Spacing(ctx)
    if not binding.modifierMode then renderActionPicker(ctx, state, binding, deps) end
    local parts = deps.action_line.Parse(binding.line)
    if not binding.modifierMode then
        renderParameters(ctx, state, binding, parts, deps)
        renderLabels(ctx, state, binding, parts, deps)
    end
    renderAdvanced(ctx, state, binding, parts, deps)
end

return M
