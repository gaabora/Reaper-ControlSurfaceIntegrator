local r = reaper
local imgui = require "imgui" "0.9.3"

local data = require("osk_data")

local M = {}

local WHEEL_SEND_INTERVAL_SECONDS = 0.040
local WHEEL_MAX_EVENTS_PER_COMMAND = 8

local pressedWidgets = {}  -- surface|widget -> true, tracks buttons currently held down
local wheelStates = {}

local function GetInteractionStateKey(surfaceName, widgetName)
    if not surfaceName or surfaceName == "" or not widgetName or widgetName == "" then return nil end
    return surfaceName .. "|" .. widgetName
end

local function GetWheelAccelerationIndex(eventInterval, wheelMagnitude)
    local accelerationIndex = 0
    if eventInterval <= 0.035 then
        accelerationIndex = 3
    elseif eventInterval <= 0.070 then
        accelerationIndex = 2
    elseif eventInterval <= 0.140 then
        accelerationIndex = 1
    end

    local magnitudeBoost = math.max(0, math.min(3, math.floor(wheelMagnitude + 0.5) - 1))
    return math.min(3, accelerationIndex + magnitudeBoost)
end

local function SendPendingWheelCommand(wheelState, now)
    if wheelState.pendingEvents == 0 then return false end
    if wheelState.lastSentTime > 0 and now - wheelState.lastSentTime < WHEEL_SEND_INTERVAL_SECONDS then return false end

    local signedEventCount = wheelState.pendingEvents * wheelState.direction
    local msg = table.concat({ wheelState.surfaceName, wheelState.widgetName, wheelState.accelerationIndex, signedEventCount }, "|")
    r.SetExtState(data.EXT_CMD_SECTION, "WidgetScroll", msg, false)
    wheelState.pendingEvents = 0
    wheelState.accelerationIndex = 0
    wheelState.lastSentTime = now
    return true
end

function M.HandlePressDown(surfaceName, cell)
    if not data.vars.interactive or not cell or not cell.name then return end

    local stateKey = GetInteractionStateKey(surfaceName, cell.name)
    if not stateKey then return end

    r.SetExtState(data.EXT_CMD_SECTION, "WidgetPressDown", surfaceName .. "|" .. cell.name, false)
    pressedWidgets[stateKey] = true
end

function M.HandlePressUp(surfaceName, cell)
    local stateKey = GetInteractionStateKey(surfaceName, cell and cell.name)
    if not stateKey or not pressedWidgets[stateKey] then return end

    pressedWidgets[stateKey] = nil
    r.SetExtState(data.EXT_CMD_SECTION, "WidgetPressUp", surfaceName .. "|" .. cell.name, false)
end

function M.HandleWheel(ctx, surfaceName, cell)
    if not cell or not cell.name then return end
    if not data.IsRelativeWidget(surfaceName, cell.name) then return end
    if not imgui.IsItemHovered(ctx) then return end
    if not data.vars.interactive then return end

    local stateKey = GetInteractionStateKey(surfaceName, cell.name)
    if not stateKey then return end

    local wheelState = wheelStates[stateKey]
    if not wheelState then
        wheelState = {
            surfaceName = surfaceName,
            widgetName = cell.name,
            direction = 0,
            pendingEvents = 0,
            accelerationIndex = 0,
            lastInputTime = 0,
            lastSentTime = 0,
        }
        wheelStates[stateKey] = wheelState
    end

    local now = r.time_precise()
    local wheelValue = imgui.GetMouseWheel(ctx)
    if wheelValue ~= 0 then
        local direction = wheelValue > 0 and 1 or -1
        local eventInterval = wheelState.lastInputTime > 0 and now - wheelState.lastInputTime or math.huge
        if direction ~= wheelState.direction then
            eventInterval = math.huge
            wheelState.pendingEvents = 0
            wheelState.accelerationIndex = 0
        end

        local eventCount = math.max(1, math.floor(math.abs(wheelValue) + 0.5))
        wheelState.direction = direction
        wheelState.pendingEvents = math.min(WHEEL_MAX_EVENTS_PER_COMMAND, wheelState.pendingEvents + eventCount)
        wheelState.accelerationIndex = math.max(wheelState.accelerationIndex, GetWheelAccelerationIndex(eventInterval, math.abs(wheelValue)))
        wheelState.lastInputTime = now
    end

    if wheelState.pendingEvents == 0 then return end
    SendPendingWheelCommand(wheelState, now)
end

function M.FlushWheelCommands()
    local now = r.time_precise()
    for stateKey, wheelState in pairs(wheelStates) do
        if SendPendingWheelCommand(wheelState, now) then return end
        if wheelState.pendingEvents == 0 and now - wheelState.lastInputTime > 2.0 then
            wheelStates[stateKey] = nil
        end
    end
end

return M
