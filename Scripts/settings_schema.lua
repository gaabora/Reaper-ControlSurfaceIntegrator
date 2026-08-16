local module = {}

local scriptDirectory = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local defaultSchemaPath = scriptDirectory .. "settings_schema.conf"
local settingKeys = {
    Category = true,
    Default = true,
    GreaterThan = true,
    Max = true,
    Min = true,
    Scopes = true,
    Setting = true,
    Type = true,
    Unit = true,
    Values = true,
}
local function trim(value)
    return tostring(value or ""):match("^%s*(.-)%s*$")
end

local function parseProperties(line, lineNumber)
    local properties = {}
    for token in line:gmatch("%S+") do
        local separatorIndex = token:find("=", 1, true)
        if not separatorIndex or separatorIndex == 1 or separatorIndex == #token then error("Invalid settings schema token at line " .. lineNumber .. ": " .. token) end
        local key = token:sub(1, separatorIndex - 1)
        if properties[key] ~= nil then error("Duplicate settings schema property at line " .. lineNumber .. ": " .. key) end
        properties[key] = token:sub(separatorIndex + 1)
    end
    return properties
end

local function requireProperty(properties, key, lineNumber)
    local value = properties[key]
    if not value or value == "" then error("Settings schema line " .. lineNumber .. " requires " .. key) end
    return value
end

local function parseInteger(value, label, lineNumber)
    if not tostring(value):match("^%d+$") then error("Settings schema " .. label .. " must be a non-negative integer at line " .. lineNumber) end
    local parsedValue = tonumber(value)
    if not parsedValue then error("Settings schema " .. label .. " is outside the supported integer range at line " .. lineNumber) end
    return parsedValue
end

local function splitUniqueValues(value, label, lineNumber)
    local values = {}
    local seenValues = {}
    for entry in (value .. ","):gmatch("(.-),") do
        if entry == "" then error("Settings schema " .. label .. " contains an empty value at line " .. lineNumber) end
        if seenValues[entry] then error("Settings schema " .. label .. " contains a duplicate value at line " .. lineNumber) end
        values[#values + 1] = entry
        seenValues[entry] = true
    end
    return values, seenValues
end

function module.Parse(source)
    local schema = { settings = {}, settingsByName = {} }
    local lineNumber = 0
    local sawSetting = false
    source = tostring(source or ""):gsub("\r\n", "\n"):gsub("\r", "\n")
    for rawLine in (source .. "\n"):gmatch("(.-)\n") do
        lineNumber = lineNumber + 1
        local line = trim(rawLine)
        if line ~= "" and not line:match("^#") then
            local properties = parseProperties(line, lineNumber)
            if properties.Version ~= nil then
                local propertyCount = 0
                for ignoredKey in pairs(properties) do propertyCount = propertyCount + 1 end
                if propertyCount ~= 1 then error("Settings schema Version must be the only property at line " .. lineNumber) end
                if schema.version ~= nil then error("Settings schema Version is duplicated") end
                if sawSetting then error("Settings schema Version must appear before settings") end
                schema.version = parseInteger(properties.Version, "Version", lineNumber)
                if schema.version ~= 1 then error("Unsupported settings schema version: " .. schema.version) end
            else
                sawSetting = true
                for key in pairs(properties) do
                    if not settingKeys[key] then error("Unknown settings schema property at line " .. lineNumber .. ": " .. key) end
                end
                local name = requireProperty(properties, "Setting", lineNumber)
                if not name:match("^[A-Z][A-Za-z0-9]*$") then error("Invalid setting name at line " .. lineNumber .. ": " .. name) end
                if schema.settingsByName[name] then error("Duplicate setting: " .. name) end
                local sourceType = requireProperty(properties, "Type", lineNumber)
                if sourceType ~= "Enum" and sourceType ~= "Integer" then error("Unsupported setting type at line " .. lineNumber .. ": " .. sourceType) end
                local scopes = splitUniqueValues(requireProperty(properties, "Scopes", lineNumber), "Scopes", lineNumber)
                for scopeIdx, scope in ipairs(scopes) do
                    if not scope:match("^[A-Z][A-Za-z0-9]*$") then error("Invalid setting scope at line " .. lineNumber .. ": " .. scope) end
                end
                local category = requireProperty(properties, "Category", lineNumber)
                if not category:match("^[A-Z][A-Za-z0-9]*$") then error("Invalid setting category at line " .. lineNumber .. ": " .. category) end
                local defaultSource = requireProperty(properties, "Default", lineNumber)
                local definition = { category = category, name = name, scopes = scopes }
                if sourceType == "Enum" then
                    if properties.Min or properties.Max or properties.Unit or properties.GreaterThan then error("Enum setting has integer-only properties at line " .. lineNumber .. ": " .. name) end
                    local enumValues, enumValueSet = splitUniqueValues(requireProperty(properties, "Values", lineNumber), "Values", lineNumber)
                    for enumValueIdx, enumValue in ipairs(enumValues) do
                        if not enumValue:match("^[A-Z][A-Za-z0-9]*$") then error("Setting Values contains an invalid enum value at line " .. lineNumber .. ": " .. name) end
                    end
                    if not enumValueSet[defaultSource] then error("Setting default is not in Values at line " .. lineNumber .. ": " .. name) end
                    definition.type = "enum"
                    definition.defaultValue = defaultSource
                    definition.enumValues = enumValues
                else
                    if properties.Values then error("Integer setting has Values at line " .. lineNumber .. ": " .. name) end
                    local minValue = parseInteger(requireProperty(properties, "Min", lineNumber), "Min", lineNumber)
                    local maxValue = parseInteger(requireProperty(properties, "Max", lineNumber), "Max", lineNumber)
                    local defaultValue = parseInteger(defaultSource, "Default", lineNumber)
                    if minValue > maxValue or defaultValue < minValue or defaultValue > maxValue then error("Setting integer range is invalid at line " .. lineNumber .. ": " .. name) end
                    local unit = requireProperty(properties, "Unit", lineNumber)
                    if unit ~= "Milliseconds" then error("Unsupported setting unit at line " .. lineNumber .. ": " .. unit) end
                    definition.type = "integer"
                    definition.defaultValue = defaultValue
                    definition.min = minValue
                    definition.max = maxValue
                    definition.unit = unit
                    definition.greaterThan = properties.GreaterThan
                end
                schema.settings[#schema.settings + 1] = definition
                schema.settingsByName[name] = definition
            end
        end
    end
    if schema.version == nil then error("Settings schema has no Version") end
    if #schema.settings == 0 then error("Settings schema has no settings") end
    for settingIdx, setting in ipairs(schema.settings) do
        if setting.greaterThan then
            local referencedSetting = schema.settingsByName[setting.greaterThan]
            if not referencedSetting then error("Setting " .. setting.name .. " references unknown GreaterThan setting: " .. setting.greaterThan) end
            if setting.type ~= "integer" or referencedSetting.type ~= "integer" then error("Setting " .. setting.name .. " has a non-integer GreaterThan relationship") end
            if setting.defaultValue <= referencedSetting.defaultValue then error("Setting " .. setting.name .. " default must be greater than " .. setting.greaterThan) end
        end
    end
    return schema
end

function module.Load(schemaPath)
    schemaPath = schemaPath or defaultSchemaPath
    local schemaFile, openError = io.open(schemaPath, "r")
    if not schemaFile then error("Cannot open settings schema " .. schemaPath .. ": " .. tostring(openError)) end
    local source = schemaFile:read("*a")
    schemaFile:close()
    return module.Parse(source)
end

function module.RunSelfChecks()
    local schema = module.Load()
    assert(schema.version == 1, "settings schema version")
    assert(schema.settingsByName.DefaultModifierMode.defaultValue == "Latch", "default modifier mode")
    assert(schema.settingsByName.DefaultPseudoModifierMode.defaultValue == "Latch", "default pseudo-modifier mode")
    assert(schema.settingsByName.LongHoldDelayMs.greaterThan == "HoldDelayMs", "long hold constraint")
    assert(schema.settingsByName.HoldRepeatIntervalMs.defaultValue == 100, "hold repeat interval")
end

return module
