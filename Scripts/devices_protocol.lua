local identity = require("product_identity")

local module = {}
local requestCounter = 0

local function splitProperties(source)
    local properties = {}
    for rawLine in (tostring(source or "") .. "\n"):gmatch("(.-)\n") do
        local line = rawLine:gsub("\r$", "")
        if line ~= "" then
            local separatorIndex = line:find("=", 1, true)
            if not separatorIndex or separatorIndex == 1 then return nil, "Invalid Devices protocol line: " .. line end
            local key = line:sub(1, separatorIndex - 1)
            if properties[key] ~= nil then return nil, "Duplicate Devices protocol property: " .. key end
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

local function number(properties, key)
    return tonumber(properties[key]) or 0
end

local function boolean(properties, key)
    return properties[key] == "1"
end

local function parseSurface(properties, prefix)
    local settingOverrides = {}
    for settingIdx = 1, number(properties, prefix .. "SettingCount") do settingOverrides[properties[prefix .. "Setting." .. settingIdx .. ".Name"] or ""] = properties[prefix .. "Setting." .. settingIdx .. ".Value"] or "" end
    return {
        active = boolean(properties, prefix .. "Active"),
        fxProfile = properties[prefix .. "FxProfile"] or "",
        fxSource = properties[prefix .. "FxSource"] or "Missing",
        ioActive = boolean(properties, prefix .. "IoActive"),
        ioType = properties[prefix .. "IoType"] or "Missing",
        line = number(properties, prefix .. "Line"),
        mainProfile = properties[prefix .. "MainProfile"] or "",
        mainSource = properties[prefix .. "MainSource"] or "Missing",
        name = properties[prefix .. "Name"] or "",
        startChannel = number(properties, prefix .. "StartChannel"),
        surfaceId = properties[prefix .. "SurfaceId"] or "",
        templateSource = properties[prefix .. "TemplateSource"] or "Missing",
        settingOverrides = settingOverrides,
        useDifferentFx = (properties[prefix .. "FxProfile"] or "") ~= (properties[prefix .. "MainProfile"] or ""),
    }
end

function module.ParseResponse(source)
    local properties, parseError = splitProperties(source)
    if not properties then return nil, parseError end
    if properties.Version ~= "1" then return nil, "Devices response Version must be 1" end
    if properties.Status == "ERROR" then return nil, properties.Message or "Devices query failed" end
    if properties.Status ~= "OK" then return nil, "Devices response Status must be OK or ERROR" end
    local response = { configVersion = properties.ConfigVersion or "", currentPage = properties.CurrentPage or "", editorAvailable = boolean(properties, "EditorAvailable"), fatalError = properties.FatalError or "", issues = {}, message = properties.Message or "", midi = {}, midiInputOptions = {}, midiOutputOptions = {}, osc = {}, pages = {}, productSettingOverrides = {}, profileOptions = {}, revision = properties.Revision or "", skippedSurfaceCount = number(properties, "SkippedSurfaceCount"), surfaceOptions = {} }
    for settingIdx = 1, number(properties, "Product.SettingCount") do response.productSettingOverrides[properties["Product.Setting." .. settingIdx .. ".Name"] or ""] = properties["Product.Setting." .. settingIdx .. ".Value"] or "" end
    for midiIdx = 1, number(properties, "MidiCount") do
        local prefix = "Midi." .. midiIdx .. "."
        response.midi[#response.midi + 1] = { active = boolean(properties, prefix .. "Active"), channels = number(properties, prefix .. "Channels"), inputName = properties[prefix .. "InputName"] or "", inputPort = number(properties, prefix .. "InputPort"), line = number(properties, prefix .. "Line"), maxMessages = number(properties, prefix .. "MaxMessages"), name = properties[prefix .. "Name"] or "", outputName = properties[prefix .. "OutputName"] or "", outputPort = number(properties, prefix .. "OutputPort"), refreshRate = number(properties, prefix .. "RefreshRate"), runtimeIssue = properties[prefix .. "RuntimeIssue"] or "" }
    end
    for oscIdx = 1, number(properties, "OscCount") do
        local prefix = "Osc." .. oscIdx .. "."
        response.osc[#response.osc + 1] = { active = boolean(properties, prefix .. "Active"), address = properties[prefix .. "Address"] or "", channels = number(properties, prefix .. "Channels"), line = number(properties, prefix .. "Line"), maxPackets = number(properties, prefix .. "MaxPackets"), name = properties[prefix .. "Name"] or "", receivePort = properties[prefix .. "ReceivePort"] or "", runtimeIssue = properties[prefix .. "RuntimeIssue"] or "", transmitPort = properties[prefix .. "TransmitPort"] or "", type = properties[prefix .. "Type"] or "" }
    end
    for pageIdx = 1, number(properties, "PageCount") do
        local prefix = "Page." .. pageIdx .. "."
        local page = { active = boolean(properties, prefix .. "Active"), current = boolean(properties, prefix .. "Current"), followsMcp = boolean(properties, prefix .. "FollowsMcp"), line = number(properties, prefix .. "Line"), listeners = {}, name = properties[prefix .. "Name"] or "", scrollLink = boolean(properties, prefix .. "ScrollLink"), scrollSynch = boolean(properties, prefix .. "ScrollSynch"), surfaces = {}, synchPages = boolean(properties, prefix .. "SynchPages") }
        for surfaceIdx = 1, number(properties, prefix .. "SurfaceCount") do page.surfaces[#page.surfaces + 1] = parseSurface(properties, prefix .. "Surface." .. surfaceIdx .. ".") end
        for listenerIdx = 1, number(properties, prefix .. "ListenerCount") do
            local listenerPrefix = prefix .. "Listener." .. listenerIdx .. "."
            page.listeners[#page.listeners + 1] = { active = boolean(properties, listenerPrefix .. "Active"), broadcaster = properties[listenerPrefix .. "Broadcaster"] or "", categories = properties[listenerPrefix .. "Categories"] or "", line = number(properties, listenerPrefix .. "Line"), listener = properties[listenerPrefix .. "Listener"] or "" }
        end
        response.pages[#response.pages + 1] = page
    end
    for issueIdx = 1, number(properties, "IssueCount") do
        local prefix = "Issue." .. issueIdx .. "."
        response.issues[#response.issues + 1] = { kind = properties[prefix .. "Kind"] or "Parser", line = number(properties, prefix .. "Line"), message = properties[prefix .. "Message"] or "" }
    end
    for inputIdx = 1, number(properties, "MidiInputOptionCount") do
        local prefix = "MidiInputOption." .. inputIdx .. "."
        response.midiInputOptions[#response.midiInputOptions + 1] = { name = properties[prefix .. "Name"] or "Unavailable", port = number(properties, prefix .. "Port") }
    end
    for outputIdx = 1, number(properties, "MidiOutputOptionCount") do
        local prefix = "MidiOutputOption." .. outputIdx .. "."
        response.midiOutputOptions[#response.midiOutputOptions + 1] = { name = properties[prefix .. "Name"] or "Unavailable", port = number(properties, prefix .. "Port") }
    end
    for surfaceIdx = 1, number(properties, "SurfaceOptionCount") do
        local prefix = "SurfaceOption." .. surfaceIdx .. "."
        response.surfaceOptions[#response.surfaceOptions + 1] = { id = properties[prefix .. "Id"] or "", source = properties[prefix .. "Source"] or "Missing" }
    end
    for profileIdx = 1, number(properties, "ProfileOptionCount") do
        local prefix = "ProfileOption." .. profileIdx .. "."
        response.profileOptions[#response.profileOptions + 1] = { fxSource = properties[prefix .. "FxSource"] or "Missing", id = properties[prefix .. "Id"] or "", mainSource = properties[prefix .. "MainSource"] or "Missing", userMain = boolean(properties, prefix .. "UserMain"), vendorMain = boolean(properties, prefix .. "VendorMain") }
    end
    return response
end

local function send(command, profileId, revision, source)
    if not reaper then return nil, "REAPER API is not available" end
    if reaper.HasExtState(identity.extState.devicesCommand, "Request") then return nil, "Another Devices request is pending" end
    local requestId = nextRequestId()
    reaper.DeleteExtState(identity.extState.devices, "Response_" .. requestId, false)
    local request = "Version=1\nRequestId=" .. requestId .. "\nCommand=" .. command .. "\n"
    if profileId and profileId ~= "" then request = request .. "Profile=" .. profileId .. "\n" end
    if revision and revision ~= "" then request = request .. "ExpectedRevision=" .. revision .. "\n" end
    if source then
        local lines = {}
        for line in (source .. "\n"):gmatch("(.-)\n") do lines[#lines + 1] = line:gsub("\r$", "") end
        if lines[#lines] == "" then table.remove(lines) end
        request = request .. "ConfigLineCount=" .. #lines .. "\n"
        for lineIdx, line in ipairs(lines) do request = request .. "ConfigLine." .. lineIdx .. "=" .. line .. "\n" end
    end
    reaper.SetExtState(identity.extState.devicesCommand, "Request", request, false)
    return requestId
end

function module.Query()
    return send("Query")
end

function module.CreateProfile(profileId)
    return send("CreateProfile", profileId)
end

function module.CopyProfile(profileId)
    return send("CopyProfile", profileId)
end

function module.OpenEditor()
    return send("OpenEditor")
end

function module.Validate(revision, source)
    return send("Validate", nil, revision, source)
end

function module.Apply(revision, source)
    return send("Apply", nil, revision, source)
end

function module.Poll(requestId)
    if not reaper or not requestId then return nil end
    local responseKey = "Response_" .. requestId
    if not reaper.HasExtState(identity.extState.devices, responseKey) then return nil end
    local source = reaper.GetExtState(identity.extState.devices, responseKey)
    reaper.DeleteExtState(identity.extState.devices, responseKey, false)
    return module.ParseResponse(source)
end

function module.Cancel(requestId)
    if not reaper or not requestId then return end
    local currentRequest = reaper.GetExtState(identity.extState.devicesCommand, "Request")
    if currentRequest:find("RequestId=" .. tostring(requestId) .. "\n", 1, true) then reaper.DeleteExtState(identity.extState.devicesCommand, "Request", false) end
    reaper.DeleteExtState(identity.extState.devices, "Response_" .. tostring(requestId), false)
end

return module
