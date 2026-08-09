local identity = require("product_identity")

local M = {}

M.SEARCH_MODE_ITEMS = "all\0csi\0reaper\0"
M.SEARCH_MODE_BY_INDEX = { "all", "csi", "reaper" }

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
    local commandToken = action_line.UnquoteValue and action_line.UnquoteValue(parts.params[1] or "") or parts.params[1]
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

function M.GetModifierLabel(binding, modifierFlags)
    local labels = {}
    if binding.hasHold then labels[#labels + 1] = "Hold" end
    if binding.hasDoublePress then labels[#labels + 1] = "DoublePress" end
    for _, modifier in ipairs(modifierFlags) do
        if ((binding.mod or 0) & modifier.bit) ~= 0 then
            labels[#labels + 1] = modifier.name
        end
    end
    if #labels == 0 then return "-" end
    return table.concat(labels, "+")
end

function M.GetOtherSummary(binding, action_line)
    local labels = {}
    if binding.isIncrease then labels[#labels + 1] = "Increase" end
    if binding.isDecrease then labels[#labels + 1] = "Decrease" end
    if binding.isValueInverted then labels[#labels + 1] = "Invert" end
    if binding.isFeedbackInverted then labels[#labels + 1] = "InvertFB" end

    local parts = action_line.Parse(binding.line)
    for _, param in ipairs(parts.params or {}) do
        labels[#labels + 1] = action_line.UnquoteValue(param)
    end
    for key, value in pairs(parts.properties or {}) do
        if key ~= "OSD" and key ~= "KeyLabel" then
            labels[#labels + 1] = key .. "=" .. tostring(value)
        end
    end
    table.sort(labels)
    return table.concat(labels, ", ")
end

function M.ParseBindingString(raw, action_line)
    local bindings = {}
    for entry in tostring(raw or ""):gmatch("[^;]+") do
        local modStr, line = entry:match("^(%-?%d+):(.+)$")
        if modStr and line then
            local metadata = {
                hasHold = false,
                hasDoublePress = false,
                isValueInverted = false,
                isFeedbackInverted = false,
                isIncrease = false,
                isDecrease = false,
            }
            while true do
                local token = line:match("%s+(__OSK_[A-Z_]+)$")
                if not token then break end
                line = line:sub(1, #line - #token - 1)
                if token == "__OSK_HOLD" then metadata.hasHold = true
                elseif token == "__OSK_DOUBLE_PRESS" then metadata.hasDoublePress = true
                elseif token == "__OSK_INVERT" then metadata.isValueInverted = true
                elseif token == "__OSK_INVERT_FB" then metadata.isFeedbackInverted = true
                elseif token == "__OSK_INCREASE" then metadata.isIncrease = true
                elseif token == "__OSK_DECREASE" then metadata.isDecrease = true
                end
            end
            local parts = action_line.Parse(line)
            bindings[#bindings + 1] = {
                mod = tonumber(modStr) or 0,
                line = line,
                actionName = parts.actionName or "",
                hasHold = metadata.hasHold,
                hasDoublePress = metadata.hasDoublePress,
                isValueInverted = metadata.isValueInverted,
                isFeedbackInverted = metadata.isFeedbackInverted,
                isIncrease = metadata.isIncrease,
                isDecrease = metadata.isDecrease,
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

function M.RefreshSearchResults(state, r)
    local query = state.searchQuery
    local results = {}

    if state.searchMode ~= "reaper" then
        for _, name in ipairs(state.csiActions) do
            if M.MatchesSearchTerms(name, query) then
                results[#results + 1] = "[" .. identity.displayName .. "] " .. name
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
                results[#results + 1] = string.format("[REAPER] %s - %s", idText, actionName)
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
            if binding.hasHold then line = line .. " __OSK_HOLD" end
            if binding.hasDoublePress then line = line .. " __OSK_DOUBLE_PRESS" end
            if binding.isValueInverted then line = line .. " __OSK_INVERT" end
            if binding.isFeedbackInverted then line = line .. " __OSK_INVERT_FB" end
            if binding.isIncrease then line = line .. " __OSK_INCREASE" end
            if binding.isDecrease then line = line .. " __OSK_DECREASE" end
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
            hasHold = binding.hasHold == true,
            hasDoublePress = binding.hasDoublePress == true,
            isValueInverted = binding.isValueInverted == true,
            isFeedbackInverted = binding.isFeedbackInverted == true,
            isIncrease = binding.isIncrease == true,
            isDecrease = binding.isDecrease == true,
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

function M.DuplicateSelectedBinding(state)
    local selected = M.GetSelectedBinding(state)
    if not selected then return end
    local copy = M.CloneBindings({ selected })[1]
    table.insert(state.bindings, state.selectedBinding + 1, copy)
    state.selectedBinding = state.selectedBinding + 1
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

    local productActionPrefix = "[" .. identity.displayName .. "] "
    local csiAction = row:sub(1, #productActionPrefix) == productActionPrefix and row:sub(#productActionPrefix + 1) or nil
    if csiAction then
        binding.line = csiAction
        M.RefreshBindingDerivedFields(binding, action_line)
        M.UpdateDirtyState(state)
        return
    end

    local commandId = row:match("^%[REAPER%]%s+(%d+)")
    if commandId then
        local namedCommand = row:match("^%[REAPER%]%s+%d+%s+%((_[^)]+)%)")
        binding.line = "Reaper " .. (namedCommand or commandId)
        M.RefreshBindingDerivedFields(binding, action_line)
        M.UpdateDirtyState(state)
    end
end

function M.SetHoldEnabled(state, binding, enabled, action_line)
    binding.hasHold = enabled
    if not enabled then
        local parts = action_line.Parse(binding.line)
        parts.properties.HoldDelay = nil
        parts.properties.HoldRepeatInterval = nil
        binding.line = action_line.Build(parts)
    end
    M.UpdateDirtyState(state)
end

return M
