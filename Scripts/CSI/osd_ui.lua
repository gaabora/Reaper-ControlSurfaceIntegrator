--[[
  osd_ui.lua - Shared OSD rendering and UI logic for ImGui-based displays.
  Used by both the OSK (as part of RenderOSDBar) and standalone OSD script.
]]

local r = reaper
local M = {}

-- OSD state and settings
M.state = {
    text = "",
    bgColor = 0x333333ff,
    showUntil = 0,
    lastMsg = nil,
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

M.EXT_SECTION = "CSI_TMP"
M.EXT_KEY = "OSD"
M.EXT_SETTINGS_SECTION = "CSI_OSD_SETTINGS"

M.settingsBackup = nil  -- Backup of settings when menu opens (for Cancel revert)

local FONT_SMALL = nil
local FONT_CACHE = {}
local DEBUG_OSD = false

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

local function resetHiddenState()
    M.state.text = ""
    M.state.showUntil = 0
    M.state.bgColor = M.hexToImCol(M.vars.osd_bg_off)
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

---Poll OSD message from ExtState
function M.PollOSD()
    local msg = r.GetExtState(M.EXT_SECTION, M.EXT_KEY)
    if msg ~= M.state.lastMsg then
        M.state.lastMsg = msg
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

---Render OSD bar (used by OSK as embedded bar)
---@param ctx ImGui context
---@param imgui ImGui module
---@param containerWidth number
---@param containerHeight number
function M.RenderOSDBar(ctx, imgui, containerWidth, containerHeight)
    if not M.state.text or M.state.text == "" then
        return
    end
    
    if not FONT_SMALL then return end
    
    containerWidth = containerWidth or imgui.GetWindowSize(ctx)
    local width = containerWidth
    local height = math.max(20, containerHeight * 0.10)
    
    imgui.PushFont(ctx, FONT_SMALL)
    
    -- Background
    local drawList = imgui.GetWindowDrawList(ctx)
    local startX = 0
    local startY = 0
    
    local bgCol = M.state.bgColor
    if M.vars.osd_transparency then
        bgCol = toAlpha(bgCol, M.vars.osd_transparency)
    end
    
    imgui.DrawList_AddRectFilled(drawList, startX, startY, startX + width, startY + height, bgCol, 4)
    
    -- Text (centered)
    local textCol = M.getContrastTextColorFromCol(M.state.bgColor)
    textCol = (textCol & 0xFFFFFF00) | 0xFF
    
    local textWidth = imgui.CalcTextSize(ctx, M.state.text)
    local textX = startX + (width - textWidth) / 2
    local textY = startY + (height - imgui.CalcTextSize(ctx, "M")) / 2
    
    imgui.DrawList_AddText(drawList, textX, textY, textCol, M.state.text)
    
    imgui.PopFont(ctx)
end

---Slider with optional manual input field
---Slider and input both update the same value; no step rounding on input
---@param useInput boolean if true, show small input field after slider
---@return changed boolean, newValue number
local function SliderWithInput(ctx, imgui, label, currentVal, minVal, maxVal, step, useInput)
    local changed = false
    local newVal = currentVal
    
    -- Slider (no label, no value display)
    local rv
    rv, newVal = imgui.SliderInt(ctx, "##" .. label, newVal, minVal, maxVal)
    if rv then
        if step and step > 0 then
            newVal = math.floor((newVal + step / 2) / step) * step
        end
        newVal = clamp(newVal, minVal, maxVal)
        changed = true
    end
    
    -- Optional input field
    if useInput then
        imgui.SameLine(ctx)
        imgui.SetNextItemWidth(ctx, 50)
        local inputVal = tostring(math.floor(newVal))
        rv, inputVal = imgui.InputText(ctx, "##" .. label .. "_input", inputVal, 5)
        if rv then
            local numVal = tonumber(inputVal)
            if numVal then
                newVal = clamp(numVal, minVal, maxVal)
                changed = true
            end
        end
    end
    
    -- Label
    imgui.SameLine(ctx)
    imgui.Text(ctx, label)
    
    return changed, newVal
end

---Combo control component (manages tempSettings)
---@return changed boolean, newIndex number
local function ComboControl(ctx, imgui, label, currentIndex, options)
    local rv, idx = imgui.Combo(ctx, label, currentIndex, options)
    return rv, idx
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
    if (not hasText and not M.state.menuOpen) then
        return false
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
        local drawList = imgui.GetWindowDrawList(ctx)
        local winX, winY = imgui.GetWindowPos(ctx)
        local winW, winH = imgui.GetWindowSize(ctx)
        
        -- Background
        local bgCol = M.state.bgColor
        if M.vars.osd_transparency then
            bgCol = toAlpha(bgCol, M.vars.osd_transparency)
        end
        
        imgui.DrawList_AddRectFilled(drawList, winX, winY, winX + winW, winY + winH, bgCol, 0)
        
        -- Text (centered)
        local textCol = M.getContrastTextColorFromCol(M.state.bgColor)
        textCol = (textCol & 0xFFFFFF00) | 0xFF
        
        local shownText = hasText and M.state.text or ""
        local renderFont = GetSizedFont(ctx, imgui, M.vars.osd_font_px)
        if renderFont then imgui.PushFont(ctx, renderFont) end
        local textWidth = imgui.CalcTextSize(ctx, shownText)
        local _, lineHeight = imgui.CalcTextSize(ctx, "M")
        local textX = winX + (winW - textWidth) / 2
        local textY = winY + (winH - lineHeight) / 2

        if shownText ~= "" then
            imgui.DrawList_AddText(drawList, textX, textY, textCol, shownText)
        end
        if renderFont then imgui.PopFont(ctx) end

        -- Right-click settings must be opened while this window is active.
        local wasMenuOpen = M.state.menuOpen
        local popupOpen = false
        if FONT_SMALL then imgui.PushFont(ctx, FONT_SMALL) end
        if imgui.BeginPopupContextWindow(ctx, "OSD_ContextMenu") then
            popupOpen = true
            M.RenderSettingsPanel(ctx, imgui)
            imgui.EndPopup(ctx)
        end
        if FONT_SMALL then imgui.PopFont(ctx) end
        M.state.menuOpen = popupOpen
        
        -- Clear settings backup when menu closes (user cancelled by closing without saving)
        if wasMenuOpen and not popupOpen and M.settingsBackup then
            for key, val in pairs(M.settingsBackup) do
                M.vars[key] = val
            end
            M.settingsBackup = nil
        end
    else
        M.state.menuOpen = false
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
        return
    end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Cancel##osd_cancel", 50, 0) then
        -- Restore from backup
        for key, val in pairs(M.settingsBackup) do
            M.vars[key] = val
        end
        M.settingsBackup = nil
        return
    end
    
    imgui.Separator(ctx)
    local rv

    -- Position
    local posIdx = M.vars.osd_position == "bottom" and 1 or 0
    rv, posIdx = ComboControl(ctx, imgui, "Position##osd_pos", posIdx, "Top\0Bottom\0")
    if rv then
        M.vars.osd_position = (posIdx == 1) and "bottom" or "top"
    end

    -- Alignment
    local alignIdx = 1
    if M.vars.osd_alignment == "left" then alignIdx = 0 end
    if M.vars.osd_alignment == "right" then alignIdx = 2 end
    rv, alignIdx = ComboControl(ctx, imgui, "Alignment##osd_align", alignIdx, "Left\0Center\0Right\0")
    if rv then
        if alignIdx == 0 then
            M.vars.osd_alignment = "left"
        elseif alignIdx == 2 then
            M.vars.osd_alignment = "right"
        else
            M.vars.osd_alignment = "center"
        end
    end


    rv, M.vars.osd_width_percent = SliderWithInput(ctx, imgui, "Width %", M.vars.osd_width_percent, 10, 100, 1, true)
    rv, M.vars.osd_height_px = SliderWithInput(ctx, imgui, "Height px", M.vars.osd_height_px, 20, 400, 10, true)

    rv, M.vars.osd_h_margin_px = SliderWithInput(ctx, imgui, "H margin px", M.vars.osd_h_margin_px, 0, 400, 10, true)
    rv, M.vars.osd_v_margin_px = SliderWithInput(ctx, imgui, "V margin px", M.vars.osd_v_margin_px, 0, 400, 10, true)
    rv, M.vars.osd_font_px = SliderWithInput(ctx, imgui, "Font px", M.vars.osd_font_px, 8, 200, 1, true)
    rv, M.vars.osd_transparency = SliderWithInput(ctx, imgui, "Transparency %", M.vars.osd_transparency, 0, 100, 5, true)
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
