local imgui = require "imgui" "0.9.3"

local identity = require("product_identity")
local protocol = require("devices_protocol")
local model = require("devices_model")
local theme = require("theme_settings")
local ui = require("ui_components")

local module = {}
local state = { afterQueryKind = "", confirmSource = nil, data = nil, deleteIo = nil, editIo = nil, editPage = nil, editSurface = nil, error = "", initialized = false, listenerIndex = 1, pageIndex = 1, pendingDraft = nil, pendingKind = "", requestId = nil, requestStarted = 0, savedData = nil, savedSignature = "", status = "", surfaceIndex = 1 }

local function sectionHeader(ctx, label, fonts)
    if fonts and fonts.section then imgui.PushFont(ctx, fonts.section) end
    imgui.Text(ctx, label)
    if fonts and fonts.section then imgui.PopFont(ctx) end
end

local function startRequest(kind, source)
    if state.requestId then return end
    local requestId, queryError
    if kind == "OpenEditor" then requestId, queryError = protocol.OpenEditor()
    elseif kind == "Apply" then requestId, queryError = protocol.Apply(state.data.revision, source)
    else requestId, queryError = protocol.Query() end
    if not requestId then
        state.error = tostring(queryError or "Cannot query Devices")
        return
    end
    state.requestId = requestId
    state.pendingKind = kind
    state.requestStarted = reaper.time_precise()
    state.error = ""
end


local function startQuery()
    startRequest("Query")
end

local function inactiveRuntimeCount(data)
    local inactiveCount = 0
    for midiIdx, device in ipairs(data.midi) do if not device.active or device.inputName == "" or device.outputName == "" then inactiveCount = inactiveCount + 1 end end
    for oscIdx, device in ipairs(data.osc) do if not device.active then inactiveCount = inactiveCount + 1 end end
    for pageIdx, page in ipairs(data.pages) do
        if not page.active then inactiveCount = inactiveCount + 1 end
        for surfaceIdx, surface in ipairs(page.surfaces) do if not surface.active then inactiveCount = inactiveCount + 1 end end
        for listenerIdx, listener in ipairs(page.listeners) do if not listener.active then inactiveCount = inactiveCount + 1 end end
    end
    return inactiveCount
end

local STATUS_ACTIVE_COLOR = 0x40c060ff
local STATUS_INACTIVE_COLOR = 0xff5050ff
local EDITOR_BUTTON_COLORS = { active = 0xb85c10ff, button = 0xd97718ff, hovered = 0xf08a27ff }
local LIST_CHILD_HEIGHT = 260
local LISTENER_LIST_WIDTH = 210
local PAGE_LIST_WIDTH = 210

local function newPage(name)
    return { active = false, current = false, followsMcp = true, listeners = {}, name = name, scrollLink = false, scrollSynch = false, surfaces = {}, synchPages = true }
end

local function ensureHomePage(data)
    if not data then return end
    for pageIdx, page in ipairs(data.pages) do
        if page.name:lower() == "home" then
            page.name = "Home"
            return
        end
    end
    table.insert(data.pages, 1, newPage("Home"))
    state.pageIndex = 1
    state.surfaceIndex = 1
    state.listenerIndex = 1
end

local function clamp(value, count)
    if count < 1 then return 1 end
    return math.max(1, math.min(value or 1, count))
end

local function textField(ctx, id, value)
    ui.SetNextFormControlWidth(ctx)
    return imgui.InputText(ctx, "##" .. id, tostring(value or ""))
end

local function integerField(ctx, id, value, minimum, maximum)
    return ui.DragInteger(ctx, "##" .. id, value, minimum, maximum, 1)
end

local function fieldRow(ctx, label, renderControl)
    imgui.TableNextRow(ctx)
    imgui.TableSetColumnIndex(ctx, 0)
    imgui.AlignTextToFramePadding(ctx)
    imgui.Text(ctx, label)
    imgui.TableSetColumnIndex(ctx, 1)
    return renderControl()
end

local function beginForm(ctx, id)
    if not imgui.BeginTable(ctx, id, 2, 0, -1, 0) then return false end
    imgui.TableSetupColumn(ctx, "Label", imgui.TableColumnFlags_WidthStretch)
    imgui.TableSetupColumn(ctx, "Value", imgui.TableColumnFlags_WidthFixed, 180)
    return true
end

local function localError(ctx, message)
    if message and message ~= "" then imgui.TextWrapped(ctx, "Error: " .. message) end
end

local function duplicateIoName(data, name, kind, selectedIdx)
    if name == "" then return "Name is required" end
    if not name:match("^[A-Za-z][A-Za-z0-9_]*$") then return "I/O ID must start with a letter and use only letters, digits, and _" end
    for midiIdx, device in ipairs(data.midi) do if device.name == name and (kind ~= "MIDI" or midiIdx ~= selectedIdx) then return "I/O name must be unique" end end
    for oscIdx, device in ipairs(data.osc) do if device.name == name and (kind ~= "OSC" or oscIdx ~= selectedIdx) then return "I/O name must be unique" end end
    return ""
end

local function ioDefinitionError(data, kind, deviceIdx, device)
    local nameError = duplicateIoName(data, device.name, kind, deviceIdx)
    if nameError ~= "" then return nameError end
    if kind == "MIDI" and (device.inputPort < 0 or device.outputPort < 0) then return "Select input and output ports" end
    if kind == "OSC" and (device.receivePort == "" or device.transmitPort == "" or device.address == "") then return "Complete all OSC connection fields" end
    return ""
end

local function uniqueIoName(data, baseName)
    local names = {}
    for midiIdx, device in ipairs(data.midi) do names[device.name] = true end
    for oscIdx, device in ipairs(data.osc) do names[device.name] = true end
    if not names[baseName] then return baseName end
    local suffix = 2
    while names[baseName .. "_" .. suffix] do suffix = suffix + 1 end
    return baseName .. "_" .. suffix
end

local function uniquePageName(data, baseName)
    local names = {}
    for pageIdx, page in ipairs(data.pages) do names[page.name] = true end
    if not names[baseName] then return baseName end
    local suffix = 2
    while names[baseName .. "_" .. suffix] do suffix = suffix + 1 end
    return baseName .. "_" .. suffix
end

local function renameIo(data, oldName, newName)
    if oldName == newName then return end
    for pageIdx, page in ipairs(data.pages) do
        for surfaceIdx, surface in ipairs(page.surfaces) do if surface.deviceId == oldName then surface.deviceId = newName end end
    end
end

local function midiPortItems(options, currentPort)
    local items = { { label = "Not selected", value = -1 } }
    local found = false
    for optionIdx, option in ipairs(options) do
        items[#items + 1] = { label = option.name .. " (#" .. option.port .. ")", value = option.port }
        if option.port == currentPort then found = true end
    end
    if currentPort >= 0 and not found then table.insert(items, 2, { label = "Unavailable (#" .. currentPort .. ")", value = currentPort }) end
    return items
end

local function midiPortName(options, currentPort)
    for optionIdx, option in ipairs(options) do if option.port == currentPort then return option.name end end
    return ""
end

local function renderRuntime(ctx, active, issue, details)
    imgui.Spacing(ctx)
    imgui.PushStyleColor(ctx, imgui.Col_Text, active and STATUS_ACTIVE_COLOR or STATUS_INACTIVE_COLOR)
    imgui.Text(ctx, active and "● Active" or "× Inactive")
    imgui.PopStyleColor(ctx)
    imgui.SameLine(ctx)
    imgui.TextDisabled(ctx, details and details ~= "" and details or (active and "CSI opened the saved I/O. Save draft changes to reconnect it." or "Check the selected ports or connection, then Save changes to reconnect CSI."))
    if issue and issue ~= "" then imgui.TextWrapped(ctx, "Runtime issue: " .. issue) end
end

local function renderListStatus(ctx, active)
    imgui.PushStyleColor(ctx, imgui.Col_Text, active and STATUS_ACTIVE_COLOR or STATUS_INACTIVE_COLOR)
    imgui.Text(ctx, active and "●" or "×")
    imgui.PopStyleColor(ctx)
end

local function beginRoundedList(ctx, id, height)
    imgui.PushStyleVar(ctx, imgui.StyleVar_ChildRounding, theme.common.rounding)
    return imgui.BeginChild(ctx, id, -1, height, imgui.ChildFlags_Border, 0)
end

local function endRoundedList(ctx)
    imgui.EndChild(ctx)
    imgui.PopStyleVar(ctx)
end

local function removeIoAndReferences(data, kind, deviceIdx)
    local devices = kind == "MIDI" and data.midi or data.osc
    local device = devices[deviceIdx]
    if not device then return end
    for pageIdx, page in ipairs(data.pages) do
        for surfaceIdx = #page.surfaces, 1, -1 do
            if page.surfaces[surfaceIdx].deviceId == device.name then
                local removedSurfaceName = page.surfaces[surfaceIdx].name
                table.remove(page.surfaces, surfaceIdx)
                for listenerIdx = #page.listeners, 1, -1 do
                    if page.listeners[listenerIdx].broadcaster == removedSurfaceName or page.listeners[listenerIdx].listener == removedSurfaceName then table.remove(page.listeners, listenerIdx) end
                end
            end
        end
    end
    table.remove(devices, deviceIdx)
    state.editIo = nil
    state.surfaceIndex = 1
    state.listenerIndex = 1
end

local function requestIoRemoval(data, kind, deviceIdx)
    local devices = kind == "MIDI" and data.midi or data.osc
    local device = devices[deviceIdx]
    if not device then return end
    local assignments = {}
    for pageIdx, page in ipairs(data.pages) do for surfaceIdx, surface in ipairs(page.surfaces) do if surface.deviceId == device.name then assignments[#assignments + 1] = page.name .. " / " .. surface.name end end end
    if #assignments == 0 then
        removeIoAndReferences(data, kind, deviceIdx)
        return
    end
    state.deleteIo = { assignments = assignments, index = deviceIdx, kind = kind, name = device.name }
end

local function addIoDefinition(data, kind)
    if kind == "MIDI" then
        data.midi[#data.midi + 1] = { active = false, channels = 8, inputName = "", inputPort = -1, maxMessages = 200, name = uniqueIoName(data, "MIDI"), outputName = "", outputPort = -1, refreshRate = 15, runtimeIssue = "", settingOverrides = {} }
        state.editIo = { creating = true, index = #data.midi, kind = "MIDI" }
    else
        data.osc[#data.osc + 1] = { active = false, address = "127.0.0.1", channels = 8, maxPackets = 0, name = uniqueIoName(data, "OSC"), receivePort = "8000", runtimeIssue = "", settingOverrides = {}, transmitPort = "9000", type = "OSC" }
        state.editIo = { creating = true, index = #data.osc, kind = "OSC" }
    end
end

local function findIo(data, name)
    for deviceIdx, device in ipairs(data.midi) do if device.name == name then return "MIDI", deviceIdx, device end end
    for deviceIdx, device in ipairs(data.osc) do if device.name == name then return "OSC", deviceIdx, device end end
end

local function renderMidiEditor(ctx, data, device, deviceIdx, fonts)
    sectionHeader(ctx, "MIDI device", fonts)
    if beginForm(ctx, "##MidiMasterForm") then
        fieldRow(ctx, "Name", function() local changed, value = textField(ctx, "MidiName", device.name) if changed then renameIo(data, device.name, value) device.name = value end end)
        fieldRow(ctx, "Channels", function() local changed; changed, device.channels = integerField(ctx, "MidiChannels", device.channels, 1, 1024) end)
        fieldRow(ctx, "Input port", function()
            local changed
            changed, device.inputPort = ui.ComboEnum(ctx, "##MidiInput", device.inputPort, midiPortItems(data.midiInputOptions, device.inputPort))
            if changed then device.inputName = midiPortName(data.midiInputOptions, device.inputPort) end
        end)
        fieldRow(ctx, "Output port", function()
            local changed
            changed, device.outputPort = ui.ComboEnum(ctx, "##MidiOutput", device.outputPort, midiPortItems(data.midiOutputOptions, device.outputPort))
            if changed then device.outputName = midiPortName(data.midiOutputOptions, device.outputPort) end
        end)
        fieldRow(ctx, "Refresh rate", function() local changed; changed, device.refreshRate = integerField(ctx, "MidiRefresh", device.refreshRate, 1, 60000) end)
        fieldRow(ctx, "Maximum messages", function() local changed; changed, device.maxMessages = integerField(ctx, "MidiMessages", device.maxMessages, 0, 1000000) end)
        imgui.EndTable(ctx)
    end
    localError(ctx, ioDefinitionError(data, "MIDI", deviceIdx, device))
    if device.runtimeIssue and device.runtimeIssue ~= "" then localError(ctx, device.runtimeIssue) end
end

local function renderOscEditor(ctx, data, device, deviceIdx, fonts)
    sectionHeader(ctx, "OSC device", fonts)
    local typeItems = { { label = "OSC", value = "OSC" }, { label = "OSCX32", value = "OSCX32" } }
    if beginForm(ctx, "##OscMasterForm") then
        fieldRow(ctx, "Name", function() local changed, value = textField(ctx, "OscName", device.name) if changed then renameIo(data, device.name, value) device.name = value end end)
        fieldRow(ctx, "Type", function() local changed; changed, device.type = ui.ComboEnum(ctx, "##OscType", device.type, typeItems) end)
        fieldRow(ctx, "Channels", function() local changed; changed, device.channels = integerField(ctx, "OscChannels", device.channels, 1, 1024) end)
        fieldRow(ctx, "Receive port", function() local changed; changed, device.receivePort = textField(ctx, "OscReceive", device.receivePort) end)
        fieldRow(ctx, "Transmit address", function() local changed; changed, device.address = textField(ctx, "OscAddress", device.address) end)
        fieldRow(ctx, "Transmit port", function() local changed; changed, device.transmitPort = textField(ctx, "OscTransmit", device.transmitPort) end)
        fieldRow(ctx, "Maximum packets", function() local changed; changed, device.maxPackets = integerField(ctx, "OscPackets", device.maxPackets, 0, 1000000) end)
        imgui.EndTable(ctx)
    end
    localError(ctx, ioDefinitionError(data, "OSC", deviceIdx, device))
    if device.runtimeIssue and device.runtimeIssue ~= "" then localError(ctx, device.runtimeIssue) end
end

local function enumItems(values)
    local items = {}
    for valueIdx, value in ipairs(values) do items[#items + 1] = { label = value, value = value } end
    return items
end

local function ioItems(data)
    local names = {}
    for midiIdx, device in ipairs(data.midi) do names[#names + 1] = device.name end
    for oscIdx, device in ipairs(data.osc) do names[#names + 1] = device.name end
    return enumItems(names)
end

local function assignmentIoItems(data, page, currentSurface)
    local assigned = {}
    for surfaceIdx, surface in ipairs(page.surfaces) do if surface ~= currentSurface then assigned[surface.deviceId] = true end end
    local items = {}
    for deviceIdx, device in ipairs(data.midi) do if not assigned[device.name] then items[#items + 1] = { label = (device.active and "● " or "× ") .. device.name .. " (MIDI)", value = device.name } end end
    for deviceIdx, device in ipairs(data.osc) do if not assigned[device.name] then items[#items + 1] = { label = (device.active and "● " or "× ") .. device.name .. " (OSC)", value = device.name } end end
    items[#items + 1] = { label = "+ Add MIDI…", value = "__ADD_MIDI__" }
    items[#items + 1] = { label = "+ Add OSC…", value = "__ADD_OSC__" }
    return items
end

local function availableIoItems(data, page)
    local assigned = {}
    for surfaceIdx, surface in ipairs(page.surfaces) do assigned[surface.deviceId] = true end
    local items = {}
    for itemIdx, item in ipairs(ioItems(data)) do if not assigned[item.value] then items[#items + 1] = item end end
    return items
end

local function templateItems(data)
    local items = {}
    for templateIdx, template in ipairs(data.surfaceOptions) do items[#items + 1] = { label = template.id .. " - " .. template.source, value = template.id } end
    return items
end

local function profileItems(data)
    local items = { { label = "Select a Zone profile", value = "" } }
    for profileIdx, profile in ipairs(data.profileOptions) do items[#items + 1] = { label = profile.id, value = profile.id } end
    return items
end

local function findTemplate(data, surfaceId)
    for templateIdx, template in ipairs(data.surfaceOptions) do if template.id == surfaceId then return template end end
end

local function findProfile(data, profileId)
    for profileIdx, profile in ipairs(data.profileOptions) do if profile.id == profileId then return profile end end
end

local function defaultProfileId(data, surfaceId)
    return findProfile(data, surfaceId) and surfaceId or ""
end

local function uniqueSurfaceName(page, baseName)
    local names = {}
    for surfaceIdx, surface in ipairs(page.surfaces) do names[surface.name] = true end
    if not names[baseName] then return baseName end
    local suffix = 2
    while names[baseName .. "_" .. suffix] do suffix = suffix + 1 end
    return baseName .. "_" .. suffix
end

local function newSurface(data, page)
    local ioDefinitions = availableIoItems(data, page)
    local templates = templateItems(data)
    local deviceId = ioDefinitions[1] and ioDefinitions[1].value or ""
    local surfaceId = templates[1] and templates[1].value or ""
    local profileId = defaultProfileId(data, surfaceId)
    local template = findTemplate(data, surfaceId)
    local profile = findProfile(data, profileId)
    return { active = false, deviceId = deviceId, fxProfile = profileId, fxSource = profile and profile.fxSource or "Missing", ioActive = false, ioType = "", mainProfile = profileId, mainSource = profile and profile.mainSource or "Missing", name = uniqueSurfaceName(page, deviceId ~= "" and deviceId or "Surface"), startChannel = 0, surfaceId = surfaceId, templateSource = template and template.source or "Missing", useDifferentFx = false }
end

local function removePage(data, pageIdx)
    if #data.pages <= 1 then return end
    table.remove(data.pages, pageIdx)
    if state.pageIndex > pageIdx then state.pageIndex = state.pageIndex - 1 else state.pageIndex = clamp(state.pageIndex, #data.pages) end
    state.surfaceIndex = 1
    state.listenerIndex = 1
end

local function renderPageList(ctx, data, fonts)
    sectionHeader(ctx, "Pages", fonts)
    local visible = beginRoundedList(ctx, "##PagesList", LIST_CHILD_HEIGHT)
    if visible then
        if imgui.Button(ctx, "Add Page", 100, 0) then data.pages[#data.pages + 1] = newPage(uniquePageName(data, "Page")) state.pageIndex = #data.pages state.surfaceIndex = 1 state.editPage = state.pageIndex end
        imgui.Spacing(ctx)
        if imgui.BeginTable(ctx, "##PageRows", 4, 0, -1, 0) then
            local actionSize = theme.NOTIFICATIONS.close_button_size
            imgui.TableSetupColumn(ctx, "Status", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Name", imgui.TableColumnFlags_WidthStretch)
            imgui.TableSetupColumn(ctx, "Edit", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Remove", imgui.TableColumnFlags_WidthFixed, actionSize)
            for pageIdx, page in ipairs(data.pages) do
                imgui.TableNextRow(ctx)
                imgui.TableSetColumnIndex(ctx, 0) renderListStatus(ctx, page.active)
                imgui.TableSetColumnIndex(ctx, 1)
                if imgui.Selectable(ctx, page.name .. "##PageList" .. pageIdx, state.pageIndex == pageIdx) then state.pageIndex = pageIdx state.surfaceIndex = 1 state.listenerIndex = 1 end
                imgui.TableSetColumnIndex(ctx, 2)
                if imgui.Button(ctx, "✎##EditPage" .. pageIdx, actionSize, actionSize) then state.editPage = pageIdx end
                ui.ItemTooltip(ctx, "Edit " .. page.name)
                imgui.TableSetColumnIndex(ctx, 3)
                local canRemove = page.name ~= "Home"
                ui.Disabled(ctx, not canRemove, function() if imgui.Button(ctx, "×##RemovePage" .. pageIdx, actionSize, actionSize) then removePage(data, pageIdx) end end)
                ui.ItemTooltip(ctx, canRemove and ("Remove " .. page.name) or "The Home Page is required")
            end
            imgui.EndTable(ctx)
        end
    end
    endRoundedList(ctx)
end

local function renderPageEditor(ctx, data, page, pageIdx)
    if page.name == "Home" then
        imgui.TextDisabled(ctx, "Home is the required default Page and cannot be renamed.")
    else
        local changed
        changed, page.name = textField(ctx, "PageName", page.name)
    end
    local changed
    changed, page.followsMcp = imgui.Checkbox(ctx, "Follow MCP", page.followsMcp)
    changed, page.synchPages = imgui.Checkbox(ctx, "Synchronize Pages", page.synchPages)
    changed, page.scrollLink = imgui.Checkbox(ctx, "Scroll Link", page.scrollLink)
    changed, page.scrollSynch = imgui.Checkbox(ctx, "Scroll Synchronize", page.scrollSynch)
    local duplicate = false
    for otherIdx, otherPage in ipairs(data.pages) do if otherIdx ~= pageIdx and otherPage.name:lower() == page.name:lower() then duplicate = true end end
    if page.name == "" then localError(ctx, "Page name is required")
    elseif not page.name:match("^[A-Za-z][A-Za-z0-9_]*$") then localError(ctx, "Page ID must start with a letter and use only letters, digits, and _")
    elseif duplicate then localError(ctx, "Page name must be unique") end
end

local function removeSurface(page, surfaceIdx)
    local surface = page.surfaces[surfaceIdx]
    if not surface then return end
    table.remove(page.surfaces, surfaceIdx)
    for listenerIdx = #page.listeners, 1, -1 do if page.listeners[listenerIdx].broadcaster == surface.name or page.listeners[listenerIdx].listener == surface.name then table.remove(page.listeners, listenerIdx) end end
    state.surfaceIndex = clamp(state.surfaceIndex, #page.surfaces)
    state.listenerIndex = clamp(state.listenerIndex, #page.listeners)
end

local function openSurfaceOsk(page, surface)
    if not reaper or not page or not page.current or not surface or not surface.active then return end
    reaper.SetExtState(identity.extState.oskCommand, "SurfaceEnabled", surface.name .. "|1", false)
    state.status = "Opening OSK for " .. surface.name
end

local function renderAssignmentList(ctx, data, page, pageIdx, fonts)
    sectionHeader(ctx, "Surface assignments", fonts)
    local visible = beginRoundedList(ctx, "##SurfaceAssignmentsList", LIST_CHILD_HEIGHT)
    if visible then
        if imgui.Button(ctx, "Add Surface", 110, 0) then
            page.surfaces[#page.surfaces + 1] = newSurface(data, page)
            state.surfaceIndex = #page.surfaces
            state.editSurface = { creating = true, page = pageIdx, surface = state.surfaceIndex }
        end
        imgui.Spacing(ctx)
        if imgui.BeginTable(ctx, "##SurfaceAssignmentRows", 5, 0, -1, 0) then
            local actionSize = theme.NOTIFICATIONS.close_button_size
            imgui.TableSetupColumn(ctx, "Status", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Name", imgui.TableColumnFlags_WidthStretch)
            imgui.TableSetupColumn(ctx, "OSK", imgui.TableColumnFlags_WidthFixed, 42)
            imgui.TableSetupColumn(ctx, "Edit", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Remove", imgui.TableColumnFlags_WidthFixed, actionSize)
            for surfaceIdx, surface in ipairs(page.surfaces) do
                imgui.TableNextRow(ctx)
                imgui.TableSetColumnIndex(ctx, 0) renderListStatus(ctx, surface.active)
                imgui.TableSetColumnIndex(ctx, 1)
                if imgui.Selectable(ctx, surface.name .. "##AssignmentList" .. surfaceIdx, state.surfaceIndex == surfaceIdx) then state.surfaceIndex = surfaceIdx end
                imgui.TableSetColumnIndex(ctx, 2)
                local canOpenOsk = page.current and surface.active
                ui.Disabled(ctx, not canOpenOsk, function() if imgui.Button(ctx, "OSK##OpenSurfaceOsk" .. surfaceIdx, 40, actionSize) then openSurfaceOsk(page, surface) end end)
                ui.ItemTooltip(ctx, canOpenOsk and ("Open OSK for " .. surface.name) or (page.current and "The Surface must be connected before OSK can open" or "Switch REAPER to this Page before opening its OSK"))
                imgui.TableSetColumnIndex(ctx, 3)
                if imgui.Button(ctx, "✎##EditSurface" .. surfaceIdx, actionSize, actionSize) then state.editSurface = { page = pageIdx, surface = surfaceIdx } end
                ui.ItemTooltip(ctx, "Edit " .. surface.name)
                imgui.TableSetColumnIndex(ctx, 4)
                if imgui.Button(ctx, "×##RemoveSurface" .. surfaceIdx, actionSize, actionSize) then removeSurface(page, surfaceIdx) end
                ui.ItemTooltip(ctx, "Remove " .. surface.name)
            end
            imgui.EndTable(ctx)
        end
    end
    endRoundedList(ctx)
end

local function surfaceEditorError(data, page, surface)
    if surface.name == "" then return "Surface ID is required" end
    if not surface.name:match("^[A-Za-z][A-Za-z0-9_]*$") then return "Surface ID must start with a letter and use only letters, digits, and _" end
    for surfaceIdx, otherSurface in ipairs(page.surfaces) do
        if otherSurface ~= surface and otherSurface.name == surface.name then return "Surface ID must be unique on this Page" end
        if surface.deviceId ~= "" and otherSurface ~= surface and otherSurface.deviceId == surface.deviceId then return "The selected I/O definition is already assigned on this Page" end
    end
    local ioKind, ioIndex, ioDefinition = findIo(data, surface.deviceId)
    if not ioDefinition then return "Select an I/O definition, or add a new MIDI or OSC definition" end
    local ioError = ioDefinitionError(data, ioKind, ioIndex, ioDefinition)
    if ioError ~= "" then return "Edit the selected I/O definition: " .. ioError end
    if not findTemplate(data, surface.surfaceId) then return "Select an existing Surface template" end
    local mainProfile = findProfile(data, surface.mainProfile)
    if not mainProfile or mainProfile.mainSource == "Missing" or mainProfile.mainSource == "Invalid" then return "Select an existing Zone profile" end
    if surface.useDifferentFx then
        local fxProfile = findProfile(data, surface.fxProfile)
        if not fxProfile or fxProfile.fxSource == "Missing" or fxProfile.fxSource == "Invalid" then return "Select an existing FX Zone profile" end
    end
    return ""
end

local function renderAssignmentEditor(ctx, data, page, surface)
    local changed
    local openedIoEditor = false
    if beginForm(ctx, "##AssignmentMasterForm") then
        fieldRow(ctx, "Surface ID", function() changed, surface.name = textField(ctx, "AssignmentName", surface.name) end)
        fieldRow(ctx, "I/O definition", function()
            local selectedIo
            changed, selectedIo = ui.ComboEnum(ctx, "##AssignmentIo", surface.deviceId, assignmentIoItems(data, page, surface), { width = 145 })
            if changed then
                if selectedIo == "__ADD_MIDI__" or selectedIo == "__ADD_OSC__" then
                    local returnSurface = state.editSurface
                    addIoDefinition(data, selectedIo == "__ADD_MIDI__" and "MIDI" or "OSC")
                    local devices = state.editIo.kind == "MIDI" and data.midi or data.osc
                    surface.deviceId = devices[state.editIo.index].name
                    state.editIo.returnSurface = returnSurface
                    state.editSurface = nil
                    openedIoEditor = true
                    imgui.CloseCurrentPopup(ctx)
                else surface.deviceId = selectedIo end
            end
            imgui.SameLine(ctx)
            local ioKind, ioIndex = findIo(data, surface.deviceId)
            local actionSize = theme.NOTIFICATIONS.close_button_size
            ui.Disabled(ctx, not ioKind or openedIoEditor, function()
                if imgui.Button(ctx, "✎##EditSurfaceIo", actionSize, actionSize) then
                    state.editIo = { index = ioIndex, kind = ioKind, returnSurface = state.editSurface }
                    state.editSurface = nil
                    openedIoEditor = true
                    imgui.CloseCurrentPopup(ctx)
                end
            end)
            ui.ItemTooltip(ctx, ioKind and ("Edit " .. surface.deviceId) or "Select or add an I/O definition")
        end)
        fieldRow(ctx, "Surface template", function()
            changed, surface.surfaceId = ui.ComboEnum(ctx, "##AssignmentTemplate", surface.surfaceId, templateItems(data))
            if changed then
                local template = findTemplate(data, surface.surfaceId)
                local matchingProfileId = defaultProfileId(data, surface.surfaceId)
                surface.templateSource = template and template.source or "Missing"
                if matchingProfileId ~= "" then
                    local profile = findProfile(data, matchingProfileId)
                    surface.mainProfile = matchingProfileId
                    surface.mainSource = profile and profile.mainSource or "Missing"
                end
            end
        end)
        fieldRow(ctx, "Zone profile", function()
            changed, surface.mainProfile = ui.ComboEnum(ctx, "##AssignmentMain", surface.mainProfile, profileItems(data))
            if changed then local profile = findProfile(data, surface.mainProfile) surface.mainSource = profile and profile.mainSource or "Missing" end
        end)
        imgui.EndTable(ctx)
    end
    if imgui.CollapsingHeader(ctx, "Advanced##SurfaceAssignmentAdvanced") then
        if beginForm(ctx, "##AssignmentAdvancedForm") then
            fieldRow(ctx, "Start channel", function() changed, surface.startChannel = integerField(ctx, "AssignmentStart", surface.startChannel, 0, 65535) end)
            imgui.EndTable(ctx)
        end
        changed, surface.useDifferentFx = imgui.Checkbox(ctx, "Use a different FX profile", surface.useDifferentFx == true)
        if surface.useDifferentFx and beginForm(ctx, "##AssignmentFxForm") then
            fieldRow(ctx, "FX Zone profile", function()
                changed, surface.fxProfile = ui.ComboEnum(ctx, "##AssignmentFx", surface.fxProfile, profileItems(data))
                if changed then local profile = findProfile(data, surface.fxProfile) surface.fxSource = profile and profile.fxSource or "Missing" end
            end)
            imgui.EndTable(ctx)
        end
    end
    if not surface.useDifferentFx then
        surface.fxProfile = surface.mainProfile
        local profile = findProfile(data, surface.fxProfile)
        surface.fxSource = profile and profile.fxSource or "Missing"
    end
    localError(ctx, surfaceEditorError(data, page, surface))
    return openedIoEditor
end

local function renderAssignmentsSection(ctx, data, fonts)
    state.pageIndex = clamp(state.pageIndex, #data.pages)
    local page = data.pages[state.pageIndex]
    if page then state.surfaceIndex = clamp(state.surfaceIndex, #page.surfaces) end
    if imgui.BeginTable(ctx, "##AssignmentMasterDetail", 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "Pages", imgui.TableColumnFlags_WidthFixed, PAGE_LIST_WIDTH)
        imgui.TableSetupColumn(ctx, "Assignments", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0) renderPageList(ctx, data, fonts)
        page = data.pages[state.pageIndex]
        if not page then imgui.EndTable(ctx) return end
        state.surfaceIndex = clamp(state.surfaceIndex, #page.surfaces)
        imgui.TableSetColumnIndex(ctx, 1) renderAssignmentList(ctx, data, page, state.pageIndex, fonts)
        imgui.EndTable(ctx)
    end
end

local function setListenerCategory(listener, category, enabled)
    local categories = {}
    for item in (listener.categories or ""):gmatch("[^,]+") do local trimmed = item:match("^%s*(.-)%s*$") if trimmed ~= "" and trimmed ~= category then categories[#categories + 1] = trimmed end end
    if enabled then categories[#categories + 1] = category end
    listener.categories = table.concat(categories, ", ")
end

local function renderListenerEditor(ctx, page, listener, fonts)
    sectionHeader(ctx, "Listener relationship", fonts)
    local names = {}
    for surfaceIdx, surface in ipairs(page.surfaces) do names[#names + 1] = surface.name end
    local items = enumItems(names)
    local changed
    if beginForm(ctx, "##ListenerMasterForm") then
        fieldRow(ctx, "Broadcaster", function() changed, listener.broadcaster = ui.ComboEnum(ctx, "##ListenerBroadcaster", listener.broadcaster, items) end)
        fieldRow(ctx, "Listener", function() changed, listener.listener = ui.ComboEnum(ctx, "##ListenerTarget", listener.listener, items) end)
        imgui.EndTable(ctx)
    end
    imgui.Text(ctx, "Categories")
    local categories = { "GoHome", "Modifiers", "FXMenu", "SelectedTrackFX", "SelectedTrackSends", "SelectedTrackReceives" }
    for categoryIdx, category in ipairs(categories) do
        local enabled = (", " .. listener.categories .. ", "):find(", " .. category .. ", ", 1, true) ~= nil
        local categoryChanged
        categoryChanged, enabled = imgui.Checkbox(ctx, category .. "##ListenerCategory" .. categoryIdx, enabled)
        if categoryChanged then setListenerCategory(listener, category, enabled) end
    end
    local broadcasterFound = false
    local listenerFound = false
    for surfaceIdx, surface in ipairs(page.surfaces) do
        if surface.name == listener.broadcaster then broadcasterFound = true end
        if surface.name == listener.listener then listenerFound = true end
    end
    if not broadcasterFound then localError(ctx, "Select an existing broadcaster on this Page") end
    if not listenerFound then localError(ctx, "Select an existing listener on this Page") end
    if listener.broadcaster == listener.listener then localError(ctx, "Broadcaster and Listener must be different") end
    for listenerIdx, otherListener in ipairs(page.listeners) do
        if otherListener ~= listener and otherListener.broadcaster == listener.broadcaster and otherListener.listener == listener.listener then localError(ctx, "This relationship already exists") break end
        if otherListener.broadcaster == listener.listener and otherListener.listener == listener.broadcaster then localError(ctx, "This relationship creates a circular pair") break end
    end
    renderRuntime(ctx, listener.active, "", listener.active and "CSI is forwarding the selected categories. No action is required." or "Check both Surface assignments, then Save changes to reconnect CSI.")
end

local function renderListenersSection(ctx, data, fonts)
    state.pageIndex = clamp(state.pageIndex, #data.pages)
    local page = data.pages[state.pageIndex]
    if not page then imgui.TextDisabled(ctx, "Add a Page first") return end
    state.listenerIndex = clamp(state.listenerIndex, #page.listeners)
    if imgui.BeginTable(ctx, "##ListenerMasterDetail", 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "List", imgui.TableColumnFlags_WidthFixed, LISTENER_LIST_WIDTH)
        imgui.TableSetupColumn(ctx, "Editor", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        sectionHeader(ctx, "Relationships", fonts)
        local pageItems = {}
        for pageIdx, itemPage in ipairs(data.pages) do pageItems[#pageItems + 1] = { label = itemPage.name, value = pageIdx } end
        local pageChanged
        pageChanged, state.pageIndex = ui.ComboEnum(ctx, "Page##ListenerPage", state.pageIndex, pageItems, { width = LISTENER_LIST_WIDTH - 20 })
        if pageChanged then page = data.pages[state.pageIndex] state.listenerIndex = 1 end
        local visible = beginRoundedList(ctx, "##RelationshipsList", LIST_CHILD_HEIGHT)
        if visible then
            ui.Disabled(ctx, #page.surfaces < 2, function() if imgui.Button(ctx, "Add Listener", 110, 0) then page.listeners[#page.listeners + 1] = { active = false, broadcaster = page.surfaces[1].name, categories = "GoHome", listener = page.surfaces[2].name } state.listenerIndex = #page.listeners end end)
            imgui.Spacing(ctx)
            if imgui.BeginTable(ctx, "##RelationshipRows", 3, 0, -1, 0) then
                local actionSize = theme.NOTIFICATIONS.close_button_size
                imgui.TableSetupColumn(ctx, "Status", imgui.TableColumnFlags_WidthFixed, actionSize)
                imgui.TableSetupColumn(ctx, "Relationship", imgui.TableColumnFlags_WidthStretch)
                imgui.TableSetupColumn(ctx, "Remove", imgui.TableColumnFlags_WidthFixed, actionSize)
                for listenerIdx, listener in ipairs(page.listeners) do
                    imgui.TableNextRow(ctx)
                    imgui.TableSetColumnIndex(ctx, 0) renderListStatus(ctx, listener.active)
                    imgui.TableSetColumnIndex(ctx, 1)
                    if imgui.Selectable(ctx, listener.broadcaster .. " -> " .. listener.listener .. "##ListenerList" .. listenerIdx, state.listenerIndex == listenerIdx) then state.listenerIndex = listenerIdx end
                    imgui.TableSetColumnIndex(ctx, 2)
                    if imgui.Button(ctx, "×##RemoveListener" .. listenerIdx, actionSize, actionSize) then table.remove(page.listeners, listenerIdx) state.listenerIndex = clamp(state.listenerIndex, #page.listeners) end
                    ui.ItemTooltip(ctx, "Remove this relationship")
                end
                imgui.EndTable(ctx)
            end
        end
        endRoundedList(ctx)
        imgui.TableSetColumnIndex(ctx, 1)
        local listener = page.listeners[state.listenerIndex]
        if listener then renderListenerEditor(ctx, page, listener, fonts) elseif #page.surfaces < 2 then imgui.TextDisabled(ctx, "Add at least two Surface assignments to this Page") else imgui.TextDisabled(ctx, "Add or select a Listener relationship") end
        imgui.EndTable(ctx)
    end
end

local function renderIssues(ctx, data)
    if data.fatalError == "" and #data.issues == 0 and data.skippedSurfaceCount == 0 then return end
    imgui.Spacing(ctx)
    if data.fatalError ~= "" then localError(ctx, data.fatalError) end
    if data.skippedSurfaceCount > 0 then localError(ctx, "Skipped Surface assignments: " .. data.skippedSurfaceCount) end
    for issueIdx, issue in ipairs(data.issues) do localError(ctx, issue.kind .. " line " .. issue.line .. ": " .. issue.message) end
end

local function openStandaloneEditor()
    if state.data and state.data.editorAvailable then
        startRequest("OpenEditor")
        return
    end
    local packageName = identity.packagePrefix .. " Configuration Editor"
    if reaper.ReaPack_BrowsePackages then
        reaper.ReaPack_BrowsePackages(packageName)
        state.status = "Install " .. packageName .. " in ReaPack, then reload the saved configuration."
    else
        state.error = "The standalone configuration editor is not installed. Install ReaPack, then install " .. packageName .. "."
    end
end

local function resetDevices()
    if state.requestId or not state.data then return end
    state.pendingDraft = model.Clone(state.data)
    state.afterQueryKind = "ResetDevices"
    reaper.Main_OnCommand(42348, 0)
    startQuery()
end

local function renderDevicesToolbar(ctx)
    local actionSize = 26
    local remainingWidth = imgui.GetContentRegionAvail(ctx)
    local resetWidth = 105
    imgui.SetCursorPosX(ctx, imgui.GetCursorPosX(ctx) + math.max(0, remainingWidth - actionSize - resetWidth - theme.common.item_spacing))
    local editorClicked = false
    ui.Disabled(ctx, state.data == nil or state.requestId ~= nil, function()
        imgui.PushStyleColor(ctx, imgui.Col_Button, EDITOR_BUTTON_COLORS.button)
        imgui.PushStyleColor(ctx, imgui.Col_ButtonHovered, EDITOR_BUTTON_COLORS.hovered)
        imgui.PushStyleColor(ctx, imgui.Col_ButtonActive, EDITOR_BUTTON_COLORS.active)
        editorClicked = imgui.Button(ctx, "✎##OpenStandaloneEditor", actionSize, actionSize)
        imgui.PopStyleColor(ctx, 3)
    end)
    ui.ItemTooltip(ctx, state.data and state.data.editorAvailable and "Open standalone configuration editor" or "Install standalone configuration editor with ReaPack")
    if editorClicked then openStandaloneEditor() end
    imgui.SameLine(ctx)
    ui.Disabled(ctx, state.requestId ~= nil or state.data == nil, function() if imgui.Button(ctx, "Reset devices", resetWidth, actionSize) then resetDevices() end end)
    ui.ItemTooltip(ctx, "Run REAPER Reset all MIDI control surface devices, then refresh connection status without discarding this draft")
end

local function mergeRuntimeStatus(draft, response)
    local function mergeDevices(targets, sources)
        for targetIdx, target in ipairs(targets) do
            target.active = false
            for sourceIdx, source in ipairs(sources) do
                if source.name == target.name then
                    target.active = source.active
                    target.runtimeIssue = source.runtimeIssue
                    target.inputName = source.inputName or target.inputName
                    target.outputName = source.outputName or target.outputName
                    break
                end
            end
        end
    end
    mergeDevices(draft.midi, response.midi)
    mergeDevices(draft.osc, response.osc)
    for pageIdx, page in ipairs(draft.pages) do
        page.active = false
        page.current = false
        for responsePageIdx, responsePage in ipairs(response.pages) do
            if responsePage.name == page.name then
                page.active = responsePage.active
                page.current = responsePage.current
                for surfaceIdx, surface in ipairs(page.surfaces) do
                    surface.active = false
                    surface.ioActive = false
                    for responseSurfaceIdx, responseSurface in ipairs(responsePage.surfaces) do
                        if responseSurface.name == surface.name then
                            surface.active = responseSurface.active
                            surface.ioActive = responseSurface.ioActive
                            surface.ioType = responseSurface.ioType
                            break
                        end
                    end
                end
                for listenerIdx, listener in ipairs(page.listeners) do
                    listener.active = false
                    for responseListenerIdx, responseListener in ipairs(responsePage.listeners) do
                        if responseListener.broadcaster == listener.broadcaster and responseListener.listener == listener.listener then listener.active = responseListener.active break end
                    end
                end
                break
            end
        end
    end
end

local function refreshDraftResourceStatus(data)
    for pageIdx, page in ipairs(data.pages) do
        for surfaceIdx, surface in ipairs(page.surfaces) do
            local template = findTemplate(data, surface.surfaceId)
            local mainProfile = findProfile(data, surface.mainProfile)
            local fxProfile = findProfile(data, surface.fxProfile)
            surface.templateSource = template and template.source or "Missing"
            surface.mainSource = mainProfile and mainProfile.mainSource or "Missing"
            surface.fxSource = fxProfile and fxProfile.fxSource or "Missing"
        end
    end
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    startQuery()
end

function module.Update()
    if not state.initialized or not state.requestId then return end
    local response, responseError = protocol.Poll(state.requestId)
    if response then
        local completedKind = state.pendingKind
        state.requestId = nil
        state.pendingKind = ""
        state.error = ""
        if completedKind == "Query" then
            if state.pendingDraft then
                mergeRuntimeStatus(state.pendingDraft, response)
                state.pendingDraft.profileOptions = response.profileOptions
                state.pendingDraft.surfaceOptions = response.surfaceOptions
                state.pendingDraft.midiInputOptions = response.midiInputOptions
                state.pendingDraft.midiOutputOptions = response.midiOutputOptions
                state.pendingDraft.editorAvailable = response.editorAvailable
                state.pendingDraft.revision = response.revision
                ensureHomePage(state.pendingDraft)
                state.data = state.pendingDraft
                state.pendingDraft = nil
                refreshDraftResourceStatus(state.data)
            else
                state.savedData = model.Clone(response)
                state.savedSignature = model.Signature(response)
                ensureHomePage(response)
                state.data = response
            end
            if state.afterQueryKind == "Apply" then
                local inactiveCount = inactiveRuntimeCount(response)
                state.status = inactiveCount == 0 and "Devices saved and reconnected" or ("Devices saved, but " .. inactiveCount .. " runtime item(s) are inactive or unavailable")
            elseif state.afterQueryKind == "ResetDevices" then
                local inactiveCount = inactiveRuntimeCount(state.data)
                state.status = inactiveCount == 0 and "Devices reset; connection status refreshed" or ("Devices reset; " .. inactiveCount .. " runtime item(s) are inactive or unavailable")
            end
            state.afterQueryKind = ""
        elseif completedKind == "OpenEditor" then
            state.status = response.message
            state.afterQueryKind = ""
        else
            state.status = response.message
            state.afterQueryKind = completedKind
            startQuery()
        end
    elseif responseError then
        state.requestId = nil
        state.pendingKind = ""
        state.error = tostring(responseError)
    elseif reaper.time_precise() - state.requestStarted > 3 then
        protocol.Cancel(state.requestId)
        state.requestId = nil
        state.pendingKind = ""
        state.error = "Devices query timed out"
    end
end

function module.IsDirty()
    return state.data ~= nil and model.Signature(state.data) ~= state.savedSignature
end

function module.IsBusy()
    return state.requestId ~= nil or state.confirmSource ~= nil
end

function module.HasError()
    return state.error ~= ""
end

function module.GetStatus()
    return state.error ~= "" and state.error or state.status
end

function module.NeedsConfigurationCreation()
    return state.data ~= nil and not state.data.configExists
end

function module.HasConfiguration()
    return state.data ~= nil and state.data.configExists == true
end

function module.Validate()
    if not state.data then return false, "Device configuration is not loaded" end
    if state.data.fatalError ~= "" or #state.data.issues > 0 or state.data.skippedSurfaceCount > 0 then return false, "Fix the listed configuration issues in the standalone editor before saving Devices" end
    local homeFound = false
    for pageIdx, page in ipairs(state.data.pages) do if page.name == "Home" then homeFound = true break end end
    if not homeFound then return false, "The required Home Page is missing" end
    if #state.data.midi == 0 and #state.data.osc == 0 then return false, "Add at least one I/O Device" end
    for midiIdx, device in ipairs(state.data.midi) do if device.inputPort < 0 or device.outputPort < 0 then return false, "Select input and output ports for MIDI I/O " .. device.name end end
    local source, serializationError = model.Serialize(state.data)
    if not source then return false, serializationError end
    if #state.data.pages == 0 then return false, "At least one Page is required" end
    for pageIdx, page in ipairs(state.data.pages) do if #page.surfaces == 0 then return false, "Assign at least one Surface to Page " .. page.name end end
    return true
end

function module.Save()
    if state.requestId then return false, "Wait for the current Devices operation" end
    local valid, validationError = module.Validate()
    if not valid then return false, validationError end
    local source, serializationError = model.Serialize(state.data)
    if not source then return false, serializationError end
    state.confirmSource = source
    return true
end

function module.Revert()
    if state.requestId then return false, "Wait for the current Devices operation" end
    if not state.savedData then return false, "Device configuration is not loaded" end
    state.data = model.Clone(state.savedData)
    ensureHomePage(state.data)
    state.error = ""
    state.status = ""
    return true
end

function module.RenderPage(ctx, fonts)
    module.Initialize()
    renderDevicesToolbar(ctx)
    if not state.data then return end
    if not state.data.configExists then
        imgui.Spacing(ctx)
        imgui.TextWrapped(ctx, "No product configuration exists yet. Add an I/O definition and assign a Surface to Home. Then use Create configuration below.")
    end
    imgui.Spacing(ctx)
    renderAssignmentsSection(ctx, state.data, fonts)
    imgui.Spacing(ctx)
    if imgui.CollapsingHeader(ctx, "Listeners##DevicesListeners") then renderListenersSection(ctx, state.data, fonts) end
    renderIssues(ctx, state.data)
end

function module.RenderModal(ctx, fonts)
    local ioPopupTitle = state.editIo and state.editIo.creating and ("Add " .. state.editIo.kind .. " I/O##DevicesIoEditor") or "Edit I/O definition##DevicesIoEditor"
    if state.editIo and state.data then
        local devices = state.editIo.kind == "MIDI" and state.data.midi or state.data.osc
        if devices[state.editIo.index] then imgui.OpenPopup(ctx, ioPopupTitle) else state.editIo = nil end
    end
    local ioEditorVisible = imgui.BeginPopupModal(ctx, ioPopupTitle, nil, imgui.WindowFlags_AlwaysAutoResize)
    if ioEditorVisible then
        local editIo = state.editIo
        local devices = editIo and editIo.kind == "MIDI" and state.data.midi or (editIo and state.data.osc or {})
        local device = editIo and devices[editIo.index] or nil
        if device then
            if editIo.kind == "MIDI" then renderMidiEditor(ctx, state.data, device, editIo.index, fonts) else renderOscEditor(ctx, state.data, device, editIo.index, fonts) end
            local returnSurface = editIo.returnSurface
            if editIo.creating then
                local ioError = ioDefinitionError(state.data, editIo.kind, editIo.index, device)
                ui.Disabled(ctx, ioError ~= "", function()
                    if imgui.Button(ctx, "Add", 100, 0) then
                        state.editIo = nil
                        state.editSurface = returnSurface
                        imgui.CloseCurrentPopup(ctx)
                    end
                end)
                imgui.SameLine(ctx)
                if imgui.Button(ctx, "Cancel", 100, 0) then
                    local deviceName = device.name
                    table.remove(devices, editIo.index)
                    local page = returnSurface and state.data.pages[returnSurface.page] or nil
                    local surface = page and page.surfaces[returnSurface.surface] or nil
                    if surface and surface.deviceId == deviceName then surface.deviceId = "" end
                    state.editIo = nil
                    state.editSurface = returnSurface
                    imgui.CloseCurrentPopup(ctx)
                end
            else
                if imgui.Button(ctx, "Remove", 100, 0) then
                    requestIoRemoval(state.data, editIo.kind, editIo.index)
                    if state.deleteIo then state.deleteIo.returnSurface = returnSurface end
                    state.editIo = nil
                    imgui.CloseCurrentPopup(ctx)
                end
                imgui.SameLine(ctx)
                if imgui.Button(ctx, "Close", 100, 0) then
                    state.editIo = nil
                    state.editSurface = returnSurface
                    imgui.CloseCurrentPopup(ctx)
                end
            end
        end
        imgui.EndPopup(ctx)
    end
    if state.editPage and state.data then
        if state.data.pages[state.editPage] then imgui.OpenPopup(ctx, "Edit Page##DevicesPageEditor") else state.editPage = nil end
    end
    local pageEditorVisible = imgui.BeginPopupModal(ctx, "Edit Page##DevicesPageEditor", nil, imgui.WindowFlags_AlwaysAutoResize)
    if pageEditorVisible then
        local page = state.editPage and state.data.pages[state.editPage] or nil
        if page then renderPageEditor(ctx, state.data, page, state.editPage) end
        if imgui.Button(ctx, "Close", 100, 0) then
            state.editPage = nil
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.EndPopup(ctx)
    end
    local surfacePopupTitle = state.editSurface and state.editSurface.creating and "Add Surface##DevicesSurfaceEditor" or "Edit Surface assignment##DevicesSurfaceEditor"
    if state.editSurface and state.data then
        local page = state.data.pages[state.editSurface.page]
        if page and page.surfaces[state.editSurface.surface] then imgui.OpenPopup(ctx, surfacePopupTitle) else state.editSurface = nil end
    end
    local surfaceEditorVisible = imgui.BeginPopupModal(ctx, surfacePopupTitle, nil, imgui.WindowFlags_AlwaysAutoResize)
    if surfaceEditorVisible then
        local surfaceEdit = state.editSurface
        local page = surfaceEdit and state.data.pages[surfaceEdit.page] or nil
        local surface = page and page.surfaces[surfaceEdit.surface] or nil
        if surface then
            local openedIoEditor = renderAssignmentEditor(ctx, state.data, page, surface)
            local creating = surfaceEdit.creating == true
            local editorError = surfaceEditorError(state.data, page, surface)
            if not openedIoEditor then
                if creating then
                    ui.Disabled(ctx, editorError ~= "", function()
                        if imgui.Button(ctx, "Add", 100, 0) then
                            state.editSurface = nil
                            imgui.CloseCurrentPopup(ctx)
                        end
                    end)
                    imgui.SameLine(ctx)
                    if imgui.Button(ctx, "Cancel", 100, 0) then
                        local surfaceIdx = surfaceEdit.surface
                        state.editSurface = nil
                        removeSurface(page, surfaceIdx)
                        imgui.CloseCurrentPopup(ctx)
                    end
                elseif imgui.Button(ctx, "Close", 100, 0) then
                    state.editSurface = nil
                    imgui.CloseCurrentPopup(ctx)
                end
            end
        elseif imgui.Button(ctx, "Close", 100, 0) then
            state.editSurface = nil
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.EndPopup(ctx)
    end
    if state.deleteIo then imgui.OpenPopup(ctx, "Remove used I/O definition?##DevicesDeleteIo") end
    local deleteVisible = imgui.BeginPopupModal(ctx, "Remove used I/O definition?##DevicesDeleteIo", nil, imgui.WindowFlags_AlwaysAutoResize)
    if deleteVisible then
        imgui.TextWrapped(ctx, "The I/O definition '" .. state.deleteIo.name .. "' is used by these Surface assignments:")
        for assignmentIdx, assignment in ipairs(state.deleteIo.assignments) do imgui.Text(ctx, "• " .. assignment) end
        imgui.TextWrapped(ctx, "Removing it also removes these assignments and every Listener relationship connected to them.")
        if imgui.Button(ctx, "Remove from configuration", 190, 0) then
            removeIoAndReferences(state.data, state.deleteIo.kind, state.deleteIo.index)
            state.deleteIo = nil
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.SameLine(ctx)
        if imgui.Button(ctx, "Cancel", 100, 0) then
            state.editSurface = state.deleteIo.returnSurface
            state.deleteIo = nil
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.EndPopup(ctx)
    end
    if state.confirmSource then imgui.OpenPopup(ctx, "Save Devices##DevicesReconnect") end
    local confirmVisible = imgui.BeginPopupModal(ctx, "Save Devices##DevicesReconnect", nil, imgui.WindowFlags_AlwaysAutoResize)
    if confirmVisible then
        local creating = state.data and not state.data.configExists
        imgui.TextWrapped(ctx, creating and "Create the product configuration and connect the configured control surfaces?" or "Saving Devices disconnects and reconnects active control surfaces. Continue?")
        if imgui.Button(ctx, creating and "Create and connect" or "Save and reconnect", 150, 0) then
            local source = state.confirmSource
            state.confirmSource = nil
            startRequest("Apply", source)
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.SameLine(ctx)
        if imgui.Button(ctx, "Cancel", 100, 0) then
            state.confirmSource = nil
            state.error = "Devices Save was cancelled"
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.EndPopup(ctx)
    end
end

function module.Shutdown()
    if state.requestId then protocol.Cancel(state.requestId) end
    state.requestId = nil
    state.confirmSource = nil
    state.deleteIo = nil
    state.editIo = nil
    state.editPage = nil
    state.editSurface = nil
end

return module
