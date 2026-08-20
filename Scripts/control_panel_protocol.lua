local identity = require("product_identity")

local module = {}

local VALID_COMMANDS = {
    Focus = true,
    Open = true,
    SelectTab = true,
}

local function parseProperties(source)
    local properties = {}
    for rawLine in (tostring(source or "") .. "\n"):gmatch("(.-)\n") do
        local line = rawLine:gsub("\r$", "")
        if line ~= "" then
            local separatorIndex = line:find("=", 1, true)
            if not separatorIndex or separatorIndex == 1 then return nil, "Invalid Control Panel protocol line: " .. line end
            local key = line:sub(1, separatorIndex - 1)
            if properties[key] ~= nil then return nil, "Duplicate Control Panel protocol property: " .. key end
            properties[key] = line:sub(separatorIndex + 1)
        end
    end
    return properties
end

local function refreshStableAction()
    if not reaper or not reaper.NamedCommandLookup or not reaper.RefreshToolbar2 then return end
    local commandId = reaper.NamedCommandLookup("_" .. identity.controlPanelActionId)
    if commandId and commandId > 0 then reaper.RefreshToolbar2(0, commandId) end
end

function module.SetOpen(isOpen)
    if not reaper then return end
    if not isOpen then reaper.DeleteExtState(identity.extState.controlPanel, "Request", false) end
    reaper.SetExtState(identity.extState.controlPanel, "State", isOpen and "Open" or "Closed", false)
    refreshStableAction()
end

function module.Poll()
    if not reaper or not reaper.HasExtState(identity.extState.controlPanel, "Request") then return nil end
    local source = reaper.GetExtState(identity.extState.controlPanel, "Request")
    reaper.DeleteExtState(identity.extState.controlPanel, "Request", false)
    local properties, parseError = parseProperties(source)
    if not properties then return nil, parseError end
    if properties.Version ~= "1" then return nil, "Control Panel request Version must be 1" end
    if not properties.RequestId or properties.RequestId == "" then return nil, "Control Panel request has no RequestId" end
    if not VALID_COMMANDS[properties.Command] then return nil, "Unsupported Control Panel command: " .. tostring(properties.Command) end
    return {
        command = properties.Command,
        requestId = properties.RequestId,
        tab = properties.Tab or "",
    }
end

return module
