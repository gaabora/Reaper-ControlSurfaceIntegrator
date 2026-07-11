--[[
 * ReaScript Name: CSI OSK on-screen keyboard
 * About: On-screen keyboard display for CSI control surfaces.
 *        Shows one interactive window per surface, with labels reflecting
 *        current zone and modifier bindings.
 * Author: CSI Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 1.0.0
--]]

local r = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local host = dofile(scriptDir .. "script_host.lua")
local imgui = host.RequireImGui(scriptDir)
if not imgui then return end

local data = require("osk_data")
local render = require("osk_render")
local config = require("osk_config")
local osd_ui = require("osd_ui")
local ui = require("ui_components")

local ctx = nil
local FONT = nil
local FONT_SMALL = nil
local surfaceWindows = {}
local requestCloseAll = false

local WINDOW_FLAGS_BASE = imgui.WindowFlags_NoScrollbar
    | imgui.WindowFlags_NoCollapse
    | imgui.WindowFlags_AlwaysAutoResize

local TOOLBAR_ACTIONS = {
    { action = 42348, title = "reset surfaces", tooltip = "Reset all MIDI control surface devices" },
    { action = 41175, title = "reset MIDI", tooltip = "Reset all MIDI devices" },
}

local function GetWindowFlags()
    local flags = WINDOW_FLAGS_BASE
    if not data.vars.titlebar_enabled then
        flags = flags | imgui.WindowFlags_NoTitleBar
    end
    return flags
end

local function SliderSetting(activeCtx, label, currentValue, storeKey, minValue, maxValue, format)
    local changed, value = imgui.SliderDouble(activeCtx, label, currentValue, minValue, maxValue, format)
    if changed then
        data.vars[storeKey] = value
        data.SaveSettings()
    end
    return changed, value
end

local function EnsureSurfaceWindow(surfName)
    local window = surfaceWindows[surfName]
    if window then return window end
    window = { open = true, positionApplied = false }
    surfaceWindows[surfName] = window
    return window
end

local function EnsureSurfaceWindows()
    local currentSurfaces = {}
    for _, surfName in ipairs(data.surfaces) do
        currentSurfaces[surfName] = true
        EnsureSurfaceWindow(surfName)
    end
    for surfName in pairs(surfaceWindows) do
        if not currentSurfaces[surfName] then
            surfaceWindows[surfName] = nil
        end
    end
end

local function AnyWindowOpen()
    for _, surfName in ipairs(data.surfaces) do
        local window = surfaceWindows[surfName]
        if window and window.open then return true end
    end
    return false
end

local function RenderContextMenu(activeCtx, popupId)
    if not imgui.BeginPopupContextWindow(activeCtx, popupId) then return end

    for _, action in ipairs(TOOLBAR_ACTIONS) do
        if imgui.MenuItem(activeCtx, action.title) then
            r.Main_OnCommand(action.action, 0)
            r.ShowConsoleMsg("[CSI OSK] Action: " .. action.action .. " (" .. action.tooltip .. ")\n")
        end
        ui.ItemTooltip(activeCtx, action.tooltip)
    end

    imgui.Separator(activeCtx)
    local changed
    changed, data.vars.titlebar_enabled = imgui.Checkbox(activeCtx, "Show titlebar", data.vars.titlebar_enabled)
    if changed then data.SaveSettings() end
    if imgui.MenuItem(activeCtx, "Close all") then requestCloseAll = true end

    imgui.Separator(activeCtx)
    SliderSetting(activeCtx, "Zoom", data.vars.zoom, "zoom", 0.5, 3.0, "%.1f")
    SliderSetting(activeCtx, "Aspect (W/H)", data.vars.aspect, "aspect", 0.5, 2.0, "%.2f")
    SliderSetting(activeCtx, "H Padding", data.vars.pad_h, "pad_h", 0, 20, "%.0f")
    SliderSetting(activeCtx, "V Padding", data.vars.pad_v, "pad_v", 0, 20, "%.0f")
    SliderSetting(activeCtx, "Arrow Angle", data.vars.arrow_angle, "arrow_angle", 60, 150, "%.0f")
    SliderSetting(activeCtx, "Window Alpha", data.vars.transparency, "transparency", 0.2, 1.0, "%.2f")
    SliderSetting(activeCtx, "Button Alpha", data.vars.btn_transparency, "btn_transparency", 0.2, 1.0, "%.2f")
    SliderSetting(activeCtx, "Tooltip Delay", data.vars.tooltip_delay, "tooltip_delay", 0.0, 5.0, "%.1fs")

    changed, data.vars.interactive = imgui.Checkbox(activeCtx, "Interactive controls", data.vars.interactive)
    if changed then data.SaveSettings() end

    imgui.Separator(activeCtx)
    changed, data.vars.label_replacements = ui.LabelReplacementEditor(
        activeCtx,
        "Label replacements",
        data.vars.label_replacements,
        data.GetLabelReplacementHelp(),
        { inputId = "##replacements" }
    )
    if changed then
        data.parseLabelReplacements(data.vars.label_replacements)
        data.SaveSettings()
    end

    imgui.Separator(activeCtx)
    osd_ui.RenderOSKPositionToggle(activeCtx, imgui)
    imgui.EndPopup(activeCtx)
end

local function main()
    if not host.IsContextValid(ctx) then
        host.SetToolbarState(-1)
        return
    end
    if not data.PollData() then return end
    if #data.surfaces == 0 then
        r.defer(main)
        return
    end

    EnsureSurfaceWindows()
    requestCloseAll = false

    for _, surfName in ipairs(data.surfaces) do
        local window = surfaceWindows[surfName]
        if window and window.open then
            local position = data.LoadSurfacePosition(surfName)
            if position and not window.positionApplied then
                imgui.SetNextWindowPos(ctx, position.x, position.y, imgui.Cond_Appearing)
                window.positionApplied = true
            end

            imgui.PushStyleColor(ctx, imgui.Col_WindowBg, data.COLORS.win_bg)
            imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, data.COLORS.win_bg)
            imgui.SetNextWindowBgAlpha(ctx, data.vars.transparency)
            local visible, open = imgui.Begin(ctx, "CSI OSK - " .. surfName, true, GetWindowFlags())
            imgui.PopStyleColor(ctx, 2)

            local x, y = imgui.GetWindowPos(ctx)
            local oldPosition = data.surfacePos[surfName]
            if not oldPosition or math.abs(oldPosition.x - x) > 0.5 or math.abs(oldPosition.y - y) > 0.5 then
                data.SetSurfacePosition(surfName, x, y)
            end

            if visible then
                imgui.PushFont(ctx, FONT)
                if not config.ShouldSuppressContextMenu or not config.ShouldSuppressContextMenu() then
                    RenderContextMenu(ctx, "OSK_ContextMenu_" .. surfName)
                end
                if osd_ui.vars.osk_bar_position == "top" then render.RenderOSDBar(ctx) end
                render.RenderSurface(ctx, surfName)
                if osd_ui.vars.osk_bar_position == "bottom" then render.RenderOSDBar(ctx) end
                imgui.PopFont(ctx)
            end
            imgui.End(ctx)
            window.open = open
        end
    end

    config.RenderConfigEditor(ctx)
    data.FlushSurfacePositions(false)

    if requestCloseAll then
        for _, window in pairs(surfaceWindows) do
            window.open = false
        end
    end

    if AnyWindowOpen() then
        r.defer(main)
        return
    end

    data.FlushSurfacePositions(true)
    data.SaveSettings()
    host.SetToolbarState(-1)
end

local function Init()
    host.SetToolbarState(1)
    data.LoadSettings()
    osd_ui.LoadSettings()

    local fonts
    ctx, fonts = host.CreateContext("CSI OSK", {
        { key = "default", family = "sans-serif", size = 13 },
        { key = "small", family = "sans-serif", size = 11 },
    })
    FONT = fonts.default
    FONT_SMALL = fonts.small
    render.SetFonts(FONT, FONT_SMALL)
    if render.SetConfigModule then render.SetConfigModule(config) end

    host.OnExit(function()
        if config.HandleShutdown then config.HandleShutdown() end
        data.FlushSurfacePositions(true)
        data.SaveSettings()
        osd_ui.SaveSettings()
        host.SetToolbarState(-1)
    end)

    data.PollData()
    EnsureSurfaceWindows()
    main()
end

Init()
