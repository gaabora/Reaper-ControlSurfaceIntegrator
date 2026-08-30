local identity = require("product_identity")

local module = {}

local requestCounter = 0

local function splitProperties(source)
    local properties = {}
    for rawLine in (tostring(source or "") .. "\n"):gmatch("(.-)\n") do
        local line = rawLine:gsub("\r$", "")
        if line ~= "" then
            local separatorIndex = line:find("=", 1, true)
            if not separatorIndex or separatorIndex == 1 then return nil, "Invalid settings protocol line: " .. line end
            local key = line:sub(1, separatorIndex - 1)
            if properties[key] ~= nil then return nil, "Duplicate settings protocol property: " .. key end
            properties[key] = line:sub(separatorIndex + 1)
        end
    end
    return properties
end

local function nextRequestId()
    requestCounter = requestCounter + 1
    local timestamp = reaper and reaper.time_precise and math.floor(reaper.time_precise() * 1000000) or math.floor(os.clock() * 1000000)
    return tostring(timestamp) .. "_" .. tostring(requestCounter)
end

local function buildRequest(command, scope, deviceId, changes, requestId)
    local lines = {
        "Version=1",
        "RequestId=" .. tostring(requestId),
        "Command=" .. tostring(command),
    }
    if scope and scope ~= "" then lines[#lines + 1] = "Scope=" .. tostring(scope) end
    if deviceId and deviceId ~= "" then lines[#lines + 1] = "Device=" .. tostring(deviceId) end
    for settingName, change in pairs(changes or {}) do
        if type(change) == "table" and change.unset == true then
            lines[#lines + 1] = "Unset." .. settingName .. "=1"
        else
            local value = type(change) == "table" and change.value or change
            lines[#lines + 1] = "Set." .. settingName .. "=" .. tostring(value)
        end
    end
    return table.concat(lines, "\n") .. "\n"
end

function module.ParseResponse(source)
    local properties, parseError = splitProperties(source)
    if not properties then return nil, parseError end
    if properties.Version ~= "1" then return nil, "Settings response Version must be 1" end
    if properties.Status ~= "OK" and properties.Status ~= "ERROR" then return nil, "Settings response Status must be OK or ERROR" end
    local response = {
        configExists = properties.ConfigExists ~= "0",
        message = properties.Message or "",
        deviceId = properties.Device or "",
        deviceOptions = {},
        inheritedValues = {},
        ok = properties.Status == "OK",
        scope = properties.Scope or "",
        sources = {},
        values = {},
    }
    local deviceOptionsByIndex = {}
    local maximumDeviceOptionIdx = 0
    for key, value in pairs(properties) do
        local valueName = key:match("^Value%.([A-Z][A-Za-z0-9]*)$")
        local sourceName = key:match("^Source%.([A-Z][A-Za-z0-9]*)$")
        local inheritedName = key:match("^Inherited%.([A-Z][A-Za-z0-9]*)$")
        local deviceOptionIdx = tonumber(key:match("^DeviceOption%.(%d+)$"))
        if valueName then response.values[valueName] = value end
        if sourceName then response.sources[sourceName] = value end
        if inheritedName then response.inheritedValues[inheritedName] = value end
        if deviceOptionIdx then deviceOptionsByIndex[deviceOptionIdx] = value maximumDeviceOptionIdx = math.max(maximumDeviceOptionIdx, deviceOptionIdx) end
    end
    for optionIdx = 1, maximumDeviceOptionIdx do if deviceOptionsByIndex[optionIdx] then response.deviceOptions[#response.deviceOptions + 1] = deviceOptionsByIndex[optionIdx] end end
    return response
end

function module.Send(command, scope, deviceId, changes)
    if not reaper then return nil, "REAPER API is not available" end
    if reaper.HasExtState(identity.extState.settingsCommand, "Request") then return nil, "Another settings request is pending" end
    local requestId = nextRequestId()
    local responseKey = "Response_" .. requestId
    reaper.DeleteExtState(identity.extState.settings, responseKey, false)
    reaper.SetExtState(identity.extState.settingsCommand, "Request", buildRequest(command, scope, deviceId, changes, requestId), false)
    return requestId
end

function module.Poll(requestId)
    if not reaper or not requestId then return nil end
    local responseKey = "Response_" .. requestId
    if not reaper.HasExtState(identity.extState.settings, responseKey) then return nil end
    local source = reaper.GetExtState(identity.extState.settings, responseKey)
    reaper.DeleteExtState(identity.extState.settings, responseKey, false)
    return module.ParseResponse(source)
end

function module.Cancel(requestId)
    if not reaper or not requestId then return end
    local currentRequest = reaper.GetExtState(identity.extState.settingsCommand, "Request")
    if currentRequest:find("RequestId=" .. tostring(requestId) .. "\n", 1, true) then reaper.DeleteExtState(identity.extState.settingsCommand, "Request", false) end
    reaper.DeleteExtState(identity.extState.settings, "Response_" .. tostring(requestId), false)
end

function module.Query(scope, deviceId)
    return module.Send("Query", scope, deviceId)
end

function module.Apply(scope, changes, deviceId)
    return module.Send("Apply", scope, deviceId, changes)
end

function module.Reload()
    return module.Send("Reload")
end

function module.RunSelfChecks()
    local request = buildRequest("Apply", "Device", "fp2", { HoldDelayMs = { value = 750 }, LongHoldDelayMs = { unset = true } }, "test_1")
    assert(request:find("Set.HoldDelayMs=750", 1, true), "settings request set")
    assert(request:find("Unset.LongHoldDelayMs=1", 1, true), "settings request unset")
    local response = assert(module.ParseResponse("Version=1\nStatus=OK\nScope=Device\nDevice=fp2\nDeviceOption.1=fp2\nValue.HoldDelayMs=750\nSource.HoldDelayMs=Device\nInherited.HoldDelayMs=1000\n"))
    assert(response.ok and response.values.HoldDelayMs == "750", "settings response value")
    assert(response.sources.HoldDelayMs == "Device", "settings response source")
    assert(response.inheritedValues.HoldDelayMs == "1000", "settings response inherited value")
    assert(response.deviceOptions[1] == "fp2", "settings response Device options")
end

return module
