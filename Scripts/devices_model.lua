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

local function propertyValue(value)
    value = tostring(value or "")
    if value:find('["\r\n]') then return nil end
    if value:find("%s") then return '"' .. value .. '"' end
    return value
end

local function appendProperty(parts, name, value)
    local encoded = propertyValue(value)
    if not encoded then return false end
    parts[#parts + 1] = name .. "=" .. encoded
    return true
end

local function appendSettings(parts, settings)
    local names = {}
    for name in pairs(settings or {}) do names[#names + 1] = name end
    table.sort(names)
    for nameIdx, name in ipairs(names) do if not appendProperty(parts, name, settings[name]) then return false end end
    return true
end

local function listenerHas(listener, category)
    for item in (listener.categories or ""):gmatch("[^,]+") do if item:match("^%s*(.-)%s*$") == category then return true end end
    return false
end

local function listenerLine(listener)
    local parts = {}
    if not appendProperty(parts, "Listener", listener.listener) then return nil end
    appendProperty(parts, "GoHome", listenerHas(listener, "GoHome") and "Yes" or "No")
    appendProperty(parts, "SelectedTrackSends", listenerHas(listener, "SelectedTrackSends") and "Yes" or "No")
    appendProperty(parts, "SelectedTrackReceives", listenerHas(listener, "SelectedTrackReceives") and "Yes" or "No")
    appendProperty(parts, "FXMenu", listenerHas(listener, "FXMenu") and "Yes" or "No")
    appendProperty(parts, "Modifiers", listenerHas(listener, "Modifiers") and "Yes" or "No")
    appendProperty(parts, "SelectedTrackFX", listenerHas(listener, "SelectedTrackFX") and "Yes" or "No")
    return "\t" .. table.concat(parts, " ")
end

function module.Clone(value)
    return copy(value)
end

function module.Serialize(data)
    if not data.configVersion or data.configVersion == "" then return nil, "Configuration version is missing" end
    local lines = { "Version=" .. data.configVersion, "" }
    local productSettings = {}
    if not appendSettings(productSettings, data.productSettingOverrides) then return nil, "Settings contain quotes or line breaks" end
    if #productSettings > 0 then
        lines[#lines + 1] = "Settings " .. table.concat(productSettings, " ")
        lines[#lines + 1] = ""
    end
    for midiIdx, device in ipairs(data.midi or {}) do
        local parts = {}
        appendProperty(parts, "SurfaceType", "MIDI")
        if not appendProperty(parts, "SurfaceName", device.name) then return nil, "MIDI name contains unsupported characters" end
        appendProperty(parts, "SurfaceChannelCount", device.channels)
        appendProperty(parts, "MidiInput", device.inputPort)
        appendProperty(parts, "MidiOutput", device.outputPort)
        appendProperty(parts, "MIDISurfaceRefreshRate", device.refreshRate)
        appendProperty(parts, "MaxMIDIMesssagesPerRun", device.maxMessages)
        lines[#lines + 1] = table.concat(parts, " ")
    end
    for oscIdx, device in ipairs(data.osc or {}) do
        local parts = {}
        appendProperty(parts, "SurfaceType", device.type)
        if not appendProperty(parts, "SurfaceName", device.name) then return nil, "OSC name contains unsupported characters" end
        appendProperty(parts, "SurfaceChannelCount", device.channels)
        appendProperty(parts, "ReceiveOnPort", device.receivePort)
        appendProperty(parts, "TransmitToPort", device.transmitPort)
        appendProperty(parts, "TransmitToIPAddress", device.address)
        appendProperty(parts, "MaxPacketsPerRun", device.maxPackets)
        lines[#lines + 1] = table.concat(parts, " ")
    end
    lines[#lines + 1] = ""
    for pageIdx, page in ipairs(data.pages or {}) do
        local pageParts = {}
        if not appendProperty(pageParts, "PageName", page.name) then return nil, "Page name contains unsupported characters" end
        appendProperty(pageParts, "PageFollowsMCP", page.followsMcp and "Yes" or "No")
        appendProperty(pageParts, "SynchPages", page.synchPages and "Yes" or "No")
        appendProperty(pageParts, "ScrollLink", page.scrollLink and "Yes" or "No")
        appendProperty(pageParts, "ScrollSynch", page.scrollSynch and "Yes" or "No")
        lines[#lines + 1] = table.concat(pageParts, " ")
        for surfaceIdx, surface in ipairs(page.surfaces or {}) do
            local parts = {}
            if not appendProperty(parts, "Surface", surface.name) or not appendProperty(parts, "SurfaceFolder", surface.surfaceId) or not appendProperty(parts, "ZoneFolder", surface.mainProfile) or not appendProperty(parts, "FXZoneFolder", surface.fxProfile) then return nil, "Surface assignment contains unsupported characters" end
            appendProperty(parts, "StartChannel", surface.startChannel)
            if not appendSettings(parts, surface.settingOverrides) then return nil, "Surface settings contain quotes or line breaks" end
            lines[#lines + 1] = "\t" .. table.concat(parts, " ")
        end
        if #(page.listeners or {}) > 0 then lines[#lines + 1] = "" end
        local activeBroadcaster = nil
        for listenerIdx, listener in ipairs(page.listeners or {}) do
            if activeBroadcaster ~= listener.broadcaster then
                activeBroadcaster = listener.broadcaster
                local encodedBroadcaster = propertyValue(activeBroadcaster)
                if not encodedBroadcaster then return nil, "Broadcaster contains unsupported characters" end
                lines[#lines + 1] = "\tBroadcaster=" .. encodedBroadcaster
            end
            local serializedListener = listenerLine(listener)
            if not serializedListener then return nil, "Listener contains unsupported characters" end
            lines[#lines + 1] = serializedListener
        end
        lines[#lines + 1] = ""
    end
    return table.concat(lines, "\n") .. "\n"
end

function module.Signature(data)
    local source = module.Serialize(data)
    return source or ""
end

return module
