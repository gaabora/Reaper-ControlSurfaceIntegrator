local imgui = require "imgui" "0.9.3"

local data = require("osk_data")
local ui = require("ui_components")

local M = {}

local DOCUMENT_TYPES = {
    { label = "Main zone", value = "main" },
    { label = "FX zone", value = "fx" },
}

local MAIN_PURPOSES = {
    { label = "Channel tracks", value = "tracks" },
    { label = "Home and global controls", value = "home" },
    { label = "Selected track", value = "selected-track" },
    { label = "Master track", value = "master-track" },
    { label = "Focused FX", value = "focused-fx" },
    { label = "Sends for channel tracks", value = "tracks-sends" },
    { label = "Receives for channel tracks", value = "tracks-receives" },
    { label = "FX for channel tracks", value = "tracks-fx" },
    { label = "Selected-track sends", value = "selected-track-sends" },
    { label = "Selected-track receives", value = "selected-track-receives" },
    { label = "Selected-track FX", value = "selected-track-fx" },
    { label = "Master-track FX", value = "master-track-fx" },
    { label = "VCA tracks", value = "vca" },
    { label = "Folder tracks", value = "folder" },
    { label = "Selected-track collection", value = "selected-tracks" },
    { label = "Reusable layer", value = "layer" },
    { label = "Last-touched FX parameter", value = "last-touched-fx-param" },
}

local state = {
    alias = "",
    documentType = "main",
    isOpen = false,
    matchFx = "",
    path = "",
    pending = false,
    purpose = "tracks",
    status = "",
    surfaceName = "",
    zoneName = "",
}

local WINDOW_FLAGS = imgui.WindowFlags_NoCollapse
if imgui.WindowFlags_NoDocking then WINDOW_FLAGS = WINDOW_FLAGS | imgui.WindowFlags_NoDocking end

local function responseKey()
    return "ZoneCreateStatus_" .. state.surfaceName
end

local function validationError()
    if not state.zoneName:match("^[A-Za-z0-9][A-Za-z0-9_-]*$") then return "Name must start with a letter or digit and use only letters, digits, _ or -." end
    if #state.zoneName > 128 then return "Name is too long." end
    if #state.alias > 200 then return "Alias is too long." end
    if state.alias:find("[\r\n\"|]") then return "Alias cannot contain quotes, |, or line breaks." end
    if state.documentType == "fx" then
        if state.matchFx == "" then return "Enter the plugin name that this FX zone matches." end
        if #state.matchFx > 300 then return "Plugin name is too long." end
        if state.matchFx:find("[\r\n\"|]") then return "Plugin name cannot contain quotes, |, or line breaks." end
    end
    return nil
end

local function pollResponse()
    if not state.pending or not reaper.HasExtState(data.EXT_SECTION, responseKey()) then return end
    local response = reaper.GetExtState(data.EXT_SECTION, responseKey())
    reaper.DeleteExtState(data.EXT_SECTION, responseKey(), false)
    local outcome, path, message = response:match("^([^|]*)|([^|]*)|(.*)$")
    state.pending = false
    if not outcome then
        state.status = "Invalid response from the extension."
        return
    end
    state.path = path or ""
    state.status = message or ""
    if outcome == "OK" then
        state.zoneName = ""
        state.alias = ""
        state.matchFx = ""
    end
end

local function sendRequest()
    local errorMessage = validationError()
    if errorMessage then
        state.status = errorMessage
        return
    end
    reaper.DeleteExtState(data.EXT_SECTION, responseKey(), false)
    local payload = table.concat({ state.surfaceName, state.documentType, state.zoneName, state.alias, state.purpose, state.matchFx }, "|")
    reaper.SetExtState(data.EXT_CMD_SECTION, "ZoneCreate", payload, false)
    state.path = ""
    state.pending = true
    state.status = "Creating zone file..."
end

function M.Open(surfaceName)
    if not surfaceName or surfaceName == "" then return end
    state.alias = ""
    state.documentType = "main"
    state.isOpen = true
    state.matchFx = ""
    state.path = ""
    state.pending = false
    state.purpose = "tracks"
    state.status = ""
    state.surfaceName = surfaceName
    state.zoneName = ""
    reaper.DeleteExtState(data.EXT_SECTION, responseKey(), false)
end

function M.IsOpen()
    return state.isOpen
end

function M.Render(ctx, font)
    if not state.isOpen then return end
    pollResponse()

    imgui.SetNextWindowSize(ctx, 520, 0, imgui.Cond_Appearing)
    if font then imgui.PushFont(ctx, font) end
    local visible, open = imgui.Begin(ctx, "Create zone file @" .. state.surfaceName .. "###osk_zone_create", true, WINDOW_FLAGS)
    if visible then
        imgui.TextWrapped(ctx, "Create one empty format 2 zone in the User profile. This does not add the zone to another zone.")

        local changed
        changed, state.documentType = ui.ComboEnum(ctx, "Type", state.documentType, DOCUMENT_TYPES)
        changed, state.zoneName = imgui.InputText(ctx, "Name", state.zoneName)
        changed, state.alias = imgui.InputText(ctx, "Alias (optional)", state.alias)
        if state.documentType == "main" then
            changed, state.purpose = ui.ComboEnum(ctx, "What it controls", state.purpose, MAIN_PURPOSES)
        else
            changed, state.matchFx = imgui.InputText(ctx, "Plugin name to match", state.matchFx)
        end

        local errorMessage = validationError()
        if errorMessage and state.zoneName ~= "" then imgui.TextWrapped(ctx, errorMessage) end
        local createClicked = ui.Disabled(ctx, state.pending or errorMessage ~= nil, function() return imgui.Button(ctx, state.pending and "Creating..." or "Create zone file") end)
        if createClicked then sendRequest() end
        imgui.SameLine(ctx)
        if imgui.Button(ctx, "Close") then open = false end

        if state.status ~= "" then imgui.TextWrapped(ctx, state.status) end
        if state.path ~= "" then imgui.TextWrapped(ctx, state.path) end
    end
    imgui.End(ctx)
    if font then imgui.PopFont(ctx) end
    state.isOpen = open
end

return M
