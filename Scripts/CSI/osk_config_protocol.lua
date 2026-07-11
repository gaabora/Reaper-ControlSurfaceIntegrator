local r = reaper

local M = {}

function M.PollResponse(data, key)
    if not r.HasExtState(data.EXT_SECTION, key) then return nil end
    local value = r.GetExtState(data.EXT_SECTION, key)
    r.DeleteExtState(data.EXT_SECTION, key, false)
    return value
end

function M.SetLocalStatus(state, outcome, operation, message)
    state.status = table.concat({ outcome or "", operation or "", message or "" }, " | ")
end

function M.ParseConfigStatus(rawStatus)
    local outcome, operation, surfaceName, widgetName, zoneName, message =
        tostring(rawStatus or ""):match("^([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|(.*)$")
    if not outcome then return nil end
    return {
        outcome = outcome,
        operation = operation,
        surfaceName = surfaceName,
        widgetName = widgetName,
        zoneName = zoneName,
        message = message,
    }
end

function M.SendConfigQuery(state, data, expectedSerialized, forceAccept)
    local payload = state.surfaceName .. "|" .. state.widgetName
    state.pendingOperation = "Query"
    state.queryExpectedSerialized = expectedSerialized
    state.forceAcceptQuery = forceAccept == true
    r.SetExtState(data.EXT_CMD_SECTION, "ConfigQuery", payload, false)
end

function M.SendApplyLive(state, data, model)
    local serialized = model.SerializeBindings(state.bindings)
    for _, binding in ipairs(state.bindings) do
        if tostring(binding.line or ""):find(";", 1, true) then
            M.SetLocalStatus(state, "ERR", "ApplyLive", "Semicolons are not supported in binding lines")
            return false
        end
    end
    if serialized:find("[\r\n]") then
        M.SetLocalStatus(state, "ERR", "ApplyLive", "Line breaks are not supported in bindings")
        return false
    end

    local payload = state.surfaceName .. "|" .. state.widgetName .. "|" .. serialized
    state.pendingOperation = "ApplyLive"
    state.pendingSerialized = serialized
    r.SetExtState(data.EXT_CMD_SECTION, "ConfigApplyLive", payload, false)
    return true
end

function M.SendSave(state, data, model)
    local payload = state.surfaceName .. "|" .. state.widgetName
    state.pendingOperation = "Save"
    state.pendingSerialized = model.SerializeBindings(state.bindings)
    r.SetExtState(data.EXT_CMD_SECTION, "ConfigSave", payload, false)
end

function M.RequestRevert(state, data)
    if state.surfaceName == "" or state.widgetName == "" then return end
    state.pendingOperation = "Revert"
    state.pendingSerialized = nil
    local payload = state.surfaceName .. "|" .. state.widgetName
    r.SetExtState(data.EXT_CMD_SECTION, "ConfigRevert", payload, false)
end

function M.RequestActionList(data)
    r.SetExtState(data.EXT_CMD_SECTION, "ActionListQuery", "", false)
end

function M.PollConfigResponses(state, data, model)
    if not state.isOpen then return end
    local surf = state.surfaceName
    local widget = state.widgetName
    if surf == "" or widget == "" then return end

    local result = M.PollResponse(data, "ConfigResult_" .. surf .. "_" .. widget)
    if result ~= nil then
        local parsedBindings = model.ParseBindingString(result, state.action_line)
        local currentSerialized = model.SerializeBindings(state.bindings)
        local shouldReplaceVisible = state.forceAcceptQuery
            or state.queryExpectedSerialized == nil
            or currentSerialized == state.queryExpectedSerialized
        model.AcceptBindings(state, parsedBindings, shouldReplaceVisible)
        state.queryExpectedSerialized = nil
        state.forceAcceptQuery = false
        state.searchSelected = 0
        model.ClampSelectedBindingIndex(state)
    end

    local zone = M.PollResponse(data, "ConfigZoneName_" .. surf .. "_" .. widget)
    if zone ~= nil then state.zoneName = zone end
    local path = M.PollResponse(data, "ConfigZonePath_" .. surf .. "_" .. widget)
    if path ~= nil then state.zoneFilePath = path end

    local rawStatus = M.PollResponse(data, "ConfigStatus_" .. surf .. "_" .. widget)
    if rawStatus ~= nil then
        local status = M.ParseConfigStatus(rawStatus)
        if status and status.surfaceName == state.surfaceName and status.widgetName == state.widgetName then
            M.SetLocalStatus(state, status.outcome, status.operation, status.message)
            local completedSerialized = state.pendingSerialized
            state.pendingOperation = nil
            state.pendingSerialized = nil

            if status.outcome == "OK" then
                if status.operation == "ApplyLive" then
                    state.hasLiveChanges = true
                    model.UpdateDirtyState(state)
                    if state.saveAfterApply then
                        state.saveAfterApply = false
                        M.SendSave(state, data, model)
                    else
                        M.SendConfigQuery(state, data, completedSerialized, false)
                    end
                elseif status.operation == "Save" then
                    state.hasLiveChanges = false
                    model.UpdateDirtyState(state)
                    M.SendConfigQuery(state, data, completedSerialized, false)
                elseif status.operation == "Revert" then
                    state.hasLiveChanges = false
                    model.UpdateDirtyState(state)
                    if state.isOpen then M.SendConfigQuery(state, data, nil, true) end
                end
            elseif status.operation == "ApplyLive" then
                state.saveAfterApply = false
            end
        end
    end

    local actions = M.PollResponse(data, "ActionList")
    if actions ~= nil then
        state.csiActions = model.ParseCsv(actions)
        model.RefreshSearchResults(state, r)
    end
end

return M
