local M = {}

M.BUILTIN_REPLACEMENTS = {
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
        return text:sub(2, -2)
    end
    return text
end

local function sortRules(rules)
    table.sort(rules, function(left, right)
        if #left.word ~= #right.word then return #left.word > #right.word end
        return left.word < right.word
    end)
end

local function caseInsensitivePattern(word)
    local parts = {}
    for idx = 1, #word do
        local char = word:sub(idx, idx)
        if char:match("%a") then
            parts[#parts + 1] = "[" .. char:upper() .. char:lower() .. "]"
        elseif char:match("[%]%^%$%(%)%%%.%[%*%+%-%?]") then
            parts[#parts + 1] = "%" .. char
        else
            parts[#parts + 1] = char
        end
    end
    return table.concat(parts)
end

local function copyMap(source)
    local copy = {}
    for key, value in pairs(source or {}) do
        copy[key] = value
    end
    return copy
end

function M.ParseReplacementPair(entry)
    local key, value = tostring(entry or ""):match("^(.-)=(.*)$")
    if not key then return nil, nil end
    key = unquote(key)
    value = unquote(value)
    if key == "" then return nil, nil end
    return key, value
end

function M.ParseReplacementText(str)
    local replacements = {}
    for entry in tostring(str or ""):gmatch("[^;]+") do
        local key, value = M.ParseReplacementPair(entry)
        if key then
            replacements[key] = value
        end
    end
    return replacements
end

function M.BuildRuleSet(builtinReplacements, userReplacements)
    local merged = copyMap(builtinReplacements or M.BUILTIN_REPLACEMENTS)
    local userRules = {}
    local builtinRules = {}

    for word, replacement in pairs(userReplacements or {}) do
        merged[word] = replacement
        userRules[#userRules + 1] = { word = word, replacement = replacement }
    end
    for word, replacement in pairs(builtinReplacements or M.BUILTIN_REPLACEMENTS) do
        if (userReplacements or {})[word] == nil then
            builtinRules[#builtinRules + 1] = { word = word, replacement = replacement }
        end
    end

    sortRules(userRules)
    sortRules(builtinRules)

    local orderedRules = {}
    for _, rule in ipairs(userRules) do
        orderedRules[#orderedRules + 1] = rule
    end
    for _, rule in ipairs(builtinRules) do
        orderedRules[#orderedRules + 1] = rule
    end

    return merged, orderedRules
end

function M.Serialize(map)
    local entries = {}
    for word, replacement in pairs(map or {}) do
        entries[#entries + 1] = word .. "=" .. replacement
    end
    table.sort(entries)
    return table.concat(entries, ";")
end

function M.GetHelpText()
    return 'Custom replacements use semicolon-separated key=value pairs. Example: "toggle"="switch";"toggle the"="whatever";effects=fx;reaper=. Quotes are optional, spaces are allowed in keys, and key= removes matched text. Built-in replacements always apply unless overridden here.'
end

function M.Apply(text, orderedRules)
    local result = tostring(text or "")
    for _, rule in ipairs(orderedRules or {}) do
        local pattern = caseInsensitivePattern(rule.word)
        result = result:gsub("%f[%w]" .. pattern .. "%f[%W]", rule.replacement)
    end
    result = result:gsub("%s+", " "):match("^%s*(.-)%s*$")
    return result
end

function M.StripLabelPrefix(text)
    local after = tostring(text or ""):match("^[^:]+:%s*(.+)$")
    return after or tostring(text or "")
end

function M.SplitPascalCase(text)
    local result = tostring(text or ""):gsub("(%l)(%u)", "%1 %2")
    result = result:gsub("(%u%u)(%u%l)", "%1 %2")
    return result
end

function M.ProcessLabel(text, orderedRules)
    local result = M.StripLabelPrefix(text)
    result = M.SplitPascalCase(result)
    result = M.Apply(result, orderedRules)
    return result
end

function M.RunSelfChecks()
    local user = M.ParseReplacementText('"toggle the"=flip;effects=proc')
    assertEqual(user["toggle the"], "flip", "parse quoted key")
    assertEqual(user.effects, "proc", "parse bare key")

    local merged, ordered = M.BuildRuleSet(M.BUILTIN_REPLACEMENTS, user)
    assertEqual(merged.effects, "proc", "user override")
    assertEqual(ordered[1].word, "toggle the", "longest user rule first")

    local processed = M.ProcessLabel("Reaper: Toggle The Effects", ordered)
    assertEqual(processed, "flip proc", "processed label")
    return true
end

return M
