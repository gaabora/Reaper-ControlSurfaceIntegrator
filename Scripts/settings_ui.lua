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
    message = "",
    open = false,
    originalExplicit = {},
    originalValues = {},
    pageName = "",
    pendingKind = "",
    requestId = nil,
    scope = "Surface",
    sources = {},
    surfaceName = "",
}

local SCOPE_ITEMS = {
    { label = "Product", value = "Product" },
    { label = "Surface", value = "Surface" },
}

local function definitionAllowsScope(definition, scope)
    for _, allowedScope in ipairs(definition.scopes or {}) do if allowedScope == scope then return true end end
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
    return true
end

local function queryCurrentScope()
    local surfaceName = state.scope == "Surface" and state.surfaceName or nil
    local pageName = state.scope == "Surface" and state.pageName or nil
    return startRequest("Query", protocol.Query(state.scope, surfaceName, pageName))
end

local function loadResponse(response)
    state.pageName = response.pageName or state.pageName
    state.originalExplicit = {}
    state.originalValues = {}
    state.inheritedValues = {}
    state.draftExplicit = {}
    state.draftValues = {}
    state.sources = {}
    for _, definition in ipairs(schema.settings) do
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

local function pollRequest()
    if not state.requestId then return end
    local response, responseError = protocol.Poll(state.requestId)
    if not response and not responseError then return end
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
    for _, definition in ipairs(schema.settings) do
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
    for _, definition in ipairs(schema.settings) do
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
            for _, enumValue in ipairs(definition.enumValues or {}) do items[#items + 1] = { label = enumValue, value = enumValue } end
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

function module.Open(surfaceName, pageName)
    state.open = true
    state.scope = "Surface"
    state.surfaceName = surfaceName or ""
    state.pageName = pageName or ""
    queryCurrentScope()
end

function module.IsOpen()
    return state.open
end

function module.Render(ctx)
    pollRequest()
    if not state.open then return end
    imgui.SetNextWindowSize(ctx, 620, 560, imgui.Cond_FirstUseEver)
    local visible, open = imgui.Begin(ctx, "Input settings", state.open)
    state.open = open
    if visible then
        local scopeChanged = false
        ui.Disabled(ctx, state.requestId ~= nil, function()
            scopeChanged, state.scope = ui.ComboEnum(ctx, "Scope", state.scope, SCOPE_ITEMS)
        end)
        if scopeChanged then
            state.pageName = ""
            queryCurrentScope()
        end
        if state.scope == "Surface" then
            imgui.Text(ctx, "Surface: " .. state.surfaceName)
            if state.pageName ~= "" then imgui.TextDisabled(ctx, "Page: " .. state.pageName) end
        end

        if state.requestId then imgui.TextDisabled(ctx, state.pendingKind .. "...") end
        if state.error ~= "" then imgui.TextWrapped(ctx, "Error: " .. state.error) end
        if state.message ~= "" then imgui.TextWrapped(ctx, state.message) end
        imgui.Separator(ctx)

        local currentCategory = ""
        for _, definition in ipairs(schema.settings) do
            if definitionAllowsScope(definition, state.scope) and state.draftValues[definition.name] ~= nil then
                if definition.category ~= currentCategory then
                    currentCategory = definition.category
                    imgui.Separator(ctx)
                    imgui.Text(ctx, currentCategory)
                end
                renderSetting(ctx, definition)
            end
        end

        local changes = buildChanges()
        local valid, validationError = validateDraft()
        if not valid then imgui.TextWrapped(ctx, "Error: " .. validationError) end
        imgui.Separator(ctx)
        ui.Disabled(ctx, state.requestId ~= nil or not valid or not hasChanges(changes), function()
            if imgui.Button(ctx, "Apply") then
                local surfaceName = state.scope == "Surface" and state.surfaceName or nil
                local pageName = state.scope == "Surface" and state.pageName or nil
                startRequest("Apply", protocol.Apply(state.scope, changes, surfaceName, pageName))
            end
        end)
        imgui.SameLine(ctx)
        ui.Disabled(ctx, state.requestId ~= nil, function()
            if imgui.Button(ctx, "Revert") then queryCurrentScope() end
        end)
        imgui.SameLine(ctx)
        ui.Disabled(ctx, state.requestId ~= nil, function()
            if imgui.Button(ctx, "Reload from file") then startRequest("Reload", protocol.Reload()) end
        end)
    end
    imgui.End(ctx)
end

return module
