local imgui = require "imgui" "0.9.3"

local data = require("osk_data")
local osd_ui = require("osd_ui")
local osk_input = require("osk_input")
local osk_widget_drawers = require("osk_widget_drawers")
local math_utils = require("osk_widget_math")
local theme = require("theme_settings")

local M = {}

local FONT = nil
local FONT_SMALL = nil
local configModule = nil

function M.SetFonts(font, fontSmall)
    FONT = font
    FONT_SMALL = fontSmall
    osd_ui.SetFont(fontSmall)
    osk_widget_drawers.SetFonts(font, fontSmall)
end

function M.SetConfigModule(module)
    configModule = module
    osk_widget_drawers.SetConfigModule(module)
end

local function getStateAndCellColors(surfName, widgetName, cellOverride)
    local widgetState = data.states[surfName] and data.states[surfName][widgetName]
    local cell = cellOverride or data.GetCellInfo(surfName, widgetName)
    local stateColor = widgetState and theme.IsMeaningfulColor(widgetState.color) and widgetState.color or nil
    local cellColor = cell and cell.color or nil
    return widgetState, stateColor, cellColor
end

local function applyInactiveButtonBoost(color)
    if not color or not theme.IsMeaningfulColor(color) then return color end
    return theme.ApplyInactiveLedBoost(color, theme.osk.inactive_led_boost)
end

local function getButtonColor(surfName, widgetName, cellOverride)
    local widgetState, stateColor, cellColor = getStateAndCellColors(surfName, widgetName, cellOverride)

    if widgetState and widgetState.value > 0 then
        if stateColor then return stateColor end
        if cellColor then return cellColor end
        return theme.OSK_COLORS.button_on
    end

    if stateColor then return applyInactiveButtonBoost(stateColor) end

    local color = cellColor and theme.DimColor(cellColor, 0.40) or theme.OSK_COLORS.button_off
    color = theme.EnsureMinLuminance(color, theme.WIDGET.min_button_luminance)
    if cellColor and theme.IsMeaningfulColor(cellColor) then return applyInactiveButtonBoost(color) end
    return color
end

local function getFaderColor(surfName, widgetName, cell)
    local _, stateColor, cellColor = getStateAndCellColors(surfName, widgetName, cell)
    if stateColor then return stateColor end
    if cellColor then return cellColor end
    return theme.EnsureMinLuminance(theme.OSK_COLORS.button_off, theme.WIDGET.min_fader_luminance)
end

local function getButtonValue(surfName, widgetName)
    local state = data.states[surfName] and data.states[surfName][widgetName]
    if state then return state.value end
    return 0
end

local function getContinuousKind(surfName, widgetName)
    return data.GetStateKind(surfName, widgetName)
end

local function getFaderValueInfo(surfName, widgetName)
    if not data.HasStateValue(surfName, widgetName) then
        data.DebugFader(surfName, widgetName, "render value unavailable", 1.50, "render")
        return 0.0, nil, false
    end
    local rawValue = getButtonValue(surfName, widgetName)
    local localValue = data.GetFaderLocalValue(surfName, widgetName, rawValue)
    if localValue then
        data.DebugFader(surfName, widgetName, string.format("render value raw=%.6f mode=local-shadow normalized=%.6f", rawValue, localValue), 1.50, "render")
        if rawValue < 0.0 or rawValue > 1.0 then
            return math_utils.ClampNormalized(localValue), function(value) return math_utils.NormalizedToDb(value) end, true
        end
        return math_utils.ClampNormalized(localValue), nil, true
    end
    if rawValue < 0.0 or rawValue > 1.0 then
        local normalizedValue = math_utils.DbToNormalized(rawValue)
        data.DebugFader(surfName, widgetName, string.format("render value raw=%.6f mode=db normalized=%.6f", rawValue, normalizedValue), 1.50, "render")
        return normalizedValue, function(value) return math_utils.NormalizedToDb(value) end, true
    end
    local normalizedValue = math_utils.ClampNormalized(rawValue)
    data.DebugFader(surfName, widgetName, string.format("render value raw=%.6f mode=normalized normalized=%.6f", rawValue, normalizedValue), 1.50, "render")
    return normalizedValue, nil, true
end

local function getButtonLabel(surfName, cell)
    local label = data.labels[surfName] and data.labels[surfName][cell.name]
    if label and label ~= "" then return label end
    if cell.label and cell.label ~= "" then return cell.label end
    return cell.name or "?"
end

function M.RenderOSDBar(ctx, surfName)
    if osd_ui.GetOSKBarPosition(surfName) == "off" then
        return
    end

    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local availWidth = imgui.GetContentRegionAvail(ctx)

    imgui.PushFont(ctx, FONT_SMALL)
    local _, lineHeight = imgui.CalcTextSize(ctx, "M")
    imgui.PopFont(ctx)
    local barHeight = lineHeight + theme.WIDGET.osd_bar_padding
    osd_ui.DrawOSDRect(ctx, imgui, cursorX, cursorY, availWidth, barHeight, osd_ui.state.text or "", osd_ui.state.bgColor, osd_ui.vars.osd_transparency, FONT_SMALL)
    imgui.Dummy(ctx, 0, barHeight)
end

local function getCellMetrics(cell)
    local shape = (cell.shape or "Rect"):lower()
    local role = tostring(cell.role or ""):lower()
    local heightFactor = cell.height or 1.0
    local rowSpan = cell.rowSpan or 1

    if role == "fader" or shape == "fader" then
        rowSpan = math.max(1, math.floor((cell.height or 1.0) + 0.5))
        heightFactor = 1.0
    else
        rowSpan = math.max(1, math.floor((rowSpan or 1) + 0.5))
        heightFactor = math.max(0.1, heightFactor)
    end

    return shape, heightFactor, rowSpan
end

function M.RenderSurface(ctx, surfName)
    osk_input.FlushWheelCommands()

    local layout = data.layouts[surfName]
    if not layout then
        imgui.Text(ctx, "Waiting for layout data from " .. surfName .. "...")
        return
    end

    osk_widget_drawers.Configure({
        data = data,
        getButtonColor = getButtonColor,
        getFaderColor = getFaderColor,
        getFaderValueInfo = getFaderValueInfo,
        getButtonLabel = getButtonLabel,
        getContinuousKind = getContinuousKind,
        configModule = configModule,
    })

    local drawList = imgui.GetWindowDrawList(ctx)
    local baseWidth = theme.WIDGET.base_button_size * theme.osk.zoom * theme.osk.aspect
    local baseHeight = theme.WIDGET.base_button_size * theme.osk.zoom
    local padH = theme.osk.pad_h * theme.osk.zoom
    local padV = theme.osk.pad_v * theme.osk.zoom

    imgui.PushStyleVar(ctx, imgui.StyleVar_ItemSpacing, 0, 0)

    local activeRowSpans = {}

    for rowIndex, row in ipairs(layout) do
        local rowMaxHeight = baseHeight
        for _, cell in ipairs(row) do
            if not cell.isSpacer then
                local _, heightFactor = getCellMetrics(cell)
                local cellHeight = baseHeight * heightFactor
                local topPad = math.max(0, (cell.top or 0.0) * baseHeight)
                local cellExtentHeight = cellHeight + topPad
                if cellExtentHeight > rowMaxHeight then rowMaxHeight = cellExtentHeight end
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
        for occupiedCol, spanInfo in pairs(activeRowSpans) do
            if spanInfo.remaining > 0 and occupiedCol > maxOccupiedCol then
                maxOccupiedCol = occupiedCol
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

        for itemIndex, item in ipairs(renderItems) do
            if itemIndex > 1 then
                imgui.SameLine(ctx, 0, padH)
            end

            if item.isOccupied then
                if item.spanCell and not item.spanCell.isSpacer then
                    imgui.Dummy(ctx, baseWidth * (item.spanCell.width or 1.0), rowMaxHeight)
                else
                    imgui.Dummy(ctx, baseWidth, rowMaxHeight)
                end
            else
                local cell = item.cell
                if cell.isSpacer then
                    imgui.Dummy(ctx, baseWidth * (cell.width or 0.5), rowMaxHeight)
                else
                    local buttonWidth = baseWidth * (cell.width or 1.0)
                    local shape, heightFactor, rowSpan = getCellMetrics(cell)
                    local buttonHeight = baseHeight * heightFactor
                    local topPad = math.max(0, (cell.top or 0.0) * baseHeight)

                    if data.IsRotaryWidget(surfName, cell.name) then
                        osk_widget_drawers.DrawRotaryControl(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, topPad)
                    elseif data.IsFaderWidget(surfName, cell.name) then
                        if rowSpan > 1 then
                            local visualHeight = baseHeight * heightFactor * rowSpan + padV * (rowSpan - 1)
                            osk_widget_drawers.DrawFaderControl(ctx, drawList, surfName, cell, buttonWidth, visualHeight, buttonHeight, topPad)
                        else
                            osk_widget_drawers.DrawFaderControl(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, nil, topPad)
                        end
                    elseif shape == "round" then
                        osk_widget_drawers.DrawRoundButton(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, baseHeight * (cell.width or 1.0), topPad)
                    elseif shape == "leftarrow" then
                        osk_widget_drawers.DrawArrowButton(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, "left", topPad)
                    elseif shape == "rightarrow" then
                        osk_widget_drawers.DrawArrowButton(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, "right", topPad)
                    elseif shape == "uparrow" then
                        osk_widget_drawers.DrawArrowButton(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, "up", topPad)
                    elseif shape == "downarrow" then
                        osk_widget_drawers.DrawArrowButton(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, "down", topPad)
                    else
                        osk_widget_drawers.DrawRectButton(ctx, drawList, surfName, cell, buttonWidth, buttonHeight, topPad)
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

        if rowIndex < #layout then
            imgui.Dummy(ctx, 0, padV)
        end
    end

    imgui.PopStyleVar(ctx)
end

return M
