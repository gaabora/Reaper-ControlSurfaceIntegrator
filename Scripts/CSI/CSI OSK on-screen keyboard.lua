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

local ctx = nil
local FONT = nil
local FONT_SMALL = nil

-- Configurable extra action buttons shown at top of right-click menu.
-- Each entry: { title = "label", tooltip = "description", action = <REAPER command ID> }
local TOOLBAR_ACTIONS = {
    { action = 42348, title = "reset surfaces", tooltip = "Reset all MIDI control surface devices" },
    { action = 41175, title = "reset MIDI", tooltip = "Reset all MIDI devices" },
    -- Add more: { title = "Label", tooltip = "Description", action = 12345 },
}

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

local function RenderContextMenu(ctx)
    if imgui.BeginPopupContextWindow(ctx, "OSK_ContextMenu") then
        -- Extra toolbar action buttons at the top
        if #TOOLBAR_ACTIONS > 0 then
            for _, act in ipairs(TOOLBAR_ACTIONS) do
                if imgui.MenuItem(ctx, act.title) then
                    r.Main_OnCommand(act.action, 0)
                    r.ShowConsoleMsg("[CSI OSK] Action: " .. act.action .. " (" .. (act.tooltip or act.title) .. ")\n")
                end
                if act.tooltip and imgui.IsItemHovered(ctx) then
                    if imgui.BeginTooltip(ctx) then
                        imgui.Text(ctx, act.tooltip)
                        imgui.EndTooltip(ctx)
                    end
                end
            end
            imgui.Separator(ctx)
        end

        -- Surface tabs (if multiple surfaces)
        if #data.surfaces > 1 then
            local toggled
            toggled, data.vars.show_all_surfaces = imgui.Checkbox(ctx, "Show all surfaces", data.vars.show_all_surfaces)
            if toggled then data.SaveSettings() end

            if not data.vars.show_all_surfaces then
                imgui.Separator(ctx)
                for i, name in ipairs(data.surfaces) do
                    local isSelected = (i == data.currentSurface)
                    if imgui.MenuItem(ctx, name, nil, isSelected) then
                        data.currentSurface = i
                    end
                end
            end
            imgui.Separator(ctx)
        end

        -- Settings
        local rv

        rv = SliderSetting(ctx, "Zoom", data.vars.zoom, "zoom", 0.5, 3.0, "%.1f")
        rv = SliderSetting(ctx, "Aspect (W/H)", data.vars.aspect, "aspect", 0.5, 2.0, "%.2f")
        rv = SliderSetting(ctx, "H Padding", data.vars.pad_h, "pad_h", 0, 20, "%.0f")
        rv = SliderSetting(ctx, "V Padding", data.vars.pad_v, "pad_v", 0, 20, "%.0f")
        rv = SliderSetting(ctx, "Arrow Angle", data.vars.arrow_angle, "arrow_angle", 60, 150, "%.0f")
        rv = SliderSetting(ctx, "Window Alpha", data.vars.transparency, "transparency", 0.2, 1.0, "%.2f")
        rv = SliderSetting(ctx, "Button Alpha", data.vars.btn_transparency, "btn_transparency", 0.2, 1.0, "%.2f")
        rv = SliderSetting(ctx, "Tooltip Delay", data.vars.tooltip_delay, "tooltip_delay", 0.0, 5.0, "%.1fs")

        rv, data.vars.clickable = imgui.Checkbox(ctx, "Clickable buttons", data.vars.clickable)
        if rv then data.SaveSettings() end

        imgui.Separator(ctx)
        imgui.Text(ctx, "Label Replacements (word=replacement):")
        local changed
        changed, data.vars.label_replacements = imgui.InputText(ctx, "##replacements", data.vars.label_replacements)
        if changed then
            data.parseLabelReplacements(data.vars.label_replacements)
            data.SaveSettings()
        end

        imgui.EndPopup(ctx)
    end
end

local function main()
    if not data.PollData() then
        return -- Close requested
    end
    data.PollActiveSurface()

    if #data.surfaces == 0 then
        r.defer(main)
        return
    end

    imgui.PushStyleColor(ctx, imgui.Col_WindowBg, data.COLORS.win_bg)
    imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, data.COLORS.win_bg)
    imgui.SetNextWindowBgAlpha(ctx, data.vars.transparency)

    local visible, p_open = imgui.Begin(ctx, 'CSI On-Screen Keyboard', true,
        imgui.WindowFlags_NoScrollbar
        | imgui.WindowFlags_NoCollapse
        | imgui.WindowFlags_AlwaysAutoResize
        | imgui.WindowFlags_NoTitleBar
    )
    imgui.PopStyleColor(ctx, 2)

    if visible then
        imgui.PushFont(ctx, FONT)

        RenderContextMenu(ctx)

        -- Render surface(s) directly (no BeginChild — allows AlwaysAutoResize to work)
        if data.vars.show_all_surfaces and #data.surfaces > 1 then
            render.RenderAllSurfaces(ctx)
        else
            local surfName = data.surfaces[data.currentSurface]
            if surfName then render.RenderSurface(ctx, surfName) end
        end

        render.RenderOSDBar(ctx)
        config.RenderConfigEditor(ctx)

        imgui.PopFont(ctx)
        imgui.End(ctx)
    end

    if p_open then
        r.defer(main)
    else
        -- Cleanup
        data.SaveSettings()
        SetToolbarButtonState(-1)
    end
end

-- ================================================================
-- Init
-- ================================================================
local function Init()
    SetToolbarButtonState(1)
    data.LoadSettings()

    ctx = imgui.CreateContext('CSI OSK')
    FONT = imgui.CreateFont('sans-serif', 13)
    FONT_SMALL = imgui.CreateFont('sans-serif', 11)
    imgui.Attach(ctx, FONT)
    imgui.Attach(ctx, FONT_SMALL)

    render.SetFonts(FONT, FONT_SMALL)

    -- Initial poll
    data.PollData()

    main()
    r.atexit(function()
        data.SaveSettings()
        SetToolbarButtonState(-1)
    end)
end

Init()
