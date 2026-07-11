local r = reaper
local imgui = require "imgui" "0.9.3"

local M = {}

local state = {
	isOpen = false,
	surfaceName = "",
	widgetName = "",
	zoneName = "",
	zoneFilePath = "",
	bindings = {},
	selectedBinding = 1,
	status = "",
	searchQuery = "",
	searchMode = "all", -- "csi" | "reaper" | "all"
	searchModeIndex = 0,
	searchResults = {},
	searchSelected = 0,
	csiActions = {},
	suppressWindowContextMenuUntil = 0,
	confirmedBindings = {},
	confirmedSerialized = "",
	hasUnappliedEdits = false,
	hasLiveChanges = false,
	isDirty = false,
	pendingOperation = nil,
	pendingSerialized = nil,
	queryExpectedSerialized = nil,
	forceAcceptQuery = false,
	saveAfterApply = false,
}

local SEARCH_MODE_ITEMS = "all\0csi\0reaper\0"
local SEARCH_MODE_BY_INDEX = { "all", "csi", "reaper" }
local CONFIG_WINDOW_FLAGS = imgui.WindowFlags_NoCollapse
if imgui.WindowFlags_NoSavedSettings then
	CONFIG_WINDOW_FLAGS = CONFIG_WINDOW_FLAGS | imgui.WindowFlags_NoSavedSettings
end

local MODIFIER_FLAGS = {
	{ name = "Shift", bit = 4 },
	{ name = "Option", bit = 8 },
	{ name = "Control", bit = 16 },
	{ name = "Alt", bit = 32 },
	{ name = "Flip", bit = 64 },
	{ name = "Global", bit = 128 },
	{ name = "Marker", bit = 256 },
	{ name = "Nudge", bit = 512 },
	{ name = "Zoom", bit = 1024 },
	{ name = "Scrub", bit = 2048 },
}

local function syncSearchModeFromIndex()
	local mode = SEARCH_MODE_BY_INDEX[(state.searchModeIndex or 0) + 1] or "all"
	state.searchMode = mode
end

local function syncSearchIndexFromMode()
	if state.searchMode == "csi" then
		state.searchModeIndex = 1
	elseif state.searchMode == "reaper" then
		state.searchModeIndex = 2
	else
		state.searchModeIndex = 0
		state.searchMode = "all"
	end
end

local function splitTokens(text)
	local out = {}
	for tok in tostring(text or ""):gmatch("%S+") do
		out[#out + 1] = tok
	end
	return out
end

local function tokenizePreservingQuotes(text)
	local tokens = {}
	local source = tostring(text or "")
	local current = ""
	local inQuote = false
	local idx = 1
	while idx <= #source do
		local ch = source:sub(idx, idx)
		if ch == '"' then
			inQuote = not inQuote
			current = current .. ch
		elseif (ch == " " or ch == "\t") and not inQuote then
			if current ~= "" then
				tokens[#tokens + 1] = current
				current = ""
			end
		else
			current = current .. ch
		end
		idx = idx + 1
	end
	if current ~= "" then
		tokens[#tokens + 1] = current
	end
	return tokens
end

local function unquoteValue(value)
	local text = tostring(value or "")
	if #text >= 2 and text:sub(1, 1) == '"' and text:sub(-1) == '"' then
		return text:sub(2, -2)
	end
	return text
end

local function quoteIfNeeded(value)
	local text = tostring(value or "")
	if text == "" then return text end
	if text:find("%s") then
		text = text:gsub('"', '\\"')
		return '"' .. text .. '"'
	end
	return text
end

local function parseActionLine(line)
	local tokens = tokenizePreservingQuotes(line)
	local parts = {
		actionName = tokens[1] or "",
		params = {},
		properties = {},
	}

	for tokenIdx = 2, #tokens do
		local token = tokens[tokenIdx]
		local key, value = token:match("^(.-)=(.+)$")
		if key and value then
			parts.properties[key] = unquoteValue(value)
		else
			parts.params[#parts.params + 1] = token
		end
	end

	return parts
end

local function buildActionLine(parts)
	local out = {}
	out[#out + 1] = (parts.actionName and parts.actionName ~= "") and parts.actionName or "NoAction"

	for _, param in ipairs(parts.params or {}) do
		if param ~= "" then
			out[#out + 1] = tostring(param)
		end
	end

	local used = {}
	local priorityKeys = { "Feedback", "HoldDelay", "HoldRepeatInterval", "RunCount", "OSD" }
	for _, key in ipairs(priorityKeys) do
		local value = parts.properties and parts.properties[key]
		if value ~= nil and tostring(value) ~= "" then
			out[#out + 1] = key .. "=" .. quoteIfNeeded(value)
			used[key] = true
		end
	end

	local remaining = {}
	for key, value in pairs(parts.properties or {}) do
		if not used[key] and value ~= nil and tostring(value) ~= "" then
			remaining[#remaining + 1] = key
		end
	end
	table.sort(remaining)
	for _, key in ipairs(remaining) do
		out[#out + 1] = key .. "=" .. quoteIfNeeded(parts.properties[key])
	end

	return table.concat(out, " ")
end

local function parseBindingString(raw)
	local bindings = {}
	for entry in tostring(raw or ""):gmatch("[^;]+") do
		local modStr, line = entry:match("^(%-?%d+):(.+)$")
		if modStr and line then
			local metadata = {
				hasHold = false,
				hasDoublePress = false,
				isValueInverted = false,
				isFeedbackInverted = false,
				isIncrease = false,
				isDecrease = false,
			}
			while true do
				local token = line:match("%s+(__OSK_[A-Z_]+)$")
				if not token then break end
				line = line:sub(1, #line - #token - 1)
				if token == "__OSK_HOLD" then metadata.hasHold = true
				elseif token == "__OSK_DOUBLE_PRESS" then metadata.hasDoublePress = true
				elseif token == "__OSK_INVERT" then metadata.isValueInverted = true
				elseif token == "__OSK_INVERT_FB" then metadata.isFeedbackInverted = true
				elseif token == "__OSK_INCREASE" then metadata.isIncrease = true
				elseif token == "__OSK_DECREASE" then metadata.isDecrease = true
				end
			end
			local tokens = splitTokens(line)
			bindings[#bindings + 1] = {
				mod = tonumber(modStr) or 0,
				line = line,
				actionName = tokens[1] or "",
				hasHold = metadata.hasHold,
				hasDoublePress = metadata.hasDoublePress,
				isValueInverted = metadata.isValueInverted,
				isFeedbackInverted = metadata.isFeedbackInverted,
				isIncrease = metadata.isIncrease,
				isDecrease = metadata.isDecrease,
			}
		end
	end
	return bindings
end

local function parseCsv(text)
	local out = {}
	for entry in tostring(text or ""):gmatch("[^,]+") do
		if entry ~= "" then out[#out + 1] = entry end
	end
	return out
end

local function pollResponse(key)
	if not r.HasExtState("CSI_OSK", key) then return nil end
	local value = r.GetExtState("CSI_OSK", key)
	r.DeleteExtState("CSI_OSK", key, false)
	return value
end

local function refreshSearchResults()
	local query = state.searchQuery:lower()
	local results = {}

	if state.searchMode ~= "reaper" then
		for _, name in ipairs(state.csiActions) do
			if query == "" or name:lower():find(query, 1, true) then
				results[#results + 1] = "[CSI] " .. name
			end
		end
	end

	if state.searchMode ~= "csi" then
		local function nextAction(index)
			-- Preferred path: REAPER API
			if type(r.EnumerateActions) == "function" then
				local ok, a, b = pcall(r.EnumerateActions, 0, index)
				if ok and type(a) == "number" then
					return a, tostring(b or "")
				end
			end
			-- Optional fallback: SWS API (if present)
			if type(r.CF_EnumerateActions) == "function" then
				local ok, cmdId, name = pcall(r.CF_EnumerateActions, 0, index, "")
				if ok and type(cmdId) == "number" then
					return cmdId, tostring(name or "")
				end
			end
			return 0, ""
		end

		local idx = 0
		local maxResults = 60
		while #results < maxResults do
			local cmdId, actionName = nextAction(idx)
			if cmdId == 0 then break end
			idx = idx + 1

			if query == "" or actionName:lower():find(query, 1, true) then
				results[#results + 1] = string.format("[REAPER] %d - %s", cmdId, actionName)
			end
		end
	end

	state.searchResults = results
end

local function serializeBindings(bindings)
	local chunks = {}
	for _, binding in ipairs(bindings or {}) do
		if binding and binding.line and binding.line ~= "" then
			local line = binding.line
			if binding.hasHold then line = line .. " __OSK_HOLD" end
			if binding.hasDoublePress then line = line .. " __OSK_DOUBLE_PRESS" end
			if binding.isValueInverted then line = line .. " __OSK_INVERT" end
			if binding.isFeedbackInverted then line = line .. " __OSK_INVERT_FB" end
			if binding.isIncrease then line = line .. " __OSK_INCREASE" end
			if binding.isDecrease then line = line .. " __OSK_DECREASE" end
			chunks[#chunks + 1] = tostring(binding.mod or 0) .. ":" .. line
		end
	end
	return table.concat(chunks, ";")
end

local function cloneBindings(bindings)
	local copy = {}
	for _, binding in ipairs(bindings or {}) do
		copy[#copy + 1] = {
			mod = binding.mod or 0,
			line = binding.line or "",
			actionName = binding.actionName or "",
			hasHold = binding.hasHold == true,
			hasDoublePress = binding.hasDoublePress == true,
			isValueInverted = binding.isValueInverted == true,
			isFeedbackInverted = binding.isFeedbackInverted == true,
			isIncrease = binding.isIncrease == true,
			isDecrease = binding.isDecrease == true,
		}
	end
	return copy
end

local function updateDirtyState()
	state.hasUnappliedEdits = serializeBindings(state.bindings) ~= state.confirmedSerialized
	state.isDirty = state.hasUnappliedEdits or state.hasLiveChanges
end

local function acceptBindings(bindings, replaceVisibleBindings)
	if replaceVisibleBindings then state.bindings = bindings end
	state.confirmedBindings = cloneBindings(bindings)
	state.confirmedSerialized = serializeBindings(bindings)
	updateDirtyState()
end

local function setLocalStatus(outcome, operation, message)
	state.status = table.concat({ outcome or "", operation or "", message or "" }, " | ")
end

local function sendConfigQuery(expectedSerialized, forceAccept)
	local payload = state.surfaceName .. "|" .. state.widgetName
	state.pendingOperation = "Query"
	state.queryExpectedSerialized = expectedSerialized
	state.forceAcceptQuery = forceAccept == true
	r.SetExtState("CSI_OSK_CMD", "ConfigQuery", payload, false)
end

local function sendApplyLive()
	local serialized = serializeBindings(state.bindings)
	for _, binding in ipairs(state.bindings) do
		if tostring(binding.line or ""):find(";", 1, true) then
			setLocalStatus("ERR", "ApplyLive", "Semicolons are not supported in binding lines")
			return false
		end
	end
	if serialized:find("[\r\n]") then
		setLocalStatus("ERR", "ApplyLive", "Line breaks are not supported in bindings")
		return false
	end

	local payload = state.surfaceName .. "|" .. state.widgetName .. "|" .. serialized
	state.pendingOperation = "ApplyLive"
	state.pendingSerialized = serialized
	r.SetExtState("CSI_OSK_CMD", "ConfigApplyLive", payload, false)
	return true
end

local function sendSave()
	local payload = state.surfaceName .. "|" .. state.widgetName
	state.pendingOperation = "Save"
	state.pendingSerialized = serializeBindings(state.bindings)
	r.SetExtState("CSI_OSK_CMD", "ConfigSave", payload, false)
end

local function requestRevert()
	if state.surfaceName == "" or state.widgetName == "" then return end
	state.pendingOperation = "Revert"
	state.pendingSerialized = nil
	local payload = state.surfaceName .. "|" .. state.widgetName
	r.SetExtState("CSI_OSK_CMD", "ConfigRevert", payload, false)
end

local function closeEditor()
	if state.hasLiveChanges or state.pendingOperation == "ApplyLive" then
		requestRevert()
	end
	state.isOpen = false
end

local function refreshBindingDerivedFields(binding)
	if not binding then return end
	local tokens = splitTokens(binding.line)
	binding.actionName = tokens[1] or ""
end

local function getSelectedBinding()
	if state.selectedBinding < 1 then return nil end
	return state.bindings[state.selectedBinding]
end

local function clampSelectedBindingIndex()
	if #state.bindings == 0 then
		state.selectedBinding = 1
		return
	end
	if state.selectedBinding < 1 then state.selectedBinding = 1 end
	if state.selectedBinding > #state.bindings then
		state.selectedBinding = #state.bindings
	end
end

local function addBinding()
	state.bindings[#state.bindings + 1] = {
		mod = 0,
		line = "NoAction",
		actionName = "NoAction",
	}
	state.selectedBinding = #state.bindings
	updateDirtyState()
end

local function removeSelectedBinding()
	if #state.bindings == 0 then return end
	table.remove(state.bindings, state.selectedBinding)
	clampSelectedBindingIndex()
	updateDirtyState()
end

local function duplicateSelectedBinding()
	local selected = getSelectedBinding()
	if not selected then return end
	local copy = {
		mod = selected.mod or 0,
		line = selected.line or "NoAction",
		actionName = selected.actionName or "NoAction",
		hasHold = selected.hasHold == true,
		hasDoublePress = selected.hasDoublePress == true,
		isValueInverted = selected.isValueInverted == true,
		isFeedbackInverted = selected.isFeedbackInverted == true,
		isIncrease = selected.isIncrease == true,
		isDecrease = selected.isDecrease == true,
	}
	table.insert(state.bindings, state.selectedBinding + 1, copy)
	state.selectedBinding = state.selectedBinding + 1
	updateDirtyState()
end

local function moveSelectedBinding(offset)
	if #state.bindings < 2 then return end
	local fromIndex = state.selectedBinding
	local toIndex = fromIndex + offset
	if toIndex < 1 or toIndex > #state.bindings then return end
	local row = table.remove(state.bindings, fromIndex)
	table.insert(state.bindings, toIndex, row)
	state.selectedBinding = toIndex
	updateDirtyState()
end

local function addBindingPreset(presetName)
	local presetLine = "NoAction"
	if presetName == "reaper" then
		presetLine = "Reaper 40044"
	elseif presetName == "track_volume" then
		presetLine = "TrackVolume"
	elseif presetName == "track_pan" then
		presetLine = "TrackPan"
	end

	state.bindings[#state.bindings + 1] = {
		mod = 0,
		line = presetLine,
		actionName = splitTokens(presetLine)[1] or "NoAction",
	}
	state.selectedBinding = #state.bindings
	updateDirtyState()
end

local function applySearchSelectionToBinding(binding)
	if not binding then return end
	local row = state.searchResults[state.searchSelected]
	if not row then return end

	local csiAction = row:match("^%[CSI%]%s+(.+)$")
	if csiAction then
		binding.line = csiAction
		refreshBindingDerivedFields(binding)
		updateDirtyState()
		return
	end

	local commandId = row:match("^%[REAPER%]%s+(%d+)%s+%-")
	if commandId then
		binding.line = "Reaper " .. commandId
		refreshBindingDerivedFields(binding)
		updateDirtyState()
	end
end

local function parseConfigStatus(rawStatus)
	local outcome, operation, surfaceName, widgetName, zoneName, message =
		tostring(rawStatus or ""):match("^([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|(.*)$")
	if not outcome then
		return nil
	end
	return {
		outcome = outcome,
		operation = operation,
		surfaceName = surfaceName,
		widgetName = widgetName,
		zoneName = zoneName,
		message = message,
	}
end

local function pollConfigResponses()
	if not state.isOpen then return end

	local surf = state.surfaceName
	local widget = state.widgetName
	if surf == "" or widget == "" then return end

	local resultKey = "ConfigResult_" .. surf .. "_" .. widget
	local result = pollResponse(resultKey)
	if result ~= nil then
		local parsedBindings = parseBindingString(result)
		local currentSerialized = serializeBindings(state.bindings)
		local shouldReplaceVisible = state.forceAcceptQuery
			or state.queryExpectedSerialized == nil
			or currentSerialized == state.queryExpectedSerialized
		acceptBindings(parsedBindings, shouldReplaceVisible)
		state.queryExpectedSerialized = nil
		state.forceAcceptQuery = false
		state.searchSelected = 0
		if state.selectedBinding < 1 then state.selectedBinding = 1 end
		if state.selectedBinding > #state.bindings then
			state.selectedBinding = math.max(1, #state.bindings)
		end
	end

	local zoneKey = "ConfigZoneName_" .. surf .. "_" .. widget
	local zone = pollResponse(zoneKey)
	if zone ~= nil then state.zoneName = zone end

	local pathKey = "ConfigZonePath_" .. surf .. "_" .. widget
	local path = pollResponse(pathKey)
	if path ~= nil then state.zoneFilePath = path end

	local scopedStatusKey = "ConfigStatus_" .. surf .. "_" .. widget
	local rawStatus = pollResponse(scopedStatusKey)
	local legacyStatus = pollResponse("ConfigStatus")
	if rawStatus == nil then rawStatus = legacyStatus end
	if rawStatus ~= nil then
		local status = parseConfigStatus(rawStatus)
		if status
			and status.surfaceName == state.surfaceName
			and status.widgetName == state.widgetName then
			setLocalStatus(status.outcome, status.operation, status.message)
			local completedSerialized = state.pendingSerialized
			state.pendingOperation = nil
			state.pendingSerialized = nil

			if status.outcome == "OK" then
				if status.operation == "ApplyLive" then
					state.hasLiveChanges = true
					updateDirtyState()
					if state.saveAfterApply then
						state.saveAfterApply = false
						sendSave()
					else
						sendConfigQuery(completedSerialized, false)
					end
				elseif status.operation == "Save" then
					state.hasLiveChanges = false
					updateDirtyState()
					sendConfigQuery(completedSerialized, false)
				elseif status.operation == "Revert" then
					state.hasLiveChanges = false
					updateDirtyState()
					if state.isOpen then sendConfigQuery(nil, true) end
				end
			elseif status.operation == "ApplyLive" then
				state.saveAfterApply = false
			end
		end
	end

	local actions = pollResponse("ActionList")
	if actions ~= nil then
		state.csiActions = parseCsv(actions)
		refreshSearchResults()
	end
end

function M.OpenConfigEditor(surfName, widgetName)
	if not surfName or surfName == "" or not widgetName or widgetName == "" then return end

	state.isOpen = true
	state.surfaceName = surfName
	state.widgetName = widgetName
	state.zoneName = ""
	state.zoneFilePath = ""
	state.bindings = {}
	state.confirmedBindings = {}
	state.confirmedSerialized = ""
	state.hasUnappliedEdits = false
	state.hasLiveChanges = false
	state.isDirty = false
	state.pendingOperation = nil
	state.pendingSerialized = nil
	state.queryExpectedSerialized = nil
	state.forceAcceptQuery = false
	state.saveAfterApply = false
	state.selectedBinding = 1
	state.status = ""
	state.searchSelected = 0
	syncSearchIndexFromMode()

	state.suppressWindowContextMenuUntil = os.clock() + 0.20

	sendConfigQuery("", true)
	if #state.csiActions == 0 then
		r.SetExtState("CSI_OSK_CMD", "ActionListQuery", "", false)
	else
		refreshSearchResults()
	end
end

function M.HandleShutdown()
	if state.isOpen or state.hasLiveChanges or state.pendingOperation == "ApplyLive" then
		closeEditor()
	end
end

function M.ShouldSuppressContextMenu()
	return os.clock() < state.suppressWindowContextMenuUntil
end

function M.RenderConfigEditor(ctx)
	if not state.isOpen then return end

	pollConfigResponses()

	local dirtyMarker = state.isDirty and " *" or ""
	local title = "Widget config: [" .. state.widgetName .. "]  @" .. state.surfaceName .. "/" .. state.zoneName .. dirtyMarker .. " ###osk_widget_config"
	imgui.SetNextWindowSize(ctx, 520, 520, imgui.Cond_Appearing)
	local visible, open = imgui.Begin(ctx, title, true, CONFIG_WINDOW_FLAGS)
	if open == false then
		closeEditor()
		imgui.End(ctx)
		return
	end

	if visible then
		if state.status ~= "" then
			imgui.Text(ctx, "Status: " .. state.status)
		end
		if state.pendingOperation then
			imgui.TextDisabled(ctx, "Pending: " .. state.pendingOperation)
		end

-- need to do next changes:

-- bindings should be table like this:
-- |modifier|action|colors|other|
-- |--------|------|------|-----|
-- | - | actionTitle*|[i][a]**| |
-- | Shift| AnotherActionTitle|[i][a]| |
-- | Hold*** | 
-- ...

-- * action title by default is talen from OSD, otherwise whatever we have after widget, but missing OSD text must be generated on edit, for example from csi/reaper action name like we do with some global garbage words replacing in osk lua script using label_replacements
-- ** [i] - inactive color square that opens colorpicker with pallete, [a] - same for active ... . this colors should be somewhere in cpp since they can be defined in .zon files like this  WidgetName  ActionName { 10 0 0 120 0 0 } - 10 0 0 inactive R G B, 120 0 0 - active.
-- *** need to add support of virtual modifiers like Hold, DoublePress, that are alreasy implemented in cpp

-- need to plan add inactive/active color picker for RGB widgets 


		local listHeight = 170
		if imgui.BeginListBox(ctx, "##bindings", -1, listHeight) then
			for i, b in ipairs(state.bindings) do
				local label = string.format("[%d] %s", b.mod or 0, b.line or "")
				local selected = (i == state.selectedBinding)
				if imgui.Selectable(ctx, label, selected) then
					state.selectedBinding = i
				end
			end
			imgui.EndListBox(ctx)
		end

		imgui.Spacing(ctx)
		if imgui.Button(ctx, "+ Add") then
			addBinding()
		end
		imgui.SameLine(ctx)
		if imgui.Button(ctx, "- Remove") then
			removeSelectedBinding()
		end
		imgui.SameLine(ctx)
		if imgui.Button(ctx, "Clone") then
			duplicateSelectedBinding()
		end

		-- Horizontal spacer
		imgui.SameLine(ctx)
		imgui.Dummy(ctx, 15, 0)
		imgui.SameLine(ctx)
		if imgui.Button(ctx, "Move Up") then
			moveSelectedBinding(-1)
		end
		imgui.SameLine(ctx)
		if imgui.Button(ctx, "Move Down") then
			moveSelectedBinding(1)
		end
        
        -- Horizontal spacer
		imgui.SameLine(ctx)
		imgui.Dummy(ctx, 15, 0)
		imgui.SameLine(ctx)
		if imgui.Button(ctx, "Add preset: NoAction") then
			addBindingPreset("no_action")
		end

		local selected = getSelectedBinding()
		if selected then
			imgui.Text(ctx, "Selected action: " .. (selected.actionName ~= "" and selected.actionName or "(unknown)"))
			local lineChanged
			lineChanged, selected.line = imgui.InputText(ctx, "Raw", selected.line or "")
			if lineChanged then
				refreshBindingDerivedFields(selected)
				updateDirtyState()
			end
			
			for idx, modifier in ipairs(MODIFIER_FLAGS) do
				local hasFlag = ((selected.mod or 0) & modifier.bit) ~= 0
				local toggled
				toggled, hasFlag = imgui.Checkbox(ctx, modifier.name .. "##mod_" .. idx, hasFlag)
				if toggled then
					if hasFlag then
						selected.mod = (selected.mod or 0) | modifier.bit
					else
						selected.mod = (selected.mod or 0) & (~modifier.bit)
					end
					updateDirtyState()
				end
				if idx % 5 ~= 0 then imgui.SameLine(ctx) end
			end

			imgui.Text(ctx, "Quick editor")
			local parts = parseActionLine(selected.line)
			local changedQuick = false

			local osdText = tostring(parts.properties.OSD or "")
			local osdChanged
			osdChanged, osdText = imgui.InputText(ctx, "OSD", osdText)
			if osdChanged then
				parts.properties.OSD = (osdText ~= "") and osdText or nil
				changedQuick = true
			end

			-- local actionChanged
			-- actionChanged, parts.actionName = imgui.InputText(ctx, "Action", parts.actionName or "")
			-- if actionChanged then changedQuick = true end

			-- local paramsText = table.concat(parts.params or {}, " ")
			-- local paramsChanged
			-- paramsChanged, paramsText = imgui.InputText(ctx, "Params", paramsText)
			-- if paramsChanged then
			-- 	parts.params = splitTokens(paramsText)
			-- 	changedQuick = true
			-- end

			local feedbackNo = tostring(parts.properties.Feedback or ""):lower() == "no"
			local feedbackChanged
			feedbackChanged, feedbackNo = imgui.Checkbox(ctx, "No Feedback", feedbackNo)
			if feedbackChanged then
				if feedbackNo then parts.properties.Feedback = "No" else parts.properties.Feedback = nil end
				changedQuick = true
			end



			local holdDelayVal = tonumber(parts.properties.HoldDelay) or 0
			local holdDelayChanged
			imgui.SetNextItemWidth(ctx, 120)
			holdDelayChanged, holdDelayVal = imgui.DragInt(ctx, "##hold_delay", holdDelayVal, 100, 0, 10000, "HoldDelay = %d ms")
			if holdDelayChanged then
				parts.properties.HoldDelay = (holdDelayVal > 0) and tostring(holdDelayVal) or nil
				changedQuick = true
			end

			imgui.SameLine(ctx, 0, 5)
			local runCountVal = tonumber(parts.properties.RunCount) or 1
			local runCountChanged
			imgui.SetNextItemWidth(ctx, 120)
			runCountChanged, runCountVal = imgui.DragInt(ctx, "##run_count", runCountVal, 1, 1, 10, "RunCount = %d")
			if runCountChanged then
				parts.properties.RunCount = (runCountVal > 1) and tostring(runCountVal) or nil
				changedQuick = true
			end

			imgui.SameLine(ctx, 0, 5)
			local holdRepeatVal = tonumber(parts.properties.HoldRepeatInterval) or 0
			local holdRepeatChanged
			imgui.SetNextItemWidth(ctx, 120)
			holdRepeatChanged, holdRepeatVal = imgui.DragInt(ctx, "##hold_repeat", holdRepeatVal, 10, 0, 1000, "Interval = %d ms")
			if holdRepeatChanged then
				parts.properties.HoldRepeatInterval = (holdRepeatVal > 0) and tostring(holdRepeatVal) or nil
				changedQuick = true
			end
			



			if changedQuick then
				selected.line = buildActionLine(parts)
				refreshBindingDerivedFields(selected)
				updateDirtyState()
			end

		else
			imgui.Text(ctx, "No binding for this widget in the active zone.")
		end

		imgui.Separator(ctx)
		imgui.Text(ctx, "Action Search (read-only)")

		local changed
		changed, state.searchQuery = imgui.InputText(ctx, "Search", state.searchQuery)
		if changed then refreshSearchResults() end

		local modeChanged
		modeChanged, state.searchModeIndex = imgui.Combo(ctx, "Source", state.searchModeIndex, SEARCH_MODE_ITEMS)
		if modeChanged then
			syncSearchModeFromIndex()
			refreshSearchResults()
		end

		if imgui.BeginListBox(ctx, "##search_results", -1, 140) then
			for idx, row in ipairs(state.searchResults) do
				local isSelected = (idx == state.searchSelected)
				if imgui.Selectable(ctx, row, isSelected) then
					state.searchSelected = idx
				end
			end
			imgui.EndListBox(ctx)
		end

		if imgui.Button(ctx, "Apply search result to selected") then
			applySearchSelectionToBinding(getSelectedBinding())
		end

		imgui.Separator(ctx)
		if imgui.Button(ctx, "Apply Live") and state.pendingOperation == nil then
			state.saveAfterApply = false
			sendApplyLive()
		end

		imgui.SameLine(ctx)
		if imgui.Button(ctx, "Save") and state.pendingOperation == nil then
			if not state.isDirty then
				setLocalStatus("OK", "Save", "No changes to save")
			elseif state.hasUnappliedEdits then
				state.saveAfterApply = true
				if not sendApplyLive() then state.saveAfterApply = false end
			else
				sendSave()
			end
		end

		imgui.SameLine(ctx)
		if imgui.Button(ctx, "Revert") and state.pendingOperation == nil then
			requestRevert()
		end

		imgui.TextDisabled(ctx, "Use Apply Live to test changes immediately, then Save to persist in .zon.")
	end

	imgui.End(ctx)
end

return M
