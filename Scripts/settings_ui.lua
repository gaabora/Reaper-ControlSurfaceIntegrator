local imgui = require "imgui" "0.9.3"

local protocol = require("settings_protocol")
local schemaLoader = require("settings_schema")
local ui = require("ui_components")

local module = {}

local schema = schemaLoader.Load()
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
    pageName = "",
    pendingKind = "",
    requestId = nil,
    requestStartedAt = 0,
    scope = "Product",
    sources = {},
    surfaceName = "",
    surfaceOptions = {},
}

local SCOPE_ITEMS = {
    { label = "Product", value = "Product" },
    { label = "Surface", value = "Surface" },
}

local function findSurfaceOptionIdx(pageName, surfaceName)
    for optionIdx, option in ipairs(state.surfaceOptions) do
        if option.pageName == pageName and option.surfaceName == surfaceName then return optionIdx end
    end
    return nil
end

local function selectSurfaceOption(optionIdx)
    local option = state.surfaceOptions[optionIdx]
    if not option then return false end
    state.pageName = option.pageName
    state.surfaceName = option.surfaceName
    return true
end

local function buildSurfaceItems()
    local items = {}
    for optionIdx, option in ipairs(state.surfaceOptions) do items[#items + 1] = { label = option.pageName .. " / " .. option.surfaceName, value = optionIdx } end
    return items
end

local function definitionAllowsScope(definition, scope)
    for scopeIndex, allowedScope in ipairs(definition.scopes or {}) do if allowedScope == scope then return true end end
    return false
end

local function startRequest(kind, requestId, requestError)
    if not requestId then
        state.error = requestError or "Cannot send settings request"
        return false
    end
    state.error = ""
    state.message = ""
    state.pendingKind = kind
    state.requestId = requestId
    state.requestStartedAt = reaper.time_precise()
    return true
end

local function queryCurrentScope()
    if state.scope == "Surface" and state.surfaceName == "" then
        state.error = "No configured Surface is selected"
        state.loaded = false
        return false
    end
    local surfaceName = state.scope == "Surface" and state.surfaceName or nil
    local pageName = state.scope == "Surface" and state.pageName or nil
    return startRequest("Query", protocol.Query(state.scope, surfaceName, pageName))
end

local function loadResponse(response)
    state.loaded = true
    state.surfaceOptions = response.surfaceOptions or {}
    if response.scope == "Surface" then
        state.pageName = response.pageName
        state.surfaceName = response.surfaceName
    end
    state.originalExplicit = {}
    state.originalValues = {}
    state.inheritedValues = {}
    state.draftExplicit = {}
    state.draftValues = {}
    state.sources = {}
    for settingIndex, definition in ipairs(schema.settings) do
        local settingName = definition.name
        local value = response.values[settingName]
        if value ~= nil then
            if definition.type == "integer" then value = tonumber(value) or definition.defaultValue end
            state.originalValues[settingName] = value
            state.draftValues[settingName] = value
            state.sources[settingName] = response.sources[settingName]
            local inheritedValue = response.inheritedValues[settingName]
            if definition.type == "integer" then inheritedValue = tonumber(inheritedValue) or definition.defaultValue end
            state.inheritedValues[settingName] = inheritedValue
            local explicitSource = response.sources[settingName] == state.scope
            state.originalExplicit[settingName] = explicitSource
            state.draftExplicit[settingName] = explicitSource
        end
    end
end

local function restoreDraft()
    state.draftExplicit = {}
    state.draftValues = {}
    for settingName, value in pairs(state.originalValues) do state.draftValues[settingName] = value end
    for settingName, explicit in pairs(state.originalExplicit) do state.draftExplicit[settingName] = explicit end
    state.error = ""
    state.message = ""
end

local function clearLoadedState()
    state.draftExplicit = {}
    state.draftValues = {}
    state.inheritedValues = {}
    state.loaded = false
    state.originalExplicit = {}
    state.originalValues = {}
    state.sources = {}
end

local function pollRequest()
    if not state.requestId then return end
    local response, responseError = protocol.Poll(state.requestId)
    if not response and not responseError then
        if reaper.time_precise() - state.requestStartedAt >= 3 then
            protocol.Cancel(state.requestId)
            state.error = "No active C++ settings response"
            state.pendingKind = ""
            state.requestId = nil
        end
        return
    end
    local completedKind = state.pendingKind
    state.requestId = nil
    state.pendingKind = ""
    if not response then
        state.error = responseError or "Invalid settings response"
        return
    end
    if not response.ok then
        state.error = response.message ~= "" and response.message or "Settings operation failed"
        return
    end
    state.error = ""
    state.message = response.message or ""
    if completedKind == "Query" then loadResponse(response)
    else queryCurrentScope() end
end

local function validateDraft()
    for settingIndex, definition in ipairs(schema.settings) do
        if definitionAllowsScope(definition, state.scope) and state.draftValues[definition.name] ~= nil then
            local value = state.draftValues[definition.name]
            if definition.type == "integer" then
                value = tonumber(value)
                if not value or value < definition.min or value > definition.max then return false, definition.name .. " must be from " .. definition.min .. " to " .. definition.max end
                if definition.greaterThan then
                    local referencedValue = tonumber(state.draftValues[definition.greaterThan])
                    if not referencedValue or value <= referencedValue then return false, definition.name .. " must be greater than " .. definition.greaterThan end
                end
            end
        end
    end
    return true
end

local function buildChanges()
    local changes = {}
    for settingIndex, definition in ipairs(schema.settings) do
        if definitionAllowsScope(definition, state.scope) then
            local settingName = definition.name
            local originalExplicit = state.originalExplicit[settingName] == true
            local draftExplicit = state.draftExplicit[settingName] == true
            if originalExplicit and not draftExplicit then
                changes[settingName] = { unset = true }
            elseif draftExplicit and (not originalExplicit or state.draftValues[settingName] ~= state.originalValues[settingName]) then
                local value = state.draftValues[settingName]
                if definition.type == "integer" then value = math.floor(tonumber(value) or 0) end
                changes[settingName] = { value = value }
            end
        end
    end
    return changes
end

local function hasChanges(changes)
    return next(changes) ~= nil
end

local function renderSetting(ctx, definition)
    local settingName = definition.name
    local explicit = state.draftExplicit[settingName] == true
    local changed
    changed, explicit = imgui.Checkbox(ctx, "Override##" .. settingName, explicit)
    if changed then
        state.draftExplicit[settingName] = explicit
        if not explicit then state.draftValues[settingName] = state.inheritedValues[settingName] end
    end
    imgui.SameLine(ctx)

    ui.Disabled(ctx, not explicit, function()
        if definition.type == "enum" then
            local items = {}
            for enumIndex, enumValue in ipairs(definition.enumValues or {}) do items[#items + 1] = { label = enumValue, value = enumValue } end
            local enumChanged, value = ui.ComboEnum(ctx, settingName, state.draftValues[settingName], items)
            if enumChanged then state.draftValues[settingName] = value end
        else
            local integerChanged, value = ui.SliderWithInput(ctx, settingName, tonumber(state.draftValues[settingName]) or definition.defaultValue, definition.min, definition.max, 1, { inputChars = 6, inputWidth = 65, sliderWidth = 180 })
            if integerChanged then state.draftValues[settingName] = math.floor(value + 0.5) end
        end
    end)

    if not explicit then
        imgui.SameLine(ctx)
        local inheritedSource = state.sources[settingName] or (state.scope == "Surface" and "Product" or "Compiled")
        imgui.TextDisabled(ctx, "Inherited from " .. inheritedSource)
    end
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    queryCurrentScope()
end

function module.SetContext(surfaceName, pageName)
    if module.IsDirty() or state.pendingKind == "Apply" or state.pendingKind == "Reload" then return false, "Save or Revert the current General draft first" end
    if state.requestId then
        protocol.Cancel(state.requestId)
        state.pendingKind = ""
        state.requestId = nil
    end
    state.initialized = true
    clearLoadedState()
    state.scope = surfaceName and surfaceName ~= "" and "Surface" or "Product"
    state.surfaceName = surfaceName or ""
    state.pageName = pageName or ""
    queryCurrentScope()
    return true
end

function module.Update()
    pollRequest()
end

function module.IsDirty()
    return hasChanges(buildChanges())
end

function module.Validate()
    return validateDraft()
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

function module.GetConfigurationStatus()
    if state.requestId and not state.loaded then return "Checking configuration...", "" end
    if state.error ~= "" and not state.loaded then return "Configuration status unavailable", state.error end
    return state.loaded and "Configuration is active" or "Configuration status unavailable", ""
end

function module.Save()
    local valid, validationError = validateDraft()
    if not valid then
        state.error = validationError
        return false, validationError
    end
    local changes = buildChanges()
    if not hasChanges(changes) then return true end
    local surfaceName = state.scope == "Surface" and state.surfaceName or nil
    local pageName = state.scope == "Surface" and state.pageName or nil
    return startRequest("Apply", protocol.Apply(state.scope, changes, surfaceName, pageName))
end

function module.Revert()
    if state.requestId then return false, "Wait for the current settings request" end
    restoreDraft()
    return true
end

function module.Reload()
    if state.requestId then return false, "Wait for the current settings request" end
    return startRequest("Reload", protocol.Reload())
end

function module.Refresh()
    if state.requestId then return false, "Wait for the current settings request" end
    if module.IsDirty() then return false, "Save or Revert the current General draft first" end
    return queryCurrentScope()
end

function module.Shutdown()
    if not state.requestId then return end
    protocol.Cancel(state.requestId)
    state.pendingKind = ""
    state.requestId = nil
end

function module.RenderPage(ctx)
    module.Initialize()
    local scopeChanged = false
    ui.Disabled(ctx, state.requestId ~= nil or module.IsDirty(), function()
        scopeChanged, state.scope = ui.ComboEnum(ctx, "Scope", state.scope, SCOPE_ITEMS)
    end)
    if scopeChanged then
        clearLoadedState()
        if state.scope == "Surface" and not findSurfaceOptionIdx(state.pageName, state.surfaceName) and not selectSurfaceOption(1) then
            state.pageName = ""
            state.surfaceName = ""
        end
        queryCurrentScope()
    end
    if state.scope == "Surface" then
        local surfaceItems = buildSurfaceItems()
        if #surfaceItems == 0 then
            imgui.TextDisabled(ctx, "No configured Surfaces are available")
        else
            local selectedOptionIdx = findSurfaceOptionIdx(state.pageName, state.surfaceName) or 1
            local surfaceChanged
            ui.Disabled(ctx, state.requestId ~= nil or module.IsDirty(), function()
                surfaceChanged, selectedOptionIdx = ui.ComboEnum(ctx, "Surface", selectedOptionIdx, surfaceItems)
            end)
            if surfaceChanged and selectSurfaceOption(selectedOptionIdx) then
                clearLoadedState()
                queryCurrentScope()
            end
        end
    end

    imgui.SameLine(ctx)
    ui.Disabled(ctx, state.requestId ~= nil or module.IsDirty(), function()
        if imgui.Button(ctx, "Reload from file") then module.Reload() end
    end)

    if state.requestId then imgui.TextDisabled(ctx, state.pendingKind .. "...") end
    if state.error ~= "" then imgui.TextWrapped(ctx, "Error: " .. state.error) end
    imgui.Separator(ctx)

    local currentCategory = ""
    for settingIndex, definition in ipairs(schema.settings) do
        if definitionAllowsScope(definition, state.scope) and state.draftValues[definition.name] ~= nil then
            if definition.category ~= currentCategory then
                currentCategory = definition.category
                imgui.Separator(ctx)
                imgui.Text(ctx, currentCategory)
            end
            renderSetting(ctx, definition)
        end
    end

    local valid, validationError = validateDraft()
    if not valid then imgui.TextWrapped(ctx, "Error: " .. validationError) end
end

return module
