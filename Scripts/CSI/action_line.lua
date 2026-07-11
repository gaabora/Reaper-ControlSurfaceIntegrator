local M = {}

local function assertEqual(actual, expected, message)
    if actual ~= expected then
        error((message or "assertEqual failed") .. ": expected [" .. tostring(expected) .. "], got [" .. tostring(actual) .. "]")
    end
end

function M.TokenizePreservingQuotes(text)
    local tokens = {}
    local source = tostring(text or "")
    local current = ""
    local inQuote = false
    local idx = 1
    while idx <= #source do
        local ch = source:sub(idx, idx)
        if inQuote and ch == "\\" and idx < #source then
            current = current .. ch .. source:sub(idx + 1, idx + 1)
            idx = idx + 1
        elseif ch == '"' then
            inQuote = not inQuote
            current = current .. ch
        elseif (ch == " " or ch == "\t") and not inQuote then
            if current ~= "" then
                tokens[#tokens + 1] = current
                current = ""
            end
        else
            current = current .. ch
        end
        idx = idx + 1
    end
    if current ~= "" then
        tokens[#tokens + 1] = current
    end
    return tokens
end

function M.UnescapeQuotedText(text)
    local out = {}
    local idx = 1
    while idx <= #text do
        local ch = text:sub(idx, idx)
        if ch == "\\" and idx < #text then
            local nextCh = text:sub(idx + 1, idx + 1)
            if nextCh == '"' or nextCh == "\\" then
                out[#out + 1] = nextCh
                idx = idx + 2
            else
                out[#out + 1] = ch
                idx = idx + 1
            end
        else
            out[#out + 1] = ch
            idx = idx + 1
        end
    end
    return table.concat(out)
end

function M.UnquoteValue(value)
    local text = tostring(value or "")
    if #text >= 2 and text:sub(1, 1) == '"' and text:sub(-1) == '"' then
        return M.UnescapeQuotedText(text:sub(2, -2))
    end
    return text
end

function M.QuoteIfNeeded(value)
    local text = tostring(value or "")
    if text == "" then return text end
    if text:find("[%s\"]") or text:find("\\", 1, true) then
        text = text:gsub("\\", "\\\\")
        text = text:gsub('"', '\\"')
        return '"' .. text .. '"'
    end
    return text
end

function M.Parse(line)
    local tokens = M.TokenizePreservingQuotes(line)
    local parts = {
        actionName = tokens[1] or "",
        params = {},
        properties = {},
        colorTokens = nil,
    }

    local tokenIdx = 2
    while tokenIdx <= #tokens do
        local token = tokens[tokenIdx]
        if token == "{" then
            local colorTokens = {}
            tokenIdx = tokenIdx + 1
            while tokenIdx <= #tokens and tokens[tokenIdx] ~= "}" do
                colorTokens[#colorTokens + 1] = tokens[tokenIdx]
                tokenIdx = tokenIdx + 1
            end
            if tokenIdx <= #tokens and tokens[tokenIdx] == "}" then
                parts.colorTokens = colorTokens
            else
                parts.params[#parts.params + 1] = "{"
                for _, colorToken in ipairs(colorTokens) do
                    parts.params[#parts.params + 1] = colorToken
                end
            end
        else
            local key, value = token:match("^(.-)=(.+)$")
            if key and value then
                parts.properties[key] = M.UnquoteValue(value)
            else
                parts.params[#parts.params + 1] = M.UnquoteValue(token)
            end
        end
        tokenIdx = tokenIdx + 1
    end

    return parts
end

function M.Build(parts)
    local out = {}
    out[#out + 1] = (parts.actionName and parts.actionName ~= "") and parts.actionName or "NoAction"

    for _, param in ipairs(parts.params or {}) do
        if param ~= "" then
            out[#out + 1] = M.QuoteIfNeeded(param)
        end
    end

    if parts.colorTokens and #parts.colorTokens > 0 then
        out[#out + 1] = "{"
        for _, colorToken in ipairs(parts.colorTokens) do
            out[#out + 1] = tostring(colorToken)
        end
        out[#out + 1] = "}"
    end

    local used = {}
    local priorityKeys = { "Feedback", "HoldDelay", "HoldRepeatInterval", "RunCount", "OSD", "KeyLabel" }
    for _, key in ipairs(priorityKeys) do
        local value = parts.properties and parts.properties[key]
        if value ~= nil and tostring(value) ~= "" then
            out[#out + 1] = key .. "=" .. M.QuoteIfNeeded(value)
            used[key] = true
        end
    end

    local remaining = {}
    for key, value in pairs(parts.properties or {}) do
        if not used[key] and value ~= nil and tostring(value) ~= "" then
            remaining[#remaining + 1] = key
        end
    end
    table.sort(remaining)
    for _, key in ipairs(remaining) do
        out[#out + 1] = key .. "=" .. M.QuoteIfNeeded(parts.properties[key])
    end

    return table.concat(out, " ")
end

function M.ClampColorChannel(value)
    return math.max(0, math.min(255, math.floor(tonumber(value) or 0)))
end

function M.PackRgb(red, green, blue)
    return (M.ClampColorChannel(red) << 24)
        | (M.ClampColorChannel(green) << 16)
        | (M.ClampColorChannel(blue) << 8)
        | 0xff
end

function M.UnpackRgb(color)
    return (color >> 24) & 0xff, (color >> 16) & 0xff, (color >> 8) & 0xff
end

function M.ParseHexColor(token)
    local hex = tostring(token or ""):match("^#?(%x%x%x%x%x%x)")
    if not hex then return nil end
    return M.PackRgb(
        tonumber(hex:sub(1, 2), 16),
        tonumber(hex:sub(3, 4), 16),
        tonumber(hex:sub(5, 6), 16)
    )
end

function M.ParseColors(parts)
    local colorTokens = parts and parts.colorTokens
    if not colorTokens or #colorTokens == 0 then return nil end

    local colors = {}
    if tostring(colorTokens[1]):sub(1, 1) == "#" then
        for _, colorToken in ipairs(colorTokens) do
            local color = M.ParseHexColor(colorToken)
            if color then colors[#colors + 1] = color end
        end
    else
        local channels = {}
        for _, colorToken in ipairs(colorTokens) do
            local channel = tonumber(colorToken)
            if channel == nil then return nil end
            channels[#channels + 1] = M.ClampColorChannel(channel)
        end
        if #channels % 3 ~= 0 then return nil end
        for channelIdx = 1, #channels, 3 do
            colors[#colors + 1] = M.PackRgb(channels[channelIdx], channels[channelIdx + 1], channels[channelIdx + 2])
        end
    end

    if #colors == 0 then return nil end
    return colors
end

function M.SetColors(parts, colors)
    parts.colorTokens = {}
    for _, color in ipairs(colors or {}) do
        local red, green, blue = M.UnpackRgb(color)
        parts.colorTokens[#parts.colorTokens + 1] = tostring(red)
        parts.colorTokens[#parts.colorTokens + 1] = tostring(green)
        parts.colorTokens[#parts.colorTokens + 1] = tostring(blue)
    end
end

function M.ClearColors(parts)
    parts.colorTokens = nil
end

function M.RunSelfChecks()
    local parsed = M.Parse('Reaper "_SWS_TEST action" OSD="Hello \\"world\\"" KeyLabel="A\\\\B"')
    assertEqual(parsed.actionName, "Reaper", "action name")
    assertEqual(parsed.params[1], "_SWS_TEST action", "quoted param")
    assertEqual(parsed.properties.OSD, 'Hello "world"', "escaped quote property")
    assertEqual(parsed.properties.KeyLabel, "A\\B", "escaped backslash property")

    local rebuilt = M.Build(parsed)
    assertEqual(rebuilt, 'Reaper "_SWS_TEST action" OSD="Hello \\"world\\"" KeyLabel="A\\\\B"', "round-trip build")

    local colorParts = M.Parse('Action { 255 0 0 0 255 0 }')
    local colors = M.ParseColors(colorParts)
    assertEqual(#colors, 2, "parsed color count")
    assertEqual(colors[1], M.PackRgb(255, 0, 0), "first parsed color")
    return true
end

return M
