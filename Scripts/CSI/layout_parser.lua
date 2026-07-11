local M = {}

local function assertEqual(actual, expected, message)
    if actual ~= expected then
        error((message or "assertEqual failed") .. ": expected [" .. tostring(expected) .. "], got [" .. tostring(actual) .. "]")
    end
end

local function trim(text)
    return tostring(text or ""):match("^%s*(.-)%s*$")
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

local function splitDelimited(text, delimiter)
    local entries = {}
    local current = {}
    local inQuote = false
    local idx = 1
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

function M.HexToImCol(hex)
    if not hex then return 0x3a3a3aff end
    if hex:sub(1, 1) == "#" then hex = hex:sub(2) end
    if #hex < 6 then return 0x3a3a3aff end
    local red = tonumber(hex:sub(1, 2), 16) or 0
    local green = tonumber(hex:sub(3, 4), 16) or 0
    local blue = tonumber(hex:sub(5, 6), 16) or 0
    return (red << 24) | (green << 16) | (blue << 8) | 0xFF
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

function M.ParseLayoutCellProperties(cellStr)
    local properties = {}
    local metadata = cellStr:match("^[^:]+:(.*)$") or ""
    for _, entry in ipairs(splitDelimited(metadata, ",")) do
        local key, value = entry:match("^([^=]+)=(.*)$")
        if key then
            properties[trim(key)] = unquote(value)
        end
    end
    return properties
end

function M.ParseLayout(layoutStr)
    local result = {}
    for rowStr in tostring(layoutStr or ""):gmatch("[^\n]+") do
        local row = {}
        for _, cellStr in ipairs(splitDelimited(rowStr, "|")) do
            if cellStr ~= "" then
                local cell = {}
                if cellStr:match("^SPACER:") then
                    cell.isSpacer = true
                    cell.width = tonumber(cellStr:match("SPACER:([%d%.]+)")) or 0.5
                else
                    local properties = M.ParseLayoutCellProperties(cellStr)
                    cell.isSpacer = false
                    cell.name = cellStr:match("^([^:]+)")
                    cell.shape = tostring(properties.Shape or "rect"):lower()
                    cell.width = tonumber(properties.Width) or 1.0
                    cell.height = tonumber(properties.Height) or 1.0
                    cell.top = tonumber(properties.Top) or 0.0
                    if cell.shape == "fader" then cell.rowSpan = cell.height else cell.rowSpan = 1 end
                    cell.group = properties.Group or ""
                    cell.label = properties.Label or ""
                    local colorHex = tostring(properties.Color or ""):match("^#?%x+")
                    if colorHex then cell.color = M.HexToImCol(colorHex) end
                end
                row[#row + 1] = cell
            end
        end
        result[#result + 1] = M.FilterGroupedDuplicates(row)
    end
    return result
end

function M.RunSelfChecks()
    local rows = M.ParseLayout('Rotary1:Shape=Round,Width=1.00,Height=1.00,Group=Rotary,Label="Hello, \\"there\\" | friend",Color=#FF8800|Rotary2:Shape=Round,Group=Rotary\nSPACER:0.5|Fader1:Shape=Fader,Height=3,Top=0.5')
    assertEqual(#rows, 2, "row count")
    assertEqual(#rows[1], 1, "grouped duplicate count")
    assertEqual(rows[1][1].label, 'Hello, "there" | friend', "quoted label parse")
    assertEqual(rows[1][1].color, M.HexToImCol("#FF8800"), "color parse")
    assertEqual(rows[2][2].rowSpan, 3, "fader row span")
    return true
end

return M
