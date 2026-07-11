local r = reaper

local M = {}

function M.Clamp(value, minValue, maxValue)
    if minValue ~= nil and value < minValue then value = minValue end
    if maxValue ~= nil and value > maxValue then value = maxValue end
    return value
end

function M.ClampStep(value, minValue, maxValue, step)
    value = M.Clamp(value, minValue, maxValue)
    if step and step > 0 then
        value = math.floor((value + step / 2) / step) * step
    end
    return value
end

local function cloneDefault(rule)
    if type(rule.default) ~= "table" then return rule.default end
    local copy = {}
    for key, value in pairs(rule.default) do
        copy[key] = value
    end
    return copy
end

function M.NormalizeValue(value, rule)
    if value == nil then value = cloneDefault(rule) end
    if rule.type == "number" then
        value = tonumber(value)
        if value == nil then value = tonumber(rule.default) or 0 end
        if rule.integer then value = math.floor(value + 0.5) end
        if rule.step then
            value = M.ClampStep(value, rule.min, rule.max, rule.step)
        else
            value = M.Clamp(value, rule.min, rule.max)
        end
        return value
    end
    if rule.type == "boolean" then
        if type(value) == "string" then return value == "true" end
        return value == true
    end
    value = tostring(value or cloneDefault(rule) or "")
    if rule.enum then
        for _, allowed in ipairs(rule.enum) do
            if value == allowed then return value end
        end
        return tostring(rule.default or "")
    end
    return value
end

function M.Load(section, schema, target)
    target = target or {}
    for key, rule in pairs(schema or {}) do
        local rawValue = r.GetExtState(section, key)
        local value = rawValue ~= "" and rawValue or nil
        target[key] = M.NormalizeValue(value, rule)
    end
    return target
end

function M.Save(section, schema, values)
    for key in pairs(schema or {}) do
        r.SetExtState(section, key, tostring(values[key]), true)
    end
end

function M.ParsePair(rawValue)
    local first, second = tostring(rawValue or ""):match("^([%-%d%.]+),([%-%d%.]+)$")
    first = tonumber(first)
    second = tonumber(second)
    if first and second then return { x = first, y = second } end
    return nil
end

function M.ReadPair(section, key)
    return M.ParsePair(r.GetExtState(section, key))
end

function M.WritePair(section, key, pair)
    if not pair or pair.x == nil or pair.y == nil then return end
    r.SetExtState(section, key, string.format("%.3f,%.3f", pair.x, pair.y), true)
end

return M
