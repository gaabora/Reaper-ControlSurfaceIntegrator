local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local identity = dofile(scriptDir .. "product_identity.lua")
local section = identity.extState.osk
local settingsSection = identity.extState.oskSettings

local PRINT_RAW_PAYLOADS = false
local PRINT_ONLY_WATCHED_WIDGETS = true
local WATCH_WIDGETS = {
    -- Solo = true,
    -- Mute = true,
    -- Arm = true,
    -- Shift = true,
    Touch = true,
    Write = true,
    Read = true,
    -- Link = true,
    -- Pan = true,
    -- Channel = true,
    -- Scroll = true,
}

local function msg(text)
    reaper.ShowConsoleMsg(tostring(text or "") .. "\n")
end

local function trim(text)
    return tostring(text or ""):match("^%s*(.-)%s*$")
end

local function lower(text)
    return tostring(text or ""):lower()
end

local function splitDelimited(text, delimiter)
    local entries = {}
    local current = {}
    local inQuote = false
    local idx = 1
    text = tostring(text or "")
    while idx <= #text do
        local ch = text:sub(idx, idx)
        if inQuote and ch == "\\" and idx < #text then
            current[#current + 1] = ch
            current[#current + 1] = text:sub(idx + 1, idx + 1)
            idx = idx + 2
        else
            if ch == '"' then inQuote = not inQuote end
            if ch == delimiter and not inQuote then
                entries[#entries + 1] = table.concat(current)
                current = {}
            else
                current[#current + 1] = ch
            end
            idx = idx + 1
        end
    end
    entries[#entries + 1] = table.concat(current)
    return entries
end

local function unquote(text)
    text = trim(text)
    if #text >= 2 and text:sub(1, 1) == '"' and text:sub(-1) == '"' then
        local out = {}
        local idx = 2
        while idx < #text do
            local ch = text:sub(idx, idx)
            if ch == "\\" and idx + 1 < #text then
                local nextCh = text:sub(idx + 1, idx + 1)
                if nextCh == "n" then
                    out[#out + 1] = "\n"
                elseif nextCh == "r" then
                    out[#out + 1] = "\r"
                elseif nextCh == '"' or nextCh == "\\" then
                    out[#out + 1] = nextCh
                else
                    out[#out + 1] = ch .. nextCh
                end
                idx = idx + 2
            else
                out[#out + 1] = ch
                idx = idx + 1
            end
        end
        return table.concat(out)
    end
    return text
end

local function parseSet(text)
    local set = {}
    for token in tostring(text or ""):gmatch("[^,%+]+") do
        token = lower(trim(token))
        if token ~= "" then set[token] = true end
    end
    return set
end

local function hexToColor(hex, fallback)
    hex = tostring(hex or "")
    if hex:sub(1, 1) == "#" then hex = hex:sub(2) end
    if #hex < 6 then return fallback or 0x000000ff end
    local red = tonumber(hex:sub(1, 2), 16) or 0
    local green = tonumber(hex:sub(3, 4), 16) or 0
    local blue = tonumber(hex:sub(5, 6), 16) or 0
    return (red << 24) | (green << 16) | (blue << 8) | 0xff
end

local function colorToHex(color)
    color = tonumber(color) or 0
    return string.format("#%02X%02X%02X", (color >> 24) & 0xff, (color >> 16) & 0xff, (color >> 8) & 0xff)
end

local function isMeaningfulColor(color)
    color = tonumber(color) or 0
    return ((color >> 24) & 0xff) >= 10 or ((color >> 16) & 0xff) >= 10 or ((color >> 8) & 0xff) >= 10
end

local function packColor(red, green, blue)
    red = math.max(0, math.min(255, math.floor((tonumber(red) or 0) + 0.5)))
    green = math.max(0, math.min(255, math.floor((tonumber(green) or 0) + 0.5)))
    blue = math.max(0, math.min(255, math.floor((tonumber(blue) or 0) + 0.5)))
    return (red << 24) | (green << 16) | (blue << 8) | 0xff
end

local function adjustColorValue(color, valueDelta)
    valueDelta = tonumber(valueDelta) or 0
    if valueDelta == 0 then return color end
    local red = (color >> 24) & 0xff
    local green = (color >> 16) & 0xff
    local blue = (color >> 8) & 0xff
    local maxChannel = math.max(red, green, blue)
    if maxChannel <= 0 then return color end
    local adjustedMax = math.max(0, math.min(255, maxChannel + valueDelta * 2.55))
    local scale = adjustedMax / maxChannel
    return packColor(red * scale, green * scale, blue * scale)
end

local function ensureMinLuminance(color, minLum)
    local red = (color >> 24) & 0xff
    local green = (color >> 16) & 0xff
    local blue = (color >> 8) & 0xff
    local luminance = 0.299 * red + 0.587 * green + 0.114 * blue
    if luminance < minLum then
        local add = minLum - luminance
        red = math.min(255, math.floor(red + add))
        green = math.min(255, math.floor(green + add))
        blue = math.min(255, math.floor(blue + add))
    end
    return (red << 24) | (green << 16) | (blue << 8) | 0xff
end

local function applyInactiveLedBoost(color, boost)
    boost = math.max(0, math.min(100, math.floor((tonumber(boost) or 50) + 0.5)))
    if boost <= 0 then return color end
    local red = (color >> 24) & 0xff
    local green = (color >> 16) & 0xff
    local blue = (color >> 8) & 0xff
    local maxChannel = math.max(red, green, blue)
    if maxChannel <= 0 then return color end
    local boostedMax = math.min(255, maxChannel + boost * 2.55)
    local scale = boostedMax / maxChannel
    red = math.max(0, math.min(255, math.floor(red * scale + 0.5)))
    green = math.max(0, math.min(255, math.floor(green * scale + 0.5)))
    blue = math.max(0, math.min(255, math.floor(blue * scale + 0.5)))
    return (red << 24) | (green << 16) | (blue << 8) | 0xff
end

local function parseLayout(layoutStr)
    local rows = {}
    local cellsByName = {}
    for rowIndex, rowStr in ipairs(splitDelimited(layoutStr, "\n")) do
        if rowStr ~= "" then
            local row = {}
            for colIndex, cellStr in ipairs(splitDelimited(rowStr, "|")) do
                if cellStr ~= "" then
                    local cell = { row = rowIndex, col = colIndex, raw = cellStr }
                    if cellStr:match("^SPACER:") then
                        cell.isSpacer = true
                        cell.name = "SPACER"
                        cell.width = tonumber(cellStr:match("SPACER:([%d%.]+)")) or 0.5
                    else
                        cell.isSpacer = false
                        cell.name = cellStr:match("^([^:]+)") or "?"
                        local metadata = cellStr:match("^[^:]+:(.*)$") or ""
                        local properties = {}
                        for _, entry in ipairs(splitDelimited(metadata, ",")) do
                            local key, value = entry:match("^([^=]+)=(.*)$")
                            if key then properties[trim(key)] = unquote(value) end
                        end
                        cell.shape = lower(properties.Shape or "rect")
                        cell.role = lower(properties.Role or "")
                        cell.input = lower(properties.Input or "")
                        cell.feedback = lower(properties.Feedback or "")
                        cell.class = properties.Class or properties.WidgetClass or ""
                        cell.group = properties.Group or ""
                        cell.label = properties.Label or ""
                        cell.width = tonumber(properties.Width) or 1.0
                        cell.height = tonumber(properties.Height) or 1.0
                        cell.inputs = parseSet(cell.input)
                        cell.feedbacks = parseSet(cell.feedback)
                        cell.colorRaw = properties.Color or ""
                        local colorHex = tostring(properties.Color or ""):match("^#?%x+")
                        if colorHex then cell.color = hexToColor(colorHex) end
                        cellsByName[cell.name] = cell
                    end
                    row[#row + 1] = cell
                end
            end
            rows[#rows + 1] = row
        end
    end
    return rows, cellsByName
end

local function parseState(stateStr)
    local states = {}
    for entry in tostring(stateStr or ""):gmatch("[^;]+") do
        local name, rest = entry:match("^(.-)=(.*)$")
        if name then
            local colorHex = rest:match("C:(#%x+)") or "#333333"
            states[name] = {
                raw = rest,
                value = tonumber(rest:match("V:([%d%.%-]+)")) or 0,
                colorHex = colorHex,
                color = hexToColor(colorHex, 0x333333ff),
                available = rest:match("A:(%d+)") or "?",
                kind = rest:match("K:([%a]+)") or "",
            }
        end
    end
    return states
end

local function parseKeyValueList(raw)
    local out = {}
    for entry in tostring(raw or ""):gmatch("[^;]+") do
        local key, value = entry:match("^(.-)=(.*)$")
        if key then out[key] = value end
    end
    return out
end

local function inferRole(cell)
    if not cell or cell.isSpacer then return "spacer" end
    if cell.role and cell.role ~= "" and cell.role ~= "unknown" then return cell.role end
    if cell.shape == "fader" or (cell.inputs and cell.inputs.absolute) then return "fader" end
    if cell.inputs and cell.inputs.relative then return "rotary" end
    return "button"
end

local function computeCurrentOskButtonColor(cell, state, boost)
    local stateColor = state and isMeaningfulColor(state.color) and state.color or nil
    local cellColor = cell and cell.color or nil
    local active = state and state.value > 0
    if active then
        if cellColor then return cellColor, "active layout color" end
        if stateColor then return stateColor, "active state color" end
        return 0xffb029ff, "active default"
    end
    if cellColor then return adjustColorValue(cellColor, -50), "inactive layout color value -50" end
    if stateColor then return applyInactiveLedBoost(stateColor, boost), "inactive state color + boost" end
    return ensureMinLuminance(0x3a3a3aff, 80), "inactive default"
end

local function readExtState(label, key)
    local value = reaper.GetExtState(section, key)
    if PRINT_RAW_PAYLOADS then
        msg(label .. " (" .. key .. ", len=" .. tostring(#value) .. "): " .. value)
    else
        msg(label .. " (" .. key .. ", len=" .. tostring(#value) .. ")")
    end
    return value
end

-- reaper.ClearConsole()
msg(identity.displayName .. " OSK state debug")
msg("===================")
local surfaces = reaper.GetExtState(section, "Surfaces")
local boost = reaper.GetExtState(settingsSection, "inactive_led_boost")
if boost == "" then boost = "50 (default/missing)" end
msg("Surfaces: " .. tostring(surfaces))
msg("Settings: inactive_led_boost=" .. tostring(boost)
    .. " btn_transparency=" .. tostring(reaper.GetExtState(settingsSection, "btn_transparency"))
    .. " SurfaceEnabled_* keys are printed per surface")

for surf in tostring(surfaces):gmatch("[^|]+") do
    msg("")
    msg("[" .. surf .. "]")
    msg("SurfaceEnabled: " .. tostring(reaper.GetExtState(settingsSection, "SurfaceEnabled_" .. surf)))
    msg("SurfacePosition: " .. tostring(reaper.GetExtState(settingsSection, "SurfacePosition_" .. surf)))

    local layoutRaw = readExtState("Layout", "Layout_" .. surf)
    local stateRaw = readExtState("State", "State_" .. surf)
    local labelsRaw = readExtState("Labels", "Labels_" .. surf)
    local labelMapRaw = readExtState("LabelMap", "LabelMap_" .. surf)

    local _, cellsByName = parseLayout(layoutRaw)
    local states = parseState(stateRaw)
    local labels = parseKeyValueList(labelsRaw)
    local labelMap = parseKeyValueList(labelMapRaw)
    local names = {}
    local seen = {}
    for name in pairs(cellsByName) do
        if not PRINT_ONLY_WATCHED_WIDGETS or WATCH_WIDGETS[name] then
            names[#names + 1] = name
            seen[name] = true
        end
    end
    for name in pairs(states) do
        if not seen[name] and (not PRINT_ONLY_WATCHED_WIDGETS or WATCH_WIDGETS[name]) then
            names[#names + 1] = name
            seen[name] = true
        end
    end
    table.sort(names)

    msg(PRINT_ONLY_WATCHED_WIDGETS and "Parsed watched widgets:" or "Parsed widgets:")
    for _, name in ipairs(names) do
        local cell = cellsByName[name]
        local state = states[name]
        local role = inferRole(cell)
        local renderColor, renderSource = computeCurrentOskButtonColor(cell, state, tonumber(boost) or 50)
        msg(string.format(
            "  %-18s role=%-7s shape=%-10s input=%-16s feedback=%-10s class=%-18s layoutColor=%-9s rawLayoutColor=%-8s stateV=%-5s stateColor=%-9s stateA=%s stateK=%-2s render=%-9s renderSource=%s label=%s labelMap=%s",
            name,
            role,
            cell and cell.shape or "?",
            cell and cell.input or "",
            cell and cell.feedback or "",
            cell and cell.class or "",
            cell and cell.color and colorToHex(cell.color) or "-",
            cell and cell.colorRaw or "",
            state and string.format("%.2f", state.value) or "-",
            state and state.colorHex or "-",
            state and state.available or "-",
            state and state.kind or "",
            colorToHex(renderColor),
            renderSource,
            labels[name] or "",
            labelMap[name] or ""
        ))
    end
end
