local imgui = require "imgui" "0.9.3"

local theme = require("theme_settings")
local ui = require("ui_components")
local log_writer = require("log_writer")

local M = {}

local TOOLBAR_ACTIONS = {
    { action = 42348, title = "reset surfaces", tooltip = "Reset all MIDI control surface devices" },
    { action = 41175, title = "reset MIDI", tooltip = "Reset all MIDI devices" },
}

local function saveBehavior(data)
    data.SaveSettings()
end

local function saveTheme()
    theme.SaveOskSettings()
end

local function sliderSetting(ctx, label, currentValue, onChange, minValue, maxValue, format)
    local changed, value = imgui.SliderDouble(ctx, label, currentValue, minValue, maxValue, format)
    if changed and onChange then onChange(value) end
    return changed, value
end

function M.RenderContextMenu(ctx, popupId, surfName, deps)
    deps = deps or {}
    local data = deps.data
    local osd_ui = deps.osd_ui
    if not imgui.BeginPopupContextWindow(ctx, popupId) then return end

    for _, action in ipairs(TOOLBAR_ACTIONS) do
        if imgui.MenuItem(ctx, action.title) then
            reaper.Main_OnCommand(action.action, 0)
            log_writer.Write("NOTICE", "[" .. data.productDisplayName .. " OSK] Action: " .. action.action .. " (" .. action.tooltip .. ")")
        end
        ui.ItemTooltip(ctx, action.tooltip)
    end

    if imgui.MenuItem(ctx, "Create zone file...") and deps.onCreateZone then deps.onCreateZone(surfName) end
    ui.ItemTooltip(ctx, "Create one empty zone file in the current surface profile")
    if imgui.MenuItem(ctx, "Input settings...") and deps.onOpenInputSettings then deps.onOpenInputSettings(surfName) end
    ui.ItemTooltip(ctx, "Edit Product defaults or overrides for this configured Surface")

    imgui.Separator(ctx)
    sliderSetting(ctx, "Zoom", theme.osk.zoom, function(value)
        theme.osk.zoom = value
        saveTheme()
    end, 0.5, 3.0, "%.1f")

    local changed
    changed, theme.osk.font_size = imgui.SliderDouble(ctx, "Font size", theme.osk.font_size, 8, 32, "%.0f px")
    if changed then
        theme.osk.font_size = math.floor(theme.osk.font_size + 0.5)
        saveTheme()
        if deps.onFontsChanged then deps.onFontsChanged() end
    end

    changed, theme.osk.font_family = ui.ComboEnum(ctx, "Font", theme.osk.font_family, theme.FONT_FAMILIES)
    if changed then
        saveTheme()
        if deps.onFontsChanged then deps.onFontsChanged() end
    end

    sliderSetting(ctx, "Line height", theme.osk.line_height, function(value)
        theme.osk.line_height = value
        saveTheme()
    end, 0.45, 1.25, "%.2f")

    changed, theme.osk.label_case = ui.ComboEnum(ctx, "Label case", theme.osk.label_case, theme.LABEL_CASES)
    if changed then
        if data then data.processedLabelCache = {} end
        saveTheme()
    end

    sliderSetting(ctx, "Aspect (W/H)", theme.osk.aspect, function(value)
        theme.osk.aspect = value
        saveTheme()
    end, 0.5, 2.0, "%.2f")
    sliderSetting(ctx, "H Padding", theme.osk.pad_h, function(value)
        theme.osk.pad_h = math.floor(value + 0.5)
        saveTheme()
    end, 0, 20, "%.0f")
    sliderSetting(ctx, "V Padding", theme.osk.pad_v, function(value)
        theme.osk.pad_v = math.floor(value + 0.5)
        saveTheme()
    end, 0, 20, "%.0f")
    sliderSetting(ctx, "Arrow Angle", theme.osk.arrow_angle, function(value)
        theme.osk.arrow_angle = math.floor(value + 0.5)
        saveTheme()
    end, 60, 150, "%.0f")
    sliderSetting(ctx, "Window Alpha", theme.osk.transparency, function(value)
        theme.osk.transparency = value
        saveTheme()
    end, 0.2, 1.0, "%.2f")
    sliderSetting(ctx, "Button Alpha", theme.osk.btn_transparency, function(value)
        theme.osk.btn_transparency = value
        saveTheme()
    end, 0.2, 1.0, "%.2f")
    sliderSetting(ctx, "OSK RGB LED Boost", theme.osk.inactive_led_boost, function(value)
        theme.osk.inactive_led_boost = math.floor(value + 0.5)
        theme.ClearInactiveLedBoostCache()
        saveTheme()
    end, 0, 100, "%.0f")
    ui.ItemTooltip(ctx, "Adds brightness to inactive RGB buttons on OSK and config table swatches so color looks closer to real LED. Does not change saved colors or device feedback.")
    sliderSetting(ctx, "Tooltip Delay", data.vars.tooltip_delay, function(value)
        data.vars.tooltip_delay = value
        saveBehavior(data)
    end, 0.0, 5.0, "%.1fs")

    changed, data.vars.interactive = imgui.Checkbox(ctx, "Interactive controls", data.vars.interactive)
    if changed then saveBehavior(data) end
    imgui.SameLine(ctx)
    changed, data.vars.invert_scroll = imgui.Checkbox(ctx, "Invert scroll", data.vars.invert_scroll)
    if changed then saveBehavior(data) end
    imgui.SameLine(ctx)
    changed, theme.osk.titlebar_enabled = imgui.Checkbox(ctx, "Show titlebar", theme.osk.titlebar_enabled)
    if changed then saveTheme() end

    imgui.Separator(ctx)
    changed, data.vars.label_replacements = ui.LabelReplacementEditor(
        ctx,
        "Label replacements",
        data.vars.label_replacements,
        data.GetLabelReplacementHelp(),
        { inputId = "##replacements" }
    )
    if changed then
        data.parseLabelReplacements(data.vars.label_replacements)
        saveBehavior(data)
    end

    imgui.Separator(ctx)
    osd_ui.RenderOSKPositionToggle(ctx, imgui, surfName)
    imgui.EndPopup(ctx)
end

return M
