--[[
  osd_ui.lua - Shared OSD rendering and UI logic for ImGui-based displays.
  Used by both the OSK (as part of RenderOSDBar) and standalone OSD script.
]]

local r = reaper

local font_cache = require("font_cache")
local identity = require("product_identity")
local log_writer = require("log_writer")
local osd_templates = require("osd_templates")
local theme = require("theme_settings")
local ui = require("ui_components")

local M = {}

M.state = {
    background = "0",
    text = "",
    bgColor = theme.HexToImCol(theme.osd.osd_bg_off, theme.OSK_COLORS.button_off),
    showUntil = 0,
    menuOpen = false,
}

M.vars = theme.osd

M.EXT_SECTION = identity.extState.osd
M.EXT_KEY = "OSD"
M.EXT_ID_KEY = "OSD_ID"
M.EXT_SETTINGS_SECTION = identity.extState.osdSettings
M.OSK_SETTINGS_SECTION = identity.extState.oskSettings

M.settingsBackup = nil

local smallFont = nil
local sizedFontCache = nil
local DEBUG_OSD = false
local oskBarPositions = {}
local lastSeenOSDEventId = nil

local function resetHiddenState()
    M.state.background = "0"
    M.state.text = ""
    M.state.showUntil = 0
    M.state.bgColor = theme.HexToImCol(M.vars.osd_bg_off, theme.OSK_COLORS.button_off)
end

function M.RefreshAppearance()
    if M.state.background and M.state.background:sub(1, 1) == "#" then
        M.state.bgColor = theme.HexToImCol(M.state.background, theme.OSK_COLORS.button_off)
    elseif M.state.background == "1" then
        M.state.bgColor = theme.HexToImCol(M.vars.osd_bg_on, theme.OSK_COLORS.button_on)
    else
        M.state.bgColor = theme.HexToImCol(M.vars.osd_bg_off, theme.OSK_COLORS.button_off)
    end
end

local function restoreSettingsBackup()
    if not M.settingsBackup then return end
    for key, value in pairs(M.settingsBackup) do
        M.vars[key] = value
    end
    M.settingsBackup = nil
end

local function finalizeSettingsPopupState(popupOpen)
    local wasMenuOpen = M.state.menuOpen
    M.state.menuOpen = popupOpen
    if wasMenuOpen and not popupOpen and M.settingsBackup then
        restoreSettingsBackup()
    end
end

local function getTextLines(text)
    local lines = {}
    for line in (tostring(text or "") .. "\n"):gmatch("(.-)\n") do lines[#lines + 1] = line end
    return lines
end

local function getSizedFont(px)
    if not sizedFontCache then return smallFont end
    return sizedFontCache:Get(theme.osk.font_family or "sans-serif", px) or smallFont
end

local function getWindowFont(text, height)
    local lineCount = #getTextLines(text)
    local requestedSize = M.vars.osd_font_px
    local fittedSize = math.max(8, math.floor((height - 12) / lineCount))
    return getSizedFont(math.min(requestedSize, fittedSize))
end

local function debugLog(...)
    if not DEBUG_OSD then return end
    local out = {}
    for index = 1, select("#", ...) do
        out[#out + 1] = tostring(select(index, ...))
    end
    log_writer.Write("DEBUG", "[" .. identity.displayName .. " OSD] " .. table.concat(out, " "))
end

local function getOskBarSettingsKey(surfaceName)
    return "OSDBarPosition_" .. tostring(surfaceName or "")
end

function M.SetFont(font)
    smallFont = font
end

function M.SetFontCache(cache)
    sizedFontCache = cache
end

function M.EnsureFontCache(imgui, ctx)
    if sizedFontCache then return sizedFontCache end
    sizedFontCache = font_cache.New(imgui, ctx)
    return sizedFontCache
end

function M.DrawOSDRect(ctx, imgui, x, y, width, height, text, bgColor, alphaPercent, font)
    local drawList = imgui.GetWindowDrawList(ctx)
    local baseBgColor = bgColor or M.state.bgColor
    local fillColor = alphaPercent and theme.ApplyAlpha(baseBgColor, alphaPercent) or baseBgColor
    imgui.DrawList_AddRectFilled(drawList, x, y, x + width, y + height, fillColor, 0)

    local shownText = text or ""
    local textColor = theme.GetContrastTextColorFromCol(baseBgColor)
    textColor = (textColor & 0xFFFFFF00) | 0xFF

    if font then imgui.PushFont(ctx, font) end
    if shownText ~= "" then
        local lines = getTextLines(shownText)
        local _, lineHeight = imgui.CalcTextSize(ctx, "M")
        local firstLineY = y + (height - lineHeight * #lines) / 2
        for lineIndex, line in ipairs(lines) do
            local lineWidth = imgui.CalcTextSize(ctx, line)
            local lineX = x + (width - lineWidth) / 2
            imgui.DrawList_AddText(drawList, lineX, firstLineY + (lineIndex - 1) * lineHeight, textColor, line)
        end
    end
    if font then imgui.PopFont(ctx) end
end

function M.PollOSD()
    if r.HasExtState(M.EXT_SECTION, M.EXT_KEY) then
        local msg = r.GetExtState(M.EXT_SECTION, M.EXT_KEY)
        local eventId = r.GetExtState(M.EXT_SECTION, M.EXT_ID_KEY)
        if eventId == "" then eventId = msg or "" end
        if eventId ~= lastSeenOSDEventId then
            lastSeenOSDEventId = eventId
            if not msg or msg == "" then
                resetHiddenState()
                return
            end

            local text, bgState, timeoutStr, explicitMessage = msg:match("^([^;]*);([^;]*);([^;]*);([^;]*)$")
            text = text and text:match("^%s*(.-)%s*$") or ""
            if explicitMessage == "1" then text = osd_templates.Expand(text, r) end

            local timeout = tonumber(timeoutStr) or theme.OSD.timeout_ms
            M.state.background = bgState or "0"
            if bgState and bgState:sub(1, 1) == "#" then
                M.state.bgColor = theme.HexToImCol(bgState, theme.OSK_COLORS.button_off)
            elseif bgState == "1" then
                M.state.bgColor = theme.HexToImCol(M.vars.osd_bg_on, theme.OSK_COLORS.button_on)
            else
                M.state.bgColor = theme.HexToImCol(M.vars.osd_bg_off, theme.OSK_COLORS.button_off)
            end

            M.state.text = text
            local now = r.time_precise()
            M.state.showUntil = now + (timeout / 1000)
            debugLog("eventId=", eventId, "msg=", msg, "timeoutMs=", timeout, "showUntil=", string.format("%.3f", M.state.showUntil))
        end
    end

    local now = r.time_precise()
    if M.state.showUntil > 0 and now > M.state.showUntil then
        if M.state.menuOpen then
            M.state.showUntil = now + theme.OSD.keep_open_seconds
        else
            resetHiddenState()
        end
    end
end

function M.GetOSKBarPosition(surfaceName)
    surfaceName = tostring(surfaceName or "")
    if surfaceName == "" then return M.vars.osk_bar_position end
    if oskBarPositions[surfaceName] then return oskBarPositions[surfaceName] end
    local value = r.GetExtState(M.OSK_SETTINGS_SECTION, getOskBarSettingsKey(surfaceName))
    if value ~= "top" and value ~= "bottom" and value ~= "off" then value = M.vars.osk_bar_position or "off" end
    oskBarPositions[surfaceName] = value
    return value
end

function M.SetOSKBarPosition(surfaceName, value)
    surfaceName = tostring(surfaceName or "")
    if value ~= "top" and value ~= "bottom" then value = "off" end
    if surfaceName == "" then
        M.vars.osk_bar_position = value
        M.SaveSettings()
        return
    end
    oskBarPositions[surfaceName] = value
    r.SetExtState(M.OSK_SETTINGS_SECTION, getOskBarSettingsKey(surfaceName), value, true)
end

function M.LoadSettings()
    theme.LoadOsdSettings()
    resetHiddenState()
end

function M.SaveSettings()
    theme.SaveOsdSettings()
end

function M.RenderOSDWindow(ctx, imgui, screenWidth, screenHeight, windowWidth, windowHeight, originX, originY)
    local hasText = M.state.text and M.state.text ~= ""
    if not hasText and not M.state.menuOpen then return false end
    if not smallFont then return false end

    M.EnsureFontCache(imgui, ctx)

    local margin = M.vars.osd_v_margin_px or 0
    local marginH = M.vars.osd_h_margin_px or 0
    local width = screenWidth * (M.vars.osd_width_percent / 100)
    local height = M.vars.osd_height_px
    local baseX = originX or 0
    local baseY = originY or 0

    local xPos = baseX
    if M.vars.osd_alignment == "left" then
        xPos = baseX + marginH
    elseif M.vars.osd_alignment == "right" then
        xPos = baseX + screenWidth - width - marginH
    else
        xPos = baseX + (screenWidth - width) / 2
    end
    local yPos = M.vars.osd_position == "top"
        and (baseY + margin)
        or (baseY + screenHeight - height - margin)

    imgui.SetNextWindowPos(ctx, xPos, yPos, imgui.Cond_Always)
    imgui.SetNextWindowSize(ctx, width, height, imgui.Cond_Always)
    imgui.SetNextWindowBgAlpha(ctx, (M.vars.osd_transparency or 50) / 100)

    local windowFlags = imgui.WindowFlags_NoTitleBar
        | imgui.WindowFlags_NoScrollbar
        | imgui.WindowFlags_NoMove
        | imgui.WindowFlags_NoResize
        | imgui.WindowFlags_NoCollapse

    imgui.PushStyleVar(ctx, imgui.StyleVar_WindowBorderSize, theme.OSD.window_border_size)
    local visible, open = imgui.Begin(ctx, "##OSD", true, windowFlags)

    if visible then
        local winX, winY = imgui.GetWindowPos(ctx)
        local winW, winH = imgui.GetWindowSize(ctx)
        local renderFont = getWindowFont(hasText and M.state.text or "", winH)
        M.DrawOSDRect(ctx, imgui, winX, winY, winW, winH, hasText and M.state.text or "", M.state.bgColor, M.vars.osd_transparency, renderFont)

        local popupOpen = false
        if smallFont then imgui.PushFont(ctx, smallFont) end
        if imgui.BeginPopupContextWindow(ctx, "OSD_ContextMenu") then
            popupOpen = true
            M.RenderSettingsPanel(ctx, imgui)
            imgui.EndPopup(ctx)
        end
        if smallFont then imgui.PopFont(ctx) end
        finalizeSettingsPopupState(popupOpen)
    else
        finalizeSettingsPopupState(false)
    end

    imgui.End(ctx)
    imgui.PopStyleVar(ctx)
    return open
end

function M.RenderSettingsPanel(ctx, imgui)
    if M.settingsBackup == nil then
        M.settingsBackup = {}
        for key, value in pairs(M.vars) do
            M.settingsBackup[key] = value
        end
    end

    imgui.Text(ctx, "OSD Settings")
    imgui.SameLine(ctx)
    local saveClicked, cancelClicked = ui.SaveCancelButtons(ctx, "Save##osd_save", "Cancel##osd_cancel", theme.OSD.popup_button_width)
    if saveClicked then
        M.SaveSettings()
        M.settingsBackup = nil
        if imgui.CloseCurrentPopup then imgui.CloseCurrentPopup(ctx) end
        return
    end
    if cancelClicked then
        restoreSettingsBackup()
        if imgui.CloseCurrentPopup then imgui.CloseCurrentPopup(ctx) end
        return
    end

    imgui.Separator(ctx)
    local changed
    changed, M.vars.osd_position = ui.ComboEnum(ctx, "Position##osd_pos", M.vars.osd_position, {
        { label = "Top", value = "top" },
        { label = "Bottom", value = "bottom" },
    })
    changed, M.vars.osd_alignment = ui.ComboEnum(ctx, "Alignment##osd_align", M.vars.osd_alignment, {
        { label = "Left", value = "left" },
        { label = "Center", value = "center" },
        { label = "Right", value = "right" },
    })
    changed, M.vars.osd_width_percent = ui.SliderWithInput(ctx, "Width %", M.vars.osd_width_percent, 10, 100, 1)
    changed, M.vars.osd_height_px = ui.SliderWithInput(ctx, "Height px", M.vars.osd_height_px, 20, 400, 10)
    changed, M.vars.osd_h_margin_px = ui.SliderWithInput(ctx, "H margin px", M.vars.osd_h_margin_px, 0, 400, 10)
    changed, M.vars.osd_v_margin_px = ui.SliderWithInput(ctx, "V margin px", M.vars.osd_v_margin_px, 0, 400, 10)
    changed, M.vars.osd_font_px = ui.SliderWithInput(ctx, "Font px", M.vars.osd_font_px, 8, 200, 1)
    changed, M.vars.osd_transparency = ui.SliderWithInput(ctx, "Transparency %", M.vars.osd_transparency, 0, 100, 5)
end

function M.RenderOSKPositionToggle(ctx, imgui, surfaceName)
    local changed, value = ui.ComboEnum(ctx, "OSK OSD Position##osk_osd_pos", M.GetOSKBarPosition(surfaceName), {
        { label = "Off", value = "off" },
        { label = "Top", value = "top" },
        { label = "Bottom", value = "bottom" },
    })
    if changed then
        M.SetOSKBarPosition(surfaceName, value)
    end
end

return M
