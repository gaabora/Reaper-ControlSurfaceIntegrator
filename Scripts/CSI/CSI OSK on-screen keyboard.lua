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
local fontCache = {}

local FONT_FAMILIES = {
    { label = "Sans", family = "sans-serif" },
    { label = "Serif", family = "serif" },
    { label = "Mono", family = "monospace" },
}

local LABEL_CASES = {
    { label = "Original", value = "original" },
    { label = "Title", value = "title" },
    { label = "Sentence", value = "sentence" },
    { label = "UPPER", value = "upper" },
    { label = "lower", value = "lower" },
}

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

local function GetFontFamilyIndex()
    for index, fontInfo in ipairs(FONT_FAMILIES) do
        if fontInfo.family == data.vars.font_family then return index - 1 end
    end
    return 0
end

local function GetFontFamilyItems()
    local labels = {}
    for _, fontInfo in ipairs(FONT_FAMILIES) do
        labels[#labels + 1] = fontInfo.label
    end
    return table.concat(labels, "\0") .. "\0"
end

local function GetLabelCaseIndex()
    for index, caseInfo in ipairs(LABEL_CASES) do
        if caseInfo.value == data.vars.label_case then return index - 1 end
    end
    return 0
end

local function GetLabelCaseItems()
    local labels = {}
    for _, caseInfo in ipairs(LABEL_CASES) do
        labels[#labels + 1] = caseInfo.label
    end
    return table.concat(labels, "\0") .. "\0"
end

local function CreateAttachedFont(family, size)
    local cacheKey = table.concat({ family, tostring(size) }, "|")
    if fontCache[cacheKey] then return fontCache[cacheKey] end
    local ok, font = pcall(imgui.CreateFont, family, size)
    if ok and font then
        imgui.Attach(ctx, font)
        fontCache[cacheKey] = font
        return font
    end
    return nil
end

local function RebuildFonts()
    if not ctx then return end
    local fontSize = math.max(8, math.min(32, tonumber(data.vars.font_size) or 13))
    local family = data.vars.font_family or "sans-serif"
    FONT = CreateAttachedFont(family, fontSize) or FONT
    FONT_SMALL = CreateAttachedFont(family, math.max(8, fontSize - 2)) or FONT_SMALL
    render.SetFonts(FONT, FONT_SMALL)
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
    SliderSetting(activeCtx, "Zoom", data.vars.zoom, "zoom", 0.5, 3.0, "%.1f")

    local changed
    changed, data.vars.font_size = imgui.SliderDouble(activeCtx, "Font size", data.vars.font_size, 8, 32, "%.0f px")
    if changed then
        data.vars.font_size = math.floor(data.vars.font_size + 0.5)
        data.SaveSettings()
        RebuildFonts()
    end
    local familyIndex = GetFontFamilyIndex()
    changed, familyIndex = imgui.Combo(activeCtx, "Font", familyIndex, GetFontFamilyItems())
    if changed then
        data.vars.font_family = FONT_FAMILIES[familyIndex + 1] and FONT_FAMILIES[familyIndex + 1].family or "sans-serif"
        data.SaveSettings()
        RebuildFonts()
    end
    SliderSetting(activeCtx, "Line height", data.vars.line_height, "line_height", 0.45, 1.25, "%.2f")
    local labelCaseIndex = GetLabelCaseIndex()
    changed, labelCaseIndex = imgui.Combo(activeCtx, "Label case", labelCaseIndex, GetLabelCaseItems())
    if changed then
        data.vars.label_case = LABEL_CASES[labelCaseIndex + 1] and LABEL_CASES[labelCaseIndex + 1].value or "original"
        data.processedLabelCache = {}
        data.SaveSettings()
    end

    SliderSetting(activeCtx, "Aspect (W/H)", data.vars.aspect, "aspect", 0.5, 2.0, "%.2f")
    SliderSetting(activeCtx, "H Padding", data.vars.pad_h, "pad_h", 0, 20, "%.0f")
    SliderSetting(activeCtx, "V Padding", data.vars.pad_v, "pad_v", 0, 20, "%.0f")
    SliderSetting(activeCtx, "Arrow Angle", data.vars.arrow_angle, "arrow_angle", 60, 150, "%.0f")
    SliderSetting(activeCtx, "Window Alpha", data.vars.transparency, "transparency", 0.2, 1.0, "%.2f")
    SliderSetting(activeCtx, "Button Alpha", data.vars.btn_transparency, "btn_transparency", 0.2, 1.0, "%.2f")
    SliderSetting(activeCtx, "Tooltip Delay", data.vars.tooltip_delay, "tooltip_delay", 0.0, 5.0, "%.1fs")

    changed, data.vars.interactive = imgui.Checkbox(activeCtx, "Interactive controls", data.vars.interactive)
    if changed then data.SaveSettings() end
    imgui.SameLine(activeCtx)
    changed, data.vars.titlebar_enabled = imgui.Checkbox(activeCtx, "Show titlebar", data.vars.titlebar_enabled)
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
    RebuildFonts()
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
