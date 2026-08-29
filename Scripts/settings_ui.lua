local imgui = require "imgui" "0.9.3"

local protocol = require("settings_protocol")
local schemaLoader = require("settings_schema")
local theme = require("theme_settings")
local ui = require("ui_components")

local module = {}

local schema = schemaLoader.Load()
local state = {
    draftExplicit = {},
    draftValues = {},
    deviceId = "",
    deviceOptions = {},
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
    scope = "Product",
}

local SCOPE_ITEMS = {
    { label = "Product", value = "Product" },
    { label = "Device", value = "Device" },
}
local SETTING_LABELS = {
    DefaultModifierMode = "Default modifier mode",
    DefaultPseudoModifierMode = "Default pseudo-modifier mode",
    DefaultButtonTrigger = "Default button trigger",
    DoublePressPolicy = "Double-press policy",
    HoldDelayMs = "Hold delay",
    LongHoldDelayMs = "Long-hold delay",
    DoublePressWindowMs = "Double-press window",
    ModifierTapWindowMs = "Modifier tap window",
    HoldRepeatIntervalMs = "Hold repeat interval",
}

local function findDeviceOptionIdx(deviceId)
    for optionIdx, option in ipairs(state.deviceOptions) do if option == deviceId then return optionIdx end end
    return nil
end

local function selectDeviceOption(optionIdx)
    local option = state.deviceOptions[optionIdx]
    if not option then return false end
    state.deviceId = option
    return true
end

local function buildDeviceItems()
    local items = {}
    for optionIdx, option in ipairs(state.deviceOptions) do items[#items + 1] = { label = option, value = optionIdx } end
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
    if state.scope == "Device" and state.deviceId == "" then
        state.error = "No configured Device is selected"
        state.loaded = false
        return false
    end
    local deviceId = state.scope == "Device" and state.deviceId or nil
    return startRequest("Query", protocol.Query(state.scope, deviceId))
end

local function loadResponse(response)
    state.loaded = true
    state.deviceOptions = response.deviceOptions or {}
    if response.scope == "Device" then state.deviceId = response.deviceId end
    state.originalExplicit = {}
    state.originalValues = {}
    state.inheritedValues = {}
    state.draftExplicit = {}
    state.draftValues = {}
    for settingIndex, definition in ipairs(schema.settings) do
        local settingName = definition.name
        local value = response.values[settingName]
        if value ~= nil then
            if definition.type == "integer" then value = tonumber(value) or definition.defaultValue end
            state.originalValues[settingName] = value
            state.draftValues[settingName] = value
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
        if definition.category ~= "Logging" and definitionAllowsScope(definition, state.scope) and state.draftValues[definition.name] ~= nil then
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
        if definition.category ~= "Logging" and definitionAllowsScope(definition, state.scope) then
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
    local changed = false
    local value = state.draftValues[settingName]
    local controlId = "##General_" .. settingName
    if definition.type == "enum" then
        local items = {}
        for enumIndex, enumValue in ipairs(definition.enumValues or {}) do items[#items + 1] = { label = enumValue, value = enumValue } end
        changed, value = ui.ComboEnum(ctx, controlId, value, items)
    else
        local step = definition.unit == "Milliseconds" and 10 or 1
        local format = definition.unit == "Milliseconds" and "%d ms" or "%d"
        changed, value = ui.DragInteger(ctx, controlId, value or definition.defaultValue, definition.min, definition.max, step, { format = format })
    end
    if changed then
        state.draftExplicit[settingName] = true
        state.draftValues[settingName] = value
    end
    local tooltip = state.scope == "Device" and (state.draftExplicit[settingName] and "Device value. Right-click to use the Product value." or "Using the Product value.") or (state.draftExplicit[settingName] and "Product value. Right-click to reset to the default." or "Using the default value.")
    local resetLabel = state.draftExplicit[settingName] and (state.scope == "Device" and "Use Product value" or "Reset to default") or nil
    ui.ValueSourceActions(ctx, tooltip, resetLabel, function()
        state.draftExplicit[settingName] = false
        state.draftValues[settingName] = state.inheritedValues[settingName]
    end)
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    queryCurrentScope()
end

function module.SetContext(deviceId)
    if module.IsDirty() or state.pendingKind == "Apply" or state.pendingKind == "Reload" then return false, "Save or Revert the current General draft first" end
    if state.requestId then
        protocol.Cancel(state.requestId)
        state.pendingKind = ""
        state.requestId = nil
    end
    state.initialized = true
    clearLoadedState()
    state.scope = deviceId and deviceId ~= "" and "Device" or "Product"
    state.deviceId = deviceId or ""
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

function module.Save()
    local valid, validationError = validateDraft()
    if not valid then
        state.error = validationError
        return false, validationError
    end
    local changes = buildChanges()
    if not hasChanges(changes) then return true end
    local deviceId = state.scope == "Device" and state.deviceId or nil
    return startRequest("Apply", protocol.Apply(state.scope, changes, deviceId))
end

function module.Revert()
    if state.requestId then return false, "Wait for the current settings request" end
    restoreDraft()
    return true
end

function module.Shutdown()
    if not state.requestId then return end
    protocol.Cancel(state.requestId)
    state.pendingKind = ""
    state.requestId = nil
end

local function renderSectionHeader(ctx, label, fonts)
    if fonts and fonts.section then imgui.PushFont(ctx, fonts.section) end
    imgui.Text(ctx, label)
    if fonts and fonts.section then imgui.PopFont(ctx) end
end

local function renderContextSelectors(ctx)
    module.Initialize()
    if imgui.BeginTable(ctx, "##GeneralContext", 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "Label", imgui.TableColumnFlags_WidthStretch)
        imgui.TableSetupColumn(ctx, "Value", imgui.TableColumnFlags_WidthFixed, theme.FORM.control_width)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        imgui.AlignTextToFramePadding(ctx)
        imgui.Text(ctx, "Scope")
        imgui.TableSetColumnIndex(ctx, 1)
        local scopeChanged = false
        ui.Disabled(ctx, state.requestId ~= nil or module.IsDirty(), function()
            scopeChanged, state.scope = ui.ComboEnum(ctx, "##GeneralScope", state.scope, SCOPE_ITEMS)
        end)
        if scopeChanged then
            clearLoadedState()
            if state.scope == "Device" and not findDeviceOptionIdx(state.deviceId) and not selectDeviceOption(1) then
                state.deviceId = ""
            end
            queryCurrentScope()
        end
        if state.scope == "Device" then
            imgui.TableNextRow(ctx)
            imgui.TableSetColumnIndex(ctx, 0)
            imgui.AlignTextToFramePadding(ctx)
            imgui.Text(ctx, "Device")
            imgui.TableSetColumnIndex(ctx, 1)
            local deviceItems = buildDeviceItems()
            if #deviceItems == 0 then
                imgui.TextDisabled(ctx, "No configured Devices")
            else
                local selectedOptionIdx = findDeviceOptionIdx(state.deviceId) or 1
                local deviceChanged
                ui.Disabled(ctx, state.requestId ~= nil or module.IsDirty(), function()
                    deviceChanged, selectedOptionIdx = ui.ComboEnum(ctx, "##GeneralDevice", selectedOptionIdx, deviceItems)
                end)
                if deviceChanged and selectDeviceOption(selectedOptionIdx) then
                    clearLoadedState()
                    queryCurrentScope()
                end
            end
        end
        imgui.EndTable(ctx)
    end
end

local function renderCategory(ctx, category, fonts)
    renderSectionHeader(ctx, category, fonts)
    if not imgui.BeginTable(ctx, "##GeneralCategory_" .. category, 2, 0, -1, 0) then return end
    imgui.TableSetupColumn(ctx, "Label", imgui.TableColumnFlags_WidthStretch)
    imgui.TableSetupColumn(ctx, "Value", imgui.TableColumnFlags_WidthFixed, theme.FORM.control_width)
    for settingIndex, definition in ipairs(schema.settings) do
        if definition.category == category and definitionAllowsScope(definition, state.scope) and state.draftValues[definition.name] ~= nil then
            imgui.TableNextRow(ctx)
            imgui.TableSetColumnIndex(ctx, 0)
            imgui.AlignTextToFramePadding(ctx)
            imgui.Text(ctx, SETTING_LABELS[definition.name] or definition.name)
            imgui.TableSetColumnIndex(ctx, 1)
            renderSetting(ctx, definition)
        end
    end
    imgui.EndTable(ctx)
end

function module.RenderPage(ctx, fonts)
    module.Initialize()
    if imgui.BeginTable(ctx, "##GeneralColumns", 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "Left", imgui.TableColumnFlags_WidthStretch)
        imgui.TableSetupColumn(ctx, "Right", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        renderContextSelectors(ctx)
        imgui.Spacing(ctx)
        renderCategory(ctx, "Behavior", fonts)
        imgui.Spacing(ctx)
        renderCategory(ctx, "Timing", fonts)
        imgui.EndTable(ctx)
    end
    if state.requestId then imgui.TextDisabled(ctx, state.pendingKind .. "...") end
    if state.error ~= "" then imgui.TextWrapped(ctx, "Error: " .. state.error) end
    local valid, validationError = validateDraft()
    if not valid then imgui.TextWrapped(ctx, "Error: " .. validationError) end
end

return module
