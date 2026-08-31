local imgui = require "imgui" "0.9.3"

local identity = require("product_identity")
local settings = require("logging_settings_ui")
local ui = require("ui_components")

local module = {}
local knownSeverities = { ERROR = true, WARNING = true, NOTICE = true, INFO = true, DEBUG = true }
local state = {
    activeRecord = nil,
    filePath = "",
    initialized = false,
    lastPollTime = 0,
    logText = "",
    navigationOffset = nil,
    offset = 0,
    recordRanges = {},
    records = {},
    selectionCallback = nil,
    sessionId = "",
    status = "",
    textDirty = true,
}

local function resetLogView(sessionId, filePath)
    state.activeRecord = nil
    state.filePath = filePath or ""
    state.logText = ""
    state.offset = 0
    state.recordRanges = {}
    state.records = {}
    state.sessionId = sessionId or ""
    state.status = ""
    state.textDirty = true
end

local function refreshLogIdentity()
    local sessionId = reaper.GetExtState(identity.extState.log, "SessionId")
    local filePath = reaper.GetExtState(identity.extState.log, "File")
    if sessionId ~= state.sessionId or filePath ~= state.filePath then resetLogView(sessionId, filePath) end
end

local function appendLine(line, byteOffset)
    local severity = line:match("^%[%d%d:%d%d:%d%d%]%s+%[(%u+)%]")
    if severity then
        state.activeRecord = nil
        if knownSeverities[severity] then
            local record = { byteOffset = byteOffset, severity = severity, text = line }
            state.records[#state.records + 1] = record
            state.activeRecord = record
            state.textDirty = true
        end
    elseif line:match("^%[%d%d:%d%d:%d%d%]") then
        state.activeRecord = nil
    elseif state.activeRecord then
        state.activeRecord.text = state.activeRecord.text .. "\n" .. line
        state.textDirty = true
    end
end

local function pollLog()
    local now = reaper.time_precise()
    if now - state.lastPollTime < 0.1 then return end
    state.lastPollTime = now
    refreshLogIdentity()
    if state.filePath == "" then return end
    local logFile = io.open(state.filePath, "rb")
    if not logFile then return end
    local fileSize = logFile:seek("end") or 0
    if fileSize < state.offset then
        state.offset = 0
        state.records = {}
        state.activeRecord = nil
        state.textDirty = true
    end
    logFile:seek("set", state.offset)
    while true do
        local byteOffset = logFile:seek() or state.offset
        local line = logFile:read("*l")
        if not line then break end
        appendLine(line, byteOffset)
        state.offset = logFile:seek() or fileSize
    end
    logFile:close()
end

local function rebuildLogText()
    if not state.textDirty then return end
    local parts = {}
    local bytePosition = 0
    state.recordRanges = {}
    for recordIdx, record in ipairs(state.records) do
        if recordIdx > 1 then
            parts[#parts + 1] = "\n"
            bytePosition = bytePosition + 1
        end
        local startPosition = bytePosition
        parts[#parts + 1] = record.text
        bytePosition = bytePosition + #record.text
        state.recordRanges[record.byteOffset] = { startPosition = startPosition, endPosition = bytePosition }
    end
    state.logText = table.concat(parts)
    state.textDirty = false
end

local function ensureSelectionCallback(ctx)
    if state.selectionCallback then return end
    state.selectionCallback = imgui.CreateFunctionFromEEL([[
        selectionRequested > 0 ? (
            CursorPos = selectionEnd;
            SelectionStart = selectionStart;
            SelectionEnd = selectionEnd;
            selectionRequested = 0;
        );
    ]])
    imgui.Attach(ctx, state.selectionCallback)
end

local function requestNativeCommand(command)
    if reaper.HasExtState(identity.extState.logCommand, "Request") then return false end
    reaper.SetExtState(identity.extState.logCommand, "Request", command, false)
    return true
end

local function renderLogToolbar(ctx)
    ui.Disabled(ctx, state.filePath == "", function()
        if imgui.Button(ctx, "Open log file") then requestNativeCommand("OpenFile") end
        imgui.SameLine(ctx)
        if imgui.Button(ctx, "Open log folder") then requestNativeCommand("OpenFolder") end
    end)
end

local function renderLogRecords(ctx)
    rebuildLogText()
    ensureSelectionCallback(ctx)
    local availableWidth, availableHeight = imgui.GetContentRegionAvail(ctx)
    if state.navigationOffset then
        local recordRange = state.recordRanges[state.navigationOffset]
        if recordRange then
            imgui.Function_SetValue(state.selectionCallback, "selectionStart", recordRange.startPosition)
            imgui.Function_SetValue(state.selectionCallback, "selectionEnd", recordRange.endPosition)
            imgui.Function_SetValue(state.selectionCallback, "selectionRequested", 1)
            imgui.SetKeyboardFocusHere(ctx)
        else
            state.status = "The notification source record is not available in the active daily log."
        end
        state.navigationOffset = nil
    end
    local flags = imgui.InputTextFlags_ReadOnly | imgui.InputTextFlags_CallbackAlways
    local displayedText = state.logText
    if displayedText == "" then displayedText = state.filePath == "" and "File logging is disabled or the active log file is not available." or "No log records in the active daily file." end
    imgui.InputTextMultiline(ctx, "##LoggingRecords", displayedText, availableWidth, math.max(120, availableHeight), flags, state.selectionCallback)
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    settings.Initialize()
    refreshLogIdentity()
    pollLog()
end

function module.Update()
    if not state.initialized then return end
    settings.Update()
    pollLog()
end

function module.RenderPage(ctx)
    module.Initialize()
    local settingsRendered = settings.Render(ctx)
    if settingsRendered then imgui.SameLine(ctx) end
    renderLogToolbar(ctx)
    imgui.Spacing(ctx)
    if state.status ~= "" then imgui.TextWrapped(ctx, state.status) end
    renderLogRecords(ctx)
end

function module.Navigate(sessionId, byteOffset)
    module.Initialize()
    if sessionId == "" or sessionId ~= state.sessionId then
        state.status = "The notification source record is not available in the active daily log."
        return false
    end
    state.status = ""
    state.navigationOffset = tonumber(byteOffset)
    return state.navigationOffset ~= nil
end

function module.IsDirty()
    return settings.IsDirty()
end

function module.IsBusy()
    return settings.IsBusy()
end

function module.HasError()
    return settings.HasError()
end

function module.GetStatus()
    return settings.GetStatus()
end

function module.Validate()
    return settings.Validate()
end

function module.Save()
    return settings.Save()
end

function module.Revert()
    return settings.Revert()
end

function module.Shutdown()
    settings.Shutdown()
end

return module
