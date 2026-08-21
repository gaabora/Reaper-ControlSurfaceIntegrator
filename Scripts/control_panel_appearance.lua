local imgui = require "imgui" "0.9.3"

local identity = require("product_identity")
local oskColorPicker = require("osk_color_picker")
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
    scriptCommandIds = {},
}

local groups = {
    {
        id = "OSK",
        label = "OSK",
        target = theme.osk,
        schema = theme.OSK_SCHEMA,
        order = theme.OSK_ORDER,
        load = theme.LoadOskSettings,
        save = theme.SaveOskSettings,
        actionLabel = "Open",
        action = "OSK",
    },
    {
        id = "OSD",
        label = "OSD",
        target = theme.osd,
        schema = theme.OSD_SCHEMA,
        order = theme.OSD_ORDER,
        load = theme.LoadOsdSettings,
        save = theme.SaveOsdSettings,
        actionLabel = "Show preview",
        action = "OSD",
    },
    {
        id = "Notifications",
        label = "Notifications",
        target = theme.notifications,
        schema = theme.NOTIFICATIONS_SCHEMA,
        order = theme.NOTIFICATIONS_ORDER,
        load = theme.LoadNotificationSettings,
        save = theme.SaveNotificationSettings,
        actionLabel = "Show preview",
        action = "Notifications",
    },
}

local groupsById = {}
for groupIndex, group in ipairs(groups) do groupsById[group.id] = group end

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

local function findRegisteredScriptCommand(scriptFilename)
    if not reaper.SectionFromUniqueID or not reaper.kbd_enumerateActions or not reaper.kbd_getTextFromCmd then return 0 end
    local section = reaper.SectionFromUniqueID(0)
    if not section then return 0 end
    local matchingCommandId = 0
    for actionIndex = 0, 100000 do
        local commandId = reaper.kbd_enumerateActions(section, actionIndex)
        if not commandId or commandId == 0 then break end
        local actionName = reaper.kbd_getTextFromCmd(commandId, 0) or ""
        if #actionName >= #scriptFilename and actionName:sub(-#scriptFilename) == scriptFilename then
            if matchingCommandId ~= 0 then return 0 end
            matchingCommandId = commandId
        end
    end
    return matchingCommandId
end

local function resolveScriptCommand(scriptFilename)
    if state.scriptCommandIds[scriptFilename] then return state.scriptCommandIds[scriptFilename] end
    local scriptPath = reaper.GetResourcePath() .. "/Scripts/" .. identity.scriptDirectory .. "/" .. scriptFilename
    if reaper.file_exists and not reaper.file_exists(scriptPath) then return 0, "ReaScript file is not installed: " .. scriptPath end
    local commandId = reaper.AddRemoveReaScript and reaper.AddRemoveReaScript(true, 0, scriptPath, true) or 0
    if not commandId or commandId == 0 then commandId = findRegisteredScriptCommand(scriptFilename) end
    if not commandId or commandId == 0 then return 0, "Cannot register or find " .. scriptFilename end
    state.scriptCommandIds[scriptFilename] = commandId
    return commandId
end

local function startScript(scriptFilename)
    local commandId, commandError = resolveScriptCommand(scriptFilename)
    if not commandId or commandId == 0 then return false, commandError end
    if reaper.GetToggleCommandState(commandId) ~= 1 then reaper.Main_OnCommand(commandId, 0) end
    return true
end

local function showOsk()
    reaper.SetExtState(identity.extState.oskCommand, "Open", "1", false)
    return true
end

local function showOsdPreview()
    local started, startError = startScript(identity.osdScriptFilename)
    if not started then return false, startError end
    local previewId = "appearance-preview-" .. tostring(math.floor(reaper.time_precise() * 1000000))
    reaper.SetExtState(identity.extState.osd, "OSD", "Appearance preview;1;30000;0", false)
    reaper.SetExtState(identity.extState.osd, "OSD_ID", previewId, false)
    return true
end

local function showNotificationPreview()
    local commandId = reaper.NamedCommandLookup and reaper.NamedCommandLookup("_" .. identity.notificationsActionId) or 0
    if commandId and commandId > 0 then
        if reaper.GetToggleCommandState(commandId) ~= 1 then reaper.Main_OnCommand(commandId, 0) end
    else
        local started, startError = startScript(identity.notificationsScriptFilename)
        if not started then return false, startError end
    end
    reaper.SetExtState(identity.extState.notifications, "AppearancePreview", tostring(math.floor(reaper.time_precise() * 1000000)), false)
    return true
end

local function runGroupAction(group)
    local opened, openError
    if group.action == "OSK" then opened, openError = showOsk()
    elseif group.action == "OSD" then opened, openError = showOsdPreview()
    elseif group.action == "Notifications" then opened, openError = showNotificationPreview()
    else return end
    state.error = opened and "" or tostring(openError or "Cannot open preview")
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
    local controlId = "##Appearance_" .. group.id .. "_" .. settingName
    local changed = false

    if rule.type == "boolean" then
        changed, value = imgui.Checkbox(ctx, controlId, value == true)
    elseif rule.enumItems then
        changed, value = ui.ComboEnum(ctx, controlId, value, rule.enumItems)
    elseif rule.type == "number" then
        if rule.integer then
            changed, value = ui.DragInteger(ctx, controlId, value or rule.default, rule.min, rule.max, rule.step or 1, { format = rule.format or "%d" })
        else
            changed, value = ui.DragNumber(ctx, controlId, value or rule.default, rule.min, rule.max, rule.step or 0.01, { format = rule.format or "%.2f" })
        end
        if changed then
            value = settingsStore.NormalizeValue(value, rule)
            if group.id == "OSK" and settingName == "inactive_led_boost" then theme.ClearInactiveLedBoostCache() end
        end
    elseif rule.type == "color" then
        changed, value = oskColorPicker.RenderHexColorPicker(ctx, group.id .. "_" .. settingName, rule.label, value, rule.default)
    end

    if changed then
        group.target[settingName] = value
        theme.PublishAppearancePreview()
        state.error = ""
        state.message = ""
    end
end

local function renderGroupHeader(ctx, group, fonts)
    if fonts and fonts.section then imgui.PushFont(ctx, fonts.section) end
    imgui.Text(ctx, group.label)
    if fonts and fonts.section then imgui.PopFont(ctx) end
    if group.action then
        imgui.SameLine(ctx)
        local buttonWidth = 110
        local remainingWidth = imgui.GetContentRegionAvail(ctx)
        imgui.SetCursorPosX(ctx, imgui.GetCursorPosX(ctx) + math.max(0, remainingWidth - buttonWidth))
        if imgui.Button(ctx, group.actionLabel .. "##AppearanceAction_" .. group.id, buttonWidth, 0) then runGroupAction(group) end
    end
end

local function renderGroup(ctx, group, fonts)
    renderGroupHeader(ctx, group, fonts)
    if imgui.BeginTable(ctx, "##AppearanceForm_" .. group.id, 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "Label", imgui.TableColumnFlags_WidthStretch)
        imgui.TableSetupColumn(ctx, "Value", imgui.TableColumnFlags_WidthFixed, theme.FORM.control_width)
        for settingIndex, settingName in ipairs(group.order) do
            local rule = group.schema[settingName]
            imgui.TableNextRow(ctx)
            imgui.TableSetColumnIndex(ctx, 0)
            imgui.AlignTextToFramePadding(ctx)
            imgui.Text(ctx, rule.label)
            imgui.TableSetColumnIndex(ctx, 1)
            renderSetting(ctx, group, settingName)
        end
        imgui.EndTable(ctx)
    end
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

function module.RenderPage(ctx, fonts)
    module.Update()
    if imgui.BeginTable(ctx, "##AppearanceColumns", 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "Left", imgui.TableColumnFlags_WidthStretch)
        imgui.TableSetupColumn(ctx, "Right", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        renderGroup(ctx, groupsById.OSK, fonts)
        imgui.TableSetColumnIndex(ctx, 1)
        renderGroup(ctx, groupsById.Notifications, fonts)
        imgui.Spacing(ctx)
        renderGroup(ctx, groupsById.OSD, fonts)
        imgui.EndTable(ctx)
    end
    local valid, validationError = module.Validate()
    if not valid then imgui.TextWrapped(ctx, "Error: " .. validationError) end
end

return module
