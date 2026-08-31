local identity = require("product_identity")
local settings_store = require("settings_store")
local reaperApi = reaper

local M = {}

M.OSK_SETTINGS_SECTION = identity.extState.oskSettings
M.OSD_SETTINGS_SECTION = identity.extState.osdSettings
M.COMMON_SETTINGS_SECTION = identity.extState.appearanceSettings
M.NOTIFICATIONS_SETTINGS_SECTION = identity.extState.notificationsSettings

M.FONT_FAMILIES = {
    { label = "Sans", family = "sans-serif", value = "sans-serif" },
    { label = "Serif", family = "serif", value = "serif" },
    { label = "Mono", family = "monospace", value = "monospace" },
}

M.LABEL_CASES = {
    { label = "Original", value = "original" },
    { label = "Title", value = "title" },
    { label = "Sentence", value = "sentence" },
    { label = "UPPER", value = "upper" },
    { label = "lower", value = "lower" },
}

M.DEFAULT_FONT_DEFINITIONS = {
    { key = "default", family = "sans-serif", size = 13 },
    { key = "small", family = "sans-serif", size = 11 },
}

M.DIRTY_BUTTON_COLORS = {
    button = 0x8f2424ff,
    hovered = 0xb83232ff,
    active = 0x701b1bff,
}

M.OSK_COLORS = {
    win_bg = 0x1e1e1eff,
    button_off = 0x3a3a3aff,
    button_on = 0xffb029ff,
    button_hover = 0x4a6a9aff,
    text_normal = 0x000000ff,
    text_dim = 0x444444ff,
    round_off = 0x444444ff,
    round_on_play = 0x40a040ff,
    round_on_stop = 0x808080ff,
    round_on_rec = 0xcc3030ff,
    arrow_off = 0x505050ff,
    arrow_on = 0x70b070ff,
}

M.CONFIG = {
    color_palette = {
        { name = "Black", value = 0x000000ff },
        { name = "White", value = 0xffffffff },
        { name = "Gray", value = 0x808080ff },
        { name = "Red", value = 0xff3030ff },
        { name = "Orange", value = 0xff8a20ff },
        { name = "Yellow", value = 0xffd830ff },
        { name = "Green", value = 0x40c060ff },
        { name = "Cyan", value = 0x30c8d8ff },
        { name = "Blue", value = 0x4080ffff },
        { name = "Purple", value = 0xa060e0ff },
    },
    default_inactive_color = 0x3a3a3aff,
    default_active_color = 0xffb029ff,
    geometry_epsilon = 0.5,
    toolbar_binding_actions_width = 210,
    color_column_width = 52,
    bindings_table_height = 155,
    action_search_mode_width = 82,
    action_search_results_height = 105,
    action_search_clear_spacing = 4,
    action_search_spacing = 5,
    run_count_width = 120,
    hold_input_width = 145,
    control_spacing = 5,
    modifier_columns = 5,
    palette_columns = 5,
    color_picker_width = 236,
    color_preview_width = 90,
    color_preview_height = 50,
    color_swatch_size = 20,
    color_recent_columns = 10,
    color_saved_columns = 10,
    color_builtin_columns = 10,
    default_window_width = 720,
    default_window_height = 620,
}

M.WIDGET = {
    base_button_size = 64,
    color_meaningful_threshold = 10,
    min_button_luminance = 80,
    min_fader_luminance = 70,
    label_small_font_delta = 2,
    tooltip_wrap_width_factor = 42,
    rect_corner_radius = 4,
    rect_text_padding = 8,
    round_hit_inset = 2,
    round_path_padding = 2,
    round_path_segments = 18,
    round_text_padding = 12,
    rotary_label_height = 16,
    rotary_value_top_padding_min = 4,
    rotary_value_top_padding_ratio = 0.12,
    rotary_min_visual_height = 8,
    rotary_body_dim_factor = 0.55,
    rotary_inner_dim_factor = 0.45,
    rotary_track_dim_factor = 0.30,
    rotary_active_brighten = 35,
    rotary_outer_track_offset = 3,
    rotary_track_thickness = 6,
    rotary_track_start_angle = 135,
    rotary_track_end_angle = 405,
    rotary_value_dot_min_radius = 3,
    rotary_value_dot_ratio = 0.12,
    rotary_value_dot_distance = 0.66,
    rotary_inner_circle_ratio = 0.68,
    rotary_circle_segments = 32,
    rotary_inner_circle_segments = 28,
    rotary_dot_segments = 12,
    text_on_dark = 0xffffffff,
    arc_segments = 48,
    arc_fallback_segments = 28,
    arc_min_delta = 0.001,
    triangle_epsilon = 1e-9,
    fader_padding = 8,
    fader_label_height = 16,
    fader_track_left_ratio = 0.35,
    fader_track_right_ratio = 0.65,
    fader_track_bg = 0x262626ff,
    fader_fill = 0xd8d8d8ff,
    fader_knob = 0xddddddff,
    fader_track_rounding = 3,
    fader_knob_height = 8,
    fader_knob_width_pad = 4,
    fader_knob_rounding = 2,
    fader_label_center_y_offset = 9,
    fader_hit_min_radius = 4,
    arrow_depth_ratio = 0.45,
    osd_bar_padding = 8,
}

M.OSD = {
    fallback_screen_width = 1920,
    fallback_screen_height = 1080,
    popup_button_width = 50,
    timeout_ms = 3000,
    keep_open_seconds = 0.25,
    window_border_size = 0,
    center_luminance_cutoff = 186,
}

M.NOTIFICATIONS = {
    close_button_size = 20,
}

M.FORM = {
    control_width = 180,
}

M.common = {
    item_spacing = 8,
    rounding = 4,
    disabled_alpha = 0.6,
}
M.notifications = {}
M.osk = {}
M.osd = {}
local inactiveLedBoostCache = {}

M.OSK_SCHEMA = {
    zoom = { type = "number", default = 0.9, min = 0.5, max = 3.0, label = "Zoom", format = "%.1f" },
    font_size = { type = "number", default = 13, min = 8, max = 32, integer = true, label = "Font size" },
    font_family = { type = "string", default = "sans-serif", enum = { "sans-serif", "serif", "monospace" }, enumItems = M.FONT_FAMILIES, label = "Font" },
    line_height = { type = "number", default = 0.6, min = 0.45, max = 1.25, label = "Line height", format = "%.2f" },
    label_case = { type = "string", default = "original", enum = { "original", "title", "sentence", "upper", "lower" }, enumItems = M.LABEL_CASES, label = "Label case" },
    aspect = { type = "number", default = 1.4, min = 0.5, max = 2.0, label = "Button aspect", format = "%.2f" },
    pad_h = { type = "number", default = 6, min = 0, max = 20, integer = true, label = "Horizontal padding" },
    pad_v = { type = "number", default = 6, min = 0, max = 20, integer = true, label = "Vertical padding" },
    transparency = { type = "number", default = 0.6, min = 0.2, max = 1.0, label = "Window opacity", format = "%.2f" },
    btn_transparency = { type = "number", default = 0.9, min = 0.2, max = 1.0, label = "Button opacity", format = "%.2f" },
    inactive_led_boost = { type = "number", default = 50, min = 0, max = 100, integer = true, label = "Inactive OSK button brightness boost" }, -- Adds brightness to inactive buttons on OSK and config table swatches so color looks closer to real LED. Does not change saved colors or device feedback.
    arrow_angle = { type = "number", default = 120, min = 60, max = 150, integer = true, label = "Arrow angle" },
    titlebar_enabled = { type = "boolean", default = true, label = "Show title bar" },
    allow_docking = { type = "boolean", default = false, label = "Allow docking" },
    interactive_controls = { type = "boolean", default = true, label = "Interactive controls" },
    invert_scroll = { type = "boolean", default = false, label = "Invert scroll" },
    tooltip_delay = { type = "number", default = 1.0, min = 0.0, max = 5.0, label = "Tooltip delay", format = "%.1f s" },
    label_replacements = { type = "string", default = "", label = "Label replacements", multiline = true },
}

M.OSD_SCHEMA = {
    osd_position = { type = "string", default = "top", enum = { "top", "bottom" }, enumItems = { { label = "Top", value = "top" }, { label = "Bottom", value = "bottom" } }, label = "Position" },
    osd_alignment = { type = "string", default = "center", enum = { "left", "center", "right" }, enumItems = { { label = "Left", value = "left" }, { label = "Center", value = "center" }, { label = "Right", value = "right" } }, label = "Alignment" },
    osk_bar_position = { type = "string", default = "off", enum = { "off", "top", "bottom" }, enumItems = { { label = "Off", value = "off" }, { label = "Top", value = "top" }, { label = "Bottom", value = "bottom" } }, label = "Default OSK bar position" },
    osd_width_percent = { type = "number", default = 50, min = 10, max = 100, step = 1, integer = true, label = "Width %" },
    osd_height_px = { type = "number", default = 100, min = 20, max = 400, step = 10, integer = true, label = "Height px" },
    osd_transparency = { type = "number", default = 30, min = 0, max = 100, step = 5, integer = true, label = "Opacity %" },
    osd_h_margin_px = { type = "number", default = 0, min = 0, max = 400, step = 10, integer = true, label = "Horizontal margin px" },
    osd_v_margin_px = { type = "number", default = 50, min = 0, max = 400, step = 10, integer = true, label = "Vertical margin px" },
    osd_font_px = { type = "number", default = 80, min = 8, max = 200, step = 1, integer = true, label = "Font size px" },
    osd_bg_on = { type = "color", default = "#7f7f7f", label = "Active background" },
    osd_bg_off = { type = "color", default = "#333333", label = "Inactive background" },
}

M.NOTIFICATIONS_SCHEMA = {
    opacity = { type = "number", default = 0.8, min = 0.2, max = 1.0, label = "Notification opacity", format = "%.2f" },
}

M.OSK_ORDER = { "zoom", "font_size", "font_family", "line_height", "label_case", "aspect", "pad_h", "pad_v", "transparency", "btn_transparency", "inactive_led_boost", "arrow_angle", "titlebar_enabled", "allow_docking", "interactive_controls", "invert_scroll", "tooltip_delay", "label_replacements" }
M.OSD_ORDER = { "osd_position", "osd_alignment", "osk_bar_position", "osd_width_percent", "osd_height_px", "osd_transparency", "osd_h_margin_px", "osd_v_margin_px", "osd_font_px", "osd_bg_on", "osd_bg_off" }
M.NOTIFICATIONS_ORDER = { "opacity" }

local APPEARANCE_PREVIEW_ACTIVE_KEY = "PreviewActive"
local APPEARANCE_PREVIEW_REVISION_KEY = "PreviewRevision"
local appearanceGroups = {
    { id = "OSK", schema = M.OSK_SCHEMA, target = M.osk },
    { id = "OSD", schema = M.OSD_SCHEMA, target = M.osd },
    { id = "Notifications", schema = M.NOTIFICATIONS_SCHEMA, target = M.notifications },
}

local function appearancePreviewKey(groupId, settingName)
    return "Preview." .. groupId .. "." .. settingName
end

function M.GetAppearanceRevision()
    return tonumber(reaperApi.GetExtState(M.COMMON_SETTINGS_SECTION, "Revision")) or 0
end

function M.GetAppearancePreviewRevision()
    return tonumber(reaperApi.GetExtState(M.COMMON_SETTINGS_SECTION, APPEARANCE_PREVIEW_REVISION_KEY)) or 0
end

function M.GetAppearanceChangeToken()
    return tostring(M.GetAppearanceRevision()) .. ":" .. tostring(M.GetAppearancePreviewRevision())
end

function M.NotifyAppearanceChanged()
    local revision = M.GetAppearanceRevision() + 1
    reaperApi.SetExtState(M.COMMON_SETTINGS_SECTION, "Revision", tostring(revision), false)
    return revision
end

local function notifyAppearancePreviewChanged()
    local revision = M.GetAppearancePreviewRevision() + 1
    reaperApi.SetExtState(M.COMMON_SETTINGS_SECTION, APPEARANCE_PREVIEW_REVISION_KEY, tostring(revision), false)
end

local function saveSettingsIfChanged(section, schema, target)
    local changed = false
    for settingName, rule in pairs(schema) do
        local rawValue = reaperApi.GetExtState(section, settingName)
        local persistedValue = settings_store.NormalizeValue(rawValue ~= "" and rawValue or nil, rule)
        if target[settingName] ~= persistedValue then
            changed = true
            break
        end
    end
    if not changed then return false end
    settings_store.Save(section, schema, target)
    M.NotifyAppearanceChanged()
    return true
end

function M.LoadNotificationSettings()
    settings_store.Load(M.NOTIFICATIONS_SETTINGS_SECTION, M.NOTIFICATIONS_SCHEMA, M.notifications)
    return M.notifications
end

function M.SaveNotificationSettings()
    return saveSettingsIfChanged(M.NOTIFICATIONS_SETTINGS_SECTION, M.NOTIFICATIONS_SCHEMA, M.notifications)
end

function M.LoadOskSettings()
    settings_store.Load(M.OSK_SETTINGS_SECTION, M.OSK_SCHEMA, M.osk)
    return M.osk
end

function M.SaveOskSettings()
    return saveSettingsIfChanged(M.OSK_SETTINGS_SECTION, M.OSK_SCHEMA, M.osk)
end

function M.LoadOsdSettings()
    settings_store.Load(M.OSD_SETTINGS_SECTION, M.OSD_SCHEMA, M.osd)
    return M.osd
end

function M.SaveOsdSettings()
    return saveSettingsIfChanged(M.OSD_SETTINGS_SECTION, M.OSD_SCHEMA, M.osd)
end

function M.PublishAppearancePreview()
    for groupIdx, group in ipairs(appearanceGroups) do
        for settingName in pairs(group.schema) do reaperApi.SetExtState(M.COMMON_SETTINGS_SECTION, appearancePreviewKey(group.id, settingName), tostring(group.target[settingName]), false) end
    end
    reaperApi.SetExtState(M.COMMON_SETTINGS_SECTION, APPEARANCE_PREVIEW_ACTIVE_KEY, "1", false)
    notifyAppearancePreviewChanged()
end

function M.ClearAppearancePreview()
    if reaperApi.GetExtState(M.COMMON_SETTINGS_SECTION, APPEARANCE_PREVIEW_ACTIVE_KEY) ~= "1" then return false end
    for groupIdx, group in ipairs(appearanceGroups) do
        for settingName in pairs(group.schema) do reaperApi.DeleteExtState(M.COMMON_SETTINGS_SECTION, appearancePreviewKey(group.id, settingName), false) end
    end
    reaperApi.DeleteExtState(M.COMMON_SETTINGS_SECTION, APPEARANCE_PREVIEW_ACTIVE_KEY, false)
    notifyAppearancePreviewChanged()
    return true
end

function M.ApplyAppearancePreview()
    if reaperApi.GetExtState(M.COMMON_SETTINGS_SECTION, APPEARANCE_PREVIEW_ACTIVE_KEY) ~= "1" then return false end
    for groupIdx, group in ipairs(appearanceGroups) do
        for settingName, rule in pairs(group.schema) do
            local rawValue = reaperApi.GetExtState(M.COMMON_SETTINGS_SECTION, appearancePreviewKey(group.id, settingName))
            if rawValue ~= "" then group.target[settingName] = settings_store.NormalizeValue(rawValue, rule) end
        end
    end
    return true
end

function M.LoadCurrentAppearance()
    M.LoadNotificationSettings()
    M.LoadOskSettings()
    M.LoadOsdSettings()
    M.ApplyAppearancePreview()
end

local function packColor(red, green, blue, alpha)
    red = settings_store.Clamp(math.floor((tonumber(red) or 0) + 0.5), 0, 255)
    green = settings_store.Clamp(math.floor((tonumber(green) or 0) + 0.5), 0, 255)
    blue = settings_store.Clamp(math.floor((tonumber(blue) or 0) + 0.5), 0, 255)
    alpha = settings_store.Clamp(math.floor((tonumber(alpha) or 255) + 0.5), 0, 255)
    return (red << 24) | (green << 16) | (blue << 8) | alpha
end

function M.ClearInactiveLedBoostCache()
    inactiveLedBoostCache = {}
end

function M.GetInactiveLedBoost()
    return settings_store.Clamp(math.floor((tonumber(M.osk.inactive_led_boost) or 50) + 0.5), 0, 100)
end

function M.AdjustColorValue(col, valueDelta)
    col = tonumber(col) or M.OSK_COLORS.button_off
    valueDelta = tonumber(valueDelta) or 0
    if valueDelta == 0 then return col end

    local alpha = col & 0xFF
    local red = (col >> 24) & 0xFF
    local green = (col >> 16) & 0xFF
    local blue = (col >> 8) & 0xFF
    local maxChannel = math.max(red, green, blue)
    if maxChannel <= 0 then return col end

    local adjustedMax = settings_store.Clamp(maxChannel + valueDelta * 2.55, 0, 255)
    local scale = adjustedMax / maxChannel
    return packColor(red * scale, green * scale, blue * scale, alpha)
end

function M.ApplyInactiveLedBoost(col, boostPercent)
    local boost = settings_store.Clamp(math.floor((tonumber(boostPercent) or M.GetInactiveLedBoost()) + 0.5), 0, 100)
    if boost <= 0 then return col end

    col = tonumber(col) or M.OSK_COLORS.button_off
    local cacheKey = tostring(col) .. ":" .. tostring(boost)
    local cached = inactiveLedBoostCache[cacheKey]
    if cached then return cached end

    local alpha = col & 0xFF
    local red = (col >> 24) & 0xFF
    local green = (col >> 16) & 0xFF
    local blue = (col >> 8) & 0xFF
    local maxChannel = math.max(red, green, blue)
    if maxChannel <= 0 then return col end

    local boostedMax = math.min(255, maxChannel + boost * 2.55)
    local scale = boostedMax / maxChannel
    local boostedColor = packColor(red * scale, green * scale, blue * scale, alpha)
    inactiveLedBoostCache[cacheKey] = boostedColor
    return boostedColor
end

function M.HexToImCol(hex, fallbackColor)
    if not hex then return fallbackColor or M.OSK_COLORS.button_off end
    if hex:sub(1, 1) == "#" then hex = hex:sub(2) end
    if #hex < 6 then return fallbackColor or M.OSK_COLORS.button_off end
    local red = tonumber(hex:sub(1, 2), 16) or 0
    local green = tonumber(hex:sub(3, 4), 16) or 0
    local blue = tonumber(hex:sub(5, 6), 16) or 0
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.ApplyAlpha(col, alpha)
    local normalizedAlpha = tonumber(alpha) or 1
    if normalizedAlpha > 1 then normalizedAlpha = normalizedAlpha / 100 end
    normalizedAlpha = settings_store.Clamp(normalizedAlpha, 0, 1)
    return (col & 0xFFFFFF00) | math.floor(normalizedAlpha * 255)
end

function M.DimColor(col, factor)
    local red = math.floor(((col >> 24) & 0xFF) * factor)
    local green = math.floor(((col >> 16) & 0xFF) * factor)
    local blue = math.floor(((col >> 8) & 0xFF) * factor)
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.BrightenColor(col, amount)
    local red = math.min(255, ((col >> 24) & 0xFF) + amount)
    local green = math.min(255, ((col >> 16) & 0xFF) + amount)
    local blue = math.min(255, ((col >> 8) & 0xFF) + amount)
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.EnsureMinLuminance(col, minLum)
    minLum = minLum or M.WIDGET.min_button_luminance
    local red = (col >> 24) & 0xFF
    local green = (col >> 16) & 0xFF
    local blue = (col >> 8) & 0xFF
    local luminance = 0.299 * red + 0.587 * green + 0.114 * blue
    if luminance < minLum then
        local add = minLum - luminance
        red = math.min(255, math.floor(red + add))
        green = math.min(255, math.floor(green + add))
        blue = math.min(255, math.floor(blue + add))
    end
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.IsMeaningfulColor(col)
    local threshold = M.WIDGET.color_meaningful_threshold
    return ((col >> 24) & 0xFF) >= threshold
        or ((col >> 16) & 0xFF) >= threshold
        or ((col >> 8) & 0xFF) >= threshold
end

function M.GetContrastTextColorFromCol(bgCol)
    local red = (bgCol >> 24) & 0xFF
    local green = (bgCol >> 16) & 0xFF
    local blue = (bgCol >> 8) & 0xFF
    local luminance = 0.299 * red + 0.587 * green + 0.114 * blue
    return luminance > M.OSD.center_luminance_cutoff and 0x000000ff or 0xFFFFFFff
end

function M.GetContrastTextColor(bgHex)
    return M.GetContrastTextColorFromCol(M.HexToImCol(bgHex, M.OSK_COLORS.button_off))
end

M.LoadCurrentAppearance()

return M
