local r = reaper
local imgui = require "imgui" "0.9.3"

local theme = require("theme_settings")
local ui = require("ui_components")

local M = {}

local USER_SWATCHES_KEY = "ConfigColorUserSwatches"
local RECENT_SWATCHES_KEY = "ConfigColorRecentSwatches"
local USER_SWATCH_COUNT = 20
local RECENT_SWATCH_COUNT = 10
local EMPTY_SWATCH_TOKEN = "-"
local EMPTY_SWATCH_COLOR = 0x00000000
local mouseButtonRight = imgui.MouseButton_Right or 1

local swatchFlags = (imgui.ColorEditFlags_NoPicker or 0) | (imgui.ColorEditFlags_NoTooltip or 0)
local emptySwatchFlags = swatchFlags | (imgui.ColorEditFlags_AlphaPreview or 0)
local previewFlags = swatchFlags
local pickerFlags = (imgui.ColorEditFlags_NoAlpha or 0)
    | (imgui.ColorEditFlags_NoInputs or 0)
    | (imgui.ColorEditFlags_NoSidePreview or 0)
    | (imgui.ColorEditFlags_NoSmallPreview or 0)
    | (imgui.ColorEditFlags_PickerHueBar or 0)

local popupState = {}
local userSwatches = {}
local recentSwatches = {}
local swatchesLoaded = false

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

local function rgbaToArgb(color)
    color = normalizeColor(color)
    return ((color >> 8) & 0x00FFFFFF) | ((color << 24) & 0xFF000000)
end

local function argbToRgba(color)
    color = tonumber(color) or 0
    return ((color << 8) & 0xFFFFFF00) | ((color >> 24) & 0xFF)
end

local function colorToHex(color)
    color = normalizeColor(color)
    return string.format("#%02X%02X%02X", (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF)
end

local function rgbToHsv(red, green, blue)
    red = clampChannel(red) / 255
    green = clampChannel(green) / 255
    blue = clampChannel(blue) / 255

    local maxChannel = math.max(red, green, blue)
    local minChannel = math.min(red, green, blue)
    local delta = maxChannel - minChannel
    local hue = 0

    if delta > 0 then
        if maxChannel == red then
            hue = 60 * (((green - blue) / delta) % 6)
        elseif maxChannel == green then
            hue = 60 * (((blue - red) / delta) + 2)
        else
            hue = 60 * (((red - green) / delta) + 4)
        end
    end

    local saturation = maxChannel == 0 and 0 or delta / maxChannel
    return math.floor(hue + 0.5), math.floor(saturation * 100 + 0.5), math.floor(maxChannel * 100 + 0.5)
end

local function hsvToColor(hue, saturation, value)
    hue = (tonumber(hue) or 0) % 360
    saturation = math.max(0, math.min(100, tonumber(saturation) or 0)) / 100
    value = math.max(0, math.min(100, tonumber(value) or 0)) / 100

    local chroma = value * saturation
    local huePrime = hue / 60
    local x = chroma * (1 - math.abs((huePrime % 2) - 1))
    local red, green, blue = 0, 0, 0

    if huePrime < 1 then
        red, green, blue = chroma, x, 0
    elseif huePrime < 2 then
        red, green, blue = x, chroma, 0
    elseif huePrime < 3 then
        red, green, blue = 0, chroma, x
    elseif huePrime < 4 then
        red, green, blue = 0, x, chroma
    elseif huePrime < 5 then
        red, green, blue = x, 0, chroma
    else
        red, green, blue = chroma, 0, x
    end

    local match = value - chroma
    return packColor((red + match) * 255, (green + match) * 255, (blue + match) * 255)
end

local function parseHexColor(text)
    local hex = tostring(text or ""):upper():gsub("^%s*#?", ""):gsub("%s*$", "")
    if not hex:match("^%x%x%x%x%x%x$") then return nil end
    return packColor(tonumber(hex:sub(1, 2), 16), tonumber(hex:sub(3, 4), 16), tonumber(hex:sub(5, 6), 16))
end

local function colorsEqual(left, right)
    if left == nil or right == nil then return false end
    return (normalizeColor(left) & 0xFFFFFF00) == (normalizeColor(right) & 0xFFFFFF00)
end

local function parseStoredSwatches(rawValue, fixedCount)
    local values = {}
    local slotIndex = 1
    for token in tostring(rawValue or ""):gmatch("[^,]+") do
        if fixedCount and slotIndex > fixedCount then break end
        token = token:match("^%s*(.-)%s*$")
        if token ~= EMPTY_SWATCH_TOKEN and token ~= "" then
            local color = parseHexColor(token)
            if fixedCount then
                values[slotIndex] = color
            elseif color then
                values[#values + 1] = color
            end
        end
        slotIndex = slotIndex + 1
    end
    return values
end

local function loadSwatches()
    if swatchesLoaded then return end
    userSwatches = parseStoredSwatches(r.GetExtState(theme.OSK_SETTINGS_SECTION, USER_SWATCHES_KEY), USER_SWATCH_COUNT)
    recentSwatches = parseStoredSwatches(r.GetExtState(theme.OSK_SETTINGS_SECTION, RECENT_SWATCHES_KEY), nil)
    swatchesLoaded = true
end

local function saveSwatches()
    local userTokens = {}
    for idx = 1, USER_SWATCH_COUNT do
        userTokens[#userTokens + 1] = userSwatches[idx] and colorToHex(userSwatches[idx]) or EMPTY_SWATCH_TOKEN
    end
    r.SetExtState(theme.OSK_SETTINGS_SECTION, USER_SWATCHES_KEY, table.concat(userTokens, ","), true)

    local recentTokens = {}
    for idx = 1, math.min(#recentSwatches, RECENT_SWATCH_COUNT) do
        if recentSwatches[idx] then recentTokens[#recentTokens + 1] = colorToHex(recentSwatches[idx]) end
    end
    r.SetExtState(theme.OSK_SETTINGS_SECTION, RECENT_SWATCHES_KEY, table.concat(recentTokens, ","), true)
end

local function rememberRecentColor(color)
    loadSwatches()
    color = normalizeColor(color)
    for idx = #recentSwatches, 1, -1 do
        if colorsEqual(recentSwatches[idx], color) then table.remove(recentSwatches, idx) end
    end
    table.insert(recentSwatches, 1, color)
    while #recentSwatches > RECENT_SWATCH_COUNT do table.remove(recentSwatches) end
    saveSwatches()
end

local function storeUserSwatch(slotIndex, color)
    loadSwatches()
    if slotIndex < 1 or slotIndex > USER_SWATCH_COUNT then return end
    userSwatches[slotIndex] = normalizeColor(color)
    saveSwatches()
end

local function saveCurrentToUserSwatches(color)
    loadSwatches()
    color = normalizeColor(color)
    for idx = 1, USER_SWATCH_COUNT do
        if colorsEqual(userSwatches[idx], color) then return idx end
    end
    for idx = 1, USER_SWATCH_COUNT do
        if userSwatches[idx] == nil then
            storeUserSwatch(idx, color)
            return idx
        end
    end
    storeUserSwatch(USER_SWATCH_COUNT, color)
    return USER_SWATCH_COUNT
end

local function getRecentDisplaySwatches()
    loadSwatches()
    local reserved = {}
    for _, paletteColor in ipairs(theme.CONFIG.color_palette) do
        reserved[colorToHex(paletteColor.value)] = true
    end
    for idx = 1, USER_SWATCH_COUNT do
        if userSwatches[idx] then reserved[colorToHex(userSwatches[idx])] = true end
    end

    local display = {}
    local seen = {}
    for _, color in ipairs(recentSwatches) do
        if color then
            local hex = colorToHex(color)
            if not reserved[hex] and not seen[hex] then
                display[#display + 1] = color
                seen[hex] = true
            end
        end
        if #display >= RECENT_SWATCH_COUNT then break end
    end
    return display
end

local function getPopupState(popupId, color)
    local state = popupState[popupId]
    if not state then
        state = { red = 0, green = 0, blue = 0, hue = 0, saturation = 0, value = 0, hexValue = "#000000" }
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
    state.hue, state.saturation, state.value = rgbToHsv(state.red, state.green, state.blue)
    state.hexValue = colorToHex(color)
end

local function getEditedColor(state)
    return parseHexColor(state.hexValue) or packColor(state.red, state.green, state.blue)
end

local function requestPreviewApply(configState, deps)
    if deps.protocol and deps.protocol.RequestPreviewApply then
        deps.protocol.RequestPreviewApply(configState, deps.data, deps.model)
    end
end

local function applyColor(configState, binding, colorIndex, color, deps, rememberRecent)
    color = normalizeColor(color)
    deps.model.SetActionColor(binding, colorIndex, color, deps.action_line, theme)
    deps.model.UpdateDirtyState(configState)
    if rememberRecent ~= false then rememberRecentColor(color) end
    requestPreviewApply(configState, deps)
    return color
end

local function resetColor(configState, binding, colorIndex, deps)
    local color = deps.model.ResetActionColor(binding, colorIndex, deps.action_line, theme)
    deps.model.UpdateDirtyState(configState)
    requestPreviewApply(configState, deps)
    return color
end

local function renderSectionTitle(ctx, text)
    imgui.Text(ctx, text)
end

local function renderChannelField(ctx, state, idSuffix, label, key, maxValue)
    maxValue = maxValue or 255
    imgui.SetNextItemWidth(ctx, 52)
    local changed
    changed, state[key] = imgui.DragInt(ctx, "##manual_color_" .. label .. "_" .. idSuffix, state[key], 1, 0, maxValue)
    if imgui.IsItemHovered(ctx) then
        local wheelValue = imgui.GetMouseWheel(ctx)
        if wheelValue ~= 0 then
            local step = wheelValue > 0 and 1 or -1
            state[key] = math.max(0, math.min(maxValue, (tonumber(state[key]) or 0) + step))
            changed = true
        end
    end
    imgui.SameLine(ctx, 0, 4)
    imgui.Text(ctx, label)
    return changed
end

local function renderManualFields(ctx, state, idSuffix)
    local edited = false
    edited = renderChannelField(ctx, state, idSuffix, "R", "red") or edited
    edited = renderChannelField(ctx, state, idSuffix, "G", "green") or edited
    edited = renderChannelField(ctx, state, idSuffix, "B", "blue") or edited
    if edited then
        local color = packColor(state.red, state.green, state.blue)
        state.hue, state.saturation, state.value = rgbToHsv(state.red, state.green, state.blue)
        state.hexValue = colorToHex(color)
    end
    return edited
end

local function renderHsvFields(ctx, state, idSuffix)
    local edited = false
    edited = renderChannelField(ctx, state, idSuffix, "H", "hue", 360) or edited
    edited = renderChannelField(ctx, state, idSuffix, "S", "saturation", 100) or edited
    edited = renderChannelField(ctx, state, idSuffix, "V", "value", 100) or edited
    if edited then
        local color = hsvToColor(state.hue, state.saturation, state.value)
        state.red = (color >> 24) & 0xFF
        state.green = (color >> 16) & 0xFF
        state.blue = (color >> 8) & 0xFF
        state.hexValue = colorToHex(color)
        return true, color
    end
    return false, nil
end

local function renderHexField(ctx, state, idSuffix)
    imgui.SetNextItemWidth(ctx, theme.CONFIG.color_preview_width)
    local changedHex
    changedHex, state.hexValue = imgui.InputText(ctx, "##manual_color_hex_" .. idSuffix, state.hexValue or "#000000")
    if changedHex then
        local parsed = parseHexColor(state.hexValue)
        if parsed then
            syncEditFields(state, parsed)
            return true, parsed
        end
    end
    return false, nil
end

local function renderColorSwatch(ctx, id, color, tooltip, flags)
    local swatchSize = theme.CONFIG.color_swatch_size
    local clicked = imgui.ColorButton(ctx, id, normalizeColor(color), flags or swatchFlags, swatchSize, swatchSize)
    local rightClicked = imgui.IsItemClicked and imgui.IsItemClicked(ctx, mouseButtonRight)
    if tooltip and tooltip ~= "" then ui.ItemTooltip(ctx, tooltip) end
    return clicked, rightClicked
end

local function renderEmptySwatch(ctx, id, tooltip)
    local swatchSize = theme.CONFIG.color_swatch_size
    local clicked = imgui.ColorButton(ctx, id, EMPTY_SWATCH_COLOR, emptySwatchFlags, swatchSize, swatchSize)
    local rightClicked = imgui.IsItemClicked and imgui.IsItemClicked(ctx, mouseButtonRight)
    if tooltip and tooltip ~= "" then ui.ItemTooltip(ctx, tooltip) end
    return clicked, rightClicked
end

local function renderRecentSwatches(ctx, state, configState, binding, colorIndex, idSuffix, deps)
    local recent = getRecentDisplaySwatches()

    renderSectionTitle(ctx, "Recent")
    for idx = 1, RECENT_SWATCH_COUNT do
        local color = recent[idx]
        if color then
            local clicked = renderColorSwatch(ctx, "##recent_color_" .. idSuffix .. "_" .. idx, color, colorToHex(color) .. "\nLeft click: use this color")
            if clicked then
                syncEditFields(state, applyColor(configState, binding, colorIndex, color, deps, true))
            end
        else
            renderEmptySwatch(ctx, "##recent_color_empty_" .. idSuffix .. "_" .. idx, string.format("Recent swatch %d is empty", idx))
        end
        if idx % theme.CONFIG.color_recent_columns ~= 0 then imgui.SameLine(ctx, 0, 4) end
    end
end

local function renderUserSwatches(ctx, state, configState, binding, colorIndex, idSuffix, deps)
    renderSectionTitle(ctx, "Saved")
    local editedColor = getEditedColor(state)
    for slotIndex = 1, USER_SWATCH_COUNT do
        local color = userSwatches[slotIndex]
        if color then
            local clicked, rightClicked = renderColorSwatch(ctx, "##user_color_" .. idSuffix .. "_" .. slotIndex, color, string.format("Saved swatch %d\nLeft click: use this color\nRight click: replace with current picker color", slotIndex))
            if clicked then
                syncEditFields(state, applyColor(configState, binding, colorIndex, color, deps, true))
            end
            if rightClicked then
                storeUserSwatch(slotIndex, editedColor)
            end
        else
            local _, rightClicked = renderEmptySwatch(ctx, "##user_color_empty_" .. idSuffix .. "_" .. slotIndex, string.format("Empty swatch %d\nRight click: save current picker color here", slotIndex))
            if rightClicked then
                storeUserSwatch(slotIndex, editedColor)
            end
        end
        if slotIndex % theme.CONFIG.color_saved_columns ~= 0 then imgui.SameLine(ctx, 0, 4) end
    end
end

local function renderBuiltinSwatches(ctx, state, configState, binding, colorIndex, idSuffix, deps)
    renderSectionTitle(ctx, "Built-in")
    for paletteIndex, paletteColor in ipairs(theme.CONFIG.color_palette) do
        local clicked = renderColorSwatch(ctx, "##builtin_color_" .. idSuffix .. "_" .. paletteIndex, paletteColor.value, paletteColor.name .. "\nLeft click: use this color")
        if clicked then
            syncEditFields(state, applyColor(configState, binding, colorIndex, paletteColor.value, deps, true))
        end
        if paletteIndex % theme.CONFIG.color_builtin_columns ~= 0 then imgui.SameLine(ctx, 0, 4) end
    end
end

local function renderColorEditor(ctx, state, configState, binding, colorIndex, idSuffix, deps)
    imgui.BeginGroup(ctx)
    local manualChanged = renderManualFields(ctx, state, "rgb_" .. idSuffix)
    if manualChanged then applyColor(configState, binding, colorIndex, getEditedColor(state), deps, false) end
    imgui.EndGroup(ctx)

    imgui.SameLine(ctx, 0, 8)
    imgui.BeginGroup(ctx)
    local hsvChanged, hsvColor = renderHsvFields(ctx, state, "hsv_" .. idSuffix)
    if hsvChanged then applyColor(configState, binding, colorIndex, hsvColor, deps, false) end
    imgui.EndGroup(ctx)

    imgui.SameLine(ctx, 0, 8)
    imgui.BeginGroup(ctx)
    imgui.ColorButton(ctx, "##active_preview_" .. idSuffix, getEditedColor(state), previewFlags, theme.CONFIG.color_preview_width, theme.CONFIG.color_preview_height)
    local hexChanged, hexColor = renderHexField(ctx, state, "main_" .. idSuffix)
    if hexChanged then applyColor(configState, binding, colorIndex, hexColor, deps, false) end
    imgui.EndGroup(ctx)

    imgui.SetNextItemWidth(ctx, theme.CONFIG.color_picker_width)
    if imgui.ColorPicker4 then
        local pickerColor = rgbaToArgb(getEditedColor(state))
        local changed, pickedColor = imgui.ColorPicker4(ctx, "##picker_" .. idSuffix, pickerColor, pickerFlags)
        if changed then
            local color = argbToRgba(pickedColor)
            syncEditFields(state, color)
            applyColor(configState, binding, colorIndex, color, deps, false)
        end
    end
end

function M.RenderBindingColorPicker(ctx, configState, binding, bindingIndex, colorIndex, label, currentColor, deps, displayColor)
    currentColor = normalizeColor(currentColor)
    displayColor = normalizeColor(displayColor or currentColor)
    local popupId = "Color " .. label .. "##binding_color_" .. bindingIndex .. "_" .. colorIndex
    local idSuffix = tostring(bindingIndex) .. "_" .. tostring(colorIndex)
    local buttonId = "##color_button_" .. idSuffix

    if imgui.ColorButton(ctx, buttonId, displayColor) then
        configState.selectedBinding = bindingIndex
        loadSwatches()
        syncEditFields(getPopupState(popupId, currentColor), currentColor)
        imgui.OpenPopup(ctx, popupId)
    end
    ui.ItemTooltip(ctx, label .. " color")

    if not imgui.BeginPopup(ctx, popupId) then return currentColor end

    local state = getPopupState(popupId, currentColor)
    renderSectionTitle(ctx, label .. " color")
    renderColorEditor(ctx, state, configState, binding, colorIndex, idSuffix, deps)

    if imgui.Button(ctx, "Apply##manual_color_" .. idSuffix) then
        currentColor = applyColor(configState, binding, colorIndex, getEditedColor(state), deps, true)
        syncEditFields(state, currentColor)
    end
    imgui.SameLine(ctx, 0, 6)
    if imgui.Button(ctx, "Reset##manual_color_" .. idSuffix) then
        currentColor = resetColor(configState, binding, colorIndex, deps)
        syncEditFields(state, currentColor)
    end
    imgui.SameLine(ctx, 0, 6)
    if imgui.Button(ctx, "Save##manual_color_" .. idSuffix) then
        saveCurrentToUserSwatches(getEditedColor(state))
    end

    renderRecentSwatches(ctx, state, configState, binding, colorIndex, idSuffix, deps)
    renderUserSwatches(ctx, state, configState, binding, colorIndex, idSuffix, deps)
    renderBuiltinSwatches(ctx, state, configState, binding, colorIndex, idSuffix, deps)

    imgui.EndPopup(ctx)
    return currentColor
end

return M