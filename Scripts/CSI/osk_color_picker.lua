local imgui = require "imgui" "0.9.3"

local theme = require("theme_settings")
local ui = require("ui_components")

local M = {}

local popupState = {}

local function clampChannel(value)
    return math.max(0, math.min(255, math.floor((tonumber(value) or 0) + 0.5)))
end

local function packColor(red, green, blue)
    return (clampChannel(red) << 24) | (clampChannel(green) << 16) | (clampChannel(blue) << 8) | 0xFF
end

local function normalizeColor(color)
    color = tonumber(color) or theme.CONFIG.default_active_color
    return (color & 0xFFFFFF00) | 0xFF
end

local function colorToHex(color)
    color = normalizeColor(color)
    return string.format("#%02X%02X%02X", (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF)
end

local function parseHexColor(text)
    local hex = tostring(text or ""):upper():gsub("^%s*#?", ""):gsub("%s*$", "")
    if not hex:match("^%x%x%x%x%x%x$") then return nil end
    return packColor(tonumber(hex:sub(1, 2), 16), tonumber(hex:sub(3, 4), 16), tonumber(hex:sub(5, 6), 16))
end

local function getPopupState(popupId, color)
    local state = popupState[popupId]
    if not state then
        state = { pickerOpen = false, red = 0, green = 0, blue = 0, hexValue = "#000000" }
        popupState[popupId] = state
    end
    state.lastColor = normalizeColor(color)
    return state
end

local function syncEditFields(state, color)
    color = normalizeColor(color)
    state.red = (color >> 24) & 0xFF
    state.green = (color >> 16) & 0xFF
    state.blue = (color >> 8) & 0xFF
    state.hexValue = colorToHex(color)
end

local function applyColor(configState, binding, colorIndex, color, deps)
    color = normalizeColor(color)
    deps.model.SetActionColor(binding, colorIndex, color, deps.action_line, theme)
    deps.model.UpdateDirtyState(configState)
    return color
end

local function resetColor(configState, binding, colorIndex, deps)
    local color = deps.model.ResetActionColor(binding, colorIndex, deps.action_line, theme)
    deps.model.UpdateDirtyState(configState)
    return color
end

local function renderSectionTitle(ctx, text)
    if imgui.SeparatorText then
        imgui.SeparatorText(ctx, text)
    else
        imgui.Separator(ctx)
        imgui.TextDisabled(ctx, text)
    end
end

local function renderManualPicker(ctx, state, idSuffix)
    local edited = false
    idSuffix = idSuffix or ""

    imgui.SetNextItemWidth(ctx, 72)
    local changedR
    changedR, state.red = imgui.DragInt(ctx, "R##manual_color_" .. idSuffix, state.red, 1, 0, 255)
    edited = edited or changedR

    imgui.SameLine(ctx, 0, 6)
    imgui.SetNextItemWidth(ctx, 72)
    local changedG
    changedG, state.green = imgui.DragInt(ctx, "G##manual_color_" .. idSuffix, state.green, 1, 0, 255)
    edited = edited or changedG

    imgui.SameLine(ctx, 0, 6)
    imgui.SetNextItemWidth(ctx, 72)
    local changedB
    changedB, state.blue = imgui.DragInt(ctx, "B##manual_color_" .. idSuffix, state.blue, 1, 0, 255)
    edited = edited or changedB

    if edited then
        state.hexValue = colorToHex(packColor(state.red, state.green, state.blue))
    end

    imgui.SetNextItemWidth(ctx, 112)
    local changedHex
    changedHex, state.hexValue = imgui.InputText(ctx, "Hex##manual_color_" .. idSuffix, state.hexValue or "#000000")
    if changedHex then
        local parsed = parseHexColor(state.hexValue)
        if parsed then syncEditFields(state, parsed) end
    end
end

function M.RenderBindingColorPicker(ctx, configState, binding, bindingIndex, colorIndex, label, currentColor, deps)
    currentColor = normalizeColor(currentColor)
    local popupId = "Color " .. label .. "##binding_color_" .. bindingIndex .. "_" .. colorIndex
    local idSuffix = tostring(bindingIndex) .. "_" .. tostring(colorIndex)
    local buttonId = "##color_button_" .. idSuffix

    if imgui.ColorButton(ctx, buttonId, currentColor) then
        configState.selectedBinding = bindingIndex
        local state = getPopupState(popupId, currentColor)
        state.pickerOpen = false
        syncEditFields(state, currentColor)
        imgui.OpenPopup(ctx, popupId)
    end
    ui.ItemTooltip(ctx, label .. " color")

    if not imgui.BeginPopup(ctx, popupId) then return currentColor end

    local state = getPopupState(popupId, currentColor)
    renderSectionTitle(ctx, label .. " color")
    imgui.ColorButton(ctx, "##current_color_preview_" .. idSuffix, currentColor)
    imgui.SameLine(ctx, 0, 8)
    imgui.Text(ctx, colorToHex(currentColor))

    if imgui.Button(ctx, "Pick##manual_color_toggle_" .. idSuffix) then
        state.pickerOpen = not state.pickerOpen
        if state.pickerOpen then syncEditFields(state, currentColor) end
    end
    imgui.SameLine(ctx, 0, 6)
    if imgui.Button(ctx, "Reset##manual_color_" .. idSuffix) then
        currentColor = resetColor(configState, binding, colorIndex, deps)
        syncEditFields(state, currentColor)
    end

    if state.pickerOpen then
        renderManualPicker(ctx, state, idSuffix)
        if imgui.Button(ctx, "Apply##manual_color_" .. idSuffix) then
            currentColor = applyColor(configState, binding, colorIndex, parseHexColor(state.hexValue) or packColor(state.red, state.green, state.blue), deps)
            syncEditFields(state, currentColor)
        end
        imgui.SameLine(ctx, 0, 6)
        if imgui.Button(ctx, "Cancel##manual_color_" .. idSuffix) then
            syncEditFields(state, currentColor)
            state.pickerOpen = false
        end
    end

    renderSectionTitle(ctx, "Built-in")
    for paletteIndex, paletteColor in ipairs(theme.CONFIG.color_palette) do
        if imgui.ColorButton(ctx, "##builtin_color_" .. idSuffix .. "_" .. paletteIndex, paletteColor.value) then
            currentColor = applyColor(configState, binding, colorIndex, paletteColor.value, deps)
            syncEditFields(state, currentColor)
        end
        ui.ItemTooltip(ctx, paletteColor.name)
        if paletteIndex % theme.CONFIG.palette_columns ~= 0 then imgui.SameLine(ctx, 0, 4) end
    end

    imgui.EndPopup(ctx)
    return currentColor
end

return M