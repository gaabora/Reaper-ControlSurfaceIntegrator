local r = reaper
local imgui = require "imgui" "0.9.3"

local data = require("osk_data")

local M = {}

local WHEEL_SEND_INTERVAL_SECONDS = 0.040
local WHEEL_MAX_EVENTS_PER_COMMAND = 8
local FADER_VALUE_EPSILON = 0.0005
local FADER_WHEEL_STEP = 0.02

local pressedWidgets = {}  -- surface|widget -> true, tracks buttons currently held down
local wheelStates = {}
local faderStates = {}

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

local function clampNormalized(value)
    return math.max(0.0, math.min(1.0, tonumber(value) or 0.0))
end

local function SendFaderTouch(surfaceName, widgetName, touched)
    data.DebugFader(surfaceName, widgetName, "send WidgetTouch=" .. tostring(touched and 1 or 0), 0.0, "touch-" .. tostring(touched))
    r.SetExtState(data.EXT_CMD_SECTION, "WidgetTouch", table.concat({ surfaceName, widgetName, touched and 1 or 0 }, "|"), false)
end

local function SendFaderValue(surfaceName, widgetName, value, commandValueMapper)
    local commandValue = clampNormalized(value)
    local mapped = false
    if commandValueMapper then commandValue = commandValueMapper(commandValue) end
    if commandValueMapper then mapped = true end
    data.SetFaderLocalValue(surfaceName, widgetName, clampNormalized(value), commandValue)
    data.DebugFader(surfaceName, widgetName, string.format("send WidgetValue displayNormalized=%.6f commandValue=%.6f mapped=%s", clampNormalized(value), tonumber(commandValue) or 0.0, tostring(mapped)), 0.0, "value")
    r.SetExtState(data.EXT_CMD_SECTION, "WidgetValue", table.concat({ surfaceName, widgetName, string.format("%.6f", commandValue) }, "|"), false)
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

function M.HandleFader(ctx, surfaceName, cell, trackTop, trackBottom, currentValue, commandValueMapper)
    if not cell or not cell.name then return end
    if not data.vars.interactive then return end
    if not trackTop or not trackBottom or trackBottom <= trackTop then return end

    local stateKey = GetInteractionStateKey(surfaceName, cell.name)
    if not stateKey then return end

    if imgui.IsItemHovered(ctx) and not imgui.IsItemActive(ctx) then
        local wheelValue = imgui.GetMouseWheel(ctx)
        if wheelValue ~= 0 then
            local value = clampNormalized((currentValue or 0.0) + wheelValue * FADER_WHEEL_STEP)
            data.DebugFader(surfaceName, cell.name, string.format("wheel rawWheel=%.6f currentNormalized=%.6f targetNormalized=%.6f mapper=%s", wheelValue, clampNormalized(currentValue), value, tostring(commandValueMapper ~= nil)), 0.0, "wheel")
            local faderState = faderStates[stateKey]
            if not faderState then
                faderState = { lastValue = nil, touched = false }
                faderStates[stateKey] = faderState
            end
            if not faderState.lastValue or math.abs(value - faderState.lastValue) >= FADER_VALUE_EPSILON then
                SendFaderValue(surfaceName, cell.name, value, commandValueMapper)
                faderState.lastValue = value
            end
        end
    end

    local faderState = faderStates[stateKey]
    if imgui.IsItemActivated(ctx) then
        faderState = { lastValue = nil, touched = true }
        faderStates[stateKey] = faderState
        data.DebugFader(surfaceName, cell.name, string.format("activated trackTop=%.2f trackBottom=%.2f currentNormalized=%.6f mapper=%s", trackTop, trackBottom, clampNormalized(currentValue), tostring(commandValueMapper ~= nil)), 0.0, "activated")
        SendFaderTouch(surfaceName, cell.name, true)
    end

    local active = imgui.IsItemActive(ctx) or imgui.IsItemDeactivated(ctx)
    if active then
        local _, mouseY = imgui.GetMousePos(ctx)
        local value = clampNormalized((trackBottom - mouseY) / (trackBottom - trackTop))
        data.DebugFader(surfaceName, cell.name, string.format("drag mouseY=%.2f trackTop=%.2f trackBottom=%.2f targetNormalized=%.6f deactivated=%s", mouseY, trackTop, trackBottom, value, tostring(imgui.IsItemDeactivated(ctx))), 0.10, "drag")
        if not faderState then
            faderState = { lastValue = nil, touched = false }
            faderStates[stateKey] = faderState
        end
        if not faderState.lastValue or math.abs(value - faderState.lastValue) >= FADER_VALUE_EPSILON or imgui.IsItemDeactivated(ctx) then
            SendFaderValue(surfaceName, cell.name, value, commandValueMapper)
            faderState.lastValue = value
        end
    end

    if imgui.IsItemDeactivated(ctx) then
        if faderState and faderState.touched then
            SendFaderTouch(surfaceName, cell.name, false)
        end
        data.DebugFader(surfaceName, cell.name, "deactivated", 0.0, "deactivated")
        faderStates[stateKey] = nil
    end
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
