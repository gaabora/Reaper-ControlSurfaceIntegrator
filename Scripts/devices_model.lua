local schema = require("settings_schema").Load()

local module = {}

local function copy(value, seen)
    if type(value) ~= "table" then return value end
    seen = seen or {}
    if seen[value] then return seen[value] end
    local result = {}
    seen[value] = result
    for key, item in pairs(value) do result[copy(key, seen)] = copy(item, seen) end
    return result
end

local function validId(value)
    value = tostring(value or "")
    return value:match("^[A-Za-z][A-Za-z0-9_]*$") ~= nil
end

local function bareValue(value)
    value = tostring(value or "")
    if value == "" or value:find("//", 1, true) or value:find('[%s"{}%[%]%(%)=+,]') then return nil end
    return value
end

local function zoneSourceMode(value)
    value = tostring(value or "VendorAndUser")
    if value == "Vendor" or value == "VendorAndUser" or value == "User" then return value end
    return nil
end

local function appendSettings(lines, indent, settings)
    local names = {}
    for name in pairs(settings or {}) do names[#names + 1] = name end
    table.sort(names)
    if #names == 0 then return true end
    lines[#lines + 1] = indent .. "Settings {"
    for nameIdx, name in ipairs(names) do
        local value = tostring(settings[name] or "")
        local definition = schema.settingsByName[name]
        if definition and definition.type == "boolean" then
            if value == "1" then value = "true"
            elseif value == "0" then value = "false"
            elseif value ~= "true" and value ~= "false" then return false end
        end
        value = bareValue(value)
        if not value then return false end
        lines[#lines + 1] = indent .. "  " .. name .. "=" .. value
    end
    lines[#lines + 1] = indent .. "}"
    return true
end

local function listenerHas(listener, category)
    for item in (listener.categories or ""):gmatch("[^,]+") do if item:match("^%s*(.-)%s*$") == category then return true end end
    return false
end

local function linkCategories(listener)
    local result = {}
    if listenerHas(listener, "GoHome") then result[#result + 1] = "Home" end
    if listenerHas(listener, "Modifiers") then result[#result + 1] = "Modifiers" end
    if listenerHas(listener, "FXMenu") then result[#result + 1] = "FXMenu" end
    if listenerHas(listener, "SelectedTrackFX") then result[#result + 1] = "SelectedTrackFX" end
    if listenerHas(listener, "SelectedTrackSends") then result[#result + 1] = "SelectedTrackSends" end
    if listenerHas(listener, "SelectedTrackReceives") then result[#result + 1] = "SelectedTrackReceives" end
    return result
end

function module.Clone(value)
    return copy(value)
end

function module.Serialize(data)
    local lines = {}
    if not appendSettings(lines, "", data.productSettingOverrides) then return nil, "Global settings contain unsupported values" end
    if #lines > 0 then lines[#lines + 1] = "" end
    for midiIdx, device in ipairs(data.midi or {}) do
        if not validId(device.name) then return nil, "MIDI Device ID must use letters, digits, and _ and start with a letter" end
        lines[#lines + 1] = "Device " .. device.name .. " {"
        lines[#lines + 1] = "  Type=MIDI"
        lines[#lines + 1] = "  Input=" .. tostring(device.inputPort)
        lines[#lines + 1] = "  Output=" .. tostring(device.outputPort)
        lines[#lines + 1] = "  RefreshRate=" .. tostring(device.refreshRate)
        lines[#lines + 1] = "  MaxMessagesPerRun=" .. tostring(device.maxMessages)
        if next(device.settingOverrides or {}) then lines[#lines + 1] = "" end
        if not appendSettings(lines, "  ", device.settingOverrides) then return nil, "MIDI Device settings contain unsupported values" end
        lines[#lines + 1] = "}"
        lines[#lines + 1] = ""
    end
    for oscIdx, device in ipairs(data.osc or {}) do
        if not validId(device.name) then return nil, "OSC Device ID must use letters, digits, and _ and start with a letter" end
        local address = bareValue(device.address)
        if not address then return nil, "OSC Address must be one unquoted value" end
        lines[#lines + 1] = "Device " .. device.name .. " {"
        lines[#lines + 1] = "  Type=OSC"
        lines[#lines + 1] = "  Protocol=" .. (device.type == "OSCX32" and "X32" or "Generic")
        lines[#lines + 1] = "  ReceivePort=" .. tostring(device.receivePort)
        lines[#lines + 1] = "  TransmitPort=" .. tostring(device.transmitPort)
        lines[#lines + 1] = "  Address=" .. address
        lines[#lines + 1] = "  MaxPacketsPerRun=" .. tostring(device.maxPackets)
        if next(device.settingOverrides or {}) then lines[#lines + 1] = "" end
        if not appendSettings(lines, "  ", device.settingOverrides) then return nil, "OSC Device settings contain unsupported values" end
        lines[#lines + 1] = "}"
        lines[#lines + 1] = ""
    end
    for pageIdx, page in ipairs(data.pages or {}) do
        if not validId(page.name) then return nil, "Page ID must use letters, digits, and _ and start with a letter" end
        lines[#lines + 1] = "Page " .. page.name .. " {"
        lines[#lines + 1] = "  FollowMCP=" .. tostring(page.followsMcp == true)
        lines[#lines + 1] = "  SyncPages=" .. tostring(page.synchPages == true)
        lines[#lines + 1] = "  ScrollLink=" .. tostring(page.scrollLink == true)
        lines[#lines + 1] = "  ScrollSync=" .. tostring(page.scrollSynch == true)
        for surfaceIdx, surface in ipairs(page.surfaces or {}) do
            if not validId(surface.name) or not validId(surface.deviceId) then return nil, "Surface and Device IDs must use letters, digits, and _ and start with a letter" end
            local template = bareValue(surface.surfaceId)
            local mainProfile = bareValue(surface.mainProfile)
            local fxProfile = bareValue(surface.fxProfile)
            if not template or not mainProfile or not fxProfile then return nil, "Surface profile IDs must be non-empty unquoted values" end
            lines[#lines + 1] = ""
            lines[#lines + 1] = "  Surface " .. surface.name .. " {"
            lines[#lines + 1] = "    Device=" .. surface.deviceId
            lines[#lines + 1] = "    Template=" .. template
            local templateSourceMode = zoneSourceMode(surface.surfaceSourceMode or "VendorAndUser")
            if not templateSourceMode or templateSourceMode == "VendorAndUser" then return nil, "Surface template source must be Vendor or User" end
            lines[#lines + 1] = "    TemplateSource=" .. templateSourceMode
            lines[#lines + 1] = "    MainProfile=" .. mainProfile
            local mainSourceMode = zoneSourceMode(surface.mainSourceMode)
            local fxSourceMode = zoneSourceMode(surface.fxSourceMode)
            if not mainSourceMode or not fxSourceMode then return nil, "Zone source must be Vendor only, Vendor + User changes, or User only" end
            lines[#lines + 1] = "    MainSource=" .. mainSourceMode
            lines[#lines + 1] = "    FXProfile=" .. fxProfile
            lines[#lines + 1] = "    FXSource=" .. fxSourceMode
            lines[#lines + 1] = "    StartChannel=" .. tostring(surface.startChannel)
            lines[#lines + 1] = "  }"
        end
        for listenerIdx, listener in ipairs(page.listeners or {}) do
            if not validId(listener.broadcaster) or not validId(listener.listener) then return nil, "Link Surface IDs are invalid" end
            local categories = linkCategories(listener)
            if #categories == 0 then return nil, "Link requires at least one Share category" end
            lines[#lines + 1] = ""
            lines[#lines + 1] = "  Link {"
            lines[#lines + 1] = "    From=" .. listener.broadcaster
            lines[#lines + 1] = "    To=" .. listener.listener
            lines[#lines + 1] = "    Share=[" .. table.concat(categories, ", ") .. "]"
            lines[#lines + 1] = "  }"
        end
        lines[#lines + 1] = "}"
        lines[#lines + 1] = ""
    end
    while lines[#lines] == "" do table.remove(lines) end
    return table.concat(lines, "\n") .. "\n"
end

function module.Signature(data)
    local source = module.Serialize(data)
    return source or ""
end

function module.RunSelfChecks()
    local source, serializationError = module.Serialize({ midi = {}, osc = {}, pages = {}, productSettingOverrides = { ShowLogInReaperConsole = "0", WriteLogFile = "1" } })
    assert(source, serializationError)
    assert(source:find("ShowLogInReaperConsole=false", 1, true), "false Boolean setting serialization")
    assert(source:find("WriteLogFile=true", 1, true), "true Boolean setting serialization")
    local deviceSource, deviceError = module.Serialize({ midi = { { inputPort = 0, maxMessages = 100, name = "fp2", outputPort = 1, refreshRate = 15 } }, osc = {}, pages = { { followsMcp = true, listeners = {}, name = "Home", scrollLink = false, scrollSynch = false, surfaces = { { deviceId = "fp2", fxProfile = "faderportv2", fxSourceMode = "User", mainProfile = "faderportv2", mainSourceMode = "VendorAndUser", name = "fp2", startChannel = 0, surfaceId = "faderportv2", surfaceSourceMode = "Vendor" } }, synchPages = true } } })
    assert(deviceSource, deviceError)
    assert(deviceSource:find("TemplateSource=Vendor", 1, true), "Vendor Surface template source serialization")
    assert(deviceSource:find("MainSource=VendorAndUser", 1, true), "combined Main Zone source serialization")
    assert(deviceSource:find("FXSource=User", 1, true), "User-only FX Zone source serialization")
    local invalidSource = module.Serialize({ midi = { { inputPort = 0, maxMessages = 100, name = "fp2", outputPort = 1, refreshRate = 15 } }, osc = {}, pages = { { followsMcp = true, listeners = {}, name = "Home", scrollLink = false, scrollSynch = false, surfaces = { { deviceId = "fp2", fxProfile = "faderportv2", fxSourceMode = "Automatic", mainProfile = "faderportv2", mainSourceMode = "Vendor", name = "fp2", startChannel = 0, surfaceId = "faderportv2" } }, synchPages = true } } })
    assert(not invalidSource, "invalid Zone source rejection")
end

return module
