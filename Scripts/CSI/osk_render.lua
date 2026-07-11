local r = reaper
local imgui = require "imgui" "0.9.3"

local data = require("osk_data")
local osd_ui = require("osd_ui")

local M = {}

local BUTTON_SIZE = 64
local hoverStartTime = {}
local FONT = nil
local FONT_SMALL = nil
local configModule = nil
local wheelStates = {}

local WHEEL_SEND_INTERVAL_SECONDS = 0.040
local WHEEL_MAX_EVENTS_PER_COMMAND = 8

function M.SetFonts(font, fontSmall)
    FONT = font
    FONT_SMALL = fontSmall
    osd_ui.SetFont(fontSmall)
end

function M.SetConfigModule(module)
    configModule = module
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

local function FlushPendingWheelCommands()
    local now = r.time_precise()
    for stateKey, wheelState in pairs(wheelStates) do
        if SendPendingWheelCommand(wheelState, now) then return end
        if wheelState.pendingEvents == 0 and now - wheelState.lastInputTime > 2.0 then
            wheelStates[stateKey] = nil
        end
    end
end

local pressedWidgets = {}  -- widgetName -> surfName, tracks buttons currently held down

local function HandleButtonPressDown(surfName, cell)
    if not data.vars.clickable or not cell.name then return end
    if not surfName then return end
    local msg = surfName .. "|" .. cell.name
    r.SetExtState(data.EXT_CMD_SECTION, "WidgetPressDown", msg, false)
    pressedWidgets[cell.name] = surfName
end

local function HandleButtonPressUp(cell)
    if not cell.name then return end
    if not pressedWidgets[cell.name] then return end
    local surfName = pressedWidgets[cell.name]
    pressedWidgets[cell.name] = nil
    local msg = surfName .. "|" .. cell.name
    r.SetExtState(data.EXT_CMD_SECTION, "WidgetPressUp", msg, false)
end

local function HandleRotaryMouseWheel(ctx, surfName, cell)
    if not cell or not cell.name then return end

    local name = tostring(cell.name):lower()
    local group = tostring(cell.group or ""):lower()
    local isRotary = name:find("rotary") or group:find("rotary")
    if not isRotary then return end
    if not imgui.IsItemHovered(ctx) then return end
    if not data.vars.clickable then return end

    if not surfName then return end

    local stateKey = surfName .. "|" .. cell.name
    local wheelState = wheelStates[stateKey]
    if not wheelState then
        wheelState = {
            surfaceName = surfName,
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

local function GetCellInfo(surfName, widgetName)
    local layout = data.layouts[surfName]
    if not layout then return nil end
    for _, row in ipairs(layout) do
        for _, cell in ipairs(row) do
            if not cell.isSpacer and cell.name == widgetName then return cell end
        end
    end
    return nil
end

local function colorIsMeaningful(col)
    return ((col >> 24) & 0xFF) > 10
        or ((col >> 16) & 0xFF) > 10
        or ((col >> 8) & 0xFF) > 10
end

local function GetButtonColor(surfName, widgetName, cellOverride)
    local widgetState = data.states[surfName] and data.states[surfName][widgetName]
    local cell = cellOverride or GetCellInfo(surfName, widgetName)

    if widgetState and widgetState.value > 0 then
        if colorIsMeaningful(widgetState.color) then return widgetState.color end
        if cell and cell.color then return cell.color end
        return data.COLORS.button_on
    end

    local color
    if cell and cell.color then
        color = data.dimColor(cell.color, 0.40)
    else
        color = data.COLORS.button_off
    end
    return data.ensureMinLuminance(color, 80)
end

local function GetButtonValue(surfName, widgetName)
    local state = data.states[surfName] and data.states[surfName][widgetName]
    if state then return state.value end
    return 0
end

local function GetButtonLabel(surfName, cell)
    local label = data.labels[surfName] and data.labels[surfName][cell.name]
    if label and label ~= "" then return label end
    if cell.label and cell.label ~= "" then return cell.label end
    return cell.name or "?"
end

local function ShowDelayedTooltip(ctx, surfName, widgetName, text)
    if imgui.IsItemHovered(ctx) then
        local now = os.clock()
        if not hoverStartTime[widgetName] then
            hoverStartTime[widgetName] = now
        end
        if now - hoverStartTime[widgetName] >= data.vars.tooltip_delay then
            local modMap = surfName and data.labelMaps[surfName] and data.labelMaps[surfName][widgetName]
            local tooltipText = text
            if modMap and next(modMap) then
                local lines = {}
                if modMap["NoMod"] then
                    lines[#lines + 1] = data.getProcessedLabel(modMap["NoMod"])
                end
                local sortedMods = {}
                for key in pairs(modMap) do
                    if key ~= "NoMod" then sortedMods[#sortedMods + 1] = key end
                end
                table.sort(sortedMods)
                for _, modName in ipairs(sortedMods) do
                    lines[#lines + 1] = modName .. " -> " .. data.getProcessedLabel(modMap[modName])
                end
                if #lines > 0 then
                    tooltipText = table.concat(lines, "\n")
                end
            end
            if imgui.BeginTooltip(ctx) then
                imgui.Text(ctx, tooltipText)
                imgui.EndTooltip(ctx)
            end
        end
    else
        hoverStartTime[widgetName] = nil
    end
end

local function PointInTriangle(px, py, ax, ay, bx, by, cx, cy)
    local v0x, v0y = cx - ax, cy - ay
    local v1x, v1y = bx - ax, by - ay
    local v2x, v2y = px - ax, py - ay
    local dot00 = v0x * v0x + v0y * v0y
    local dot01 = v0x * v1x + v0y * v1y
    local dot02 = v0x * v2x + v0y * v2y
    local dot11 = v1x * v1x + v1y * v1y
    local dot12 = v1x * v2x + v1y * v2y
    local invDen = 1 / (dot00 * dot11 - dot01 * dot01 + 1e-9)
    local u = (dot11 * dot02 - dot01 * dot12) * invDen
    local v = (dot00 * dot12 - dot01 * dot02) * invDen
    return u >= 0 and v >= 0 and (u + v) <= 1
end

local function PointInRoundedShape(px, py, x, y, w, h, inset)
    inset = inset or 0
    x, y = x + inset, y + inset
    w, h = w - inset * 2, h - inset * 2
    if w <= 0 or h <= 0 then return false end

    local r = math.min(w, h) / 2
    local cx, cy = x + w / 2, y + h / 2
    if w >= h then
        local left, right = x + r, x + w - r
        if px >= left and px <= right and py >= y and py <= y + h then return true end
        local dlx, dly = px - left, py - cy
        local drx, dry = px - right, py - cy
        return (dlx * dlx + dly * dly) <= r * r or (drx * drx + dry * dry) <= r * r
    end

    local top, bottom = y + r, y + h - r
    if px >= x and px <= x + w and py >= top and py <= bottom then return true end
    local dtx, dty = px - cx, py - top
    local dbx, dby = px - cx, py - bottom
    return (dtx * dtx + dty * dty) <= r * r or (dbx * dbx + dby * dby) <= r * r
end

local function PointInArrowShape(px, py, x, y, w, h, direction, pointDepth)
    if direction == "left" then
        local bodyL = x + pointDepth
        local inBody = px >= bodyL and px <= x + w and py >= y and py <= y + h
        return inBody or PointInTriangle(px, py, x, y + h / 2, bodyL, y, bodyL, y + h)
    elseif direction == "right" then
        local bodyR = x + w - pointDepth
        local inBody = px >= x and px <= bodyR and py >= y and py <= y + h
        return inBody or PointInTriangle(px, py, x + w, y + h / 2, bodyR, y, bodyR, y + h)
    elseif direction == "up" then
        local bodyT = y + pointDepth
        local inBody = px >= x and px <= x + w and py >= bodyT and py <= y + h
        return inBody or PointInTriangle(px, py, x + w / 2, y, x, bodyT, x + w, bodyT)
    elseif direction == "down" then
        local bodyB = y + h - pointDepth
        local inBody = px >= x and px <= x + w and py >= y and py <= bodyB
        return inBody or PointInTriangle(px, py, x + w / 2, y + h, x, bodyB, x + w, bodyB)
    end
    return false
end

local function DrawButtonInteraction(ctx, surfName, cell, bw, bh, label, hitTestFn)
    local id = "##btn_" .. (cell.name or "")
    imgui.InvisibleButton(ctx, id, bw, bh)

    local hovered = imgui.IsItemHovered(ctx)
    local insideShape = hovered
    if hovered and hitTestFn then
        local mouseX, mouseY = imgui.GetMousePos(ctx)
        insideShape = hitTestFn(mouseX, mouseY)
    end

    if insideShape then
        ShowDelayedTooltip(ctx, surfName, cell.name or "", label)
        HandleRotaryMouseWheel(ctx, surfName, cell)
    elseif cell.name then
        hoverStartTime[cell.name] = nil
    end

    local mouseButtonRight = imgui.MouseButton_Right or 1
    if insideShape and cell.name and configModule and configModule.OpenConfigEditor and imgui.IsItemClicked(ctx, mouseButtonRight) then
        configModule.OpenConfigEditor(surfName, cell.name)
    end

    if insideShape and imgui.IsItemActivated(ctx) then
        HandleButtonPressDown(surfName, cell)
    end
    -- Always check deactivated so a press-up is never missed even if cursor drifted outside shape
    if imgui.IsItemDeactivated(ctx) then
        HandleButtonPressUp(cell)
    end
end

local function RenderCenteredWrappedText(ctx, drawList, text, centerX, centerY, maxW, maxH)
    imgui.PushFont(ctx, FONT_SMALL)
    local lines = data.wrapText(ctx, text, maxW, imgui)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local totalH = #lines * lineH
    local startY = centerY - totalH / 2
    local textCol = data.applyAlpha(data.COLORS.text_normal, data.vars.btn_transparency)
    for index, line in ipairs(lines) do
        local textWidth = imgui.CalcTextSize(ctx, line)
        local textX = centerX - textWidth / 2
        local textY = startY + (index - 1) * lineH
        if maxH and textY + lineH > centerY + maxH / 2 then break end
        imgui.DrawList_AddText(drawList, textX, textY, textCol, line)
    end
    imgui.PopFont(ctx)
end

local function DrawRectButton(ctx, drawList, surfName, cell, bw, bh, yOffset)
    local label = data.getProcessedLabel(GetButtonLabel(surfName, cell))
    local bgCol = data.applyAlpha(GetButtonColor(surfName, cell.name), data.vars.btn_transparency)
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)

    imgui.DrawList_AddRectFilled(drawList, cursorX, drawY, cursorX + bw, drawY + bh, bgCol, 4)
    RenderCenteredWrappedText(ctx, drawList, label, cursorX + bw / 2, drawY + bh / 2, bw - 8, bh)
    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    DrawButtonInteraction(ctx, surfName, cell, bw, bh, label)
end

local function DrawRoundButton(ctx, drawList, surfName, cell, bw, bh, visualW, yOffset)
    local label = data.getProcessedLabel(GetButtonLabel(surfName, cell))
    local bgCol = data.applyAlpha(GetButtonColor(surfName, cell.name), data.vars.btn_transparency)
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)

    local offsetX = (bw - visualW) / 2
    local visualX = cursorX + offsetX
    local pad2 = 2
    local centerX = visualX + visualW / 2
    local centerY = drawY + bh / 2
    local segments = 18

    local innerW = visualW - pad2 * 2
    local innerH = bh - pad2 * 2
    local isCircle = math.abs(innerW - innerH) < 2

    local function drawStadiumPath(inset)
        if isCircle then
            local radius = math.min(innerW, innerH) / 2 - inset
            for index = 0, segments * 2 - 1 do
                local angle = (index / (segments * 2)) * math.pi * 2
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
        elseif innerW > innerH then
            local radius = innerH / 2 - inset
            local bodyLeft = visualX + pad2 + radius + inset
            local bodyRight = visualX + visualW - pad2 - radius - inset
            for index = 0, segments do
                local angle = -math.pi / 2 + (index / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    bodyRight + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
            for index = 0, segments do
                local angle = math.pi / 2 + (index / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    bodyLeft + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
        else
            local radius = innerW / 2 - inset
            local bodyTop = drawY + pad2 + radius + inset
            local bodyBot = drawY + bh - pad2 - radius - inset
            for index = 0, segments do
                local angle = math.pi + (index / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    bodyTop + radius * math.sin(angle))
            end
            for index = 0, segments do
                local angle = (index / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    bodyBot + radius * math.sin(angle))
            end
        end
    end

    drawStadiumPath(0)
    imgui.DrawList_PathFillConvex(drawList, bgCol)
    RenderCenteredWrappedText(ctx, drawList, label, centerX, centerY, visualW - 12, bh)

    local function roundHitTest(mouseX, mouseY)
        return PointInRoundedShape(mouseX, mouseY, visualX, drawY, visualW, bh, 2)
    end

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    DrawButtonInteraction(ctx, surfName, cell, bw, bh, label, roundHitTest)
end

local function DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, direction, yOffset)
    local label = data.getProcessedLabel(GetButtonLabel(surfName, cell))
    local bgCol = data.applyAlpha(GetButtonColor(surfName, cell.name), data.vars.btn_transparency)
    local value = GetButtonValue(surfName, cell.name)
    if value > 0 then bgCol = data.applyAlpha(data.COLORS.arrow_on, data.vars.btn_transparency) end

    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)
    local halfAngleRad = math.rad(data.vars.arrow_angle / 2)
    local pointDepth
    if direction == "left" or direction == "right" then
        pointDepth = (bh / 2) / math.tan(halfAngleRad)
        pointDepth = math.min(pointDepth, bw * 0.45)
    else
        pointDepth = (bw / 2) / math.tan(halfAngleRad)
        pointDepth = math.min(pointDepth, bh * 0.45)
    end

    local function drawArrowPath()
        if direction == "left" then
            local bodyL = cursorX + pointDepth
            local bodyR = cursorX + bw
            imgui.DrawList_PathLineTo(drawList, bodyL, drawY)
            imgui.DrawList_PathLineTo(drawList, bodyR, drawY)
            imgui.DrawList_PathLineTo(drawList, bodyR, drawY + bh)
            imgui.DrawList_PathLineTo(drawList, bodyL, drawY + bh)
            imgui.DrawList_PathLineTo(drawList, cursorX, drawY + bh / 2)
        elseif direction == "right" then
            local bodyL = cursorX
            local bodyR = cursorX + bw - pointDepth
            imgui.DrawList_PathLineTo(drawList, bodyL, drawY)
            imgui.DrawList_PathLineTo(drawList, bodyR, drawY)
            imgui.DrawList_PathLineTo(drawList, cursorX + bw, drawY + bh / 2)
            imgui.DrawList_PathLineTo(drawList, bodyR, drawY + bh)
            imgui.DrawList_PathLineTo(drawList, bodyL, drawY + bh)
        elseif direction == "up" then
            local bodyT = drawY + pointDepth
            local bodyB = drawY + bh
            imgui.DrawList_PathLineTo(drawList, cursorX + bw / 2, drawY)
            imgui.DrawList_PathLineTo(drawList, cursorX + bw, bodyT)
            imgui.DrawList_PathLineTo(drawList, cursorX + bw, bodyB)
            imgui.DrawList_PathLineTo(drawList, cursorX, bodyB)
            imgui.DrawList_PathLineTo(drawList, cursorX, bodyT)
        elseif direction == "down" then
            local bodyT = drawY
            local bodyB = drawY + bh - pointDepth
            imgui.DrawList_PathLineTo(drawList, cursorX, bodyT)
            imgui.DrawList_PathLineTo(drawList, cursorX + bw, bodyT)
            imgui.DrawList_PathLineTo(drawList, cursorX + bw, bodyB)
            imgui.DrawList_PathLineTo(drawList, cursorX + bw / 2, drawY + bh)
            imgui.DrawList_PathLineTo(drawList, cursorX, bodyB)
        end
    end

    drawArrowPath()
    imgui.DrawList_PathFillConvex(drawList, bgCol)

    local labelCX, labelCY
    if direction == "left" then
        labelCX = cursorX + pointDepth + (bw - pointDepth) / 2
        labelCY = drawY + bh / 2
    elseif direction == "right" then
        labelCX = cursorX + (bw - pointDepth) / 2
        labelCY = drawY + bh / 2
    elseif direction == "up" then
        labelCX = cursorX + bw / 2
        labelCY = drawY + pointDepth + (bh - pointDepth) / 2
    elseif direction == "down" then
        labelCX = cursorX + bw / 2
        labelCY = drawY + (bh - pointDepth) / 2
    else
        labelCX = cursorX + bw / 2
        labelCY = drawY + bh / 2
    end

    local bodyW = (direction == "left" or direction == "right") and (bw - pointDepth) or bw
    RenderCenteredWrappedText(ctx, drawList, label, labelCX, labelCY, bodyW - 8, bh)

    local function arrowHitTest(mouseX, mouseY)
        return PointInArrowShape(mouseX, mouseY, cursorX, drawY, bw, bh, direction, pointDepth)
    end

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    DrawButtonInteraction(ctx, surfName, cell, bw, bh, label, arrowHitTest)
end

local function DrawFaderControl(ctx, drawList, surfName, cell, bw, visualH, hitH, yOffset)
    hitH = hitH or visualH
    local label = data.getProcessedLabel(GetButtonLabel(surfName, cell))
    local baseColor = GetButtonColor(surfName, cell.name, cell)
    local btnAlpha = data.vars.btn_transparency
    local bgCol = data.applyAlpha(baseColor, btnAlpha)
    local value = math.max(0.0, math.min(1.0, GetButtonValue(surfName, cell.name) or 0.0))

    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)
    imgui.DrawList_AddRectFilled(drawList, cursorX, drawY, cursorX + bw, drawY + visualH, bgCol, 4)

    local pad = 8
    local labelH = 16
    local trackL = cursorX + bw * 0.35
    local trackR = cursorX + bw * 0.65
    local trackT = drawY + pad
    local trackB = drawY + visualH - pad - labelH

    local trackBg = data.applyAlpha(data.dimColor(baseColor, 0.35), btnAlpha)
    imgui.DrawList_AddRectFilled(drawList, trackL, trackT, trackR, trackB, trackBg, 3)

    local fillTop = trackB - (trackB - trackT) * value
    local fillCol = data.applyAlpha(data.brightenColor(baseColor, 25), btnAlpha)
    imgui.DrawList_AddRectFilled(drawList, trackL, fillTop, trackR, trackB, fillCol, 3)

    local knobH = 8
    local knobCol = data.applyAlpha(0xDDDDDDff, btnAlpha)
    imgui.DrawList_AddRectFilled(drawList, trackL - 4, fillTop - knobH / 2, trackR + 4, fillTop + knobH / 2, knobCol, 2)

    RenderCenteredWrappedText(ctx, drawList, label, cursorX + bw / 2, drawY + visualH - 9, bw - 8, 16)
    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    DrawButtonInteraction(ctx, surfName, cell, bw, hitH, label)
end

function M.RenderOSDBar(ctx)
    osd_ui.PollOSD()
    
    if osd_ui.vars.osk_bar_position == "off" then
        return
    end

    local drawList = imgui.GetWindowDrawList(ctx)
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local availWidth = imgui.GetContentRegionAvail(ctx)

    imgui.PushFont(ctx, FONT_SMALL)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local padV = 4
    local barH = lineH + padV * 2

    local bgCol = osd_ui.state.bgColor
    if osd_ui.vars.osd_transparency then
        bgCol = (bgCol & 0xFFFFFF00) | math.floor((osd_ui.vars.osd_transparency / 100) * 255)
    end
    imgui.DrawList_AddRectFilled(drawList, cursorX, cursorY, cursorX + availWidth, cursorY + barH, bgCol, 0)

    local textCol = osd_ui.getContrastTextColor(string.format("#%06X", (osd_ui.state.bgColor >> 8) & 0xFFFFFF))
    -- Keep text fully opaque for readability even when bar background is transparent.
    textCol = (textCol & 0xFFFFFF00) | 0xFF

    local shownText = osd_ui.state.text or ""
    local textWidth = imgui.CalcTextSize(ctx, shownText)
    local textX = cursorX + (availWidth - textWidth) / 2
    local textY = cursorY + padV
    imgui.DrawList_AddText(drawList, textX, textY, textCol, shownText)

    imgui.Dummy(ctx, 0, barH)
    imgui.PopFont(ctx)
end

local function getCellMetrics(cell)
    local shape = (cell.shape or "Rect"):lower()
    local heightFactor = cell.height or 1.0
    local rowSpan = cell.rowSpan or 1

    if shape == "fader" then
        rowSpan = math.max(1, math.floor((cell.height or 1.0) + 0.5))
        heightFactor = 1.0
    else
        rowSpan = math.max(1, math.floor((rowSpan or 1) + 0.5))
        heightFactor = math.max(0.1, heightFactor)
    end

    return shape, heightFactor, rowSpan
end

function M.RenderSurface(ctx, surfName)
    FlushPendingWheelCommands()

    local layout = data.layouts[surfName]
    if not layout then
        imgui.Text(ctx, "Waiting for layout data from " .. surfName .. "...")
        return
    end

    local drawList = imgui.GetWindowDrawList(ctx)
    local baseW = BUTTON_SIZE * data.vars.zoom * data.vars.aspect
    local baseH = BUTTON_SIZE * data.vars.zoom
    local padH = data.vars.pad_h * data.vars.zoom
    local padV = data.vars.pad_v * data.vars.zoom

    imgui.PushStyleVar(ctx, imgui.StyleVar_ItemSpacing, 0, 0)

    local activeRowSpans = {}

    for rowIdx, row in ipairs(layout) do
        local rowMaxH = baseH
        for _, cell in ipairs(row) do
            if not cell.isSpacer then
                local _, heightFactor = getCellMetrics(cell)
                local cellH = baseH * heightFactor
                local topPad = math.max(0, (cell.top or 0.0) * baseH)
                local cellExtentH = cellH + topPad
                if cellExtentH > rowMaxH then rowMaxH = cellExtentH end
            end
        end

        local renderItems = {}
        local col = 1
        for _, cell in ipairs(row) do
            while activeRowSpans[col] and activeRowSpans[col].remaining > 0 do
                renderItems[#renderItems + 1] = { isOccupied = true, spanCell = activeRowSpans[col].cell }
                col = col + 1
            end
            renderItems[#renderItems + 1] = { cell = cell }
            if not cell.isSpacer then
                local _, _, rowSpan = getCellMetrics(cell)
                if rowSpan > 1 then
                    activeRowSpans[col] = {
                        cell = cell,
                        remaining = math.max((activeRowSpans[col] and activeRowSpans[col].remaining) or 0, rowSpan - 1),
                        newlyAdded = true,
                    }
                end
            end
            col = col + 1
        end

        local maxOccupiedCol = 0
        for c, spanInfo in pairs(activeRowSpans) do
            if spanInfo.remaining > 0 and c > maxOccupiedCol then
                maxOccupiedCol = c
            end
        end
        while col <= maxOccupiedCol do
            if activeRowSpans[col] and activeRowSpans[col].remaining > 0 then
                renderItems[#renderItems + 1] = { isOccupied = true, spanCell = activeRowSpans[col].cell }
            else
                renderItems[#renderItems + 1] = { isOccupied = true }
            end
            col = col + 1
        end

        for itemIdx, item in ipairs(renderItems) do
            if itemIdx > 1 then
                imgui.SameLine(ctx, 0, padH)
            end

            if item.isOccupied then
                if item.spanCell and not item.spanCell.isSpacer then
                    imgui.Dummy(ctx, baseW * (item.spanCell.width or 1.0), rowMaxH)
                else
                    imgui.Dummy(ctx, baseW, rowMaxH)
                end
            else
                local cell = item.cell
                if cell.isSpacer then
                    imgui.Dummy(ctx, baseW * (cell.width or 0.5), rowMaxH)
                else
                    local bw = baseW * (cell.width or 1.0)
                    local shape, heightFactor, rowSpan = getCellMetrics(cell)
                    local bh = baseH * heightFactor
                    local topPad = math.max(0, (cell.top or 0.0) * baseH)

                    if shape == "round" then
                        local visualW = baseH * (cell.width or 1.0)
                        DrawRoundButton(ctx, drawList, surfName, cell, bw, bh, visualW, topPad)
                    elseif shape == "leftarrow" then
                        DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "left", topPad)
                    elseif shape == "rightarrow" then
                        DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "right", topPad)
                    elseif shape == "uparrow" then
                        DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "up", topPad)
                    elseif shape == "downarrow" then
                        DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "down", topPad)
                    elseif shape == "fader" then
                        if rowSpan > 1 then
                            local visualH = baseH * heightFactor * rowSpan + padV * (rowSpan - 1)
                            DrawFaderControl(ctx, drawList, surfName, cell, bw, visualH, bh, topPad)
                        else
                            DrawFaderControl(ctx, drawList, surfName, cell, bw, bh, nil, topPad)
                        end
                    else
                        DrawRectButton(ctx, drawList, surfName, cell, bw, bh, topPad)
                    end
                end
            end
        end

        for colIndex, spanInfo in pairs(activeRowSpans) do
            if spanInfo.newlyAdded then
                spanInfo.newlyAdded = false
            else
                spanInfo.remaining = spanInfo.remaining - 1
                if spanInfo.remaining <= 0 then
                    activeRowSpans[colIndex] = nil
                end
            end
        end

        if rowIdx < #layout then
            imgui.Dummy(ctx, 0, padV)
        end
    end

    imgui.PopStyleVar(ctx)
end

function M.RenderAllSurfaces(ctx)
    if #data.surfaces == 0 then return end
    for index, surfName in ipairs(data.surfaces) do
        imgui.PushFont(ctx, FONT_SMALL)
        imgui.Text(ctx, surfName)
        imgui.PopFont(ctx)
        M.RenderSurface(ctx, surfName)
        if index < #data.surfaces then
            imgui.Separator(ctx)
        end
    end
end

return M
