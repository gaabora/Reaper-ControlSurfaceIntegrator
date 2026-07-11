local r = reaper

local M = {}

M.EXT_SECTION = "CSI_OSK"
M.EXT_CMD_SECTION = "CSI_OSK_CMD"
M.EXT_SETTINGS = "CSI_OSK_SETTINGS"

local EXT_OSD_SECTION = "CSI_TMP"
local EXT_OSD_KEY = "OSD"

M.COLORS = {
    win_bg = 0x1e1e1eff,
    button_off = 0x3a3a3aff,
    button_on = 0xffb029ff,
    button_hover = 0x4a6a9aff,
    text_normal = 0x000000ff,
    text_dim = 0x444444ff,
    tab_bg = 0x2a2a2aff,
    tab_active = 0x3c6191ff,
    round_off = 0x444444ff,
    round_on_play = 0x40a040ff,
    round_on_stop = 0x808080ff,
    round_on_rec = 0xcc3030ff,
    arrow_off = 0x505050ff,
    arrow_on = 0x70b070ff,
}

M.surfaces = {}
M.currentSurface = 1
M.layouts = {}
M.states = {}
M.labels = {}
M.labelMaps = {}
M.rawLayouts = {}
M.rawStates = {}
M.rawLabels = {}
M.rawLabelMaps = {}
M.processedLabelCache = {}
M.osd = {
    text = "",
    bgColor = 0x333333ff,
    showUntil = 0,
    lastMsg = nil,
}

M.LABEL_REPLACEMENTS = {
    ["toggle"] = "",
    ["reaper"] = "",
    ["toolbar"] = "",
    ["move edit cursor to"] = "",
    ["cycle"] = "",
    ["previous"] = "prev",
    ["current"] = "curr",
    ["one"] = "1",
    ["and"] = "&",
    ["show"] = "",
    ["track"] = "",
    ["effects"] = "fx",
    ["set"] = "",
    ["blink"] = "",
    ["go zone"] = "",
    ["go to"] = "",
    ["record"] = "rec",
}

M.vars = {
    wlen = 500, hlen = 500, xpos = 200, ypos = 200, docked = 0,
    zoom = 0.9, clickable = true, aspect = 1.4,
    pad_h = 6, pad_v = 6,
    transparency = 0.6,
    btn_transparency = 0.9,
    tooltip_delay = 1.0,
    arrow_angle = 120,
    show_all_surfaces = true,
    window_mode = "combined",
    label_replacements = "",
}

local lastActiveSurface = ""

local function clearTable(tbl)
    for key in pairs(tbl) do
        tbl[key] = nil
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
    local after = text:match("^[^:]+:%s*(.+)$")
    return after or text
end

function M.splitPascalCase(text)
    local result = text:gsub("(%l)(%u)", "%1 %2")
    result = result:gsub("(%u%u)(%u%l)", "%1 %2")
    return result
end

local function caseInsensitivePattern(word)
    return word:gsub("%a", function(char)
        return "[" .. char:upper() .. char:lower() .. "]"
    end)
end

function M.applyLabelReplacements(text)
    for word, replacement in pairs(M.LABEL_REPLACEMENTS) do
        local pattern = caseInsensitivePattern(word)
        text = text:gsub("%f[%w]" .. pattern .. "%f[%W]", replacement)
    end
    text = text:gsub("%s+", " "):match("^%s*(.-)%s*$")
    return text
end

function M.processLabel(text)
    text = M.stripLabelPrefix(text)
    text = M.splitPascalCase(text)
    text = M.applyLabelReplacements(text)
    return text
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
    clearTable(M.LABEL_REPLACEMENTS)
    clearTable(M.processedLabelCache)
    if not str or str == "" then return end
    for entry in str:gmatch("[^;]+") do
        local key, value = entry:match("^(.-)=(.*)$")
        if key and key ~= "" then
            M.LABEL_REPLACEMENTS[key] = value
        end
    end
end

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

function M.PollOSD()
    local msg = r.GetExtState(EXT_OSD_SECTION, EXT_OSD_KEY)
    if msg ~= M.osd.lastMsg then
        M.osd.lastMsg = msg
        if not msg or msg == "" then
            M.osd.text = ""
            M.osd.showUntil = 0
            M.osd.bgColor = 0x333333ff
            return
        end

        local text, bgStr, timeoutStr = msg:match("([^;]*);?([^;]*);?([^;]*)")
        text = text and text:match("^%s*(.-)%s*$") or ""
        local timeoutMs = tonumber(timeoutStr) or 3000
        if bgStr == "1" then
            M.osd.bgColor = 0xA4A4A4ff
        elseif bgStr == "" or bgStr == "0" then
            M.osd.bgColor = 0x333333ff
        else
            M.osd.bgColor = M.hexToImCol(bgStr)
        end
        M.osd.text = text
        M.osd.showUntil = (timeoutMs > 0) and (r.time_precise() + timeoutMs / 1000) or 0
    end

    if M.osd.showUntil > 0 and r.time_precise() > M.osd.showUntil then
        M.osd.text = ""
        M.osd.showUntil = 0
    end
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
    local filtered = {}
    local seenGroups = {}

    for _, cell in ipairs(row) do
        if cell.isSpacer or not cell.group or cell.group == "" then
            filtered[#filtered + 1] = cell
        else
            local groupKey = tostring(cell.group):lower()
            if not seenGroups[groupKey] then
                seenGroups[groupKey] = true
                filtered[#filtered + 1] = cell
            end
        end
    end

    return filtered
end

function M.ParseLayout(layoutStr)
    local result = {}
    for rowStr in layoutStr:gmatch("[^\n]+") do
        local row = {}
        for cellStr in rowStr:gmatch("[^|]+") do
            local cell = {}
            if cellStr:match("^SPACER:") then
                cell.isSpacer = true
                cell.width = tonumber(cellStr:match("SPACER:([%d%.]+)")) or 0.5
            else
                cell.isSpacer = false
                cell.name = cellStr:match("^([^:]+)")
                cell.shape = (cellStr:match("Shape=([^,]+)") or "rect"):lower()
                cell.width = tonumber(cellStr:match("Width=([%d%.]+)")) or 1.0
                cell.height = tonumber(cellStr:match("Height=([%d%.]+)")) or 1.0
                cell.top = tonumber(cellStr:match("Top=([%d%.]+)")) or 0.0
                if cell.shape == "fader" then cell.rowSpan = cell.height else cell.rowSpan = 1 end
                cell.group = cellStr:match("Group=([^,]+)") or ""
                cell.label = cellStr:match("Label=(.+)$") or ""
                local colorHex = cellStr:match("Color=(#?%x+)")
                if colorHex then cell.color = M.hexToImCol(colorHex) end
            end
            row[#row + 1] = cell
        end
        result[#result + 1] = M.FilterGroupedDuplicates(row)
    end
    return result
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
            if M.currentSurface > #M.surfaces then M.currentSurface = 1 end
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

function M.PollActiveSurface()
    local active = r.GetExtState(M.EXT_SECTION, "ActiveSurface")
    if active == lastActiveSurface then return end
    lastActiveSurface = active
    if active == "" then return end
    if not M.vars.show_all_surfaces then
        for index, name in ipairs(M.surfaces) do
            if name == active then
                M.currentSurface = index
                break
            end
        end
    end
end

return M