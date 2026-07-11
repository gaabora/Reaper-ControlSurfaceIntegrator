local imgui = require "imgui" "0.9.3"

local osk_color_picker = require("osk_color_picker")
local theme = require("theme_settings")
local ui = require("ui_components")

local M = {}

local function selectBinding(state, bindingIndex)
    state.selectedBinding = bindingIndex
end

local function renderBindingColors(ctx, state, binding, bindingIndex, deps)
    local parts = deps.action_line.Parse(binding.line)
    local colors = deps.action_line.ParseColors(parts)
    local inactiveColor = colors and colors[1] or theme.CONFIG.default_inactive_color
    local activeColor = colors and (colors[2] or colors[1]) or theme.CONFIG.default_active_color

    inactiveColor = osk_color_picker.RenderBindingColorPicker(ctx, state, binding, bindingIndex, 1, "Inactive", inactiveColor, deps)
    imgui.SameLine(ctx, 0, 3)
    osk_color_picker.RenderBindingColorPicker(ctx, state, binding, bindingIndex, 2, "Active", activeColor, deps)
end

local function renderBindingTable(ctx, state, deps)
    if not imgui.BeginTable(ctx, "##bindings_table", 4, deps.tableFlags, -1, theme.CONFIG.bindings_table_height, 0) then return end

    imgui.TableSetupColumn(ctx, "Modifier", imgui.TableColumnFlags_WidthFixed, 95, 0)
    imgui.TableSetupColumn(ctx, "Action", imgui.TableColumnFlags_WidthStretch, 0.52, 1)
    imgui.TableSetupColumn(ctx, "Colors", imgui.TableColumnFlags_WidthFixed, theme.CONFIG.color_column_width, 2)
    imgui.TableSetupColumn(ctx, "Other", imgui.TableColumnFlags_WidthStretch, 0.28, 3)
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

        imgui.TableSetColumnIndex(ctx, 3)
        local other = deps.model.GetOtherSummary(binding, deps.action_line)
        if other == "" then other = "-" end
        if imgui.Selectable(ctx, other .. "##other_" .. bindingIndex, selected) then
            selectBinding(state, bindingIndex)
        end
    end

    imgui.EndTable(ctx)
end

local function renderActionPicker(ctx, state, binding, deps)
    imgui.SetNextItemWidth(ctx, theme.CONFIG.action_search_mode_width)
    local modeChanged
    modeChanged, state.searchModeIndex = imgui.Combo(ctx, "##action_search_source", state.searchModeIndex, deps.model.SEARCH_MODE_ITEMS)
    if modeChanged then
        deps.model.SyncSearchModeFromIndex(state)
        deps.model.RefreshSearchResults(state, deps.reaper)
    end

    imgui.SameLine(ctx, 0, theme.CONFIG.action_search_spacing)
    imgui.SetNextItemWidth(ctx, -28)
    local queryChanged
    if imgui.InputTextWithHint then
        queryChanged, state.searchQuery = imgui.InputTextWithHint(ctx, "##action_search", "Find action...", state.searchQuery)
    else
        queryChanged, state.searchQuery = imgui.InputText(ctx, "##action_search", state.searchQuery)
    end
    if queryChanged then deps.model.RefreshSearchResults(state, deps.reaper) end
    imgui.SameLine(ctx, 0, theme.CONFIG.action_search_clear_spacing)
    if imgui.Button(ctx, "x##clear_action_search") then
        state.searchQuery = ""
        state.searchResults = {}
        state.searchSelected = 0
    end
    ui.ItemTooltip(ctx, "Clear action search")

    if state.searchQuery:match("%S") and imgui.BeginListBox(ctx, "##action_search_results", -1, theme.CONFIG.action_search_results_height) then
        for idx, row in ipairs(state.searchResults) do
            if imgui.Selectable(ctx, row, idx == state.searchSelected) then
                state.searchSelected = idx
                deps.model.ApplySearchSelectionToBinding(state, binding, row, deps.action_line)
            end
        end
        imgui.EndListBox(ctx)
    end
end

function M.RenderToolbar(ctx, state, deps)
    if not imgui.BeginTable(ctx, "##config_toolbar", 2, imgui.TableFlags_SizingStretchProp, -1, 0, 0) then return end
    imgui.TableSetupColumn(ctx, "##config_actions", imgui.TableColumnFlags_WidthStretch, 1, 0)
    imgui.TableSetupColumn(ctx, "##binding_actions", imgui.TableColumnFlags_WidthFixed, theme.CONFIG.toolbar_binding_actions_width, 1)
    imgui.TableNextRow(ctx)

    imgui.TableSetColumnIndex(ctx, 0)
    local operationReady = state.pendingOperation == nil and not state.previewApplyPending
    ui.DirtyActionButton(ctx, "Apply Live", operationReady and state.hasUnappliedEdits, function()
        state.saveAfterApply = false
        deps.protocol.SendApplyLive(state, deps.data, deps.model)
    end)
    imgui.SameLine(ctx)
    ui.DirtyActionButton(ctx, "Save", operationReady and state.isDirty, function()
        if state.hasUnappliedEdits then
            state.saveAfterApply = true
            if not deps.protocol.SendApplyLive(state, deps.data, deps.model) then state.saveAfterApply = false end
        else
            deps.protocol.SendSave(state, deps.data, deps.model)
        end
    end)
    imgui.SameLine(ctx)
    ui.DirtyActionButton(ctx, "Revert", operationReady and state.isDirty, function()
        deps.protocol.RequestRevert(state, deps.data)
    end)

    if state.pendingOperation then
        imgui.SameLine(ctx)
        imgui.TextDisabled(ctx, state.pendingOperation .. "...")
    elseif state.previewApplyPending then
        imgui.SameLine(ctx)
        imgui.TextDisabled(ctx, "Preview...")
    elseif state.status:match("^ERR%s*|") then
        imgui.SameLine(ctx)
        imgui.Text(ctx, state.status)
    end

    imgui.TableSetColumnIndex(ctx, 1)
    if imgui.Button(ctx, "+ Add") then deps.model.AddBinding(state) end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "- Remove") then deps.model.RemoveSelectedBinding(state) end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Clone") then deps.model.DuplicateSelectedBinding(state) end
    imgui.SameLine(ctx)
    if imgui.ArrowButton(ctx, "##move_binding_up", imgui.Dir_Up) then deps.model.MoveSelectedBinding(state, -1) end
    ui.ItemTooltip(ctx, "Move binding up")
    imgui.SameLine(ctx)
    if imgui.ArrowButton(ctx, "##move_binding_down", imgui.Dir_Down) then deps.model.MoveSelectedBinding(state, 1) end
    ui.ItemTooltip(ctx, "Move binding down")

    imgui.EndTable(ctx)
end

function M.RenderBody(ctx, state, deps)
    renderBindingTable(ctx, state, deps)

    local selected = deps.model.GetSelectedBinding(state)
    if not selected then
        imgui.Separator(ctx)
        imgui.Text(ctx, "No binding for this widget in the active zone.")
        return
    end

    imgui.Separator(ctx)
    local parts = deps.action_line.Parse(selected.line)

    local actionChanged
    actionChanged, parts.actionName = imgui.InputText(ctx, "Action", parts.actionName or "")
    if actionChanged then
        selected.line = deps.action_line.Build(parts)
        deps.model.RefreshBindingDerivedFields(selected, deps.action_line)
        deps.model.UpdateDirtyState(state)
    end

    renderActionPicker(ctx, state, selected, deps)
    parts = deps.action_line.Parse(selected.line)
    local changedQuick = false

    local paramsText = table.concat(parts.params or {}, " ")
    local paramsChanged
    paramsChanged, paramsText = imgui.InputText(ctx, "Parameters", paramsText)
    if paramsChanged then
        parts.params = deps.action_line.TokenizePreservingQuotes(paramsText)
        changedQuick = true
    end

    local storedOsdText = tostring(parts.properties.OSD or "")
    local osdText = storedOsdText
    if osdText == "" or osdText == "?" or osdText:lower() == "no" then
        osdText = deps.model.GetBindingTitle(selected, deps.action_line, deps.data, deps.reaper)
    end
    local osdChanged
    osdChanged, osdText = imgui.InputText(ctx, "OSD", osdText)
    if osdChanged then
        parts.properties.OSD = (osdText ~= "") and osdText or nil
        changedQuick = true
    end

    local keyLabelText = tostring(parts.properties.KeyLabel or "")
    local keyLabelChanged
    if imgui.InputTextWithHint then
        local keyLabelHint = osdText ~= "" and osdText or "Uses OSD when empty"
        keyLabelChanged, keyLabelText = imgui.InputTextWithHint(ctx, "KeyLabel", keyLabelHint, keyLabelText)
    else
        keyLabelChanged, keyLabelText = imgui.InputText(ctx, "KeyLabel (OSD default)", keyLabelText)
    end
    if keyLabelChanged then
        parts.properties.KeyLabel = (keyLabelText ~= "") and keyLabelText or nil
        changedQuick = true
    end

    local toggled, enabled = imgui.Checkbox(ctx, "Hold##pseudo_hold", selected.hasHold == true)
    if toggled then
        deps.model.SetHoldEnabled(state, selected, enabled, deps.action_line)
        parts = deps.action_line.Parse(selected.line)
    end
    imgui.SameLine(ctx)
    toggled, enabled = imgui.Checkbox(ctx, "DoublePress##pseudo_double", selected.hasDoublePress == true)
    if toggled then
        selected.hasDoublePress = enabled
        deps.model.UpdateDirtyState(state)
    end

    if deps.data.IsRelativeWidget(state.surfaceName, state.widgetName) then
        imgui.SameLine(ctx)
        toggled, enabled = imgui.Checkbox(ctx, "Increase##direction_increase", selected.isIncrease == true)
        if toggled then
            selected.isIncrease = enabled
            if enabled then selected.isDecrease = false end
            deps.model.UpdateDirtyState(state)
        end
        imgui.SameLine(ctx)
        toggled, enabled = imgui.Checkbox(ctx, "Decrease##direction_decrease", selected.isDecrease == true)
        if toggled then
            selected.isDecrease = enabled
            if enabled then selected.isIncrease = false end
            deps.model.UpdateDirtyState(state)
        end
    end

    toggled, enabled = imgui.Checkbox(ctx, "Invert value", selected.isValueInverted == true)
    if toggled then
        selected.isValueInverted = enabled
        deps.model.UpdateDirtyState(state)
    end
    imgui.SameLine(ctx)
    toggled, enabled = imgui.Checkbox(ctx, "Invert feedback", selected.isFeedbackInverted == true)
    if toggled then
        selected.isFeedbackInverted = enabled
        deps.model.UpdateDirtyState(state)
    end
    imgui.SameLine(ctx)
    local feedbackNo = tostring(parts.properties.Feedback or ""):lower() == "no"
    local feedbackChanged
    feedbackChanged, feedbackNo = imgui.Checkbox(ctx, "No Feedback", feedbackNo)
    if feedbackChanged then
        if feedbackNo then parts.properties.Feedback = "No" else parts.properties.Feedback = nil end
        changedQuick = true
    end

    imgui.TextDisabled(ctx, "Modifiers:")
    imgui.SameLine(ctx)
    for idx, modifier in ipairs(deps.modifierFlags) do
        local hasFlag = ((selected.mod or 0) & modifier.bit) ~= 0
        toggled, hasFlag = imgui.Checkbox(ctx, modifier.name .. "##mod_" .. idx, hasFlag)
        if toggled then
            if hasFlag then
                selected.mod = (selected.mod or 0) | modifier.bit
            else
                selected.mod = (selected.mod or 0) & (~modifier.bit)
            end
            deps.model.UpdateDirtyState(state)
        end
        if idx ~= #deps.modifierFlags and idx % theme.CONFIG.modifier_columns ~= 0 then imgui.SameLine(ctx) end
    end

    local runCountVal = tonumber(parts.properties.RunCount) or 1
    local runCountChanged
    imgui.SetNextItemWidth(ctx, theme.CONFIG.run_count_width)
    runCountChanged, runCountVal = imgui.DragInt(ctx, "##run_count", runCountVal, 1, 1, 10, "RunCount = %d")
    if runCountChanged then
        parts.properties.RunCount = (runCountVal > 1) and tostring(runCountVal) or nil
        changedQuick = true
    end

    if selected.hasHold then
        imgui.SameLine(ctx, 0, theme.CONFIG.control_spacing)
        local holdDelayVal = tonumber(parts.properties.HoldDelay) or 0
        local holdDelayChanged
        imgui.SetNextItemWidth(ctx, theme.CONFIG.hold_input_width)
        holdDelayChanged, holdDelayVal = imgui.DragInt(ctx, "##hold_delay", holdDelayVal, 100, 0, 10000, "HoldDelay = %d ms")
        if holdDelayChanged then
            parts.properties.HoldDelay = (holdDelayVal > 0) and tostring(holdDelayVal) or nil
            changedQuick = true
        end

        imgui.SameLine(ctx, 0, theme.CONFIG.control_spacing)
        local holdRepeatVal = tonumber(parts.properties.HoldRepeatInterval) or 0
        local holdRepeatChanged
        imgui.SetNextItemWidth(ctx, theme.CONFIG.hold_input_width)
        holdRepeatChanged, holdRepeatVal = imgui.DragInt(ctx, "##hold_repeat", holdRepeatVal, 10, 0, 1000, "Repeat = %d ms")
        if holdRepeatChanged then
            parts.properties.HoldRepeatInterval = (holdRepeatVal > 0) and tostring(holdRepeatVal) or nil
            changedQuick = true
        end
    end

    if changedQuick then
        selected.line = deps.action_line.Build(parts)
        deps.model.RefreshBindingDerivedFields(selected, deps.action_line)
        deps.model.UpdateDirtyState(state)
    end

    imgui.Separator(ctx)
    local lineChanged
    lineChanged, selected.line = imgui.InputText(ctx, "Raw", selected.line or "")
    if lineChanged then
        deps.model.RefreshBindingDerivedFields(selected, deps.action_line)
        deps.model.UpdateDirtyState(state)
    end
end

return M
