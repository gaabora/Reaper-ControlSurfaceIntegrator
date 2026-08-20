--[[
 * ReaScript Name: OSK on-screen keyboard
 * About: On-screen keyboard display for linked control surfaces.
 *        Shows one interactive window per surface, with labels reflecting
 *        current zone and modifier bindings.
 * Author: Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 1.0.0
--]]

-- local VSDEBUG = dofile("t:/apps/DevTools/VSCode/data/extensions/antoinebalaine.reascript-docs-0.1.16/debugger/LoadDebug.lua") -- TODO: move to some common script where it can be adjusted centralized so it can be used for all scripts

local r = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local host = dofile(scriptDir .. "script_host.lua")
local imgui = host.RequireImGui(scriptDir)
if not imgui then return end

local identity = require("product_identity")
local data = require("osk_data")
local font_cache = require("font_cache")
local log_writer = require("log_writer")
local render = require("osk_render")
local config = require("osk_config")
local osd_ui = require("osd_ui")
local osk_settings_ui = require("osk_settings_ui")
local control_panel_protocol = require("control_panel_protocol")
local osk_zone_create = require("osk_zone_create")
local theme = require("theme_settings")

local ctx = nil
local FONT = nil
local FONT_SMALL = nil
local CONFIG_FONT = nil
local surfaceWindows = {}
local fontCache = nil
local lastAppearanceChangeToken = ""

local WINDOW_FLAGS_BASE = imgui.WindowFlags_NoScrollbar
    | imgui.WindowFlags_NoCollapse
    | imgui.WindowFlags_AlwaysAutoResize

local function GetWindowFlags()
    local flags = WINDOW_FLAGS_BASE
    if not theme.osk.titlebar_enabled then
        flags = flags | imgui.WindowFlags_NoTitleBar
    end
    return flags
end

local function RebuildFonts()
    if not ctx or not fontCache then return end
    local fontSize = theme.osk.font_size
    local family = theme.osk.font_family or "sans-serif"
    FONT = fontCache:Get(family, fontSize) or FONT
    FONT_SMALL = fontCache:Get(family, math.max(8, fontSize - theme.WIDGET.label_small_font_delta)) or FONT_SMALL
    render.SetFonts(FONT, FONT_SMALL)
    osd_ui.SetFont(FONT_SMALL)
end

local function ReloadAppearanceIfNeeded()
    local changeToken = theme.GetAppearanceChangeToken()
    if changeToken == lastAppearanceChangeToken then return end
    theme.LoadCurrentAppearance()
    theme.ClearInactiveLedBoostCache()
    data.processedLabelCache = {}
    RebuildFonts()
    osd_ui.RefreshAppearance()
    lastAppearanceChangeToken = changeToken
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

local function RenderContextMenu(activeCtx, popupId, surfName)
    osk_settings_ui.RenderContextMenu(activeCtx, popupId, surfName, {
        data = data,
        osd_ui = osd_ui,
        onCreateZone = osk_zone_create.Open,
        onOpenInputSettings = function(surfaceName)
            local opened, openError = control_panel_protocol.Open("General", { surface = surfaceName, page = data.pageName })
            if not opened then log_writer.Write("ERROR", "[" .. identity.displayName .. " OSK] " .. tostring(openError)) end
        end,
        onFontsChanged = RebuildFonts,
    })
end

local function main()
    if not host.IsContextValid(ctx) then
        host.SetToolbarState(-1)
        return
    end
    ReloadAppearanceIfNeeded()
    if not data.PollData() then return end
    if #data.surfaces == 0 then
        r.defer(main)
        return
    end

    osd_ui.PollOSD()

    EnsureSurfaceWindows()
    for _, surfName in ipairs(data.surfaces) do
        local window = surfaceWindows[surfName]
        if window and window.open then
            local position = data.LoadSurfacePosition(surfName)
            if position and not window.positionApplied then
                imgui.SetNextWindowPos(ctx, position.x, position.y, imgui.Cond_Appearing)
                window.positionApplied = true
            end

            imgui.PushStyleColor(ctx, imgui.Col_WindowBg, theme.OSK_COLORS.win_bg)
            imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, theme.OSK_COLORS.win_bg)
            imgui.SetNextWindowBgAlpha(ctx, theme.osk.transparency)
            local visible, open = imgui.Begin(ctx, identity.displayName .. " OSK - " .. surfName, true, GetWindowFlags())
            imgui.PopStyleColor(ctx, 2)

            local x, y = imgui.GetWindowPos(ctx)
            local oldPosition = data.surfacePos[surfName]
            if not oldPosition or math.abs(oldPosition.x - x) > 0.5 or math.abs(oldPosition.y - y) > 0.5 then
                data.SetSurfacePosition(surfName, x, y)
            end

            if visible then
                imgui.PushFont(ctx, FONT)
                if not config.ShouldSuppressContextMenu or not config.ShouldSuppressContextMenu() then
                    RenderContextMenu(ctx, "OSK_ContextMenu_" .. surfName, surfName)
                end
                if osd_ui.GetOSKBarPosition(surfName) == "top" then render.RenderOSDBar(ctx, surfName) end
                render.RenderSurface(ctx, surfName)
                if osd_ui.GetOSKBarPosition(surfName) == "bottom" then render.RenderOSDBar(ctx, surfName) end
                imgui.PopFont(ctx)
            end
            imgui.End(ctx)
            if window.open and not open then
                data.SetSurfaceEnabled(surfName, false)
            end
            window.open = open
        end
    end

    config.RenderConfigEditor(ctx, CONFIG_FONT)
    osk_zone_create.Render(ctx, CONFIG_FONT)
    data.FlushSurfacePositions(false)

    if AnyWindowOpen() or osk_zone_create.IsOpen() then
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
    theme.LoadCurrentAppearance()
    osd_ui.RefreshAppearance()
    lastAppearanceChangeToken = theme.GetAppearanceChangeToken()

    ctx = host.CreateContext(identity.displayName .. " OSK")
    fontCache = font_cache.New(imgui, ctx)
    osd_ui.SetFontCache(fontCache)
    local fonts = fontCache:Build(theme.DEFAULT_FONT_DEFINITIONS)
    FONT = fonts.default
    FONT_SMALL = fonts.small
    CONFIG_FONT = fonts.default
    RebuildFonts()
    if render.SetConfigModule then render.SetConfigModule(config) end

    host.OnExit(function()
        if config.HandleShutdown then config.HandleShutdown() end
        data.FlushSurfacePositions(true)
        data.SaveSettings()
        host.SetToolbarState(-1)
    end)

    data.PollData()
    EnsureSurfaceWindows()
    main()
end

Init()
