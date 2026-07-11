local imgui = require "imgui" "0.9.3"

local math_utils = require("osk_widget_math")
local osk_draw = require("osk_draw_primitives")
local osk_input = require("osk_input")
local theme = require("theme_settings")
local ui = require("ui_components")

local M = {}

local deps = {
    data = nil,
    font = nil,
    fontSmall = nil,
    configModule = nil,
    getButtonColor = nil,
    getFaderColor = nil,
    getFaderValueInfo = nil,
    getButtonLabel = nil,
    getContinuousKind = nil,
}

local hoverStartTime = {}

function M.Configure(newDeps)
    for key, value in pairs(newDeps or {}) do
        deps[key] = value
    end
end

function M.SetFonts(font, fontSmall)
    deps.font = font
    deps.fontSmall = fontSmall
end

function M.SetConfigModule(module)
    deps.configModule = module
end

local function getTextColor(baseColor)
    return theme.ApplyAlpha(baseColor or theme.OSK_COLORS.text_normal, theme.osk.btn_transparency)
end

local function appendTooltipLines(lines, modMap, heading)
    if not modMap or not next(modMap) then return end
    if heading and heading ~= "" then lines[#lines + 1] = heading end
    if modMap["NoMod"] then
        lines[#lines + 1] = deps.data.getProcessedLabel(modMap["NoMod"])
    end
    local sortedMods = {}
    for key in pairs(modMap) do
        if key ~= "NoMod" then sortedMods[#sortedMods + 1] = key end
    end
    table.sort(sortedMods)
    for _, modName in ipairs(sortedMods) do
        lines[#lines + 1] = "+ " .. modName .. " -> " .. deps.data.getProcessedLabel(modMap[modName])
    end
end

local function showDelayedTooltip(ctx, surfName, widgetName, text, pressTarget)
    local stateKey = math_utils.GetInteractionStateKey(surfName, widgetName)
    if not stateKey then return end
    if imgui.IsItemHovered(ctx) then
        local now = os.clock()
        if not hoverStartTime[stateKey] then
            hoverStartTime[stateKey] = now
        end
        if now - hoverStartTime[stateKey] >= deps.data.vars.tooltip_delay then
            local modMap = surfName and deps.data.labelMaps[surfName] and deps.data.labelMaps[surfName][widgetName]
            local pushMap = pressTarget and pressTarget ~= "" and pressTarget ~= widgetName
                and deps.data.labelMaps[surfName] and deps.data.labelMaps[surfName][pressTarget]
            local tooltipText = text
            if (modMap and next(modMap)) or (pushMap and next(pushMap)) then
                local lines = {}
                appendTooltipLines(lines, modMap)
                appendTooltipLines(lines, pushMap, "Push:")
                if #lines > 0 then
                    tooltipText = table.concat(lines, "\n")
                end
            end
            ui.Tooltip(ctx, tooltipText)
        end
    else
        hoverStartTime[stateKey] = nil
    end
end

local function drawButtonInteraction(ctx, surfName, cell, width, height, label, hitTestFn)
    local id = "##btn_" .. (cell.name or "")
    imgui.InvisibleButton(ctx, id, width, height)

    local hovered = imgui.IsItemHovered(ctx)
    local insideShape = hovered
    if hovered and hitTestFn then
        local mouseX, mouseY = imgui.GetMousePos(ctx)
        insideShape = hitTestFn(mouseX, mouseY)
    end

    if insideShape then
        showDelayedTooltip(ctx, surfName, cell.name or "", label, deps.data.GetPressTarget(surfName, cell))
        osk_input.HandleWheel(ctx, surfName, cell)
    elseif cell.name then
        local stateKey = math_utils.GetInteractionStateKey(surfName, cell.name)
        if stateKey then hoverStartTime[stateKey] = nil end
    end

    local mouseButtonRight = imgui.MouseButton_Right or 1
    if insideShape and cell.name and deps.configModule and deps.configModule.OpenConfigEditor and imgui.IsItemClicked(ctx, mouseButtonRight) then
        deps.configModule.OpenConfigEditor(surfName, cell.name)
    end

    if insideShape and imgui.IsItemActivated(ctx) then
        osk_input.HandlePressDown(surfName, cell)
    end
    if imgui.IsItemDeactivated(ctx) then
        osk_input.HandlePressUp(surfName, cell)
    end
end

local function drawFaderInteraction(ctx, surfName, cell, width, visualHeight, layoutHeight, label, currentValue, commandValueMapper, trackTop, trackBottom, valueHitTest)
    local id = "##fader_" .. (cell.name or "")
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    imgui.InvisibleButton(ctx, id, width, visualHeight)

    local hovered = imgui.IsItemHovered(ctx)
    local valueHitHovered = false
    if hovered and valueHitTest then
        local mouseX, mouseY = imgui.GetMousePos(ctx)
        valueHitHovered = valueHitTest(mouseX, mouseY)
    end
    if hovered then
        deps.data.DebugFader(surfName, cell.name or "", string.format("hover visualH=%.2f layoutH=%.2f currentNormalized=%.6f mapper=%s", visualHeight, layoutHeight, math_utils.ClampNormalized(currentValue), tostring(commandValueMapper ~= nil)), 0.50, "hover")
        showDelayedTooltip(ctx, surfName, cell.name or "", label, deps.data.GetPressTarget(surfName, cell))
    elseif cell.name then
        local stateKey = math_utils.GetInteractionStateKey(surfName, cell.name)
        if stateKey then hoverStartTime[stateKey] = nil end
    end

    local mouseButtonRight = imgui.MouseButton_Right or 1
    if hovered and cell.name and deps.configModule and deps.configModule.OpenConfigEditor and imgui.IsItemClicked(ctx, mouseButtonRight) then
        deps.configModule.OpenConfigEditor(surfName, cell.name)
    end

    osk_input.HandleFader(ctx, surfName, cell, trackTop, trackBottom, currentValue, commandValueMapper, valueHitHovered)

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    imgui.Dummy(ctx, width, layoutHeight)
end

function M.DrawRectButton(ctx, drawList, surfName, cell, width, height, yOffset)
    local label = deps.data.getProcessedLabel(deps.getButtonLabel(surfName, cell))
    local bgColor = theme.ApplyAlpha(deps.getButtonColor(surfName, cell.name), theme.osk.btn_transparency)
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)

    imgui.DrawList_AddRectFilled(drawList, cursorX, drawY, cursorX + width, drawY + height, bgColor, theme.WIDGET.rect_corner_radius)
    osk_draw.DrawCenteredWrappedText(ctx, imgui, drawList, label, cursorX + width / 2, drawY + height / 2, width - theme.WIDGET.rect_text_padding, height, {
        font = deps.fontSmall,
        wrapTextFn = deps.data.wrapText,
        lineHeight = theme.osk.line_height,
        textColor = getTextColor(theme.OSK_COLORS.text_normal),
    })
    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    drawButtonInteraction(ctx, surfName, cell, width, height, label)
end

function M.DrawRoundButton(ctx, drawList, surfName, cell, width, height, visualWidth, yOffset)
    local label = deps.data.getProcessedLabel(deps.getButtonLabel(surfName, cell))
    local bgColor = theme.ApplyAlpha(deps.getButtonColor(surfName, cell.name), theme.osk.btn_transparency)
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)
    local offsetX = (width - visualWidth) / 2
    local visualX = cursorX + offsetX
    local centerX = visualX + visualWidth / 2
    local centerY = drawY + height / 2

    local innerWidth = visualWidth - theme.WIDGET.round_path_padding * 2
    local innerHeight = height - theme.WIDGET.round_path_padding * 2
    local isCircle = math.abs(innerWidth - innerHeight) < 2

    local function drawStadiumPath(inset)
        if isCircle then
            local radius = math.min(innerWidth, innerHeight) / 2 - inset
            for index = 0, theme.WIDGET.round_path_segments * 2 - 1 do
                local angle = (index / (theme.WIDGET.round_path_segments * 2)) * math.pi * 2
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
        elseif innerWidth > innerHeight then
            local radius = innerHeight / 2 - inset
            local bodyLeft = visualX + theme.WIDGET.round_path_padding + radius + inset
            local bodyRight = visualX + visualWidth - theme.WIDGET.round_path_padding - radius - inset
            for index = 0, theme.WIDGET.round_path_segments do
                local angle = -math.pi / 2 + (index / theme.WIDGET.round_path_segments) * math.pi
                imgui.DrawList_PathLineTo(drawList, bodyRight + radius * math.cos(angle), centerY + radius * math.sin(angle))
            end
            for index = 0, theme.WIDGET.round_path_segments do
                local angle = math.pi / 2 + (index / theme.WIDGET.round_path_segments) * math.pi
                imgui.DrawList_PathLineTo(drawList, bodyLeft + radius * math.cos(angle), centerY + radius * math.sin(angle))
            end
        else
            local radius = innerWidth / 2 - inset
            local bodyTop = drawY + theme.WIDGET.round_path_padding + radius + inset
            local bodyBottom = drawY + height - theme.WIDGET.round_path_padding - radius - inset
            for index = 0, theme.WIDGET.round_path_segments do
                local angle = math.pi + (index / theme.WIDGET.round_path_segments) * math.pi
                imgui.DrawList_PathLineTo(drawList, centerX + radius * math.cos(angle), bodyTop + radius * math.sin(angle))
            end
            for index = 0, theme.WIDGET.round_path_segments do
                local angle = (index / theme.WIDGET.round_path_segments) * math.pi
                imgui.DrawList_PathLineTo(drawList, centerX + radius * math.cos(angle), bodyBottom + radius * math.sin(angle))
            end
        end
    end

    drawStadiumPath(0)
    imgui.DrawList_PathFillConvex(drawList, bgColor)
    osk_draw.DrawCenteredWrappedText(ctx, imgui, drawList, label, centerX, centerY, visualWidth - theme.WIDGET.round_text_padding, height, {
        font = deps.fontSmall,
        wrapTextFn = deps.data.wrapText,
        lineHeight = theme.osk.line_height,
        textColor = getTextColor(theme.OSK_COLORS.text_normal),
    })

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    drawButtonInteraction(ctx, surfName, cell, width, height, label, function(mouseX, mouseY)
        return osk_draw.PointInRoundedShape(mouseX, mouseY, visualX, drawY, visualWidth, height, theme.WIDGET.round_hit_inset)
    end)
end

function M.DrawRotaryControl(ctx, drawList, surfName, cell, width, height, yOffset)
    local label = deps.data.getProcessedLabel(deps.getButtonLabel(surfName, cell))
    local baseColor = deps.getFaderColor(surfName, cell.name, cell)
    local value, _, hasValue = deps.getFaderValueInfo(surfName, cell.name)
    local continuousKind = deps.getContinuousKind(surfName, cell.name)
    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)
    local labelHeight = hasValue and theme.WIDGET.rotary_label_height or 0
    local valueTopPad = hasValue and math.max(theme.WIDGET.rotary_value_top_padding_min, height * theme.WIDGET.rotary_value_top_padding_ratio) or 0
    local visualY = drawY + valueTopPad
    local visualHeight = math.max(theme.WIDGET.rotary_min_visual_height, height - labelHeight - valueTopPad)
    local diameter = math.min(width, visualHeight) - theme.WIDGET.rect_corner_radius
    local radius = math.max(theme.WIDGET.fader_hit_min_radius, diameter / 2)
    local centerX = cursorX + width / 2
    local centerY = visualY + visualHeight / 2
    local bodyColor = theme.ApplyAlpha(theme.DimColor(baseColor, theme.WIDGET.rotary_body_dim_factor), theme.osk.btn_transparency)
    local activeColor = theme.ApplyAlpha(theme.BrightenColor(baseColor, theme.WIDGET.rotary_active_brighten), theme.osk.btn_transparency)
    local trackColor = theme.ApplyAlpha(theme.DimColor(baseColor, theme.WIDGET.rotary_track_dim_factor), theme.osk.btn_transparency)
    local startAngle = math.rad(theme.WIDGET.rotary_track_start_angle)
    local endAngle = math.rad(theme.WIDGET.rotary_track_end_angle)
    local currentAngle = startAngle + (endAngle - startAngle) * math_utils.ClampNormalized(value)

    imgui.DrawList_AddCircleFilled(drawList, centerX, centerY, radius, bodyColor, theme.WIDGET.rotary_circle_segments)
    if hasValue then
        local rotaryStyle = tostring(cell.rotaryStyle or "wiper"):lower()
        if rotaryStyle == "wiper" then
            osk_draw.DrawArc(imgui, drawList, centerX, centerY, radius + theme.WIDGET.rotary_outer_track_offset, startAngle, endAngle, trackColor, theme.WIDGET.rotary_track_thickness)
            osk_draw.DrawArc(imgui, drawList, centerX, centerY, radius + theme.WIDGET.rotary_outer_track_offset, startAngle, currentAngle, activeColor, theme.WIDGET.rotary_track_thickness)
            imgui.DrawList_AddCircleFilled(drawList, centerX, centerY, radius * theme.WIDGET.rotary_inner_circle_ratio, theme.ApplyAlpha(theme.DimColor(baseColor, theme.WIDGET.rotary_inner_dim_factor), theme.osk.btn_transparency), theme.WIDGET.rotary_inner_circle_segments)
        else
            local dotRadius = math.max(theme.WIDGET.rotary_value_dot_min_radius, radius * theme.WIDGET.rotary_value_dot_ratio)
            local dotX = centerX + math.cos(currentAngle) * radius * theme.WIDGET.rotary_value_dot_distance
            local dotY = centerY + math.sin(currentAngle) * radius * theme.WIDGET.rotary_value_dot_distance
            imgui.DrawList_AddCircleFilled(drawList, dotX, dotY, dotRadius, activeColor, theme.WIDGET.rotary_dot_segments)
        end
        if continuousKind == "V" or continuousKind == "P" then
            osk_draw.DrawCenteredSingleText(ctx, imgui, drawList, continuousKind, centerX, centerY, getTextColor(theme.WIDGET.text_on_dark), deps.font)
        end
        osk_draw.DrawCenteredWrappedText(ctx, imgui, drawList, label, cursorX + width / 2, visualY + visualHeight + labelHeight / 2, width - theme.WIDGET.rect_text_padding, labelHeight, {
            font = deps.fontSmall,
            wrapTextFn = deps.data.wrapText,
            lineHeight = theme.osk.line_height,
            textColor = getTextColor(theme.WIDGET.text_on_dark),
        })
    else
        osk_draw.DrawCenteredWrappedText(ctx, imgui, drawList, label, centerX, centerY, diameter - 6, diameter - 6, {
            font = deps.fontSmall,
            wrapTextFn = deps.data.wrapText,
            lineHeight = theme.osk.line_height,
            textColor = getTextColor(theme.WIDGET.text_on_dark),
        })
    end

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    drawButtonInteraction(ctx, surfName, cell, width, height + (yOffset or 0), label, function(mouseX, mouseY)
        local dx, dy = mouseX - centerX, mouseY - centerY
        return dx * dx + dy * dy <= radius * radius
    end)
end

function M.DrawArrowButton(ctx, drawList, surfName, cell, width, height, direction, yOffset)
    local label = deps.data.getProcessedLabel(deps.getButtonLabel(surfName, cell))
    local bgColor = theme.ApplyAlpha(deps.getButtonColor(surfName, cell.name), theme.osk.btn_transparency)
    if (deps.data.states[surfName] and deps.data.states[surfName][cell.name] and deps.data.states[surfName][cell.name].value or 0) > 0 then
        bgColor = theme.ApplyAlpha(theme.OSK_COLORS.arrow_on, theme.osk.btn_transparency)
    end

    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)
    local halfAngleRad = math.rad(theme.osk.arrow_angle / 2)
    local pointDepth
    if direction == "left" or direction == "right" then
        pointDepth = (height / 2) / math.tan(halfAngleRad)
        pointDepth = math.min(pointDepth, width * theme.WIDGET.arrow_depth_ratio)
    else
        pointDepth = (width / 2) / math.tan(halfAngleRad)
        pointDepth = math.min(pointDepth, height * theme.WIDGET.arrow_depth_ratio)
    end

    local function drawArrowPath()
        if direction == "left" then
            local bodyLeft = cursorX + pointDepth
            local bodyRight = cursorX + width
            imgui.DrawList_PathLineTo(drawList, bodyLeft, drawY)
            imgui.DrawList_PathLineTo(drawList, bodyRight, drawY)
            imgui.DrawList_PathLineTo(drawList, bodyRight, drawY + height)
            imgui.DrawList_PathLineTo(drawList, bodyLeft, drawY + height)
            imgui.DrawList_PathLineTo(drawList, cursorX, drawY + height / 2)
        elseif direction == "right" then
            local bodyRight = cursorX + width - pointDepth
            imgui.DrawList_PathLineTo(drawList, cursorX, drawY)
            imgui.DrawList_PathLineTo(drawList, bodyRight, drawY)
            imgui.DrawList_PathLineTo(drawList, cursorX + width, drawY + height / 2)
            imgui.DrawList_PathLineTo(drawList, bodyRight, drawY + height)
            imgui.DrawList_PathLineTo(drawList, cursorX, drawY + height)
        elseif direction == "up" then
            local bodyTop = drawY + pointDepth
            imgui.DrawList_PathLineTo(drawList, cursorX + width / 2, drawY)
            imgui.DrawList_PathLineTo(drawList, cursorX + width, bodyTop)
            imgui.DrawList_PathLineTo(drawList, cursorX + width, drawY + height)
            imgui.DrawList_PathLineTo(drawList, cursorX, drawY + height)
            imgui.DrawList_PathLineTo(drawList, cursorX, bodyTop)
        else
            local bodyBottom = drawY + height - pointDepth
            imgui.DrawList_PathLineTo(drawList, cursorX, drawY)
            imgui.DrawList_PathLineTo(drawList, cursorX + width, drawY)
            imgui.DrawList_PathLineTo(drawList, cursorX + width, bodyBottom)
            imgui.DrawList_PathLineTo(drawList, cursorX + width / 2, drawY + height)
            imgui.DrawList_PathLineTo(drawList, cursorX, bodyBottom)
        end
    end

    drawArrowPath()
    imgui.DrawList_PathFillConvex(drawList, bgColor)

    local labelCenterX, labelCenterY
    if direction == "left" then
        labelCenterX = cursorX + pointDepth + (width - pointDepth) / 2
        labelCenterY = drawY + height / 2
    elseif direction == "right" then
        labelCenterX = cursorX + (width - pointDepth) / 2
        labelCenterY = drawY + height / 2
    elseif direction == "up" then
        labelCenterX = cursorX + width / 2
        labelCenterY = drawY + pointDepth + (height - pointDepth) / 2
    else
        labelCenterX = cursorX + width / 2
        labelCenterY = drawY + (height - pointDepth) / 2
    end

    local bodyWidth = (direction == "left" or direction == "right") and (width - pointDepth) or width
    osk_draw.DrawCenteredWrappedText(ctx, imgui, drawList, label, labelCenterX, labelCenterY, bodyWidth - theme.WIDGET.rect_text_padding, height, {
        font = deps.fontSmall,
        wrapTextFn = deps.data.wrapText,
        lineHeight = theme.osk.line_height,
        textColor = getTextColor(theme.OSK_COLORS.text_normal),
    })

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    drawButtonInteraction(ctx, surfName, cell, width, height, label, function(mouseX, mouseY)
        return osk_draw.PointInArrowShape(mouseX, mouseY, cursorX, drawY, width, height, direction, pointDepth)
    end)
end

function M.DrawFaderControl(ctx, drawList, surfName, cell, width, visualHeight, hitHeight, yOffset)
    local layoutHeight = hitHeight or visualHeight
    local label = deps.data.getProcessedLabel(deps.getButtonLabel(surfName, cell))
    local baseColor = deps.getFaderColor(surfName, cell.name, cell)
    local value, commandValueMapper = deps.getFaderValueInfo(surfName, cell.name)

    local cursorX, cursorY = imgui.GetCursorScreenPos(ctx)
    local drawY = cursorY + (yOffset or 0)
    imgui.DrawList_AddRectFilled(drawList, cursorX, drawY, cursorX + width, drawY + visualHeight, theme.ApplyAlpha(baseColor, theme.osk.btn_transparency), theme.WIDGET.rect_corner_radius)

    local trackLeft = cursorX + width * theme.WIDGET.fader_track_left_ratio
    local trackRight = cursorX + width * theme.WIDGET.fader_track_right_ratio
    local trackTop = drawY + theme.WIDGET.fader_padding
    local trackBottom = drawY + visualHeight - theme.WIDGET.fader_padding - theme.WIDGET.fader_label_height
    imgui.DrawList_AddRectFilled(drawList, trackLeft, trackTop, trackRight, trackBottom, theme.ApplyAlpha(theme.WIDGET.fader_track_bg, theme.osk.btn_transparency), theme.WIDGET.fader_track_rounding)

    local fillTop = trackBottom - (trackBottom - trackTop) * value
    imgui.DrawList_AddRectFilled(drawList, trackLeft, fillTop, trackRight, trackBottom, theme.ApplyAlpha(theme.WIDGET.fader_fill, theme.osk.btn_transparency), theme.WIDGET.fader_track_rounding)

    local knobLeft = trackLeft - theme.WIDGET.fader_knob_width_pad
    local knobRight = trackRight + theme.WIDGET.fader_knob_width_pad
    local knobTop = fillTop - theme.WIDGET.fader_knob_height / 2
    local knobBottom = fillTop + theme.WIDGET.fader_knob_height / 2
    imgui.DrawList_AddRectFilled(drawList, knobLeft, knobTop, knobRight, knobBottom, theme.ApplyAlpha(theme.WIDGET.fader_knob, theme.osk.btn_transparency), theme.WIDGET.fader_knob_rounding)

    osk_draw.DrawCenteredWrappedText(ctx, imgui, drawList, label, cursorX + width / 2, drawY + visualHeight - theme.WIDGET.fader_label_center_y_offset, width - theme.WIDGET.rect_text_padding, theme.WIDGET.fader_label_height, {
        font = deps.fontSmall,
        wrapTextFn = deps.data.wrapText,
        lineHeight = theme.osk.line_height,
        textColor = getTextColor(theme.OSK_COLORS.text_normal),
    })

    imgui.SetCursorScreenPos(ctx, cursorX, cursorY)
    drawFaderInteraction(ctx, surfName, cell, width, visualHeight, layoutHeight, label, value, commandValueMapper, trackTop, trackBottom, function(mouseX, mouseY)
        local onTrack = mouseX >= trackLeft and mouseX <= trackRight and mouseY >= trackTop and mouseY <= trackBottom
        local onKnob = mouseX >= knobLeft and mouseX <= knobRight and mouseY >= knobTop and mouseY <= knobBottom
        return onTrack or onKnob
    end)
end

return M
