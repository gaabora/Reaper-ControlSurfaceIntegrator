
--[[
 * ReaScript Name: CSI OSD on-screen display
 * About: Show OSD text box with data from CSI control surface using ImGui.
 *        Displays at top or bottom of screen, with configurable size and transparency.
 * Author: CSI Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 2.0.0
 * Notes: Requires ReaImGui. Shares OSD UI logic with CSI OSK via osd_ui module.
--]]

-- Requires ReaImGui
if not reaper.ImGui_GetBuiltinPath then
    reaper.ShowMessageBox("Script needs ReaImGui.\nPlease install it in the next window.", "MISSING DEPENDENCY", 0)
    reaper.ReaPack_BrowsePackages('^ReaImGui:')
    return
end

local r = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
package.path = scriptDir .. "?.lua;" .. r.ImGui_GetBuiltinPath() .. '/?.lua;' .. package.path

local imgui = require "imgui" "0.9.3"
local osd_ui = require("osd_ui")

local ctx = nil
local FONT = nil
local FONT_SMALL = nil

local function SetToolbarButtonState(set)
    local _, _, sec, cmd = r.get_action_context()
    r.SetToggleCommandState(sec, cmd, set or 0)
    r.RefreshToolbar2(sec, cmd)
end

local function RenderContextMenu()
    if imgui.BeginPopupContextWindow(ctx, "OSD_ContextMenu") then
        -- Position options
        local isTop = osd_ui.vars.osd_position == "top"
        if imgui.MenuItem(ctx, "Top", nil, isTop) then
            osd_ui.vars.osd_position = "top"
            osd_ui.SaveSettings()
        end
        if imgui.MenuItem(ctx, "Bottom", nil, not isTop) then
            osd_ui.vars.osd_position = "bottom"
            osd_ui.SaveSettings()
        end
        
        imgui.Separator(ctx)
        
        -- Configurable settings
        osd_ui.RenderSettingsPanel(ctx, imgui)
        
        imgui.EndPopup(ctx)
    end
end

local function main()
    osd_ui.PollOSD()
    
    if not osd_ui.vars.osd_enabled or not osd_ui.state.text or osd_ui.state.text == "" then
        r.defer(main)
        return
    end
    
    local screenW, screenH = 1920, 1080
    if r.my_getViewport then
        local left, top, right, bottom = r.my_getViewport(0, 0, 0, 0, 0, 0, 0, 0, true)
        if left and top and right and bottom and right > left and bottom > top then
            screenW = right - left
            screenH = bottom - top
        end
    end

    local p_open = osd_ui.RenderOSDWindow(ctx, imgui, screenW, screenH, screenW, screenH)
    
    RenderContextMenu()
    
    if not p_open then
        osd_ui.SaveSettings()
        SetToolbarButtonState(-1)
        return
    end
    
    r.defer(main)
end

local function Init()
    SetToolbarButtonState(1)
    osd_ui.LoadSettings()

    ctx = imgui.CreateContext('CSI OSD')
    FONT = imgui.CreateFont('sans-serif', 13)
    FONT_SMALL = imgui.CreateFont('sans-serif', 11)
    imgui.Attach(ctx, FONT)
    imgui.Attach(ctx, FONT_SMALL)
    
    osd_ui.SetFont(FONT_SMALL)

    main()
    
    r.atexit(function()
        osd_ui.SaveSettings()
        SetToolbarButtonState(-1)
    end)
end

Init()
