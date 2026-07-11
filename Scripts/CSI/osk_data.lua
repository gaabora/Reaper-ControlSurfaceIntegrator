local r = reaper
local layout_parser = require("layout_parser")
local label_replacements = require("label_replacements")

local M = {}

M.EXT_SECTION = "ReaCtrlSurf_OSK"
M.EXT_CMD_SECTION = "ReaCtrlSurf_OSK_CMD"
M.EXT_SETTINGS = "ReaCtrlSurf_OSK_SETTINGS"

local SURFACE_POSITION_PREFIX = "SurfacePosition_"
local POSITION_SAVE_DELAY_SECONDS = 0.25

M.COLORS = {
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

M.surfaces = {}
M.layouts = {}
M.states = {}
M.labels = {}
M.labelMaps = {}
M.rawLayouts = {}
M.rawStates = {}
M.rawLabels = {}
M.rawLabelMaps = {}
M.processedLabelCache = {}
M.surfacePos = {}
local dirtySurfacePositions = {}
local surfacePositionSaveDue = 0

M.BUILTIN_LABEL_REPLACEMENTS = label_replacements.BUILTIN_REPLACEMENTS
M.USER_LABEL_REPLACEMENTS = {}
M.LABEL_REPLACEMENTS = {}
M.labelReplacementRules = {}

M.vars = {
    zoom = 0.9, interactive = true, aspect = 1.4,
    pad_h = 6, pad_v = 6,
    transparency = 0.6,
    btn_transparency = 0.9,
    tooltip_delay = 1.0,
    arrow_angle = 120,
    titlebar_enabled = true,
    label_replacements = "",
}

local function clearTable(tbl)
    for key in pairs(tbl) do
        tbl[key] = nil
    end
end

local function rebuildLabelReplacementRules()
    local merged, orderedRules = label_replacements.BuildRuleSet(M.BUILTIN_LABEL_REPLACEMENTS, M.USER_LABEL_REPLACEMENTS)
    clearTable(M.LABEL_REPLACEMENTS)
    clearTable(M.labelReplacementRules)
    for word, replacement in pairs(merged) do
        M.LABEL_REPLACEMENTS[word] = replacement
    end
    for _, rule in ipairs(orderedRules) do
        M.labelReplacementRules[#M.labelReplacementRules + 1] = rule
    end
end

local function replaceArray(dst, src)
    clearTable(dst)
    for index, value in ipairs(src) do
        dst[index] = value
    end
end

function M.hexToImCol(hex)
    if not hex then return M.COLORS.button_off end
    if hex:sub(1, 1) == "#" then hex = hex:sub(2) end
    if #hex < 6 then return M.COLORS.button_off end
    local red = tonumber(hex:sub(1, 2), 16) or 0
    local green = tonumber(hex:sub(3, 4), 16) or 0
    local blue = tonumber(hex:sub(5, 6), 16) or 0
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.dimColor(col, factor)
    local red = math.floor(((col >> 24) & 0xFF) * factor)
    local green = math.floor(((col >> 16) & 0xFF) * factor)
    local blue = math.floor(((col >> 8) & 0xFF) * factor)
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.brightenColor(col, amount)
    local red = math.min(255, ((col >> 24) & 0xFF) + amount)
    local green = math.min(255, ((col >> 16) & 0xFF) + amount)
    local blue = math.min(255, ((col >> 8) & 0xFF) + amount)
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
end

function M.ensureMinLuminance(col, minLum)
    minLum = minLum or 80
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

function M.applyAlpha(col, alpha)
    return (col & 0xFFFFFF00) | math.floor(alpha * 255)
end

function M.stripLabelPrefix(text)
    return label_replacements.StripLabelPrefix(text)
end

function M.splitPascalCase(text)
    return label_replacements.SplitPascalCase(text)
end

function M.GetLabelReplacementHelp()
    return label_replacements.GetHelpText()
end

function M.GetBuiltInLabelReplacementText()
    return label_replacements.Serialize(M.BUILTIN_LABEL_REPLACEMENTS)
end

function M.GetActiveLabelReplacementText()
    return label_replacements.Serialize(M.LABEL_REPLACEMENTS)
end

function M.applyLabelReplacements(text)
    return label_replacements.Apply(text, M.labelReplacementRules)
end

function M.processLabel(text)
    return label_replacements.ProcessLabel(text, M.labelReplacementRules)
end

function M.getProcessedLabel(text)
    if not text or text == "" then return "" end
    local cached = M.processedLabelCache[text]
    if cached then return cached end
    cached = M.processLabel(text)
    M.processedLabelCache[text] = cached
    return cached
end

function M.parseLabelReplacements(str)
    clearTable(M.USER_LABEL_REPLACEMENTS)
    clearTable(M.processedLabelCache)
    local parsed = label_replacements.ParseReplacementText(str)
    for key, value in pairs(parsed) do
        M.USER_LABEL_REPLACEMENTS[key] = value
    end
    rebuildLabelReplacementRules()
end

rebuildLabelReplacementRules()

function M.wrapText(ctx, text, maxW, imgui)
    local lines = {}
    local words = {}
    for word in text:gmatch("%S+") do
        words[#words + 1] = word
    end
    if #words == 0 then return { text } end

    local line = words[1]
    for index = 2, #words do
        local test = line .. " " .. words[index]
        local textWidth = imgui.CalcTextSize(ctx, test)
        if textWidth > maxW and line ~= "" then
            lines[#lines + 1] = line
            line = words[index]
        else
            line = test
        end
    end

    lines[#lines + 1] = line
    return lines
end

function M.PollExtStateEntry(surfName, suffix, rawStore, parsedStore, parser)
    local key = suffix .. "_" .. surfName
    local raw = r.GetExtState(M.EXT_SECTION, key)
    if raw and raw ~= rawStore[surfName] then
        rawStore[surfName] = raw
        parsedStore[surfName] = parser(raw)
    end
end

function M.FilterGroupedDuplicates(row)
    return layout_parser.FilterGroupedDuplicates(row)
end

function M.ParseLayout(layoutStr)
    return layout_parser.ParseLayout(layoutStr)
end

function M.GetCellInfo(surfName, widgetName)
    for _, row in ipairs(M.layouts[surfName] or {}) do
        for _, cell in ipairs(row) do
            if not cell.isSpacer and cell.name == widgetName then return cell end
        end
    end
    return nil
end

function M.IsRelativeWidget(surfName, widgetName)
    local cell = M.GetCellInfo(surfName, widgetName)
    if not cell then return false end
    local name = tostring(cell.name or ""):lower()
    local group = tostring(cell.group or ""):lower()
    local isButtonLike = name:find("push", 1, true) ~= nil
        or name:find("press", 1, true) ~= nil
        or name:find("touch", 1, true) ~= nil
        or name:find("button", 1, true) ~= nil
    if isButtonLike then return false end
    return name:find("rotary", 1, true) ~= nil
        or name:find("encoder", 1, true) ~= nil
        or group:find("rotary", 1, true) ~= nil
        or group:find("encoder", 1, true) ~= nil
end

function M.ParseKeyValueList(str, entryParser)
    local result = {}
    if not str or str == "" then return result end
    for entry in str:gmatch("[^;]+") do
        local key, value = entry:match("^(.-)=(.*)$")
        if key then
            entryParser(result, key, value)
        end
    end
    return result
end

function M.ParseState(stateStr)
    return M.ParseKeyValueList(stateStr, function(result, name, rest)
        local value = tonumber(rest:match("V:([%d%.%-]+)")) or 0
        local colorHex = rest:match("C:(#%x+)") or "#333333"
        result[name] = {
            value = value,
            color = M.hexToImCol(colorHex),
        }
    end)
end

function M.ParseLabels(labelsStr)
    return M.ParseKeyValueList(labelsStr, function(result, name, label)
        result[name] = label
    end)
end

function M.ParseLabelMap(str)
    return M.ParseKeyValueList(str, function(result, name, modPairs)
        if not modPairs or modPairs == "" then return end
        local mods = {}
        for modEntry in modPairs:gmatch("[^|]+") do
            local modName, label = modEntry:match("^([^:]+):(.+)$")
            if modName and label then
                mods[modName] = label
            end
        end
        result[name] = mods
    end)
end

function M.LoadSettings()
    for key, value in pairs(M.vars) do
        if r.HasExtState(M.EXT_SETTINGS, key) then
            local extValue = r.GetExtState(M.EXT_SETTINGS, key)
            if type(value) == "number" then
                M.vars[key] = tonumber(extValue) or value
            elseif type(value) == "boolean" then
                M.vars[key] = extValue == "true"
            else
                M.vars[key] = extValue
            end
        end
    end
    M.parseLabelReplacements(M.vars.label_replacements)
end

function M.SaveSettings()
    for key, value in pairs(M.vars) do
        r.SetExtState(M.EXT_SETTINGS, key, tostring(value), true)
    end
end

function M.LoadSurfacePosition(surfName)
    if M.surfacePos[surfName] then return M.surfacePos[surfName] end
    local rawPosition = r.GetExtState(M.EXT_SETTINGS, SURFACE_POSITION_PREFIX .. surfName)
    local xText, yText = rawPosition:match("^([%-%d%.]+),([%-%d%.]+)$")
    local x = tonumber(xText)
    local y = tonumber(yText)
    if x and y then
        M.surfacePos[surfName] = { x = x, y = y }
    end
    return M.surfacePos[surfName]
end

function M.SetSurfacePosition(surfName, x, y)
    M.surfacePos[surfName] = { x = x, y = y }
    dirtySurfacePositions[surfName] = true
    surfacePositionSaveDue = r.time_precise() + POSITION_SAVE_DELAY_SECONDS
end

function M.FlushSurfacePositions(force)
    if not force and (surfacePositionSaveDue == 0 or r.time_precise() < surfacePositionSaveDue) then return end
    for surfName in pairs(dirtySurfacePositions) do
        local position = M.surfacePos[surfName]
        if position then
            local serialized = string.format("%.3f,%.3f", position.x, position.y)
            r.SetExtState(M.EXT_SETTINGS, SURFACE_POSITION_PREFIX .. surfName, serialized, true)
        end
        dirtySurfacePositions[surfName] = nil
    end
    surfacePositionSaveDue = 0
end

function M.PollData()
    if r.HasExtState(M.EXT_SECTION, "Command") then
        local cmd = r.GetExtState(M.EXT_SECTION, "Command")
        if cmd == "Close" then
            r.DeleteExtState(M.EXT_SECTION, "Command", false)
            return false
        end
        r.DeleteExtState(M.EXT_SECTION, "Command", false)
    end

    local surfStr = r.GetExtState(M.EXT_SECTION, "Surfaces")
    if surfStr and surfStr ~= "" then
        local newSurfaces = {}
        for name in surfStr:gmatch("[^|]+") do
            newSurfaces[#newSurfaces + 1] = name
        end
        if #newSurfaces > 0 then
            replaceArray(M.surfaces, newSurfaces)
            for _, surfName in ipairs(M.surfaces) do
                M.LoadSurfacePosition(surfName)
            end
        end
    end

    for _, surfName in ipairs(M.surfaces) do
        M.PollExtStateEntry(surfName, "Layout", M.rawLayouts, M.layouts, function(raw)
            if raw and raw ~= "" then return M.ParseLayout(raw) end
            return M.layouts[surfName]
        end)
        M.PollExtStateEntry(surfName, "State", M.rawStates, M.states, M.ParseState)
        M.PollExtStateEntry(surfName, "Labels", M.rawLabels, M.labels, M.ParseLabels)
        M.PollExtStateEntry(surfName, "LabelMap", M.rawLabelMaps, M.labelMaps, M.ParseLabelMap)
    end

    return true
end

return M
