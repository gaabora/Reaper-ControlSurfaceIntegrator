local imgui = require "imgui" "0.9.3"

local data = require("osk_data")
local ui = require("ui_components")

local M = {}

local SCAFFOLD_TYPES = {
    { label = "Main zone", value = "main" },
    { label = "Home zone", value = "home" },
    { label = "Go zone", value = "go" },
    { label = "Subzone", value = "subzone" },
    { label = "Included zone", value = "included" },
    { label = "Learn zone", value = "learn" },
    { label = "FX zone", value = "fx" },
}

local NAVIGATORS = {
    { label = "Default (Track)", value = "" },
    { label = "Track", value = "TrackNavigator" },
    { label = "Master track", value = "MasterTrackNavigator" },
    { label = "Selected track", value = "SelectedTrackNavigator" },
    { label = "Focused FX", value = "FocusedFXNavigator" },
    { label = "VCA", value = "VCANavigator" },
    { label = "Folder", value = "FolderNavigator" },
}

local state = {
    alias = "",
    isOpen = false,
    navigator = "",
    path = "",
    pending = false,
    scaffoldType = "main",
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
    end
end

local function sendRequest()
    local errorMessage = validationError()
    if errorMessage then
        state.status = errorMessage
        return
    end
    reaper.DeleteExtState(data.EXT_SECTION, responseKey(), false)
    local payload = table.concat({ state.surfaceName, state.scaffoldType, state.zoneName, state.alias, state.navigator }, "|")
    reaper.SetExtState(data.EXT_CMD_SECTION, "ZoneCreate", payload, false)
    state.path = ""
    state.pending = true
    state.status = "Creating zone file..."
end

function M.Open(surfaceName)
    if not surfaceName or surfaceName == "" then return end
    state.alias = ""
    state.isOpen = true
    state.navigator = ""
    state.path = ""
    state.pending = false
    state.scaffoldType = "main"
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
        imgui.TextWrapped(ctx, "Create one empty zone file in this surface profile. This does not add a link from another zone.")

        local changed
        changed, state.scaffoldType = ui.ComboEnum(ctx, "Type", state.scaffoldType, SCAFFOLD_TYPES)
        changed, state.zoneName = imgui.InputText(ctx, "Name", state.zoneName)
        changed, state.alias = imgui.InputText(ctx, "Alias (optional)", state.alias)
        changed, state.navigator = ui.ComboEnum(ctx, "Navigator", state.navigator, NAVIGATORS)

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
