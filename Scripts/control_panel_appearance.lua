local imgui = require "imgui" "0.9.3"

local settingsStore = require("settings_store")
local theme = require("theme_settings")
local ui = require("ui_components")

local module = {}

local state = {
    error = "",
    initialized = false,
    knownRevision = 0,
    message = "",
    saved = {},
    revisionConflict = false,
}

local groups = {
    {
        id = "Common",
        label = "Common interface",
        target = theme.common,
        schema = theme.COMMON_SCHEMA,
        order = theme.COMMON_ORDER,
        load = theme.LoadCommonSettings,
        save = theme.SaveCommonSettings,
    },
    {
        id = "OSK",
        label = "OSK",
        target = theme.osk,
        schema = theme.OSK_SCHEMA,
        order = theme.OSK_ORDER,
        load = theme.LoadOskSettings,
        save = theme.SaveOskSettings,
    },
    {
        id = "OSD",
        label = "OSD",
        target = theme.osd,
        schema = theme.OSD_SCHEMA,
        order = theme.OSD_ORDER,
        load = theme.LoadOsdSettings,
        save = theme.SaveOsdSettings,
    },
    {
        id = "Notifications",
        label = "Notifications",
        target = theme.notifications,
        schema = theme.NOTIFICATIONS_SCHEMA,
        order = theme.NOTIFICATIONS_ORDER,
        load = theme.LoadNotificationSettings,
        save = theme.SaveNotificationSettings,
    },
}

local function copyValues(source, schema)
    local values = {}
    for settingName in pairs(schema) do values[settingName] = source[settingName] end
    return values
end

local function snapshotSavedValues()
    for groupIndex, group in ipairs(groups) do state.saved[group.id] = copyValues(group.target, group.schema) end
end

local function loadPersistedValues()
    for groupIndex, group in ipairs(groups) do group.load() end
    snapshotSavedValues()
    state.knownRevision = theme.GetAppearanceRevision()
    state.revisionConflict = false
end

local function groupIsDirty(group)
    local saved = state.saved[group.id] or {}
    for settingName in pairs(group.schema) do
        if group.target[settingName] ~= saved[settingName] then return true end
    end
    return false
end

local function validateColor(value, label)
    if tostring(value or ""):match("^#%x%x%x%x%x%x$") then return true end
    return false, label .. " must use #RRGGBB format"
end

local function validateGroup(group)
    for settingIndex, settingName in ipairs(group.order) do
        local rule = group.schema[settingName]
        local value = group.target[settingName]
        if rule.type == "color" then
            local valid, validationError = validateColor(value, rule.label)
            if not valid then return false, validationError end
        elseif rule.type == "number" then
            local numericValue = tonumber(value)
            if not numericValue or numericValue < rule.min or numericValue > rule.max then return false, rule.label .. " is outside the allowed range" end
        end
    end
    return true
end

local function renderSetting(ctx, group, settingName)
    local rule = group.schema[settingName]
    local value = group.target[settingName]
    local controlId = rule.label .. "##Appearance_" .. group.id .. "_" .. settingName
    local changed = false

    if rule.type == "boolean" then
        changed, value = imgui.Checkbox(ctx, controlId, value == true)
    elseif rule.enumItems then
        changed, value = ui.ComboEnum(ctx, controlId, value, rule.enumItems)
    elseif rule.type == "number" then
        imgui.SetNextItemWidth(ctx, 260)
        changed, value = imgui.SliderDouble(ctx, controlId, tonumber(value) or rule.default, rule.min, rule.max, rule.format or (rule.integer and "%.0f" or "%.2f"))
        if changed then
            value = settingsStore.NormalizeValue(value, rule)
            if group.id == "OSK" and settingName == "inactive_led_boost" then theme.ClearInactiveLedBoostCache() end
        end
    elseif rule.type == "color" then
        imgui.SetNextItemWidth(ctx, 100)
        changed, value = imgui.InputText(ctx, controlId, tostring(value or ""))
        imgui.SameLine(ctx)
        imgui.PushStyleColor(ctx, imgui.Col_Button, theme.HexToImCol(value, 0x333333FF))
        imgui.Button(ctx, "Preview##AppearanceColor_" .. group.id .. "_" .. settingName, 60, 0)
        imgui.PopStyleColor(ctx)
    end

    if changed then
        group.target[settingName] = value
        theme.PublishAppearancePreview()
        state.error = ""
        state.message = ""
    end
end

local function renderPreview(ctx)
    imgui.Separator(ctx)
    imgui.Text(ctx, "Preview")
    imgui.PushStyleVar(ctx, imgui.StyleVar_ItemSpacing, theme.common.item_spacing, theme.common.item_spacing)
    imgui.PushStyleVar(ctx, imgui.StyleVar_FrameRounding, theme.common.rounding)
    imgui.Button(ctx, "Normal control##AppearancePreview")
    imgui.SameLine(ctx)
    ui.Disabled(ctx, true, function() imgui.Button(ctx, "Disabled control##AppearancePreview") end)
    imgui.PushStyleColor(ctx, imgui.Col_Button, theme.HexToImCol(theme.osd.osd_bg_on, 0x7F7F7FFF))
    imgui.Button(ctx, "OSD active##AppearancePreview")
    imgui.PopStyleColor(ctx)
    imgui.SameLine(ctx)
    imgui.PushStyleVar(ctx, imgui.StyleVar_Alpha, theme.notifications.opacity)
    imgui.Button(ctx, "Notification##AppearancePreview")
    imgui.PopStyleVar(ctx)
    imgui.PopStyleVar(ctx, 2)
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    theme.ClearAppearancePreview()
    loadPersistedValues()
end

function module.Update()
    module.Initialize()
    local revision = theme.GetAppearanceRevision()
    if revision == state.knownRevision then return end
    if module.IsDirty() then
        state.revisionConflict = true
        state.error = "Appearance settings changed in another window. Revert this draft to load the current values."
        return
    end
    loadPersistedValues()
    state.message = "Appearance settings updated"
end

function module.IsDirty()
    module.Initialize()
    for groupIndex, group in ipairs(groups) do if groupIsDirty(group) then return true end end
    return false
end

function module.Validate()
    if state.revisionConflict then return false, state.error end
    for groupIndex, group in ipairs(groups) do
        local valid, validationError = validateGroup(group)
        if not valid then return false, validationError end
    end
    return true
end

function module.IsBusy()
    return false
end

function module.GetStatus()
    if state.error ~= "" then return state.error end
    return state.message
end

function module.Save()
    local valid, validationError = module.Validate()
    if not valid then
        state.error = validationError
        return false, validationError
    end
    for groupIndex, group in ipairs(groups) do if groupIsDirty(group) then group.save() end end
    theme.ClearAppearancePreview()
    snapshotSavedValues()
    state.knownRevision = theme.GetAppearanceRevision()
    state.error = ""
    state.message = "Appearance settings saved"
    return true
end

function module.Revert()
    theme.ClearAppearancePreview()
    loadPersistedValues()
    state.error = ""
    state.message = "Appearance draft reverted"
    return true
end

function module.Shutdown()
    theme.ClearAppearancePreview()
end

function module.RenderPage(ctx)
    module.Update()
    imgui.Text(ctx, "Appearance")
    imgui.TextDisabled(ctx, "Each change is previewed in running OSK, OSD, and Notifications. Save makes it persistent.")
    for groupIndex, group in ipairs(groups) do
        imgui.Separator(ctx)
        imgui.Text(ctx, group.label)
        for settingIndex, settingName in ipairs(group.order) do renderSetting(ctx, group, settingName) end
    end
    renderPreview(ctx)
    local valid, validationError = module.Validate()
    if not valid then imgui.TextWrapped(ctx, "Error: " .. validationError) end
end

return module
