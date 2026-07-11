local theme = require("theme_settings")

local M = {}

function M.PointInTriangle(px, py, ax, ay, bx, by, cx, cy)
    local v0x, v0y = cx - ax, cy - ay
    local v1x, v1y = bx - ax, by - ay
    local v2x, v2y = px - ax, py - ay
    local dot00 = v0x * v0x + v0y * v0y
    local dot01 = v0x * v1x + v0y * v1y
    local dot02 = v0x * v2x + v0y * v2y
    local dot11 = v1x * v1x + v1y * v1y
    local dot12 = v1x * v2x + v1y * v2y
    local invDen = 1 / (dot00 * dot11 - dot01 * dot01 + theme.WIDGET.triangle_epsilon)
    local u = (dot11 * dot02 - dot01 * dot12) * invDen
    local v = (dot00 * dot12 - dot01 * dot02) * invDen
    return u >= 0 and v >= 0 and (u + v) <= 1
end

function M.PointInRoundedShape(px, py, x, y, width, height, inset)
    inset = inset or 0
    x, y = x + inset, y + inset
    width, height = width - inset * 2, height - inset * 2
    if width <= 0 or height <= 0 then return false end

    local radius = math.min(width, height) / 2
    local centerX, centerY = x + width / 2, y + height / 2
    if width >= height then
        local left = x + radius
        local right = x + width - radius
        if px >= left and px <= right and py >= y and py <= y + height then return true end
        local deltaLeftX, deltaLeftY = px - left, py - centerY
        local deltaRightX, deltaRightY = px - right, py - centerY
        return (deltaLeftX * deltaLeftX + deltaLeftY * deltaLeftY) <= radius * radius
            or (deltaRightX * deltaRightX + deltaRightY * deltaRightY) <= radius * radius
    end

    local top = y + radius
    local bottom = y + height - radius
    if px >= x and px <= x + width and py >= top and py <= bottom then return true end
    local deltaTopX, deltaTopY = px - centerX, py - top
    local deltaBottomX, deltaBottomY = px - centerX, py - bottom
    return (deltaTopX * deltaTopX + deltaTopY * deltaTopY) <= radius * radius
        or (deltaBottomX * deltaBottomX + deltaBottomY * deltaBottomY) <= radius * radius
end

function M.PointInArrowShape(px, py, x, y, width, height, direction, pointDepth)
    if direction == "left" then
        local bodyLeft = x + pointDepth
        local inBody = px >= bodyLeft and px <= x + width and py >= y and py <= y + height
        return inBody or M.PointInTriangle(px, py, x, y + height / 2, bodyLeft, y, bodyLeft, y + height)
    elseif direction == "right" then
        local bodyRight = x + width - pointDepth
        local inBody = px >= x and px <= bodyRight and py >= y and py <= y + height
        return inBody or M.PointInTriangle(px, py, x + width, y + height / 2, bodyRight, y, bodyRight, y + height)
    elseif direction == "up" then
        local bodyTop = y + pointDepth
        local inBody = px >= x and px <= x + width and py >= bodyTop and py <= y + height
        return inBody or M.PointInTriangle(px, py, x + width / 2, y, x, bodyTop, x + width, bodyTop)
    elseif direction == "down" then
        local bodyBottom = y + height - pointDepth
        local inBody = px >= x and px <= x + width and py >= y and py <= bodyBottom
        return inBody or M.PointInTriangle(px, py, x + width / 2, y + height, x, bodyBottom, x + width, bodyBottom)
    end
    return false
end

function M.DrawArc(imgui, drawList, centerX, centerY, radius, startAngle, endAngle, color, thickness)
    if math.abs(endAngle - startAngle) < theme.WIDGET.arc_min_delta then return end
    if imgui.DrawList_PathStroke then
        if imgui.DrawList_PathClear then imgui.DrawList_PathClear(drawList) end
        for index = 0, theme.WIDGET.arc_segments do
            local t = index / theme.WIDGET.arc_segments
            local angle = startAngle + (endAngle - startAngle) * t
            imgui.DrawList_PathLineTo(drawList, centerX + math.cos(angle) * radius, centerY + math.sin(angle) * radius)
        end
        imgui.DrawList_PathStroke(drawList, color, 0, thickness)
        return
    end

    local previousX, previousY
    for index = 0, theme.WIDGET.arc_fallback_segments do
        local t = index / theme.WIDGET.arc_fallback_segments
        local angle = startAngle + (endAngle - startAngle) * t
        local x = centerX + math.cos(angle) * radius
        local y = centerY + math.sin(angle) * radius
        if previousX then
            imgui.DrawList_AddLine(drawList, previousX, previousY, x, y, color, thickness)
        end
        previousX, previousY = x, y
    end
end

function M.DrawCenteredWrappedText(ctx, imgui, drawList, text, centerX, centerY, maxWidth, maxHeight, options)
    options = options or {}
    if options.font then imgui.PushFont(ctx, options.font) end
    local lines = (options.wrapTextFn and options.wrapTextFn(ctx, text, maxWidth, imgui)) or { text }
    local _, fontHeight = imgui.CalcTextSize(ctx, "M")
    local lineAdvance = fontHeight * (tonumber(options.lineHeight) or 1.0)
    local totalHeight = #lines > 0 and (fontHeight + (#lines - 1) * lineAdvance) or 0
    local startY = centerY - totalHeight / 2
    for index, line in ipairs(lines) do
        local textWidth = imgui.CalcTextSize(ctx, line)
        local textX = centerX - textWidth / 2
        local textY = startY + (index - 1) * lineAdvance
        if maxHeight and textY + fontHeight > centerY + maxHeight / 2 then break end
        imgui.DrawList_AddText(drawList, textX, textY, options.textColor, line)
    end
    if options.font then imgui.PopFont(ctx) end
end

function M.DrawCenteredSingleText(ctx, imgui, drawList, text, centerX, centerY, textColor, font)
    if font then imgui.PushFont(ctx, font) end
    local textWidth, textHeight = imgui.CalcTextSize(ctx, text)
    imgui.DrawList_AddText(drawList, centerX - textWidth / 2, centerY - textHeight / 2, textColor, text)
    if font then imgui.PopFont(ctx) end
end

return M
