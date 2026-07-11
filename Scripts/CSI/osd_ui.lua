--[[
  osd_ui.lua - Shared OSD rendering and UI logic for ImGui-based displays.
  Used by both the OSK (as part of RenderOSDBar) and standalone OSD script.
]]

local r = reaper
local ui = require("ui_components")
local M = {}

-- OSD state and settings
M.state = {
    text = "",
    bgColor = 0x333333ff,
    showUntil = 0,
    menuOpen = false,
}

M.vars = {
    -- OSD Display Settings (only for dedicated OSD script)
    osd_position = "top",          -- "top" or "bottom" (dedicated OSD overlay)
    osd_alignment = "center",      -- "left", "center", "right"
    osk_bar_position = "off",   -- "off", "top" or "bottom" (OSK embedded line only)
    osd_width_percent = 50,       -- 0-100 (100% = full width)
    osd_height_px = 100,           -- 20..400 step 10
    osd_transparency = 30,         -- 0-100
    osd_h_margin_px = 0,           -- horizontal margin for left/right alignment
    osd_v_margin_px = 50,           -- vertical margin from top/bottom edge
    osd_font_px = 80,              -- explicit font size in px
    osd_bg_on = "#7f7f7f",         -- background when state=1
    osd_bg_off = "#333333",        -- background when state=0
}

M.EXT_SECTION = "ReaCtrlSurf_OSD"
M.EXT_KEY = "OSD"
M.EXT_SETTINGS_SECTION = "ReaCtrlSurf_OSD_SETTINGS"

M.settingsBackup = nil  -- Backup of settings when menu opens (for Cancel revert)

local FONT_SMALL = nil
local FONT_CACHE = {}
local DEBUG_OSD = false
local IDLE_SETTINGS_POPUP_ID = "OSD_IdleSettings"

local function clamp(value, minVal, maxVal)
    return math.max(minVal, math.min(maxVal, value))
end

local function clampStep(value, minVal, maxVal, step)
    value = clamp(value, minVal, maxVal)
    return math.floor((value + step / 2) / step) * step
end

local function toAlpha(col, alphaPercent)
    return (col & 0xFFFFFF00) | math.floor((alphaPercent / 100) * 255)
end

local function GetCenteredTextPosition(ctx, imgui, x, y, width, height, text)
    local textWidth = imgui.CalcTextSize(ctx, text)
    local _, lineHeight = imgui.CalcTextSize(ctx, "M")
    return x + (width - textWidth) / 2, y + (height - lineHeight) / 2
end

local function resetHiddenState()
    M.state.text = ""
    M.state.showUntil = 0
    M.state.bgColor = M.hexToImCol(M.vars.osd_bg_off)
end

local function restoreSettingsBackup()
    if not M.settingsBackup then return end
    for key, val in pairs(M.settingsBackup) do
        M.vars[key] = val
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

function M.SetFont(font)
    FONT_SMALL = font
end

local function GetSizedFont(ctx, imgui, px)
    px = math.max(8, math.floor((tonumber(px) or 12) + 0.5))
    local font = FONT_CACHE[px]
    if font then return font end

    if not imgui.CreateFont or not imgui.Attach then
        return FONT_SMALL
    end

    font = imgui.CreateFont("sans-serif", px)
    if not font then return FONT_SMALL end
    imgui.Attach(ctx, font)
    FONT_CACHE[px] = font
    return font
end

local function DebugLog(...)
    if not DEBUG_OSD then return end
    local out = {}
    for i = 1, select("#", ...) do
        out[#out + 1] = tostring(select(i, ...))
    end
    r.ShowConsoleMsg("[CSI OSD] " .. table.concat(out, " ") .. "\n")
end

local function IsValidContext(ctx)
    if not ctx then return false end
    if r.ImGui_ValidatePtr then
        return r.ImGui_ValidatePtr(ctx, "ImGui_Context*")
    end
    return true
end

---Hex string to ImGui color (ABGR format)
function M.hexToImCol(hex)
    if not hex then return 0x333333ff end
    if hex:sub(1, 1) == "#" then hex = hex:sub(2) end
    if #hex < 6 then return 0x333333ff end
    local red = tonumber(hex:sub(1, 2), 16) or 0
    local green = tonumber(hex:sub(3, 4), 16) or 0
    local blue = tonumber(hex:sub(5, 6), 16) or 0
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.getContrastTextColorFromCol(bgCol)
    local red = (bgCol >> 24) & 0xFF
    local green = (bgCol >> 16) & 0xFF
    local blue = (bgCol >> 8) & 0xFF
    local luminance = 0.299 * red + 0.587 * green + 0.114 * blue
    return luminance > 186 and 0x000000ff or 0xFFFFFFff
end

---Get contrast text color for given background
function M.getContrastTextColor(bgHex)
    return M.getContrastTextColorFromCol(M.hexToImCol(bgHex))
end

function M.DrawOSDRect(ctx, imgui, x, y, width, height, text, bgColor, alphaPercent, font)
    local drawList = imgui.GetWindowDrawList(ctx)
    local baseBgColor = bgColor or M.state.bgColor
    local fillColor = alphaPercent and toAlpha(baseBgColor, alphaPercent) or baseBgColor
    imgui.DrawList_AddRectFilled(drawList, x, y, x + width, y + height, fillColor, 0)

    local shownText = text or ""
    local textColor = M.getContrastTextColorFromCol(baseBgColor)
    textColor = (textColor & 0xFFFFFF00) | 0xFF

    if font then imgui.PushFont(ctx, font) end
    if shownText ~= "" then
        local textX, textY = GetCenteredTextPosition(ctx, imgui, x, y, width, height, shownText)
        imgui.DrawList_AddText(drawList, textX, textY, textColor, shownText)
    end
    if font then imgui.PopFont(ctx) end
end

---Poll OSD message from ExtState
function M.PollOSD()
    if r.HasExtState(M.EXT_SECTION, M.EXT_KEY) then
        local msg = r.GetExtState(M.EXT_SECTION, M.EXT_KEY)
        r.DeleteExtState(M.EXT_SECTION, M.EXT_KEY, false)
        if not msg or msg == "" then
            resetHiddenState()
            return
        end

        -- Parse message format: "text;bgState;timeoutMs"
        local text, bgState, timeoutStr = msg:match("([^;]*);?([^;]*);?([^;]*)")
        text = text and text:match("^%s*(.-)%s*$") or ""

        local timeout = tonumber(timeoutStr) or 3000  -- default 3 seconds
        if bgState and bgState:sub(1, 1) == "#" then
            M.state.bgColor = M.hexToImCol(bgState)
        elseif bgState == "1" then
            M.state.bgColor = M.hexToImCol(M.vars.osd_bg_on)
        else
            M.state.bgColor = M.hexToImCol(M.vars.osd_bg_off)
        end

        M.state.text = text
        local now = r.time_precise()
        M.state.showUntil = now + (timeout / 1000)
        DebugLog("msg=", msg, "timeoutMs=", timeout, "showUntil=", string.format("%.3f", M.state.showUntil))
    end

    -- Check if OSD should still be visible
    local now = r.time_precise()
    if M.state.showUntil > 0 and now > M.state.showUntil then
        resetHiddenState()
    end
end

---Load settings from ExtState
function M.LoadSettings()
    for key, defaultVal in pairs(M.vars) do
        local strVal = r.GetExtState(M.EXT_SETTINGS_SECTION, key)
        if strVal ~= "" then
            local valType = type(defaultVal)
            if valType == "number" then
                M.vars[key] = tonumber(strVal) or defaultVal
            elseif valType == "boolean" then
                M.vars[key] = strVal == "true"
            else
                M.vars[key] = strVal
            end
        end
    end

    -- Sanity clamps
    if M.vars.osd_position ~= "top" and M.vars.osd_position ~= "bottom" then
        M.vars.osd_position = "top"
    end

    if M.vars.osd_alignment ~= "left" and M.vars.osd_alignment ~= "center" and M.vars.osd_alignment ~= "right" then
        M.vars.osd_alignment = "center"
    end

    if M.vars.osk_bar_position ~= "off" and M.vars.osk_bar_position ~= "top" and M.vars.osk_bar_position ~= "bottom" then
        M.vars.osk_bar_position = "off"
    end

    M.vars.osd_width_percent = clamp(tonumber(M.vars.osd_width_percent) or 100, 10, 100)
    M.vars.osd_height_px = clampStep(tonumber(M.vars.osd_height_px) or 140, 20, 400, 10)
    M.vars.osd_transparency = clamp(tonumber(M.vars.osd_transparency) or 50, 10, 100)
    M.vars.osd_h_margin_px = clamp(tonumber(M.vars.osd_h_margin_px) or 0, 0, 400)
    M.vars.osd_v_margin_px = clamp(tonumber(M.vars.osd_v_margin_px) or 0, 0, 400)
    M.vars.osd_font_px = clamp(tonumber(M.vars.osd_font_px) or 44, 8, 200)
end

---Save settings to ExtState
function M.SaveSettings()
    for key, val in pairs(M.vars) do
        r.SetExtState(M.EXT_SETTINGS_SECTION, key, tostring(val), true)
    end
end

local function RenderIdleSettingsLauncher(ctx, imgui, screenWidth, originX, originY)
    if not FONT_SMALL then return false end

    local launcherFlags = imgui.WindowFlags_NoTitleBar
        | imgui.WindowFlags_NoScrollbar
        | imgui.WindowFlags_NoMove
        | imgui.WindowFlags_NoResize
        | imgui.WindowFlags_NoCollapse
        | imgui.WindowFlags_AlwaysAutoResize
    if imgui.WindowFlags_NoSavedSettings then
        launcherFlags = launcherFlags | imgui.WindowFlags_NoSavedSettings
    end

    local baseX = originX or 0
    local baseY = originY or 0
    imgui.SetNextWindowPos(ctx, baseX + screenWidth - 66, baseY + 12, imgui.Cond_Always)
    imgui.SetNextWindowBgAlpha(ctx, 0.72)
    local visible = imgui.Begin(ctx, "##OSD_idle_launcher", true, launcherFlags)
    local popupOpen = false
    if visible then
        imgui.PushFont(ctx, FONT_SMALL)
        if imgui.Button(ctx, "OSD") then
            imgui.OpenPopup(ctx, IDLE_SETTINGS_POPUP_ID)
        end
        ui.ItemTooltip(ctx, "Open OSD settings")
        if imgui.BeginPopup(ctx, IDLE_SETTINGS_POPUP_ID) then
            popupOpen = true
            M.RenderSettingsPanel(ctx, imgui)
            imgui.EndPopup(ctx)
        end
        imgui.PopFont(ctx)
    end
    imgui.End(ctx)
    finalizeSettingsPopupState(popupOpen)
    return true
end

---Render OSD window (for standalone OSD script)
---@param ctx ImGui context
---@param imgui ImGui module
---@param screenWidth number Full screen width
---@param screenHeight number Full screen height
---@param windowWidth number Reference window width (for sizing)
---@param windowHeight number Reference window height (for sizing)
---@param originX number|nil Viewport left offset
---@param originY number|nil Viewport top offset
function M.RenderOSDWindow(ctx, imgui, screenWidth, screenHeight, windowWidth, windowHeight, originX, originY)
    if not IsValidContext(ctx) then
        DebugLog("invalid imgui context in RenderOSDWindow")
        return false
    end

    local hasText = M.state.text and M.state.text ~= ""
    if not hasText then
        return RenderIdleSettingsLauncher(ctx, imgui, screenWidth, originX, originY)
    end
    
    if not FONT_SMALL then return false end
    
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
    
    imgui.PushStyleVar(ctx, imgui.StyleVar_WindowBorderSize, 0)
    local visible, p_open = imgui.Begin(ctx, "##OSD", true, windowFlags)
    
    if visible then
        local winX, winY = imgui.GetWindowPos(ctx)
        local winW, winH = imgui.GetWindowSize(ctx)
        local renderFont = GetSizedFont(ctx, imgui, M.vars.osd_font_px)
        M.DrawOSDRect(ctx, imgui, winX, winY, winW, winH, hasText and M.state.text or "", M.state.bgColor, M.vars.osd_transparency, renderFont)

        -- Right-click settings must be opened while this window is active.
        local popupOpen = false
        if FONT_SMALL then imgui.PushFont(ctx, FONT_SMALL) end
        if imgui.BeginPopupContextWindow(ctx, "OSD_ContextMenu") then
            popupOpen = true
            M.RenderSettingsPanel(ctx, imgui)
            imgui.EndPopup(ctx)
        end
        if imgui.BeginPopup(ctx, IDLE_SETTINGS_POPUP_ID) then
            popupOpen = true
            M.RenderSettingsPanel(ctx, imgui)
            imgui.EndPopup(ctx)
        end
        if FONT_SMALL then imgui.PopFont(ctx) end
        finalizeSettingsPopupState(popupOpen)
    else
        finalizeSettingsPopupState(false)
    end
    
    imgui.End(ctx)
    imgui.PopStyleVar(ctx)
    return p_open
end

---Render full OSD settings panel (for dedicated OSD script only)
---Changes apply instantly; only persisted on Save click
---@param ctx ImGui context
---@param imgui ImGui module
function M.RenderSettingsPanel(ctx, imgui)
    -- Backup original settings on first panel open (for Cancel revert)
    if M.settingsBackup == nil then
        M.settingsBackup = {}
        for key, val in pairs(M.vars) do
            M.settingsBackup[key] = val
        end
    end
    
    -- Title with Save/Cancel buttons
    imgui.Text(ctx, "OSD Settings")
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Save##osd_save", 50, 0) then
        M.SaveSettings()
        M.settingsBackup = nil
        if imgui.CloseCurrentPopup then imgui.CloseCurrentPopup(ctx) end
        return
    end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Cancel##osd_cancel", 50, 0) then
        -- Restore from backup
        restoreSettingsBackup()
        if imgui.CloseCurrentPopup then imgui.CloseCurrentPopup(ctx) end
        return
    end
    
    imgui.Separator(ctx)
    local rv

    -- Position
    local posIdx = M.vars.osd_position == "bottom" and 1 or 0
    rv, posIdx = imgui.Combo(ctx, "Position##osd_pos", posIdx, "Top\0Bottom\0")
    if rv then
        M.vars.osd_position = (posIdx == 1) and "bottom" or "top"
    end

    -- Alignment
    local alignIdx = 1
    if M.vars.osd_alignment == "left" then alignIdx = 0 end
    if M.vars.osd_alignment == "right" then alignIdx = 2 end
    rv, alignIdx = imgui.Combo(ctx, "Alignment##osd_align", alignIdx, "Left\0Center\0Right\0")
    if rv then
        if alignIdx == 0 then
            M.vars.osd_alignment = "left"
        elseif alignIdx == 2 then
            M.vars.osd_alignment = "right"
        else
            M.vars.osd_alignment = "center"
        end
    end


    rv, M.vars.osd_width_percent = ui.SliderWithInput(ctx, "Width %", M.vars.osd_width_percent, 10, 100, 1)
    rv, M.vars.osd_height_px = ui.SliderWithInput(ctx, "Height px", M.vars.osd_height_px, 20, 400, 10)

    rv, M.vars.osd_h_margin_px = ui.SliderWithInput(ctx, "H margin px", M.vars.osd_h_margin_px, 0, 400, 10)
    rv, M.vars.osd_v_margin_px = ui.SliderWithInput(ctx, "V margin px", M.vars.osd_v_margin_px, 0, 400, 10)
    rv, M.vars.osd_font_px = ui.SliderWithInput(ctx, "Font px", M.vars.osd_font_px, 8, 200, 1)
    rv, M.vars.osd_transparency = ui.SliderWithInput(ctx, "Transparency %", M.vars.osd_transparency, 0, 100, 5)
end

---Render OSD position toggle for OSK (simple on/off/top/bottom)
---@param ctx ImGui context
---@param imgui ImGui module
function M.RenderOSKPositionToggle(ctx, imgui)
    local posIdx
    if M.vars.osk_bar_position == "off" then
        posIdx = 0
    elseif M.vars.osk_bar_position == "top" then
        posIdx = 1
    else
        posIdx = 2
    end
    
    local rv
    rv, posIdx = imgui.Combo(ctx, "OSK OSD Position##osk_osd_pos", posIdx, "Off\0Top\0Bottom\0")
    if rv then
        if posIdx == 0 then
            M.vars.osk_bar_position = "off"
        elseif posIdx == 1 then
            M.vars.osk_bar_position = "top"
        else
            M.vars.osk_bar_position = "bottom"
        end
        M.SaveSettings()
    end
end

return M
