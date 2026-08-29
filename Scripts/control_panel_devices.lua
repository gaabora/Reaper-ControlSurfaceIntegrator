local imgui = require "imgui" "0.9.3"

local identity = require("product_identity")
local protocol = require("devices_protocol")
local model = require("devices_model")
local theme = require("theme_settings")
local ui = require("ui_components")

local module = {}
local state = { afterQueryKind = "", confirmSource = nil, createProfileId = "", data = nil, deleteIo = nil, error = "", initialized = false, ioIndex = 1, ioKind = "MIDI", listenerIndex = 1, pageIndex = 1, pendingDraft = nil, pendingKind = "", requestId = nil, requestStarted = 0, savedData = nil, savedSignature = "", section = "IO", status = "", surfaceIndex = 1 }

local function sectionHeader(ctx, label, fonts)
    if fonts and fonts.section then imgui.PushFont(ctx, fonts.section) end
    imgui.Text(ctx, label)
    if fonts and fonts.section then imgui.PopFont(ctx) end
end

local function startRequest(kind, profileId, source)
    if state.requestId then return end
    if (kind == "CreateProfile" or kind == "CopyProfile") and module.IsDirty() then state.pendingDraft = model.Clone(state.data) end
    local requestId, queryError
    if kind == "CreateProfile" then requestId, queryError = protocol.CreateProfile(profileId)
    elseif kind == "CopyProfile" then requestId, queryError = protocol.CopyProfile(profileId)
    elseif kind == "OpenEditor" then requestId, queryError = protocol.OpenEditor()
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

local SECTION_ITEMS = { { id = "IO", label = "1  I/O Devices" }, { id = "Assignments", label = "2  Pages & Surfaces" }, { id = "Listeners", label = "3  Listeners" } }
local SELECTED_SECTION_COLOR = 0x5278A8FF
local STATUS_ACTIVE_COLOR = 0x40c060ff
local STATUS_INACTIVE_COLOR = 0xff5050ff
local EDITOR_BUTTON_COLORS = { active = 0xb85c10ff, button = 0xd97718ff, hovered = 0xf08a27ff }
local LIST_CHILD_HEIGHT = 260
local PAGE_LIST_CHILD_HEIGHT = 170
local LIST_WIDTH = 210
local PAGE_LIST_WIDTH = 210
local ASSIGNMENT_LIST_WIDTH = 190

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
    for midiIdx, device in ipairs(data.midi) do if device.name == name and (kind ~= "MIDI" or midiIdx ~= selectedIdx) then return "I/O name must be unique" end end
    for oscIdx, device in ipairs(data.osc) do if device.name == name and (kind ~= "OSC" or oscIdx ~= selectedIdx) then return "I/O name must be unique" end end
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
    state.ioKind = kind
    state.ioIndex = clamp(deviceIdx, #devices)
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

local function duplicateIo(data, kind, deviceIdx)
    local devices = kind == "MIDI" and data.midi or data.osc
    local device = devices[deviceIdx]
    if not device then return end
    local copy = model.Clone(device)
    copy.active = false
    copy.name = uniqueIoName(data, device.name .. "_Copy")
    copy.runtimeIssue = ""
    table.insert(devices, deviceIdx + 1, copy)
    state.ioKind = kind
    state.ioIndex = deviceIdx + 1
end

local function renderIoRows(ctx, data, kind, devices)
    imgui.TextDisabled(ctx, kind)
    if not imgui.BeginTable(ctx, "##IoRows_" .. kind, 4, 0, -1, 0) then return end
    local actionSize = theme.NOTIFICATIONS.close_button_size
    imgui.TableSetupColumn(ctx, "Status", imgui.TableColumnFlags_WidthFixed, actionSize)
    imgui.TableSetupColumn(ctx, "Name", imgui.TableColumnFlags_WidthStretch)
    imgui.TableSetupColumn(ctx, "Duplicate", imgui.TableColumnFlags_WidthFixed, actionSize)
    imgui.TableSetupColumn(ctx, "Remove", imgui.TableColumnFlags_WidthFixed, actionSize)
    for deviceIdx, device in ipairs(devices) do
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        renderListStatus(ctx, device.active)
        imgui.TableSetColumnIndex(ctx, 1)
        if imgui.Selectable(ctx, device.name .. "##IoList" .. kind .. deviceIdx, state.ioKind == kind and state.ioIndex == deviceIdx) then state.ioKind = kind state.ioIndex = deviceIdx end
        imgui.TableSetColumnIndex(ctx, 2)
        if imgui.Button(ctx, "⧉##DuplicateIo" .. kind .. deviceIdx, actionSize, actionSize) then duplicateIo(data, kind, deviceIdx) end
        ui.ItemTooltip(ctx, "Duplicate " .. device.name)
        imgui.TableSetColumnIndex(ctx, 3)
        if imgui.Button(ctx, "×##RemoveIo" .. kind .. deviceIdx, actionSize, actionSize) then requestIoRemoval(data, kind, deviceIdx) end
        ui.ItemTooltip(ctx, "Remove " .. device.name)
    end
    imgui.EndTable(ctx)
end

local function renderIoList(ctx, data, fonts)
    sectionHeader(ctx, "I/O definitions", fonts)
    local visible = beginRoundedList(ctx, "##IoDefinitionsList", LIST_CHILD_HEIGHT)
    if visible then
        if imgui.Button(ctx, "Add MIDI", 95, 0) then
            data.midi[#data.midi + 1] = { active = false, channels = 8, inputName = "", inputPort = -1, maxMessages = 200, name = uniqueIoName(data, "MIDI"), outputName = "", outputPort = -1, refreshRate = 15, runtimeIssue = "", settingOverrides = {} }
            state.ioKind = "MIDI"
            state.ioIndex = #data.midi
        end
        imgui.SameLine(ctx)
        if imgui.Button(ctx, "Add OSC", 95, 0) then
            data.osc[#data.osc + 1] = { active = false, address = "127.0.0.1", channels = 8, maxPackets = 0, name = uniqueIoName(data, "OSC"), receivePort = "8000", runtimeIssue = "", settingOverrides = {}, transmitPort = "9000", type = "OSC" }
            state.ioKind = "OSC"
            state.ioIndex = #data.osc
        end
        imgui.Spacing(ctx)
        renderIoRows(ctx, data, "MIDI", data.midi)
        imgui.Spacing(ctx)
        renderIoRows(ctx, data, "OSC", data.osc)
    end
    endRoundedList(ctx)
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
    localError(ctx, duplicateIoName(data, device.name, "MIDI", deviceIdx))
    if device.inputPort < 0 then localError(ctx, "Select an input port") end
    if device.outputPort < 0 then localError(ctx, "Select an output port") end
    renderRuntime(ctx, device.active, device.runtimeIssue)
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
    localError(ctx, duplicateIoName(data, device.name, "OSC", deviceIdx))
    if device.receivePort == "" or device.transmitPort == "" or device.address == "" then localError(ctx, "OSC endpoints are required") end
    renderRuntime(ctx, device.active, device.runtimeIssue)
end

local function renderIoSection(ctx, data, fonts)
    if imgui.BeginTable(ctx, "##IoMasterDetail", 2, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "List", imgui.TableColumnFlags_WidthStretch)
        imgui.TableSetupColumn(ctx, "Editor", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        renderIoList(ctx, data, fonts)
        imgui.TableSetColumnIndex(ctx, 1)
        local devices = state.ioKind == "MIDI" and data.midi or data.osc
        state.ioIndex = clamp(state.ioIndex, #devices)
        local device = devices[state.ioIndex]
        if not device then imgui.TextDisabled(ctx, "Add or select an I/O device") else
            if state.ioKind == "MIDI" then renderMidiEditor(ctx, data, device, state.ioIndex, fonts) else renderOscEditor(ctx, data, device, state.ioIndex, fonts) end
        end
        imgui.EndTable(ctx)
    end
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
    local items = { { label = "Select or create a Zone profile", value = "" } }
    for profileIdx, profile in ipairs(data.profileOptions) do items[#items + 1] = { label = profile.id .. " - Main: " .. profile.mainSource .. ", FX: " .. profile.fxSource, value = profile.id } end
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

local function newSurface(data, page)
    local ioDefinitions = availableIoItems(data, page)
    local templates = templateItems(data)
    if #ioDefinitions == 0 or #templates == 0 then return nil end
    local surfaceId = templates[1].value
    local profileId = defaultProfileId(data, surfaceId)
    local template = findTemplate(data, surfaceId)
    local profile = findProfile(data, profileId)
    return { active = false, deviceId = ioDefinitions[1].value, fxProfile = profileId, fxSource = profile and profile.fxSource or "Missing", ioActive = false, ioType = "", mainProfile = profileId, mainSource = profile and profile.mainSource or "Missing", name = ioDefinitions[1].value, startChannel = 0, surfaceId = surfaceId, templateSource = template and template.source or "Missing", useDifferentFx = false }
end

local function duplicatePage(data, pageIdx)
    local page = data.pages[pageIdx]
    if not page then return end
    local copy = model.Clone(page)
    copy.active = false
    copy.current = false
    copy.name = uniquePageName(data, page.name .. "_Copy")
    for surfaceIdx, surface in ipairs(copy.surfaces) do surface.active = false surface.ioActive = false end
    for listenerIdx, listener in ipairs(copy.listeners) do listener.active = false end
    table.insert(data.pages, pageIdx + 1, copy)
    state.pageIndex = pageIdx + 1
    state.surfaceIndex = 1
    state.listenerIndex = 1
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
    local visible = beginRoundedList(ctx, "##PagesList", PAGE_LIST_CHILD_HEIGHT)
    if visible then
        if imgui.Button(ctx, "Add Page", 100, 0) then data.pages[#data.pages + 1] = { active = false, current = false, followsMcp = true, listeners = {}, name = uniquePageName(data, "Page"), scrollLink = false, scrollSynch = false, surfaces = {}, synchPages = true } state.pageIndex = #data.pages state.surfaceIndex = 1 end
        imgui.Spacing(ctx)
        if imgui.BeginTable(ctx, "##PageRows", 4, 0, -1, 0) then
            local actionSize = theme.NOTIFICATIONS.close_button_size
            imgui.TableSetupColumn(ctx, "Status", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Name", imgui.TableColumnFlags_WidthStretch)
            imgui.TableSetupColumn(ctx, "Duplicate", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Remove", imgui.TableColumnFlags_WidthFixed, actionSize)
            for pageIdx, page in ipairs(data.pages) do
                imgui.TableNextRow(ctx)
                imgui.TableSetColumnIndex(ctx, 0) renderListStatus(ctx, page.active)
                imgui.TableSetColumnIndex(ctx, 1)
                if imgui.Selectable(ctx, page.name .. "##PageList" .. pageIdx, state.pageIndex == pageIdx) then state.pageIndex = pageIdx state.surfaceIndex = 1 state.listenerIndex = 1 end
                imgui.TableSetColumnIndex(ctx, 2)
                if imgui.Button(ctx, "⧉##DuplicatePage" .. pageIdx, actionSize, actionSize) then duplicatePage(data, pageIdx) end
                ui.ItemTooltip(ctx, "Duplicate " .. page.name)
                imgui.TableSetColumnIndex(ctx, 3)
                ui.Disabled(ctx, #data.pages <= 1, function() if imgui.Button(ctx, "×##RemovePage" .. pageIdx, actionSize, actionSize) then removePage(data, pageIdx) end end)
                ui.ItemTooltip(ctx, #data.pages <= 1 and "At least one Page is required" or ("Remove " .. page.name))
            end
            imgui.EndTable(ctx)
        end
    end
    endRoundedList(ctx)
    local page = data.pages[state.pageIndex]
    if not page then return end
    imgui.Spacing(ctx)
    imgui.Text(ctx, "Page settings")
    local changed
    changed, page.name = textField(ctx, "PageName", page.name)
    changed, page.followsMcp = imgui.Checkbox(ctx, "Follow MCP", page.followsMcp)
    changed, page.synchPages = imgui.Checkbox(ctx, "Synchronize Pages", page.synchPages)
    changed, page.scrollLink = imgui.Checkbox(ctx, "Scroll Link", page.scrollLink)
    changed, page.scrollSynch = imgui.Checkbox(ctx, "Scroll Synchronize", page.scrollSynch)
    local duplicate = false
    for pageIdx, otherPage in ipairs(data.pages) do if pageIdx ~= state.pageIndex and otherPage.name == page.name then duplicate = true end end
    if page.name == "" then localError(ctx, "Page name is required") elseif duplicate then localError(ctx, "Page name must be unique") end
end

local function duplicateSurface(data, page, surfaceIdx)
    local surface = page.surfaces[surfaceIdx]
    local unusedIo = availableIoItems(data, page)
    if not surface or #unusedIo == 0 then return end
    local copy = model.Clone(surface)
    copy.active = false
    copy.ioActive = false
    copy.ioType = ""
    copy.deviceId = unusedIo[1].value
    copy.name = unusedIo[1].value
    table.insert(page.surfaces, surfaceIdx + 1, copy)
    state.surfaceIndex = surfaceIdx + 1
end

local function removeSurface(page, surfaceIdx)
    local surface = page.surfaces[surfaceIdx]
    if not surface then return end
    table.remove(page.surfaces, surfaceIdx)
    for listenerIdx = #page.listeners, 1, -1 do if page.listeners[listenerIdx].broadcaster == surface.name or page.listeners[listenerIdx].listener == surface.name then table.remove(page.listeners, listenerIdx) end end
    state.surfaceIndex = clamp(state.surfaceIndex, #page.surfaces)
    state.listenerIndex = clamp(state.listenerIndex, #page.listeners)
end

local function renderAssignmentList(ctx, data, page, fonts)
    sectionHeader(ctx, "Surface assignments", fonts)
    local visible = beginRoundedList(ctx, "##SurfaceAssignmentsList", LIST_CHILD_HEIGHT)
    if visible then
        local canAdd = #availableIoItems(data, page) > 0 and #templateItems(data) > 0
        ui.Disabled(ctx, not canAdd, function() if imgui.Button(ctx, "Add Surface", 110, 0) then local surface = newSurface(data, page) if surface then page.surfaces[#page.surfaces + 1] = surface state.surfaceIndex = #page.surfaces end end end)
        imgui.Spacing(ctx)
        if imgui.BeginTable(ctx, "##SurfaceAssignmentRows", 4, 0, -1, 0) then
            local actionSize = theme.NOTIFICATIONS.close_button_size
            imgui.TableSetupColumn(ctx, "Status", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Name", imgui.TableColumnFlags_WidthStretch)
            imgui.TableSetupColumn(ctx, "Duplicate", imgui.TableColumnFlags_WidthFixed, actionSize)
            imgui.TableSetupColumn(ctx, "Remove", imgui.TableColumnFlags_WidthFixed, actionSize)
            for surfaceIdx, surface in ipairs(page.surfaces) do
                imgui.TableNextRow(ctx)
                imgui.TableSetColumnIndex(ctx, 0) renderListStatus(ctx, surface.active)
                imgui.TableSetColumnIndex(ctx, 1)
                if imgui.Selectable(ctx, surface.name .. "##AssignmentList" .. surfaceIdx, state.surfaceIndex == surfaceIdx) then state.surfaceIndex = surfaceIdx end
                imgui.TableSetColumnIndex(ctx, 2)
                local canDuplicate = #availableIoItems(data, page) > 0
                ui.Disabled(ctx, not canDuplicate, function() if imgui.Button(ctx, "⧉##DuplicateSurface" .. surfaceIdx, actionSize, actionSize) then duplicateSurface(data, page, surfaceIdx) end end)
                ui.ItemTooltip(ctx, canDuplicate and ("Duplicate " .. surface.name) or "Every I/O definition is already assigned on this Page")
                imgui.TableSetColumnIndex(ctx, 3)
                if imgui.Button(ctx, "×##RemoveSurface" .. surfaceIdx, actionSize, actionSize) then removeSurface(page, surfaceIdx) end
                ui.ItemTooltip(ctx, "Remove " .. surface.name)
            end
            imgui.EndTable(ctx)
        end
    end
    endRoundedList(ctx)
end

local function renderProfileActions(ctx, data, surface)
    local selectedProfile = findProfile(data, surface.mainProfile)
    ui.Disabled(ctx, not selectedProfile or not selectedProfile.vendorMain or selectedProfile.userMain, function() if imgui.Button(ctx, "Copy Main to User") then startRequest("CopyProfile", surface.mainProfile) end end)
    imgui.Text(ctx, "New User profile ID")
    ui.SetNextFormControlWidth(ctx, 140)
    state.createProfileId = select(2, imgui.InputText(ctx, "##NewProfile", state.createProfileId))
    imgui.SameLine(ctx)
    local validProfileId = state.createProfileId:match("^[A-Za-z0-9][A-Za-z0-9_-]*$") ~= nil
    ui.Disabled(ctx, not validProfileId, function() if imgui.Button(ctx, "Create User profile") then startRequest("CreateProfile", state.createProfileId) end end)
end

local function renderAssignmentEditor(ctx, data, page, surface, fonts)
    sectionHeader(ctx, "Surface assignment", fonts)
    local changed
    if beginForm(ctx, "##AssignmentMasterForm") then
        fieldRow(ctx, "Surface ID", function() changed, surface.name = textField(ctx, "AssignmentName", surface.name) end)
        fieldRow(ctx, "I/O definition", function() changed, surface.deviceId = ui.ComboEnum(ctx, "##AssignmentIo", surface.deviceId, ioItems(data)) end)
        fieldRow(ctx, "Surface template", function()
            changed, surface.surfaceId = ui.ComboEnum(ctx, "##AssignmentTemplate", surface.surfaceId, templateItems(data))
            if changed then local template = findTemplate(data, surface.surfaceId) surface.templateSource = template and template.source or "Missing" if surface.mainProfile == "" then surface.mainProfile = defaultProfileId(data, surface.surfaceId) end end
        end)
        fieldRow(ctx, "Start channel", function() changed, surface.startChannel = integerField(ctx, "AssignmentStart", surface.startChannel, 0, 65535) end)
        fieldRow(ctx, "Zone profile", function()
            changed, surface.mainProfile = ui.ComboEnum(ctx, "##AssignmentMain", surface.mainProfile, profileItems(data))
            if changed then local profile = findProfile(data, surface.mainProfile) surface.mainSource = profile and profile.mainSource or "Missing" end
        end)
        imgui.EndTable(ctx)
    end
    changed, surface.useDifferentFx = imgui.Checkbox(ctx, "Use a different FX profile", surface.useDifferentFx == true)
    if surface.useDifferentFx then
        if beginForm(ctx, "##AssignmentFxForm") then
            fieldRow(ctx, "FX Zone profile", function()
                changed, surface.fxProfile = ui.ComboEnum(ctx, "##AssignmentFx", surface.fxProfile, profileItems(data))
                if changed then local profile = findProfile(data, surface.fxProfile) surface.fxSource = profile and profile.fxSource or "Missing" end
            end)
            imgui.EndTable(ctx)
        end
    else
        surface.fxProfile = surface.mainProfile
        local profile = findProfile(data, surface.fxProfile)
        surface.fxSource = profile and profile.fxSource or "Missing"
    end
    imgui.Spacing(ctx)
    imgui.Text(ctx, "Zone profile actions")
    renderProfileActions(ctx, data, surface)
    local ioFound = false
    for itemIdx, item in ipairs(ioItems(data)) do if item.value == surface.deviceId then ioFound = true end end
    if not ioFound then localError(ctx, "Select an existing I/O definition") end
    local duplicateAssignment = false
    for surfaceIdx, otherSurface in ipairs(page.surfaces) do if otherSurface ~= surface and otherSurface.name == surface.name then duplicateAssignment = true end end
    if duplicateAssignment then localError(ctx, "This I/O definition is already assigned on the Page") end
    if not findTemplate(data, surface.surfaceId) then localError(ctx, "Select an existing Surface template") end
    if surface.mainProfile == "" or surface.mainSource == "Missing" or surface.mainSource == "Invalid" then localError(ctx, "Select or create a valid Main Zone profile before Save") end
    imgui.TextDisabled(ctx, "Template: " .. surface.templateSource .. "   Main: " .. surface.mainSource .. "   FX: " .. surface.fxSource)
    local runtimeIoType = surface.ioType ~= "" and surface.ioType or "Unresolved"
    renderRuntime(ctx, surface.active, "", surface.active and (runtimeIoType .. " I/O is connected. No action is required.") or "Check this assignment and its I/O definition, then Save changes to reconnect CSI.")
end

local function renderAssignmentsSection(ctx, data, fonts)
    state.pageIndex = clamp(state.pageIndex, #data.pages)
    local page = data.pages[state.pageIndex]
    if not page then imgui.TextDisabled(ctx, "Add a Page first") return end
    state.surfaceIndex = clamp(state.surfaceIndex, #page.surfaces)
    if imgui.BeginTable(ctx, "##AssignmentMasterDetail", 3, 0, -1, 0) then
        imgui.TableSetupColumn(ctx, "Pages", imgui.TableColumnFlags_WidthFixed, PAGE_LIST_WIDTH)
        imgui.TableSetupColumn(ctx, "Assignments", imgui.TableColumnFlags_WidthFixed, ASSIGNMENT_LIST_WIDTH)
        imgui.TableSetupColumn(ctx, "Editor", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0) renderPageList(ctx, data, fonts)
        page = data.pages[state.pageIndex]
        if not page then imgui.EndTable(ctx) return end
        state.surfaceIndex = clamp(state.surfaceIndex, #page.surfaces)
        imgui.TableSetColumnIndex(ctx, 1) renderAssignmentList(ctx, data, page, fonts)
        imgui.TableSetColumnIndex(ctx, 2)
        local surface = page.surfaces[state.surfaceIndex]
        if surface then renderAssignmentEditor(ctx, data, page, surface, fonts) else imgui.TextDisabled(ctx, "Add or select a Surface assignment") end
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
        imgui.TableSetupColumn(ctx, "List", imgui.TableColumnFlags_WidthFixed, LIST_WIDTH)
        imgui.TableSetupColumn(ctx, "Editor", imgui.TableColumnFlags_WidthStretch)
        imgui.TableNextRow(ctx)
        imgui.TableSetColumnIndex(ctx, 0)
        sectionHeader(ctx, "Relationships", fonts)
        local pageItems = {}
        for pageIdx, itemPage in ipairs(data.pages) do pageItems[#pageItems + 1] = { label = itemPage.name, value = pageIdx } end
        local pageChanged
        pageChanged, state.pageIndex = ui.ComboEnum(ctx, "Page##ListenerPage", state.pageIndex, pageItems, { width = LIST_WIDTH - 20 })
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

local function renderSectionNavigation(ctx)
    for sectionIdx, section in ipairs(SECTION_ITEMS) do
        if sectionIdx > 1 then imgui.SameLine(ctx) end
        local selected = state.section == section.id
        if selected then imgui.PushStyleColor(ctx, imgui.Col_Button, SELECTED_SECTION_COLOR) end
        if imgui.Button(ctx, section.label .. "##DevicesSection" .. section.id, 165, 0) then state.section = section.id end
        if selected then imgui.PopStyleColor(ctx) end
    end
    local actionSize = 26
    imgui.SameLine(ctx)
    local remainingWidth = imgui.GetContentRegionAvail(ctx)
    imgui.SetCursorPosX(ctx, imgui.GetCursorPosX(ctx) + math.max(0, remainingWidth - actionSize * 2 - theme.common.item_spacing))
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
    ui.Disabled(ctx, state.requestId ~= nil or module.IsDirty(), function() if imgui.Button(ctx, "↻##ReloadSavedDevices", actionSize, actionSize) then startQuery() end end)
    ui.ItemTooltip(ctx, module.IsDirty() and "Revert unsaved changes before reloading the saved configuration" or "Reload saved configuration")
    imgui.TextDisabled(ctx, "Create I/O  ->  assign it to a Page  ->  optionally connect listeners")
end

local function renderSectionDescription(ctx)
    if state.section == "IO" then imgui.TextDisabled(ctx, "Define the MIDI ports or OSC endpoint that CSI will use.")
    elseif state.section == "Assignments" then imgui.TextDisabled(ctx, "Place an I/O definition on a Page, then select its Surface template and Zone profile.")
    else imgui.TextDisabled(ctx, "Optionally forward selected CSI state from one Surface assignment to another on the same Page.") end
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
                state.pendingDraft.profileOptions = response.profileOptions
                state.pendingDraft.surfaceOptions = response.surfaceOptions
                state.pendingDraft.midiInputOptions = response.midiInputOptions
                state.pendingDraft.midiOutputOptions = response.midiOutputOptions
                state.pendingDraft.editorAvailable = response.editorAvailable
                state.pendingDraft.revision = response.revision
                state.data = state.pendingDraft
                state.pendingDraft = nil
                refreshDraftResourceStatus(state.data)
            else
                state.data = response
                state.savedData = model.Clone(response)
                state.savedSignature = model.Signature(response)
            end
            if state.afterQueryKind == "Apply" then
                local inactiveCount = inactiveRuntimeCount(response)
                state.status = inactiveCount == 0 and "Devices saved and reconnected" or ("Devices saved, but " .. inactiveCount .. " runtime item(s) are inactive or unavailable")
            end
            state.afterQueryKind = ""
        elseif completedKind == "OpenEditor" then
            state.status = response.message
            state.afterQueryKind = ""
        else
            state.status = response.message
            state.createProfileId = completedKind == "CreateProfile" and "" or state.createProfileId
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

function module.Validate()
    if not state.data then return false, "Device configuration is not loaded" end
    if state.data.fatalError ~= "" or #state.data.issues > 0 or state.data.skippedSurfaceCount > 0 then return false, "Fix the listed configuration issues in the standalone editor before saving Devices" end
    for midiIdx, device in ipairs(state.data.midi) do if device.inputPort < 0 or device.outputPort < 0 then return false, "Select input and output ports for MIDI I/O " .. device.name end end
    local source, serializationError = model.Serialize(state.data)
    if not source then return false, serializationError end
    if #state.data.pages == 0 then return false, "At least one Page is required" end
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
    state.error = ""
    state.status = ""
    return true
end

function module.RenderPage(ctx, fonts)
    module.Initialize()
    renderSectionNavigation(ctx)
    renderSectionDescription(ctx)
    if state.requestId then imgui.TextDisabled(ctx, state.pendingKind .. "...") end
    if state.error ~= "" then imgui.TextWrapped(ctx, "Error: " .. state.error) end
    if state.status ~= "" then imgui.TextWrapped(ctx, state.status) end
    if not state.data then return end
    imgui.Spacing(ctx)
    if state.section == "IO" then renderIoSection(ctx, state.data, fonts)
    elseif state.section == "Assignments" then renderAssignmentsSection(ctx, state.data, fonts)
    else renderListenersSection(ctx, state.data, fonts) end
    renderIssues(ctx, state.data)
end

function module.RenderModal(ctx)
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
            state.deleteIo = nil
            imgui.CloseCurrentPopup(ctx)
        end
        imgui.EndPopup(ctx)
    end
    if state.confirmSource then imgui.OpenPopup(ctx, "Reconnect CSI##DevicesReconnect") end
    local confirmVisible = imgui.BeginPopupModal(ctx, "Reconnect CSI##DevicesReconnect", nil, imgui.WindowFlags_AlwaysAutoResize)
    if confirmVisible then
        imgui.TextWrapped(ctx, "Saving Devices disconnects and reconnects active control surfaces. Continue?")
        if imgui.Button(ctx, "Save and reconnect", 150, 0) then
            local source = state.confirmSource
            state.confirmSource = nil
            startRequest("Apply", nil, source)
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
end

return module
