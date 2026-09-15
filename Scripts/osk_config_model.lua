local identity = require("product_identity")

local M = {}

M.SEARCH_MODE_ITEMS = "all\0csi\0reaper\0"
M.SEARCH_MODE_BY_INDEX = { "all", "csi", "reaper" }
M.MODIFIER_MODE_ITEMS = "Use Device setting\0Momentary\0Latch\0Hybrid\0"
M.MODIFIER_MODE_BY_INDEX = { "Default", "Momentary", "Latch", "Hybrid" }
M.MODIFIER_MODE_INDEX = { Default = 0, Momentary = 1, Latch = 2, Hybrid = 3 }
M.GESTURES = {
    { label = "Default", value = "Legacy" },
    { label = "Press", value = "Press" },
    { label = "Tap", value = "Tap" },
    { label = "Release", value = "Release" },
    { label = "Hold", value = "Hold" },
    { label = "Long hold", value = "LongHold" },
    { label = "Double press", value = "DoublePress" },
}

function M.ParseCsv(text)
    local out = {}
    for entry in tostring(text or ""):gmatch("[^,]+") do
        if entry ~= "" then out[#out + 1] = entry end
    end
    return out
end

function M.SyncSearchModeFromIndex(state)
    state.searchMode = M.SEARCH_MODE_BY_INDEX[(state.searchModeIndex or 0) + 1] or "all"
end

function M.SyncSearchIndexFromMode(state)
    if state.searchMode == "csi" then
        state.searchModeIndex = 1
    elseif state.searchMode == "reaper" then
        state.searchModeIndex = 2
    else
        state.searchModeIndex = 0
        state.searchMode = "all"
    end
end

function M.SetActionColor(binding, colorIndex, color, action_line, theme)
    local parts = action_line.Parse(binding.line)
    local colors = action_line.ParseColors(parts) or { theme.CONFIG.default_inactive_color, theme.CONFIG.default_active_color }
    if #colors == 1 then colors[2] = colors[1] end
    colors[colorIndex] = color
    action_line.SetColors(parts, { colors[1], colors[2] })
    binding.line = action_line.Build(parts)
end

function M.ResetActionColor(binding, colorIndex, action_line, theme)
    local defaultColor = colorIndex == 2 and theme.CONFIG.default_active_color or theme.CONFIG.default_inactive_color
    M.SetActionColor(binding, colorIndex, defaultColor, action_line, theme)
    return defaultColor
end

function M.ClearActionColors(binding, action_line)
    local parts = action_line.Parse(binding.line)
    action_line.ClearColors(parts)
    binding.line = action_line.Build(parts)
end

local function getReaperActionTitle(parts, action_line, r)
    if tostring(parts.actionName or ""):lower() ~= "reaper" then return nil end
    local commandToken = tostring(action_line.UnquoteValue and action_line.UnquoteValue(parts.params[1] or "") or parts.params[1] or "")
    local commandId = tonumber(commandToken)
    if not commandId and commandToken ~= "" and type(r.NamedCommandLookup) == "function" then
        commandId = r.NamedCommandLookup(commandToken)
    end
    if not commandId or commandId == 0 or type(r.kbd_getTextFromCmd) ~= "function" then return nil end
    local title = r.kbd_getTextFromCmd(commandId, 0)
    if title and title ~= "" then return title end
    return nil
end

function M.GetBindingTitle(binding, action_line, data, r)
    local parts = action_line.Parse(binding.line)
    local explicitTitle = parts.properties.OSD
    if explicitTitle == nil or explicitTitle == "" or explicitTitle == "?" or explicitTitle == "No" then
        explicitTitle = parts.properties.KeyLabel
    end
    if explicitTitle and explicitTitle ~= "" then return explicitTitle end
    local reaperTitle = getReaperActionTitle(parts, action_line, r)
    if reaperTitle then return reaperTitle end
    local csiTitle = data.getProcessedLabel(parts.actionName)
    if csiTitle and csiTitle ~= "" then return csiTitle end
    return binding.line ~= "" and binding.line or "NoAction"
end

function M.GetActionTitle(binding, action_line, reaperApi)
    local parts = action_line.Parse(binding.line)
    local title = getReaperActionTitle(parts, action_line, reaperApi)
    if title then return title end
    local params = {}
    for _, param in ipairs(parts.params) do if param ~= "Blink" and param ~= "InvertValue" then params[#params + 1] = param end end
    return (table.concat({ parts.actionName or "NoAction", table.concat(params, " ") }, " "):gsub("%s+$", ""))
end

function M.GetModifierLabel(binding, modifierFlags)
    local labels = {}
    if binding.inputEvent and binding.inputEvent ~= "Legacy" then labels[#labels + 1] = binding.inputEvent end
    for _, modifier in ipairs(modifierFlags) do
        if ((binding.mod or 0) & modifier.bit) ~= 0 then
            labels[#labels + 1] = modifier.name
        end
    end
    if #labels == 0 then return "-" end
    return table.concat(labels, "+")
end

function M.ParseBindingString(raw, action_line)
    local bindings = {}
    for entry in tostring(raw or ""):gmatch("[^;]+") do
        local modStr, line = entry:match("^(%-?%d+):(.+)$")
        if modStr and line then
            local metadata = {
                inputEvent = "Legacy",
                isValueInverted = false,
                isFeedbackInverted = false,
                isIncrease = false,
                isDecrease = false,
                modifierMode = nil,
                feedbackAvailable = true,
            }
            while true do
                local token = line:match("%s+(__OSK_[A-Z_]+)$")
                if not token then break end
                line = line:sub(1, #line - #token - 1)
                if token == "__OSK_EVENT_PRESS" then metadata.inputEvent = "Press"
                elseif token == "__OSK_EVENT_TAP" then metadata.inputEvent = "Tap"
                elseif token == "__OSK_EVENT_RELEASE" then metadata.inputEvent = "Release"
                elseif token == "__OSK_EVENT_HOLD" then metadata.inputEvent = "Hold"
                elseif token == "__OSK_EVENT_LONG_HOLD" then metadata.inputEvent = "LongHold"
                elseif token == "__OSK_EVENT_DOUBLE_PRESS" then metadata.inputEvent = "DoublePress"
                elseif token == "__OSK_FEEDBACK_UNAVAILABLE" then metadata.feedbackAvailable = false
                elseif token == "__OSK_INVERT" then metadata.isValueInverted = true
                elseif token == "__OSK_INVERT_FB" then metadata.isFeedbackInverted = true
                elseif token == "__OSK_INCREASE" then metadata.isIncrease = true
                elseif token == "__OSK_DECREASE" then metadata.isDecrease = true
                elseif token == "__OSK_MODIFIER_DEFAULT" then metadata.modifierMode = "Default"
                elseif token == "__OSK_MODIFIER_MOMENTARY" then metadata.modifierMode = "Momentary"
                elseif token == "__OSK_MODIFIER_LATCH" then metadata.modifierMode = "Latch"
                elseif token == "__OSK_MODIFIER_HYBRID" then metadata.modifierMode = "Hybrid"
                end
            end
            local parts = action_line.Parse(line)
            if parts.properties.Feedback == "No" then parts.properties.Feedback = "None"
            elseif parts.properties.Feedback == "Yes" or parts.properties.Feedback == "Value" then parts.properties.Feedback = "Default" end
            if not metadata.feedbackAvailable then parts.properties.Feedback = "None" end
            if metadata.isFeedbackInverted and metadata.feedbackAvailable then parts.properties.Feedback = "Invert" end
            metadata.isFeedbackInverted = false
            if metadata.isValueInverted and not metadata.modifierMode then
                local hasInvertValue = false
                for _, param in ipairs(parts.params) do if param == "InvertValue" then hasInvertValue = true end end
                if not hasInvertValue then parts.params[#parts.params + 1] = "InvertValue" end
                metadata.isValueInverted = false
            end
            if not metadata.modifierMode and parts.properties.Feedback == nil then parts.properties.Feedback = "Default" end
            for paramIdx = #parts.params, 1, -1 do
                if parts.params[paramIdx] == "Blink" and not metadata.modifierMode then
                    table.remove(parts.params, paramIdx)
                    parts.properties.Blink = parts.properties.Feedback == "None" and "Always" or "Active"
                end
            end
            local legacyBlinkInterval = tonumber(parts.properties.Blink)
            if legacyBlinkInterval then
                parts.properties.Blink = parts.properties.Feedback == "None" and "Always" or "Active"
                parts.properties.BlinkIntervalMs = tostring(legacyBlinkInterval)
            end
            line = action_line.Build(parts)
            bindings[#bindings + 1] = {
                mod = tonumber(modStr) or 0,
                line = line,
                actionName = parts.actionName or "",
                inputEvent = metadata.inputEvent,
                isValueInverted = metadata.isValueInverted,
                isFeedbackInverted = metadata.isFeedbackInverted,
                isIncrease = metadata.isIncrease,
                isDecrease = metadata.isDecrease,
                modifierMode = metadata.modifierMode,
                feedbackAvailable = metadata.feedbackAvailable,
            }
        end
    end
    return bindings
end

function M.MatchesSearchTerms(text, query)
    local candidate = tostring(text or ""):lower()
    for term in tostring(query or ""):lower():gmatch("%S+") do
        if not candidate:find(term, 1, true) then return false end
    end
    return true
end

function M.GetNamedCommandId(r, commandId)
    if type(r.ReverseNamedCommandLookup) ~= "function" then return "" end
    local ok, namedCommand = pcall(r.ReverseNamedCommandLookup, commandId)
    if not ok or not namedCommand or namedCommand == "" then return "" end
    namedCommand = tostring(namedCommand)
    if namedCommand:sub(1, 1) ~= "_" then namedCommand = "_" .. namedCommand end
    return namedCommand
end

function M.IsFeedbackAvailable(binding, action_line, reaperApi)
    local parts = action_line.Parse(binding.line)
    if tostring(parts.actionName or ""):lower() ~= "reaper" then return binding.feedbackAvailable ~= false end
    local commandToken = tostring(action_line.UnquoteValue(parts.params[1] or ""))
    local commandId = tonumber(commandToken)
    if not commandId and commandToken ~= "" and type(reaperApi.NamedCommandLookup) == "function" then commandId = reaperApi.NamedCommandLookup(commandToken) end
    if not commandId or commandId == 0 or type(reaperApi.GetToggleCommandState) ~= "function" then return true end
    return reaperApi.GetToggleCommandState(commandId) ~= -1
end

function M.RefreshSearchResults(state, r)
    local query = state.searchQuery
    local results = {}

    if state.searchMode ~= "reaper" then
        for _, name in ipairs(state.csiActions) do
            if M.MatchesSearchTerms(name, query) then
                results[#results + 1] = name .. " [" .. identity.displayName .. "]"
            end
        end
    end

    if state.searchMode ~= "csi" then
        local function nextAction(index)
            if type(r.EnumerateActions) == "function" then
                local ok, a, b = pcall(r.EnumerateActions, 0, index)
                if ok and type(a) == "number" then
                    return a, tostring(b or "")
                end
            end
            if type(r.CF_EnumerateActions) == "function" then
                local ok, cmdId, name = pcall(r.CF_EnumerateActions, 0, index, "")
                if ok and type(cmdId) == "number" then
                    return cmdId, tostring(name or "")
                end
            end
            return 0, ""
        end

        local index = 0
        while #results < 60 do
            local commandId, actionName = nextAction(index)
            if commandId == 0 then break end
            index = index + 1

            local namedCommand = M.GetNamedCommandId(r, commandId)
            local searchableText = table.concat({ tostring(commandId), namedCommand, actionName }, " ")
            if M.MatchesSearchTerms(searchableText, query) then
                local idText = namedCommand ~= "" and string.format("%d (%s)", commandId, namedCommand) or tostring(commandId)
                results[#results + 1] = string.format("%s - ID %s", actionName, idText)
            end
        end
    end

    state.searchResults = results
end

function M.SerializeBindings(bindings)
    local chunks = {}
    for _, binding in ipairs(bindings or {}) do
        if binding and binding.line and binding.line ~= "" then
            local line = binding.line
            if binding.inputEvent and binding.inputEvent ~= "Legacy" and not binding.modifierMode then
                local eventToken = binding.inputEvent:gsub("(%l)(%u)", "%1_%2"):upper()
                line = line .. " __OSK_EVENT_" .. eventToken
            end
            if binding.isIncrease then line = line .. " __OSK_INCREASE" end
            if binding.isDecrease then line = line .. " __OSK_DECREASE" end
            if binding.modifierMode then line = line .. " __OSK_MODIFIER_" .. binding.modifierMode:upper() end
            chunks[#chunks + 1] = tostring(binding.mod or 0) .. ":" .. line
        end
    end
    return table.concat(chunks, ";")
end

function M.CloneBindings(bindings)
    local copy = {}
    for _, binding in ipairs(bindings or {}) do
        copy[#copy + 1] = {
            mod = binding.mod or 0,
            line = binding.line or "",
            actionName = binding.actionName or "",
            inputEvent = binding.inputEvent or "Legacy",
            isValueInverted = binding.isValueInverted == true,
            isFeedbackInverted = binding.isFeedbackInverted == true,
            isIncrease = binding.isIncrease == true,
            isDecrease = binding.isDecrease == true,
            modifierMode = binding.modifierMode,
            feedbackAvailable = binding.feedbackAvailable ~= false,
        }
    end
    return copy
end

function M.UpdateDirtyState(state)
    state.hasUnappliedEdits = M.SerializeBindings(state.bindings) ~= state.confirmedSerialized
    state.isDirty = state.hasUnappliedEdits or state.hasLiveChanges
end

function M.AcceptBindings(state, bindings, replaceVisibleBindings)
    if replaceVisibleBindings then state.bindings = bindings end
    state.confirmedBindings = M.CloneBindings(bindings)
    state.confirmedSerialized = M.SerializeBindings(bindings)
    M.UpdateDirtyState(state)
end

function M.RefreshBindingDerivedFields(binding, action_line)
    if not binding then return end
    binding.actionName = action_line.Parse(binding.line).actionName or ""
end

function M.GetSelectedBinding(state)
    if state.selectedBinding < 1 then return nil end
    return state.bindings[state.selectedBinding]
end

function M.ClampSelectedBindingIndex(state)
    if #state.bindings == 0 then
        state.selectedBinding = 1
        return
    end
    if state.selectedBinding < 1 then state.selectedBinding = 1 end
    if state.selectedBinding > #state.bindings then
        state.selectedBinding = #state.bindings
    end
end

function M.AddBinding(state)
    state.bindings[#state.bindings + 1] = {
        mod = 0,
        line = "NoAction",
        actionName = "NoAction",
        inputEvent = "Legacy",
    }
    state.selectedBinding = #state.bindings
    M.UpdateDirtyState(state)
end

function M.RemoveSelectedBinding(state)
    if #state.bindings == 0 then return end
    table.remove(state.bindings, state.selectedBinding)
    M.ClampSelectedBindingIndex(state)
    M.UpdateDirtyState(state)
end

function M.MoveSelectedBinding(state, offset)
    if #state.bindings < 2 then return end
    local fromIndex = state.selectedBinding
    local toIndex = fromIndex + offset
    if toIndex < 1 or toIndex > #state.bindings then return end
    local row = table.remove(state.bindings, fromIndex)
    table.insert(state.bindings, toIndex, row)
    state.selectedBinding = toIndex
    M.UpdateDirtyState(state)
end

function M.ApplySearchSelectionToBinding(state, binding, row, action_line)
    if not binding then return end
    row = row or state.searchResults[state.searchSelected]
    if not row then return end

    local productActionSuffix = " [" .. identity.displayName .. "]"
    local csiAction = row:sub(-#productActionSuffix) == productActionSuffix and row:sub(1, #row - #productActionSuffix) or nil
    if csiAction then
        local parts = action_line.Parse(binding.line)
        local hasBlink = false
        local hasInvertValue = false
        for _, param in ipairs(parts.params) do if param == "Blink" then hasBlink = true end end
        for _, param in ipairs(parts.params) do if param == "InvertValue" then hasInvertValue = true end end
        if parts.actionName ~= csiAction then
            parts.params = hasBlink and { "Blink" } or {}
            if hasInvertValue then parts.params[#parts.params + 1] = "InvertValue" end
        end
        parts.actionName = csiAction
        binding.line = action_line.Build(parts)
        binding.feedbackAvailable = nil
        M.RefreshBindingDerivedFields(binding, action_line)
        M.UpdateDirtyState(state)
        return
    end

    local commandId = row:match("%- ID%s+(%d+)")
    if commandId then
        local namedCommand = row:match("%- ID%s+%d+%s+%((_[^)]+)%)")
        local parts = action_line.Parse(binding.line)
        local hasBlink = false
        local hasInvertValue = false
        for _, param in ipairs(parts.params) do if param == "Blink" then hasBlink = true end end
        for _, param in ipairs(parts.params) do if param == "InvertValue" then hasInvertValue = true end end
        parts.actionName = "Reaper"
        parts.params = { namedCommand or commandId }
        if hasBlink then parts.params[#parts.params + 1] = "Blink" end
        if hasInvertValue then parts.params[#parts.params + 1] = "InvertValue" end
        binding.line = action_line.Build(parts)
        binding.feedbackAvailable = nil
        M.RefreshBindingDerivedFields(binding, action_line)
        M.UpdateDirtyState(state)
    end
end

function M.SetInputEvent(state, binding, inputEvent, action_line)
    binding.inputEvent = inputEvent or "Legacy"
    if binding.inputEvent ~= "Hold" and binding.inputEvent ~= "LongHold" then
        local parts = action_line.Parse(binding.line)
        parts.properties.HoldDelay = nil
        parts.properties.HoldRepeatInterval = nil
        binding.line = action_line.Build(parts)
    end
    M.UpdateDirtyState(state)
end

function M.GetZoneLinePreview(widgetName, binding, modifierFlags)
    if binding.modifierMode then
        local line = widgetName .. " Modifier " .. binding.line
        if binding.modifierMode ~= "Default" then line = line .. " Mode=" .. binding.modifierMode end
        return line
    end
    local selectors = {}
    for _, modifier in ipairs(modifierFlags) do if ((binding.mod or 0) & modifier.bit) ~= 0 then selectors[#selectors + 1] = "[" .. modifier.name .. "]" end end
    if binding.inputEvent and binding.inputEvent ~= "Legacy" then selectors[#selectors + 1] = "(" .. binding.inputEvent .. ")" end
    if binding.isIncrease then selectors[#selectors + 1] = "(Increase)" end
    if binding.isDecrease then selectors[#selectors + 1] = "(Decrease)" end
    local prefix = #selectors > 0 and table.concat(selectors, "+") .. "+" or ""
    return prefix .. widgetName .. " " .. binding.line
end

return M
