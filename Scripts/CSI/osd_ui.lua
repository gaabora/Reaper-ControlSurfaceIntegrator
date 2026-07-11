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
}

M.vars = {
    -- OSD Display Settings (only for dedicated OSD script)
    osd_enabled = true,
    osd_position = "top",          -- "top" or "bottom" (dedicated OSD overlay)
    osk_bar_position = "bottom",   -- "off", "top" or "bottom" (OSK embedded line only)
    osd_width_percent = 100,       -- 0-100 (100% = full width)
    osd_height_percent = 10,       -- 0-100 (10% = 10% of screen)
    osd_transparency = 50,         -- 0-100
    osd_margin = 0,                -- pixels from edge
    osd_debug = false,
    osd_show_idle_hint = true,
    osd_text_color = "#FFFFFF",    -- hex color for text (auto-contrast if needed)
    osd_bg_on = "#A4A4A4",         -- background when state=1
    osd_bg_off = "#333333",        -- background when state=0
}

M.EXT_SECTION = "CSI_TMP"
M.EXT_KEY = "OSD"
M.EXT_FALLBACK_SECTION = "CSI_OSD"
M.EXT_SETTINGS_SECTION = "CSI_OSD_SETTINGS"

local FONT_SMALL = nil

function M.SetFont(font)
    FONT_SMALL = font
end

local function DebugLog(...)
    if not M.vars.osd_debug then return end
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

---Get contrast text color for given background
function M.getContrastTextColor(bgHex)
    local bgCol = M.hexToImCol(bgHex)
    local red = (bgCol >> 24) & 0xFF
    local green = (bgCol >> 16) & 0xFF
    local blue = (bgCol >> 8) & 0xFF
    local luminance = 0.299 * red + 0.587 * green + 0.114 * blue
    return luminance > 186 and 0x000000ff or 0xFFFFFFff
end

---Poll OSD message from ExtState
function M.PollOSD()
    local msg = r.GetExtState(M.EXT_SECTION, M.EXT_KEY)
    if (not msg or msg == "") and M.EXT_FALLBACK_SECTION and M.EXT_FALLBACK_SECTION ~= "" then
        msg = r.GetExtState(M.EXT_FALLBACK_SECTION, M.EXT_KEY)
    end
    if msg ~= M.state.lastMsg then
        M.state.lastMsg = msg
        if not msg or msg == "" then
            M.state.text = ""
            M.state.showUntil = 0
            M.state.bgColor = M.hexToImCol(M.vars.osd_bg_off)
            return
        end
        
        -- Parse message format: "text;bgState;timeoutMs"
        local text, bgState, timeoutStr = msg:match("([^;]*);?([^;]*);?([^;]*)")
        text = text and text:match("^%s*(.-)%s*$") or ""
        
        local timeout = tonumber(timeoutStr) or 3000  -- default 3 seconds
        if bgState == "1" then
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
        M.state.text = ""
        M.state.showUntil = 0
        M.state.bgColor = M.hexToImCol(M.vars.osd_bg_off)
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

    -- Backward compatibility and sanity clamps
    if M.vars.osd_position ~= "top" and M.vars.osd_position ~= "bottom" then
        M.vars.osd_position = "top"
    end

    if M.vars.osk_bar_position ~= "off" and M.vars.osk_bar_position ~= "top" and M.vars.osk_bar_position ~= "bottom" then
        -- Migrate from older single-position setting if needed.
        if M.vars.osd_position == "top" or M.vars.osd_position == "bottom" then
            M.vars.osk_bar_position = M.vars.osd_position
        else
            M.vars.osk_bar_position = "bottom"
        end
    end

    M.vars.osd_width_percent = math.max(10, math.min(100, tonumber(M.vars.osd_width_percent) or 100))
    M.vars.osd_height_percent = math.max(5, math.min(50, tonumber(M.vars.osd_height_percent) or 10))
    M.vars.osd_transparency = math.max(10, math.min(100, tonumber(M.vars.osd_transparency) or 50))
    M.vars.osd_margin = math.max(0, math.min(200, tonumber(M.vars.osd_margin) or 0))
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
    if not M.vars.osd_enabled or not M.state.text or M.state.text == "" then
        return
    end
    
    if not FONT_SMALL then return end
    
    containerWidth = containerWidth or imgui.GetWindowSize(ctx)
    local margin = M.vars.osd_margin or 0
    local width = containerWidth * (M.vars.osd_width_percent / 100) - (margin * 2)
    local height = math.max(20, containerHeight * (M.vars.osd_height_percent / 100))
    
    imgui.PushFont(ctx, FONT_SMALL)
    
    -- Background
    local drawList = imgui.GetWindowDrawList(ctx)
    local startX = margin
    local startY = margin
    
    local bgCol = M.state.bgColor
    if M.vars.osd_transparency then
        bgCol = (bgCol & 0xFFFFFF00) | math.floor((M.vars.osd_transparency / 100) * 255)
    end
    
    imgui.DrawList_AddRectFilled(drawList, startX, startY, startX + width, startY + height, bgCol, 4)
    
    -- Text (centered)
    local textCol = M.getContrastTextColor(string.format("#%06X", (M.state.bgColor >> 8) & 0xFFFFFF))
    if M.vars.osd_transparency then
        textCol = (textCol & 0xFFFFFF00) | math.floor((M.vars.osd_transparency / 100) * 255)
    end
    
    local textWidth = imgui.CalcTextSize(ctx, M.state.text)
    local textX = startX + (width - textWidth) / 2
    local textY = startY + (height - imgui.CalcTextSize(ctx, "M")) / 2
    
    imgui.DrawList_AddText(drawList, textX, textY, textCol, M.state.text)
    
    imgui.PopFont(ctx)
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
    if (not hasText and not M.vars.osd_show_idle_hint) then
        return false
    end
    
    if not FONT_SMALL then return false end
    
    local margin = M.vars.osd_margin or 0
    local width = screenWidth * (M.vars.osd_width_percent / 100)
    local height = math.max(30, screenHeight * (M.vars.osd_height_percent / 100))
    if not hasText then
        height = 28
    end
    
    local baseX = originX or 0
    local baseY = originY or 0
    local xPos = baseX + margin
    local yPos = M.vars.osd_position == "top"
        and (baseY + margin)
        or (baseY + screenHeight - height - margin)

    DebugLog("render overlay x=", math.floor(xPos), "y=", math.floor(yPos), "w=", math.floor(width), "h=", math.floor(height), "screen=", math.floor(screenWidth) .. "x" .. math.floor(screenHeight))
    
    imgui.SetNextWindowPos(ctx, xPos, yPos, imgui.Cond_Always)
    imgui.SetNextWindowSize(ctx, width, height, imgui.Cond_Always)
    imgui.SetNextWindowBgAlpha(ctx, (M.vars.osd_transparency or 50) / 100)
    
    local windowFlags = imgui.WindowFlags_NoTitleBar
        | imgui.WindowFlags_NoScrollbar
        | imgui.WindowFlags_NoMove
        | imgui.WindowFlags_NoResize
        | imgui.WindowFlags_NoCollapse
    
    local visible, p_open = imgui.Begin(ctx, "##OSD", true, windowFlags)
    
    if visible then
        imgui.PushFont(ctx, FONT_SMALL)
        
        local drawList = imgui.GetWindowDrawList(ctx)
        local winX, winY = imgui.GetWindowPos(ctx)
        local winW, winH = imgui.GetWindowSize(ctx)
        
        -- Background
        local bgCol = M.state.bgColor
        if M.vars.osd_transparency then
            bgCol = (bgCol & 0xFFFFFF00) | math.floor((M.vars.osd_transparency / 100) * 255)
        end
        
        imgui.DrawList_AddRectFilled(drawList, winX, winY, winX + winW, winY + winH, bgCol, 0)
        
        -- Text (centered)
        local textCol = M.getContrastTextColor(string.format("#%06X", (M.state.bgColor >> 8) & 0xFFFFFF))
        textCol = (textCol & 0xFFFFFF00) | 0xFF
        
        local shownText = hasText and M.state.text or "OSD idle - right-click for settings"
        local textWidth = imgui.CalcTextSize(ctx, shownText)
        local _, lineHeight = imgui.CalcTextSize(ctx, "M")
        local textX = winX + (winW - textWidth) / 2
        local textY = winY + (winH - lineHeight) / 2
        
        imgui.DrawList_AddText(drawList, textX, textY, textCol, shownText)

        -- Right-click settings must be opened while this window is active.
        if imgui.BeginPopupContextWindow(ctx, "OSD_ContextMenu") then
            local isTop = M.vars.osd_position == "top"
            if imgui.MenuItem(ctx, "Top", nil, isTop) then
                M.vars.osd_position = "top"
                M.SaveSettings()
            end
            if imgui.MenuItem(ctx, "Bottom", nil, not isTop) then
                M.vars.osd_position = "bottom"
                M.SaveSettings()
            end

            M.RenderSettingsPanel(ctx, imgui)
            imgui.EndPopup(ctx)
        end
        
        imgui.PopFont(ctx)
    end
    
    imgui.End(ctx)
    return p_open
end

---Render OSD settings panel (for right-click menu)
---@param ctx ImGui context
---@param imgui ImGui module
---Render full OSD settings panel (for dedicated OSD script only)
---@param ctx ImGui context
---@param imgui ImGui module
function M.RenderSettingsPanel(ctx, imgui)
    imgui.Separator(ctx)
    imgui.Text(ctx, "OSD Settings:")
    
    local rv
    rv, M.vars.osd_enabled = imgui.Checkbox(ctx, "OSD Enabled##osd", M.vars.osd_enabled)
    if rv then M.SaveSettings() end

    rv, M.vars.osd_debug = imgui.Checkbox(ctx, "Debug console logs##osd", M.vars.osd_debug)
    if rv then M.SaveSettings() end
    
    -- Position
    local posIdx = M.vars.osd_position == "bottom" and 1 or 0
    rv, posIdx = imgui.Combo(ctx, "Position##osd_pos", posIdx, "Top\0Bottom\0")
    if rv then
        M.vars.osd_position = (posIdx == 1) and "bottom" or "top"
        M.SaveSettings()
    end
    
    -- Width percentage (0-100)
    rv, M.vars.osd_width_percent = imgui.SliderInt(ctx, "Width %##osd_w", M.vars.osd_width_percent, 10, 100)
    if rv then M.SaveSettings() end
    
    -- Height percentage (0-100)
    rv, M.vars.osd_height_percent = imgui.SliderInt(ctx, "Height %##osd_h", M.vars.osd_height_percent, 5, 50)
    if rv then M.SaveSettings() end
    
    -- Transparency (0-100)
    rv, M.vars.osd_transparency = imgui.SliderInt(ctx, "Transparency %##osd_alpha", M.vars.osd_transparency, 10, 100)
    if rv then M.SaveSettings() end
    
    -- Margin
    rv, M.vars.osd_margin = imgui.SliderInt(ctx, "Margin##osd_margin", M.vars.osd_margin, 0, 50)
    if rv then M.SaveSettings() end
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
