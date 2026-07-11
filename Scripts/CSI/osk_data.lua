local r = reaper
local layout_parser = require("layout_parser")
local label_replacements = require("label_replacements")
local settings_store = require("settings_store")
local theme = require("theme_settings")

local M = {}

M.EXT_SECTION = "ReaCtrlSurf_OSK"
M.EXT_CMD_SECTION = "ReaCtrlSurf_OSK_CMD"
M.EXT_SETTINGS = "ReaCtrlSurf_OSK_SETTINGS"

local SURFACE_POSITION_PREFIX = "SurfacePosition_"
local SURFACE_ENABLED_PREFIX = "SurfaceEnabled_"
local POSITION_SAVE_DELAY_SECONDS = 0.25

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
M.FADER_DEBUG = false
local dirtySurfacePositions = {}
local surfacePositionSaveDue = 0
local faderDebugLast = {}
local faderDebugLastMessage = {}
local faderLocalValues = {}

M.BUILTIN_LABEL_REPLACEMENTS = label_replacements.BUILTIN_REPLACEMENTS
M.USER_LABEL_REPLACEMENTS = {}
M.LABEL_REPLACEMENTS = {}
M.labelReplacementRules = {}

M.vars = {
    interactive = true,
    invert_scroll = false,
    tooltip_delay = 1.0,
    label_replacements = "",
}

local SETTINGS_SCHEMA = {
    interactive = { type = "boolean", default = true },
    invert_scroll = { type = "boolean", default = false },
    tooltip_delay = { type = "number", default = 1.0, min = 0.0, max = 5.0 },
    label_replacements = { type = "string", default = "" },
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

function M.DebugFader(surfName, widgetName, message, throttleSeconds, keySuffix)
    if not M.FADER_DEBUG then return end
    if not r or not r.ShowConsoleMsg then return end
    local now = r.time_precise and r.time_precise() or os.clock()
    local key = table.concat({ tostring(surfName or "?"), tostring(widgetName or "?"), tostring(keySuffix or message or "") }, "|")
    if faderDebugLastMessage[key] == message and throttleSeconds and throttleSeconds > 0 then return end
    if throttleSeconds and throttleSeconds > 0 then
        local last = faderDebugLast[key]
        if last and now - last < throttleSeconds then return end
    end
    faderDebugLast[key] = now
    faderDebugLastMessage[key] = message
    r.ShowConsoleMsg(string.format("[CSI OSK FADER DEBUG] %s|%s %s\n", tostring(surfName or "?"), tostring(widgetName or "?"), tostring(message or "")))
end

function M.GetStateValue(surfName, widgetName)
    local state = M.states[surfName] and M.states[surfName][widgetName]
    return state and state.value or 0.0
end

function M.HasStateValue(surfName, widgetName)
    local state = M.states[surfName] and M.states[surfName][widgetName]
    if not state then return false end
    return state.hasValue ~= false
end

function M.GetStateKind(surfName, widgetName)
    local state = M.states[surfName] and M.states[surfName][widgetName]
    return state and state.kind or ""
end

function M.SetFaderLocalValue(surfName, widgetName, displayValue, commandValue)
    local stateKey = tostring(surfName or "") .. "|" .. tostring(widgetName or "")
    faderLocalValues[stateKey] = {
        displayValue = tonumber(displayValue) or 0.0,
        commandValue = tonumber(commandValue) or 0.0,
        sourceRawValue = M.GetStateValue(surfName, widgetName),
        setTime = r.time_precise and r.time_precise() or os.clock(),
    }
    M.DebugFader(surfName, widgetName, string.format("local shadow set displayNormalized=%.6f commandValue=%.6f sourceRaw=%.6f", faderLocalValues[stateKey].displayValue, faderLocalValues[stateKey].commandValue, faderLocalValues[stateKey].sourceRawValue), 0.0, "shadow-set")
end

function M.GetFaderLocalValue(surfName, widgetName, rawValue)
    local stateKey = tostring(surfName or "") .. "|" .. tostring(widgetName or "")
    local localValue = faderLocalValues[stateKey]
    if not localValue then return nil end

    rawValue = tonumber(rawValue) or 0.0
    if math.abs(rawValue - localValue.sourceRawValue) > 0.0005 then
        M.DebugFader(surfName, widgetName, string.format("local shadow cleared raw changed sourceRaw=%.6f raw=%.6f", localValue.sourceRawValue, rawValue), 0.0, "shadow-clear")
        faderLocalValues[stateKey] = nil
        return nil
    end

    return localValue.displayValue
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

local function applyTitleCase(text)
    local lowered = tostring(text or ""):lower()
    return (lowered:gsub("(%a)([%w']*)", function(first, rest)
        return first:upper() .. rest
    end))
end

local function applySentenceCase(text)
    local lowered = tostring(text or ""):lower()
    return (lowered:gsub("^%l", string.upper))
end

function M.applyLabelCase(text)
    local mode = tostring(theme.osk.label_case or "original")
    if mode == "title" then return applyTitleCase(text) end
    if mode == "sentence" then return applySentenceCase(text) end
    if mode == "upper" then return tostring(text or ""):upper() end
    if mode == "lower" then return tostring(text or ""):lower() end
    return text
end

function M.getProcessedLabel(text)
    if not text or text == "" then return "" end
    local cacheKey = tostring(theme.osk.label_case or "original") .. "\0" .. text
    local cached = M.processedLabelCache[cacheKey]
    if cached then return cached end
    cached = M.applyLabelCase(M.processLabel(text))
    M.processedLabelCache[cacheKey] = cached
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
        return true
    end
    return false
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

local function cellHasInput(cell, inputName)
    return cell and cell.inputs and cell.inputs[inputName] == true
end

local function cellHasFeedback(cell, feedbackName)
    return cell and cell.feedbacks and cell.feedbacks[feedbackName] == true
end

local function isRelativeByFallback(cell)
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

function M.GetWidgetRole(surfName, widgetName)
    local cell = M.GetCellInfo(surfName, widgetName)
    if not cell then return "unknown" end
    local role = tostring(cell.role or ""):lower()
    if role ~= "" and role ~= "unknown" then return role end
    if cellHasInput(cell, "absolute") or tostring(cell.shape or ""):lower() == "fader" then return "fader" end
    if cellHasInput(cell, "relative") or isRelativeByFallback(cell) then return "rotary" end
    if cellHasInput(cell, "press") then return "button" end
    return "button"
end

function M.IsFaderWidget(surfName, widgetName)
    return M.GetWidgetRole(surfName, widgetName) == "fader"
end

function M.IsRotaryWidget(surfName, widgetName)
    return M.GetWidgetRole(surfName, widgetName) == "rotary"
end

function M.IsButtonWidget(surfName, widgetName)
    return M.GetWidgetRole(surfName, widgetName) == "button"
end

function M.IsRelativeWidget(surfName, widgetName)
    local cell = M.GetCellInfo(surfName, widgetName)
    if cellHasInput(cell, "relative") then return true end
    if cell and tostring(cell.input or "") ~= "" then return false end
    return isRelativeByFallback(cell)
end

function M.HasValueFeedback(surfName, widgetName)
    local cell = M.GetCellInfo(surfName, widgetName)
    return cellHasFeedback(cell, "value")
end

function M.GetPressTarget(surfName, cell)
    if not cell then return nil end
    if cell.pressTarget and cell.pressTarget ~= "" then return cell.pressTarget end
    if cellHasInput(cell, "press") then return cell.name end
    if tostring(cell.input or "") == "" and not M.IsRelativeWidget(surfName, cell.name) then return cell.name end
    return nil
end

function M.GetScrollTarget(surfName, cell)
    if not cell then return nil end
    if cell.scrollTarget and cell.scrollTarget ~= "" then return cell.scrollTarget end
    if M.IsRelativeWidget(surfName, cell.name) then return cell.name end
    return nil
end

function M.GetValueTarget(surfName, cell)
    if not cell then return nil end
    if cell.valueTarget and cell.valueTarget ~= "" then return cell.valueTarget end
    if M.IsFaderWidget(surfName, cell.name) or cellHasInput(cell, "absolute") then return cell.name end
    return nil
end

function M.GetTouchTarget(surfName, cell)
    if not cell then return nil end
    if cell.touchTarget and cell.touchTarget ~= "" then return cell.touchTarget end
    if cellHasInput(cell, "touch") then return cell.name end
    return nil
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
        local availability = rest:match("A:(%d+)")
        local kind = rest:match("K:([%a]+)") or ""
        result[name] = {
            value = value,
            color = theme.HexToImCol(colorHex, theme.OSK_COLORS.button_off),
            hasValue = availability ~= "0",
            kind = kind,
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
    settings_store.Load(M.EXT_SETTINGS, SETTINGS_SCHEMA, M.vars)
    M.parseLabelReplacements(M.vars.label_replacements)
end

function M.SaveSettings()
    settings_store.Save(M.EXT_SETTINGS, SETTINGS_SCHEMA, M.vars)
end

function M.LoadSurfacePosition(surfName)
    if M.surfacePos[surfName] then return M.surfacePos[surfName] end
    M.surfacePos[surfName] = settings_store.ReadPair(M.EXT_SETTINGS, SURFACE_POSITION_PREFIX .. surfName)
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
            settings_store.WritePair(M.EXT_SETTINGS, SURFACE_POSITION_PREFIX .. surfName, position)
        end
        dirtySurfacePositions[surfName] = nil
    end
    surfacePositionSaveDue = 0
end

function M.SetSurfaceEnabled(surfName, enabled)
    if not surfName or surfName == "" then return end
    local value = enabled and "true" or "false"
    r.SetExtState(M.EXT_SETTINGS, SURFACE_ENABLED_PREFIX .. surfName, value, true)
    r.SetExtState(M.EXT_CMD_SECTION, "SurfaceEnabled", surfName .. "|" .. (enabled and "1" or "0"), false)
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
        local stateChanged = M.PollExtStateEntry(surfName, "State", M.rawStates, M.states, M.ParseState)
        if stateChanged then
            for _, row in ipairs(M.layouts[surfName] or {}) do
                for _, cell in ipairs(row) do
                    if not cell.isSpacer and (M.IsFaderWidget(surfName, cell.name) or M.IsRotaryWidget(surfName, cell.name)) then
                        local state = M.states[surfName] and M.states[surfName][cell.name]
                        if state then
                            M.DebugFader(surfName, cell.name, string.format("state rawValue=%.6f role=%s shape=%s rowSpan=%s rawStateLen=%d", tonumber(state.value) or 0.0, tostring(M.GetWidgetRole(surfName, cell.name)), tostring(cell.shape), tostring(cell.rowSpan), #(M.rawStates[surfName] or "")), 0.0, "state")
                        else
                            M.DebugFader(surfName, cell.name, "state missing for value widget in parsed state", 0.0, "state-missing")
                        end
                    end
                end
            end
        end
        M.PollExtStateEntry(surfName, "Labels", M.rawLabels, M.labels, M.ParseLabels)
        M.PollExtStateEntry(surfName, "LabelMap", M.rawLabelMaps, M.labelMaps, M.ParseLabelMap)
    end

    return true
end

return M
