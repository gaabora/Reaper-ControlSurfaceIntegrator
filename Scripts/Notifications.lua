--[[
 * ReaScript Name: ReaControlSurface Notifications
 * About: Show important ReaControlSurface log messages without opening the REAPER console.
 * Author: Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 1.0.0
--]]

local reaperApi = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local host = dofile(scriptDir .. "script_host.lua")
local imgui = host.RequireImGui(scriptDir)
if not imgui then return end

local identity = require("product_identity")
local controlPanelProtocol = require("control_panel_protocol")
local theme = require("theme_settings")

local ctx = host.CreateContext(identity.displayName .. " Notifications")
local logPath = reaperApi.GetExtState(identity.extState.log, "File")
local logSessionId = reaperApi.GetExtState(identity.extState.log, "SessionId")
local startOffset = tonumber(reaperApi.GetExtState(identity.extState.notifications, "StartOffset")) or 0
local logFile = nil
local notifications = {}
local activeNotification = nil
local lastPollTime = 0
local lastAppearanceChangeToken = theme.GetAppearanceChangeToken()

local LEVELS = {
    ERROR = { color = 0xFF5A5AFF, durationSec = 600 },
    WARNING = { color = 0xFFB84DFF, durationSec = 60 },
    NOTICE = { color = 0x66CCFFFF, durationSec = 20 },
}

local function openLog()
    if logFile then return true end
    logFile = io.open(logPath, "rb")
    if not logFile then return false end
    local fileSize = logFile:seek("end") or 0
    logFile:seek("set", math.min(startOffset, fileSize))
    startOffset = nil
    return true
end

local function addNotification(line, severity, byteOffset, options)
    options = options or {}
    local level = LEVELS[severity]
    if not level then return end
    notifications[#notifications + 1] = { text = line, color = level.color, expireTs = reaperApi.time_precise() + (options.durationSec or level.durationSec), byteOffset = byteOffset, sessionId = options.preview and "" or logSessionId, preview = options.preview == true }
    while #notifications > 6 do table.remove(notifications, 1) end
    if not options.preview then activeNotification = notifications[#notifications] end
end

local function pollAppearancePreview()
    if not reaperApi.HasExtState(identity.extState.notifications, "AppearancePreview") then return end
    reaperApi.DeleteExtState(identity.extState.notifications, "AppearancePreview", false)
    addNotification("[NOTICE] Appearance preview", "NOTICE", nil, { durationSec = 30, preview = true })
end

local function processLine(line, byteOffset)
    local severity = line:match("%[(ERROR)%]") or line:match("%[(WARNING)%]") or line:match("%[(NOTICE)%]") or line:match("%[(INFO)%]") or line:match("%[(DEBUG)%]")
    if severity then
        activeNotification = nil
        if LEVELS[severity] then addNotification(line, severity, byteOffset) end
    elseif line:match("^%[%d%d:%d%d:%d%d%]") then
        activeNotification = nil
    elseif activeNotification then
        activeNotification.text = activeNotification.text .. "\n" .. line
    end
end

local function refreshLogIdentity()
    local currentPath = reaperApi.GetExtState(identity.extState.log, "File")
    local currentSessionId = reaperApi.GetExtState(identity.extState.log, "SessionId")
    if currentPath == logPath and currentSessionId == logSessionId then return end
    if logFile then logFile:close() end
    logFile = nil
    logPath = currentPath
    logSessionId = currentSessionId
    startOffset = 0
    activeNotification = nil
end

local function pollLog()
    local now = reaperApi.time_precise()
    if now - lastPollTime < 0.1 then return end
    lastPollTime = now
    refreshLogIdentity()
    if not openLog() then return end
    local currentOffset = logFile:seek() or 0
    local fileSize = logFile:seek("end") or 0
    logFile:seek("set", fileSize < currentOffset and 0 or currentOffset)
    while true do
        local byteOffset = logFile:seek() or 0
        local line = logFile:read("*l")
        if not line then break end
        processLine(line, byteOffset)
    end
end

local function refreshStableAction()
    if not reaperApi.NamedCommandLookup or not reaperApi.RefreshToolbar2 then return end
    local commandId = reaperApi.NamedCommandLookup("_" .. identity.notificationsActionId)
    if commandId and commandId > 0 then reaperApi.RefreshToolbar2(0, commandId) end
end

local function setLifecycleState(value)
    reaperApi.SetExtState(identity.extState.notifications, "State", value, false)
    refreshStableAction()
end

local function shouldStop()
    if reaperApi.GetExtState(identity.extState.notifications, "Command") ~= "Stop" then return false end
    reaperApi.DeleteExtState(identity.extState.notifications, "Command", false)
    return true
end

local function removeExpiredNotifications()
    local now = reaperApi.time_precise()
    for notificationIndex = #notifications, 1, -1 do
        if notifications[notificationIndex].expireTs <= now then table.remove(notifications, notificationIndex) end
    end
end

local function reloadAppearanceIfNeeded()
    local changeToken = theme.GetAppearanceChangeToken()
    if changeToken == lastAppearanceChangeToken then return end
    theme.LoadCurrentAppearance()
    lastAppearanceChangeToken = changeToken
end

local function renderNotifications()
    if #notifications == 0 then return true end
    local viewport = imgui.GetMainViewport(ctx)
    local workX, workY = imgui.Viewport_GetWorkPos(viewport)
    local workWidth = imgui.Viewport_GetWorkSize(viewport)
    local windowWidth = math.min(500, workWidth - 24)
    imgui.SetNextWindowPos(ctx, workX + workWidth - windowWidth - 12, workY + 12, imgui.Cond_Always)
    imgui.SetNextWindowSize(ctx, windowWidth, 0, imgui.Cond_Always)
    local windowFlags = imgui.WindowFlags_NoTitleBar | imgui.WindowFlags_NoResize | imgui.WindowFlags_NoMove | imgui.WindowFlags_AlwaysAutoResize
    if imgui.WindowFlags_NoDocking then windowFlags = windowFlags | imgui.WindowFlags_NoDocking end
    if imgui.WindowFlags_NoSavedSettings then windowFlags = windowFlags | imgui.WindowFlags_NoSavedSettings end
    imgui.PushStyleVar(ctx, imgui.StyleVar_Alpha, theme.notifications.opacity)
    imgui.PushStyleVar(ctx, imgui.StyleVar_WindowRounding, theme.common.rounding)
    local visible = imgui.Begin(ctx, "##ReaControlSurfaceNotifications", true, windowFlags)
    local dismissIndex = nil
    if visible then
        for notificationIndex, notification in ipairs(notifications) do
            if notificationIndex > 1 then imgui.Separator(ctx) end
            local closeButtonSize = theme.NOTIFICATIONS.close_button_size
            if imgui.Button(ctx, "×##DismissNotification_" .. notificationIndex, closeButtonSize, closeButtonSize) then dismissIndex = notificationIndex end
            imgui.SameLine(ctx)
            imgui.PushTextWrapPos(ctx, windowWidth - 20)
            imgui.TextColored(ctx, notification.color, notification.text)
            imgui.PopTextWrapPos(ctx)
            if not notification.preview and imgui.IsItemClicked and imgui.IsItemClicked(ctx, 0) then controlPanelProtocol.Open("Logging", { logSessionId = notification.sessionId, logOffset = notification.byteOffset }) end
        end
    end
    imgui.End(ctx)
    imgui.PopStyleVar(ctx, 2)
    if dismissIndex then table.remove(notifications, dismissIndex) end
    return true
end

local function main()
    if shouldStop() then return end
    if not host.IsContextValid(ctx) then
        host.SetToolbarState(-1)
        return
    end
    reloadAppearanceIfNeeded()
    pollAppearancePreview()
    pollLog()
    removeExpiredNotifications()
    renderNotifications()
    reaperApi.defer(main)
end

host.SetToolbarState(1)
reaperApi.SetExtState(identity.extState.notifications, "Enabled", "1", false)
setLifecycleState("Open")
theme.LoadCurrentAppearance()
host.OnExit(function()
    if logFile then logFile:close() end
    setLifecycleState("Closed")
    host.SetToolbarState(-1)
end)
main()
