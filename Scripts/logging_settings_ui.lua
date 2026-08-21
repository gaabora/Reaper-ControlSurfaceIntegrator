local imgui = require "imgui" "0.9.3"

local protocol = require("settings_protocol")
local schemaLoader = require("settings_schema")
local ui = require("ui_components")

local module = {}
local schema = schemaLoader.Load()
local definitions = {}
local labels = {
    DebugLevel = "Log level",
    SurfaceRawInDisplay = "Raw MIDI input",
    SurfaceInDisplay = "Surface input",
    SurfaceOutDisplay = "Surface output",
}
local descriptions = {
    DebugLevel = "Write this severity and all more important severities.",
    SurfaceRawInDisplay = "Write each raw three-byte MIDI input message.",
    SurfaceInDisplay = "Write processed MIDI widget and OSC input values.",
    SurfaceOutDisplay = "Write MIDI and OSC output messages.",
}
for definitionIdx, definition in ipairs(schema.settings) do if definition.category == "Logging" then definitions[#definitions + 1] = definition end end

local state = {
    draftExplicit = {},
    draftValues = {},
    error = "",
    inheritedValues = {},
    initialized = false,
    loaded = false,
    message = "",
    originalExplicit = {},
    originalValues = {},
    pendingKind = "",
    requestId = nil,
    requestStartedAt = 0,
    retryAt = 0,
    retryQuery = false,
}

local function startRequest(kind, requestId, requestError)
    if not requestId then
        state.retryQuery = kind == "Query" and requestError == "Another settings request is pending"
        state.retryAt = reaper.time_precise() + 0.25
        state.error = state.retryQuery and "Waiting for the current settings operation..." or tostring(requestError or "Cannot send Logging settings request")
        return false
    end
    state.error = ""
    state.message = ""
    state.pendingKind = kind
    state.requestId = requestId
    state.requestStartedAt = reaper.time_precise()
    state.retryQuery = false
    return true
end

local function queryCurrentValues()
    local requestId, requestError = protocol.Query("Product")
    return startRequest("Query", requestId, requestError)
end

local function loadResponse(response)
    state.loaded = true
    state.originalExplicit = {}
    state.originalValues = {}
    state.inheritedValues = {}
    state.draftExplicit = {}
    state.draftValues = {}
    for definitionIdx, definition in ipairs(definitions) do
        local settingName = definition.name
        local value = response.values[settingName]
        if value ~= nil then
            state.originalValues[settingName] = value
            state.draftValues[settingName] = value
            state.inheritedValues[settingName] = response.inheritedValues[settingName] or definition.defaultValue
            local explicit = response.sources[settingName] == "Product"
            state.originalExplicit[settingName] = explicit
            state.draftExplicit[settingName] = explicit
        end
    end
end

local function buildChanges()
    local changes = {}
    for definitionIdx, definition in ipairs(definitions) do
        local settingName = definition.name
        local originalExplicit = state.originalExplicit[settingName] == true
        local draftExplicit = state.draftExplicit[settingName] == true
        if originalExplicit and not draftExplicit then
            changes[settingName] = { unset = true }
        elseif draftExplicit and (not originalExplicit or state.draftValues[settingName] ~= state.originalValues[settingName]) then
            changes[settingName] = { value = state.draftValues[settingName] }
        end
    end
    return changes
end

local function hasChanges(changes)
    return next(changes) ~= nil
end

local function pollRequest()
    if not state.requestId then return end
    local response, responseError = protocol.Poll(state.requestId)
    if not response and not responseError then
        if reaper.time_precise() - state.requestStartedAt >= 3 then
            protocol.Cancel(state.requestId)
            state.error = "No active C++ Logging settings response"
            state.pendingKind = ""
            state.requestId = nil
        end
        return
    end
    local completedKind = state.pendingKind
    state.requestId = nil
    state.pendingKind = ""
    if not response then
        state.error = responseError or "Invalid Logging settings response"
        return
    end
    if not response.ok then
        state.error = response.message ~= "" and response.message or "Logging settings operation failed"
        return
    end
    state.error = ""
    state.message = response.message or ""
    if completedKind == "Query" then loadResponse(response) else queryCurrentValues() end
end

local function renderDefinition(ctx, definition)
    local settingName = definition.name
    local label = labels[settingName] or settingName
    local changed = false
    local value = state.draftValues[settingName]
    if definition.type == "enum" then
        local items = {}
        for enumIdx, enumValue in ipairs(definition.enumValues or {}) do items[#items + 1] = { label = enumValue, value = enumValue } end
        changed, value = ui.ComboEnum(ctx, label .. "##Logging_" .. settingName, value, items)
    else
        local checked = value == "1"
        changed, checked = imgui.Checkbox(ctx, label .. "##Logging_" .. settingName, checked)
        value = checked and "1" or "0"
    end
    if changed then
        state.draftExplicit[settingName] = true
        state.draftValues[settingName] = value
    end
    local sourceText = state.draftExplicit[settingName] and "Product value. Right-click to reset to the default." or "Using the default value."
    local description = descriptions[settingName] and descriptions[settingName] .. "\n\n" or ""
    local resetLabel = state.draftExplicit[settingName] and "Reset to default" or nil
    ui.ValueSourceActions(ctx, description .. sourceText, resetLabel, function()
        state.draftExplicit[settingName] = false
        state.draftValues[settingName] = state.inheritedValues[settingName]
    end)
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    queryCurrentValues()
end

function module.Update()
    if not state.initialized then return end
    pollRequest()
    if state.retryQuery and not state.requestId and reaper.time_precise() >= state.retryAt then queryCurrentValues() end
end

function module.Render(ctx)
    module.Initialize()
    if state.requestId and not state.loaded then imgui.TextDisabled(ctx, state.pendingKind .. "...") end
    if state.error ~= "" then imgui.TextWrapped(ctx, "Error: " .. state.error) end
    if not state.loaded then return false end
    for definitionIdx, definition in ipairs(definitions) do
        if definitionIdx > 1 then imgui.SameLine(ctx) end
        renderDefinition(ctx, definition)
    end
    return true
end

function module.IsDirty()
    return hasChanges(buildChanges())
end

function module.IsBusy()
    return state.requestId ~= nil
end

function module.HasError()
    return state.error ~= ""
end

function module.GetStatus()
    if state.requestId then return state.pendingKind .. "..." end
    if state.error ~= "" then return state.error end
    return state.message
end

function module.Validate()
    return true
end

function module.Save()
    if state.requestId then return false, "Wait for the current Logging settings request" end
    local changes = buildChanges()
    if not hasChanges(changes) then return true end
    local requestId, requestError = protocol.Apply("Product", changes)
    return startRequest("Apply", requestId, requestError)
end

function module.Revert()
    if state.requestId then return false, "Wait for the current Logging settings request" end
    state.draftExplicit = {}
    state.draftValues = {}
    for settingName, value in pairs(state.originalValues) do state.draftValues[settingName] = value end
    for settingName, explicit in pairs(state.originalExplicit) do state.draftExplicit[settingName] = explicit end
    state.error = ""
    state.message = ""
    return true
end

function module.Shutdown()
    if not state.requestId then return end
    protocol.Cancel(state.requestId)
    state.pendingKind = ""
    state.requestId = nil
end

return module
