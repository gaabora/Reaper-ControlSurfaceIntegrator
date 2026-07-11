--[[
 * ReaScript Name: CSI OSK on-screen keyboard
 * About: On-screen keyboard display for CSI control surfaces.
 *        Shows colorful buttons matching the physical surface layout,
 *        with labels reflecting current zone/modifier bindings.
 * Author: CSI Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 1.0.0
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
local data = require("osk_data")
local render = require("osk_render")
local config = require("osk_config")
local osd_ui = require("osd_ui")

local ctx = nil
local FONT = nil
local FONT_SMALL = nil
local surfaceWindows = {}
local requestCloseAll = false

local WINDOW_FLAGS_BASE = imgui.WindowFlags_NoScrollbar
    | imgui.WindowFlags_NoCollapse
    | imgui.WindowFlags_AlwaysAutoResize

local function GetWindowFlags()
    local flags = WINDOW_FLAGS_BASE
    if not data.vars.titlebar_enabled then
        flags = flags | imgui.WindowFlags_NoTitleBar
    end
    return flags
end

-- Configurable extra action buttons shown at top of right-click menu.
-- Each entry: { title = "label", tooltip = "description", action = <REAPER command ID> }
local TOOLBAR_ACTIONS = {
    { action = 42348, title = "reset surfaces", tooltip = "Reset all MIDI control surface devices" },
    { action = 41175, title = "reset MIDI", tooltip = "Reset all MIDI devices" },
    -- Add more: { title = "Label", tooltip = "Description", action = 12345 },
}

local main_separate

local function IsValidContext()
    if not ctx then return false end
    if r.ImGui_ValidatePtr then
        return r.ImGui_ValidatePtr(ctx, "ImGui_Context*")
    end
    return true
end

local function SetToolbarButtonState(set)
    local _, _, sec, cmd = r.get_action_context()
    r.SetToggleCommandState(sec, cmd, set or 0)
    r.RefreshToolbar2(sec, cmd)
end

local function SliderSetting(ctx, label, currentValue, storeKey, min, max, fmt)
    local rv, value = imgui.SliderDouble(ctx, label, currentValue, min, max, fmt)
    if rv then
        data.vars[storeKey] = value
        data.SaveSettings()
    end
    return rv, value
end

local function EnsureSurfaceWindow(surfName)
    local win = surfaceWindows[surfName]
    if win then return win end

    win = {
        open = true,
        posApplied = false,
    }
    surfaceWindows[surfName] = win
    return win
end

local function EnsureSurfaceWindows()
    for _, surfName in ipairs(data.surfaces) do
        EnsureSurfaceWindow(surfName)
    end
end

local function AnyWindowOpen()
    for _, surfName in ipairs(data.surfaces) do
        local win = surfaceWindows[surfName]
        if win and win.open then
            return true
        end
    end
    return false
end

local function SetWindowMode(mode)
    if data.vars.window_mode == mode then return end
    data.vars.window_mode = mode
    data.SaveSettings()

    if mode == "separate" then
        EnsureSurfaceWindows()
        for _, surfName in ipairs(data.surfaces) do
            local win = surfaceWindows[surfName]
            if win then
                win.open = true
                win.posApplied = false
            end
        end
    end
end

local function RenderContextMenu(activeCtx, options)
    local popupId = options.popupId or "OSK_ContextMenu"
    if imgui.BeginPopupContextWindow(activeCtx, popupId) then
        -- Extra toolbar action buttons at the top
        if #TOOLBAR_ACTIONS > 0 then
            for _, act in ipairs(TOOLBAR_ACTIONS) do
                if imgui.MenuItem(activeCtx, act.title) then
                    r.Main_OnCommand(act.action, 0)
                    r.ShowConsoleMsg("[CSI OSK] Action: " .. act.action .. " (" .. (act.tooltip or act.title) .. ")\n")
                end
                if act.tooltip and imgui.IsItemHovered(activeCtx) then
                    if imgui.BeginTooltip(activeCtx) then
                        imgui.Text(activeCtx, act.tooltip)
                        imgui.EndTooltip(activeCtx)
                    end
                end
            end
            imgui.Separator(activeCtx)
        end

        local isCombined = data.vars.window_mode ~= "separate"
        if imgui.MenuItem(activeCtx, "Combined window", nil, isCombined) then
            SetWindowMode("combined")
        end
        if imgui.MenuItem(activeCtx, "Separate windows", nil, not isCombined) then
            SetWindowMode("separate")
        end

        imgui.Separator(activeCtx)
        local toggled
        toggled, data.vars.titlebar_enabled = imgui.Checkbox(activeCtx, "Show titlebar", data.vars.titlebar_enabled)
        if toggled then data.SaveSettings() end

        if options.allowCloseAll then
            if imgui.MenuItem(activeCtx, "Close all") then
                requestCloseAll = true
            end
            imgui.Separator(activeCtx)
        end

        -- Surface tabs (if multiple surfaces)
        if options.allowSurfaceSelection and #data.surfaces > 1 then
            local toggled
            toggled, data.vars.show_all_surfaces = imgui.Checkbox(activeCtx, "Show all surfaces", data.vars.show_all_surfaces)
            if toggled then data.SaveSettings() end

            if not data.vars.show_all_surfaces then
                imgui.Separator(activeCtx)
                for i, name in ipairs(data.surfaces) do
                    local isSelected = (i == data.currentSurface)
                    if imgui.MenuItem(activeCtx, name, nil, isSelected) then
                        data.currentSurface = i
                    end
                end
            end
            imgui.Separator(activeCtx)
        end

        -- Settings
        local rv

        rv = SliderSetting(activeCtx, "Zoom", data.vars.zoom, "zoom", 0.5, 3.0, "%.1f")
        rv = SliderSetting(activeCtx, "Aspect (W/H)", data.vars.aspect, "aspect", 0.5, 2.0, "%.2f")
        rv = SliderSetting(activeCtx, "H Padding", data.vars.pad_h, "pad_h", 0, 20, "%.0f")
        rv = SliderSetting(activeCtx, "V Padding", data.vars.pad_v, "pad_v", 0, 20, "%.0f")
        rv = SliderSetting(activeCtx, "Arrow Angle", data.vars.arrow_angle, "arrow_angle", 60, 150, "%.0f")
        rv = SliderSetting(activeCtx, "Window Alpha", data.vars.transparency, "transparency", 0.2, 1.0, "%.2f")
        rv = SliderSetting(activeCtx, "Button Alpha", data.vars.btn_transparency, "btn_transparency", 0.2, 1.0, "%.2f")
        rv = SliderSetting(activeCtx, "Tooltip Delay", data.vars.tooltip_delay, "tooltip_delay", 0.0, 5.0, "%.1fs")

        rv, data.vars.clickable = imgui.Checkbox(activeCtx, "Clickable buttons", data.vars.clickable)
        if rv then data.SaveSettings() end

        imgui.Separator(activeCtx)
        imgui.Text(activeCtx, "Label Replacements (word=replacement):")
        local changed
        changed, data.vars.label_replacements = imgui.InputText(activeCtx, "##replacements", data.vars.label_replacements)
        if changed then
            data.parseLabelReplacements(data.vars.label_replacements)
            data.SaveSettings()
        end

        imgui.Separator(activeCtx)
        osd_ui.RenderOSKPositionToggle(activeCtx, imgui)

        imgui.EndPopup(activeCtx)
    end
end

local function main_combined()
    if not IsValidContext() then
        SetToolbarButtonState(-1)
        return
    end

    if not data.PollData() then
        return -- Close requested
    end
    data.PollActiveSurface()

    if #data.surfaces == 0 then
        r.defer(main_combined)
        return
    end

    imgui.PushStyleColor(ctx, imgui.Col_WindowBg, data.COLORS.win_bg)
    imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, data.COLORS.win_bg)
    imgui.SetNextWindowBgAlpha(ctx, data.vars.transparency)

    local visible, p_open = imgui.Begin(ctx, 'CSI On-Screen Keyboard', true,
        GetWindowFlags()
    )
    imgui.PopStyleColor(ctx, 2)

    if visible then
        imgui.PushFont(ctx, FONT)

        if not config.ShouldSuppressContextMenu or not config.ShouldSuppressContextMenu() then
            RenderContextMenu(ctx, { popupId = "OSK_ContextMenu_Combined", allowSurfaceSelection = true, allowCloseAll = false })
        end

        if osd_ui.vars.osk_bar_position == "top" then
            render.RenderOSDBar(ctx)
        end

        -- Render surface(s) directly (no BeginChild — allows AlwaysAutoResize to work)
        if data.vars.show_all_surfaces and #data.surfaces > 1 then
            render.RenderAllSurfaces(ctx)
        else
            local surfName = data.surfaces[data.currentSurface]
            if surfName then render.RenderSurface(ctx, surfName) end
        end

        if osd_ui.vars.osk_bar_position == "bottom" then
            render.RenderOSDBar(ctx)
        end
        imgui.PopFont(ctx)
    end
    imgui.End(ctx)

    -- Config editor is a separate top-level window; render outside main window's Begin/End
    config.RenderConfigEditor(ctx)

    if not p_open then
        -- Cleanup
        data.SaveSettings()
        SetToolbarButtonState(-1)
        return
    end

    if data.vars.window_mode == "separate" then
        EnsureSurfaceWindows()
        r.defer(main_separate)
    else
        r.defer(main_combined)
    end
end

main_separate = function()
    if not IsValidContext() then
        SetToolbarButtonState(-1)
        return
    end

    if not data.PollData() then
        return
    end
    data.PollActiveSurface()

    if #data.surfaces == 0 then
        r.defer(main_separate)
        return
    end

    EnsureSurfaceWindows()
    requestCloseAll = false

    for _, surfName in ipairs(data.surfaces) do
        local win = surfaceWindows[surfName]
        if win and win.open then
            local pos = data.surfacePos[surfName]
            if pos and not win.posApplied then
                if IsValidContext() then
                    imgui.SetNextWindowPos(ctx, pos.x, pos.y, imgui.Cond_Always)
                else
                    SetToolbarButtonState(-1)
                    return
                end
                win.posApplied = true
            end

            imgui.PushStyleColor(ctx, imgui.Col_WindowBg, data.COLORS.win_bg)
            imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, data.COLORS.win_bg)
            imgui.SetNextWindowBgAlpha(ctx, data.vars.transparency)

            local visible, p_open = imgui.Begin(ctx, 'CSI OSK - ' .. surfName, true, GetWindowFlags())
            imgui.PopStyleColor(ctx, 2)

            local x, y = imgui.GetWindowPos(ctx)
            local oldPos = data.surfacePos[surfName]
            if not oldPos or math.abs(oldPos.x - x) > 0.5 or math.abs(oldPos.y - y) > 0.5 then
                data.surfacePos[surfName] = { x = x, y = y }
                data.SaveSettings()
            end

            if visible then
                imgui.PushFont(ctx, FONT)
                if not config.ShouldSuppressContextMenu or not config.ShouldSuppressContextMenu() then
                    RenderContextMenu(ctx, { popupId = "OSK_ContextMenu_" .. surfName, allowSurfaceSelection = false, allowCloseAll = true })
                end

                if osd_ui.vars.osk_bar_position == "top" then
                    render.RenderOSDBar(ctx)
                end

                render.RenderSurface(ctx, surfName)

                if osd_ui.vars.osk_bar_position == "bottom" then
                    render.RenderOSDBar(ctx)
                end

                imgui.PopFont(ctx)
            end
            imgui.End(ctx)

            win.open = p_open
        end
    end

    -- Config editor is a separate top-level window; render outside per-surface loop
    config.RenderConfigEditor(ctx)

    if requestCloseAll then
        for _, surfName in ipairs(data.surfaces) do
            local win = surfaceWindows[surfName]
            if win then win.open = false end
        end
    end

    if data.vars.window_mode ~= "separate" then
        render.SetFonts(FONT, FONT_SMALL)
        r.defer(main_combined)
        return
    end

    if AnyWindowOpen() then
        r.defer(main_separate)
        return
    end

    data.SaveSettings()
    SetToolbarButtonState(-1)
end

-- ================================================================
-- Init
-- ================================================================
local function Init()
    SetToolbarButtonState(1)
    data.LoadSettings()
    osd_ui.LoadSettings()

    ctx = imgui.CreateContext('CSI OSK')
    FONT = imgui.CreateFont('sans-serif', 13)
    FONT_SMALL = imgui.CreateFont('sans-serif', 11)
    imgui.Attach(ctx, FONT)
    imgui.Attach(ctx, FONT_SMALL)

    render.SetFonts(FONT, FONT_SMALL)
    if render.SetConfigModule then
        render.SetConfigModule(config)
    end

    -- Initial poll
    data.PollData()

    if data.vars.window_mode == "separate" then
        EnsureSurfaceWindows()
        main_separate()
    else
        main_combined()
    end
    r.atexit(function()
        data.SaveSettings()
        osd_ui.SaveSettings()
        SetToolbarButtonState(-1)
    end)
end

Init()
