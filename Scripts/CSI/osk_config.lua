local r = reaper
local imgui = require "imgui" "0.9.3"
local data = require("osk_data")
local ui = require("ui_components")

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
if imgui.WindowFlags_NoDocking then
	CONFIG_WINDOW_FLAGS = CONFIG_WINDOW_FLAGS | imgui.WindowFlags_NoDocking
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

local COLOR_PALETTE = {
	{ name = "Black", value = 0x000000ff },
	{ name = "White", value = 0xffffffff },
	{ name = "Gray", value = 0x808080ff },
	{ name = "Red", value = 0xff3030ff },
	{ name = "Orange", value = 0xff8a20ff },
	{ name = "Yellow", value = 0xffd830ff },
	{ name = "Green", value = 0x40c060ff },
	{ name = "Cyan", value = 0x30c8d8ff },
	{ name = "Blue", value = 0x4080ffff },
	{ name = "Purple", value = 0xa060e0ff },
}

local TABLE_FLAGS = imgui.TableFlags_Borders
	| imgui.TableFlags_RowBg
	| imgui.TableFlags_Resizable
	| imgui.TableFlags_ScrollY

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
		if inQuote and ch == "\\" and idx < #source then
			current = current .. ch .. source:sub(idx + 1, idx + 1)
			idx = idx + 1
		elseif ch == '"' then
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

local function unescapeQuotedText(text)
	local out = {}
	local idx = 1
	while idx <= #text do
		local ch = text:sub(idx, idx)
		if ch == "\\" and idx < #text then
			local nextCh = text:sub(idx + 1, idx + 1)
			if nextCh == '"' or nextCh == "\\" then
				out[#out + 1] = nextCh
				idx = idx + 2
			else
				out[#out + 1] = ch
				idx = idx + 1
			end
		else
			out[#out + 1] = ch
			idx = idx + 1
		end
	end
	return table.concat(out)
end

local function unquoteValue(value)
	local text = tostring(value or "")
	if #text >= 2 and text:sub(1, 1) == '"' and text:sub(-1) == '"' then
		return unescapeQuotedText(text:sub(2, -2))
	end
	return text
end

local function quoteIfNeeded(value)
	local text = tostring(value or "")
	if text == "" then return text end
	if text:find("[%s\"]") or text:find("\\", 1, true) then
		text = text:gsub("\\", "\\\\")
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
		colorTokens = nil,
	}

	local tokenIdx = 2
	while tokenIdx <= #tokens do
		local token = tokens[tokenIdx]
		if token == "{" then
			local colorTokens = {}
			tokenIdx = tokenIdx + 1
			while tokenIdx <= #tokens and tokens[tokenIdx] ~= "}" do
				colorTokens[#colorTokens + 1] = tokens[tokenIdx]
				tokenIdx = tokenIdx + 1
			end
			if tokenIdx <= #tokens and tokens[tokenIdx] == "}" then
				parts.colorTokens = colorTokens
			else
				parts.params[#parts.params + 1] = "{"
				for _, colorToken in ipairs(colorTokens) do
					parts.params[#parts.params + 1] = colorToken
				end
			end
		else
			local key, value = token:match("^(.-)=(.+)$")
			if key and value then
				parts.properties[key] = unquoteValue(value)
			else
				parts.params[#parts.params + 1] = unquoteValue(token)
			end
		end
		tokenIdx = tokenIdx + 1
	end

	return parts
end

local function buildActionLine(parts)
	local out = {}
	out[#out + 1] = (parts.actionName and parts.actionName ~= "") and parts.actionName or "NoAction"

	for _, param in ipairs(parts.params or {}) do
		if param ~= "" then
			out[#out + 1] = quoteIfNeeded(param)
		end
	end

	if parts.colorTokens and #parts.colorTokens > 0 then
		out[#out + 1] = "{"
		for _, colorToken in ipairs(parts.colorTokens) do
			out[#out + 1] = tostring(colorToken)
		end
		out[#out + 1] = "}"
	end

	local used = {}
	local priorityKeys = { "Feedback", "HoldDelay", "HoldRepeatInterval", "RunCount", "OSD", "KeyLabel" }
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

local function clampColorChannel(value)
	return math.max(0, math.min(255, math.floor(tonumber(value) or 0)))
end

local function packRgb(red, green, blue)
	return (clampColorChannel(red) << 24)
		| (clampColorChannel(green) << 16)
		| (clampColorChannel(blue) << 8)
		| 0xff
end

local function unpackRgb(color)
	return (color >> 24) & 0xff, (color >> 16) & 0xff, (color >> 8) & 0xff
end

local function parseHexColor(token)
	local hex = tostring(token or ""):match("^#?(%x%x%x%x%x%x)")
	if not hex then return nil end
	return packRgb(
		tonumber(hex:sub(1, 2), 16),
		tonumber(hex:sub(3, 4), 16),
		tonumber(hex:sub(5, 6), 16)
	)
end

local function parseActionColors(parts)
	local colorTokens = parts and parts.colorTokens
	if not colorTokens or #colorTokens == 0 then return nil end

	local colors = {}
	if tostring(colorTokens[1]):sub(1, 1) == "#" then
		for _, colorToken in ipairs(colorTokens) do
			local color = parseHexColor(colorToken)
			if color then colors[#colors + 1] = color end
		end
	else
		local channels = {}
		for _, colorToken in ipairs(colorTokens) do
			local channel = tonumber(colorToken)
			if channel == nil then return nil end
			channels[#channels + 1] = clampColorChannel(channel)
		end
		if #channels % 3 ~= 0 then return nil end
		for channelIdx = 1, #channels, 3 do
			colors[#colors + 1] = packRgb(channels[channelIdx], channels[channelIdx + 1], channels[channelIdx + 2])
		end
	end

	if #colors == 0 then return nil end
	return colors
end

local function setActionColor(binding, colorIndex, color)
	local parts = parseActionLine(binding.line)
	local colors = parseActionColors(parts) or { 0x3a3a3aff, 0xffb029ff }
	if #colors == 1 then colors[2] = colors[1] end
	colors[colorIndex] = color

	parts.colorTokens = {}
	for idx = 1, 2 do
		local red, green, blue = unpackRgb(colors[idx])
		parts.colorTokens[#parts.colorTokens + 1] = tostring(red)
		parts.colorTokens[#parts.colorTokens + 1] = tostring(green)
		parts.colorTokens[#parts.colorTokens + 1] = tostring(blue)
	end
	binding.line = buildActionLine(parts)
end

local function clearActionColors(binding)
	local parts = parseActionLine(binding.line)
	parts.colorTokens = nil
	binding.line = buildActionLine(parts)
end

local function getReaperActionTitle(parts)
	if tostring(parts.actionName or ""):lower() ~= "reaper" then return nil end
	local commandToken = unquoteValue(parts.params[1] or "")
	local commandId = tonumber(commandToken)
	if not commandId and commandToken ~= "" and type(r.NamedCommandLookup) == "function" then
		commandId = r.NamedCommandLookup(commandToken)
	end
	if not commandId or commandId == 0 or type(r.kbd_getTextFromCmd) ~= "function" then return nil end
	local title = r.kbd_getTextFromCmd(commandId, 0)
	if title and title ~= "" then return title end
	return nil
end

local function getBindingTitle(binding)
	local parts = parseActionLine(binding.line)
	local explicitTitle = parts.properties.OSD
	if explicitTitle == nil or explicitTitle == "" or explicitTitle == "?" or explicitTitle == "No" then
		explicitTitle = parts.properties.KeyLabel
	end
	if explicitTitle and explicitTitle ~= "" then return explicitTitle end

	local reaperTitle = getReaperActionTitle(parts)
	if reaperTitle then return reaperTitle end

	local csiTitle = data.getProcessedLabel(parts.actionName)
	if csiTitle and csiTitle ~= "" then return csiTitle end
	return binding.line ~= "" and binding.line or "NoAction"
end

local function getModifierLabel(binding)
	local labels = {}
	if binding.hasHold then labels[#labels + 1] = "Hold" end
	if binding.hasDoublePress then labels[#labels + 1] = "DoublePress" end
	for _, modifier in ipairs(MODIFIER_FLAGS) do
		if ((binding.mod or 0) & modifier.bit) ~= 0 then
			labels[#labels + 1] = modifier.name
		end
	end
	if #labels == 0 then return "-" end
	return table.concat(labels, "+")
end

local function getOtherSummary(binding)
	local labels = {}
	if binding.isIncrease then labels[#labels + 1] = "Increase" end
	if binding.isDecrease then labels[#labels + 1] = "Decrease" end
	if binding.isValueInverted then labels[#labels + 1] = "Invert" end
	if binding.isFeedbackInverted then labels[#labels + 1] = "InvertFB" end

	local parts = parseActionLine(binding.line)
	for _, param in ipairs(parts.params or {}) do
		labels[#labels + 1] = unquoteValue(param)
	end
	for key, value in pairs(parts.properties or {}) do
		if key ~= "OSD" and key ~= "KeyLabel" then
			labels[#labels + 1] = key .. "=" .. tostring(value)
		end
	end
	table.sort(labels)
	return table.concat(labels, ", ")
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
	if not r.HasExtState(data.EXT_SECTION, key) then return nil end
	local value = r.GetExtState(data.EXT_SECTION, key)
	r.DeleteExtState(data.EXT_SECTION, key, false)
	return value
end

local function matchesSearchTerms(text, query)
	local candidate = tostring(text or ""):lower()
	for term in tostring(query or ""):lower():gmatch("%S+") do
		if not candidate:find(term, 1, true) then return false end
	end
	return true
end

local function getNamedCommandId(commandId)
	if type(r.ReverseNamedCommandLookup) ~= "function" then return "" end
	local ok, namedCommand = pcall(r.ReverseNamedCommandLookup, commandId)
	if not ok or not namedCommand or namedCommand == "" then return "" end
	namedCommand = tostring(namedCommand)
	if namedCommand:sub(1, 1) ~= "_" then namedCommand = "_" .. namedCommand end
	return namedCommand
end

local function refreshSearchResults()
	local query = state.searchQuery
	local results = {}

	if state.searchMode ~= "reaper" then
		for _, name in ipairs(state.csiActions) do
			if matchesSearchTerms(name, query) then
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

			local namedCommand = getNamedCommandId(cmdId)
			local searchableText = table.concat({ tostring(cmdId), namedCommand, actionName }, " ")
			if matchesSearchTerms(searchableText, query) then
				local idText = namedCommand ~= "" and string.format("%d (%s)", cmdId, namedCommand) or tostring(cmdId)
				results[#results + 1] = string.format("[REAPER] %s - %s", idText, actionName)
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
	r.SetExtState(data.EXT_CMD_SECTION, "ConfigQuery", payload, false)
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
	r.SetExtState(data.EXT_CMD_SECTION, "ConfigApplyLive", payload, false)
	return true
end

local function sendSave()
	local payload = state.surfaceName .. "|" .. state.widgetName
	state.pendingOperation = "Save"
	state.pendingSerialized = serializeBindings(state.bindings)
	r.SetExtState(data.EXT_CMD_SECTION, "ConfigSave", payload, false)
end

local function requestRevert()
	if state.surfaceName == "" or state.widgetName == "" then return end
	state.pendingOperation = "Revert"
	state.pendingSerialized = nil
	local payload = state.surfaceName .. "|" .. state.widgetName
	r.SetExtState(data.EXT_CMD_SECTION, "ConfigRevert", payload, false)
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

local function applySearchSelectionToBinding(binding, row)
	if not binding then return end
	row = row or state.searchResults[state.searchSelected]
	if not row then return end

	local csiAction = row:match("^%[CSI%]%s+(.+)$")
	if csiAction then
		binding.line = csiAction
		refreshBindingDerivedFields(binding)
		updateDirtyState()
		return
	end

	local commandId = row:match("^%[REAPER%]%s+(%d+)")
	if commandId then
		local namedCommand = row:match("^%[REAPER%]%s+%d+%s+%((_[^)]+)%)")
		binding.line = "Reaper " .. (namedCommand or commandId)
		refreshBindingDerivedFields(binding)
		updateDirtyState()
	end
end

local function selectBinding(bindingIndex)
	state.selectedBinding = bindingIndex
end

local function renderColorPopup(ctx, binding, bindingIndex, colorIndex, label, currentColor)
	local popupId = "Color " .. label .. "##binding_color_" .. bindingIndex .. "_" .. colorIndex
	if imgui.ColorButton(ctx, "##color_button_" .. bindingIndex .. "_" .. colorIndex, currentColor) then
		selectBinding(bindingIndex)
		imgui.OpenPopup(ctx, popupId)
	end
	ui.ItemTooltip(ctx, label .. " color")

	if imgui.BeginPopup(ctx, popupId) then
		imgui.Text(ctx, label .. " color")
		local changed, editedColor = imgui.ColorEdit3(ctx, "##picker_" .. bindingIndex .. "_" .. colorIndex, currentColor, imgui.ColorEditFlags_NoInputs)
		if changed then
			setActionColor(binding, colorIndex, editedColor)
			updateDirtyState()
			currentColor = editedColor
		end

		imgui.Separator(ctx)
		for paletteIdx, paletteColor in ipairs(COLOR_PALETTE) do
			if imgui.ColorButton(ctx, "##palette_" .. bindingIndex .. "_" .. colorIndex .. "_" .. paletteIdx, paletteColor.value) then
				setActionColor(binding, colorIndex, paletteColor.value)
				updateDirtyState()
				currentColor = paletteColor.value
			end
			ui.ItemTooltip(ctx, paletteColor.name)
			if paletteIdx % 5 ~= 0 then imgui.SameLine(ctx) end
		end

		if imgui.Button(ctx, "Clear / default##colors_" .. bindingIndex .. "_" .. colorIndex) then
			clearActionColors(binding)
			updateDirtyState()
			imgui.CloseCurrentPopup(ctx)
		end
		imgui.EndPopup(ctx)
	end

	return currentColor
end

local function renderBindingColors(ctx, binding, bindingIndex)
	local parts = parseActionLine(binding.line)
	local colors = parseActionColors(parts)
	local inactiveColor = colors and colors[1] or 0x3a3a3aff
	local activeColor = colors and (colors[2] or colors[1]) or 0xffb029ff

	inactiveColor = renderColorPopup(ctx, binding, bindingIndex, 1, "Inactive", inactiveColor)
	imgui.SameLine(ctx, 0, 3)
	renderColorPopup(ctx, binding, bindingIndex, 2, "Active", activeColor)
end

local function renderBindingTable(ctx)
	if not imgui.BeginTable(ctx, "##bindings_table", 4, TABLE_FLAGS, -1, 155, 0) then return end

	imgui.TableSetupColumn(ctx, "Modifier", imgui.TableColumnFlags_WidthFixed, 95, 0)
	imgui.TableSetupColumn(ctx, "Action", imgui.TableColumnFlags_WidthStretch, 0.52, 1)
	imgui.TableSetupColumn(ctx, "Colors", imgui.TableColumnFlags_WidthFixed, 52, 2)
	imgui.TableSetupColumn(ctx, "Other", imgui.TableColumnFlags_WidthStretch, 0.28, 3)
	imgui.TableHeadersRow(ctx)

	for bindingIndex, binding in ipairs(state.bindings) do
		local selected = bindingIndex == state.selectedBinding
		imgui.TableNextRow(ctx, imgui.TableRowFlags_None, 0)

		imgui.TableSetColumnIndex(ctx, 0)
		if imgui.Selectable(ctx, getModifierLabel(binding) .. "##modifier_" .. bindingIndex, selected) then
			selectBinding(bindingIndex)
		end

		imgui.TableSetColumnIndex(ctx, 1)
		if imgui.Selectable(ctx, getBindingTitle(binding) .. "##action_" .. bindingIndex, selected) then
			selectBinding(bindingIndex)
		end
		ui.ItemTooltip(ctx, binding.line)

		imgui.TableSetColumnIndex(ctx, 2)
		renderBindingColors(ctx, binding, bindingIndex)

		imgui.TableSetColumnIndex(ctx, 3)
		local other = getOtherSummary(binding)
		if other == "" then other = "-" end
		if imgui.Selectable(ctx, other .. "##other_" .. bindingIndex, selected) then
			selectBinding(bindingIndex)
		end
	end

	imgui.EndTable(ctx)
end

local function setHoldEnabled(binding, enabled)
	binding.hasHold = enabled
	if not enabled then
		local parts = parseActionLine(binding.line)
		parts.properties.HoldDelay = nil
		parts.properties.HoldRepeatInterval = nil
		binding.line = buildActionLine(parts)
	end
	updateDirtyState()
end

local function renderActionPicker(ctx, binding)
	imgui.SetNextItemWidth(ctx, 82)
	local modeChanged
	modeChanged, state.searchModeIndex = imgui.Combo(ctx, "##action_search_source", state.searchModeIndex, SEARCH_MODE_ITEMS)
	if modeChanged then
		syncSearchModeFromIndex()
		refreshSearchResults()
	end

	imgui.SameLine(ctx, 0, 5)
	imgui.SetNextItemWidth(ctx, -28)
	local queryChanged
	if imgui.InputTextWithHint then
		queryChanged, state.searchQuery = imgui.InputTextWithHint(ctx, "##action_search", "Find action...", state.searchQuery)
	else
		queryChanged, state.searchQuery = imgui.InputText(ctx, "##action_search", state.searchQuery)
	end
	if queryChanged then refreshSearchResults() end
	imgui.SameLine(ctx, 0, 4)
	if imgui.Button(ctx, "x##clear_action_search") then
		state.searchQuery = ""
		state.searchResults = {}
		state.searchSelected = 0
	end
	ui.ItemTooltip(ctx, "Clear action search")

	if state.searchQuery:match("%S") and imgui.BeginListBox(ctx, "##action_search_results", -1, 105) then
		for idx, row in ipairs(state.searchResults) do
			if imgui.Selectable(ctx, row, idx == state.searchSelected) then
				state.searchSelected = idx
				applySearchSelectionToBinding(binding, row)
			end
		end
		imgui.EndListBox(ctx)
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
		r.SetExtState(data.EXT_CMD_SECTION, "ActionListQuery", "", false)
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

local function renderDirtyActionButton(ctx, label, enabled, handler)
	ui.DirtyActionButton(ctx, label, enabled, handler)
end

local function renderConfigToolbar(ctx)
	if not imgui.BeginTable(ctx, "##config_toolbar", 2, imgui.TableFlags_SizingStretchProp, -1, 0, 0) then return end
	imgui.TableSetupColumn(ctx, "##config_actions", imgui.TableColumnFlags_WidthStretch, 1, 0)
	imgui.TableSetupColumn(ctx, "##binding_actions", imgui.TableColumnFlags_WidthFixed, 210, 1)
	imgui.TableNextRow(ctx)

	imgui.TableSetColumnIndex(ctx, 0)
	local operationReady = state.pendingOperation == nil
	renderDirtyActionButton(ctx, "Apply Live", operationReady and state.hasUnappliedEdits, function()
		state.saveAfterApply = false
		sendApplyLive()
	end)
	imgui.SameLine(ctx)
	renderDirtyActionButton(ctx, "Save", operationReady and state.isDirty, function()
		if state.hasUnappliedEdits then
			state.saveAfterApply = true
			if not sendApplyLive() then state.saveAfterApply = false end
		else
			sendSave()
		end
	end)
	imgui.SameLine(ctx)
	renderDirtyActionButton(ctx, "Revert", operationReady and state.isDirty, requestRevert)

	if state.pendingOperation then
		imgui.SameLine(ctx)
		imgui.TextDisabled(ctx, state.pendingOperation .. "...")
	elseif state.status:match("^ERR%s*|") then
		imgui.SameLine(ctx)
		imgui.Text(ctx, state.status)
	end

	imgui.TableSetColumnIndex(ctx, 1)
	if imgui.Button(ctx, "+ Add") then addBinding() end
	imgui.SameLine(ctx)
	if imgui.Button(ctx, "- Remove") then removeSelectedBinding() end
	imgui.SameLine(ctx)
	if imgui.Button(ctx, "Clone") then duplicateSelectedBinding() end
	imgui.SameLine(ctx)
	if imgui.ArrowButton(ctx, "##move_binding_up", imgui.Dir_Up) then moveSelectedBinding(-1) end
	ui.ItemTooltip(ctx, "Move binding up")
	imgui.SameLine(ctx)
	if imgui.ArrowButton(ctx, "##move_binding_down", imgui.Dir_Down) then moveSelectedBinding(1) end
	ui.ItemTooltip(ctx, "Move binding down")

	imgui.EndTable(ctx)
end

function M.RenderConfigEditor(ctx)
	if not state.isOpen then return end

	pollConfigResponses()

	local dirtyMarker = state.isDirty and " *" or ""
	local title = "Widget config: [" .. state.widgetName .. "]  @" .. state.surfaceName .. "/" .. state.zoneName .. dirtyMarker .. " ###osk_widget_config"
	imgui.SetNextWindowSize(ctx, 720, 620, imgui.Cond_Appearing)
	local visible, open = imgui.Begin(ctx, title, true, CONFIG_WINDOW_FLAGS)
	if open == false then
		closeEditor()
		imgui.End(ctx)
		return
	end

	if visible then
		renderConfigToolbar(ctx)
		imgui.Separator(ctx)
		local bodyVisible = imgui.BeginChild(ctx, "##config_body", -1, -1, 0, 0)
		if bodyVisible then
			renderBindingTable(ctx)

			local selected = getSelectedBinding()
			if selected then
				imgui.Separator(ctx)
				local parts = parseActionLine(selected.line)

				local actionChanged
				actionChanged, parts.actionName = imgui.InputText(ctx, "Action", parts.actionName or "")
				if actionChanged then
					selected.line = buildActionLine(parts)
					refreshBindingDerivedFields(selected)
					updateDirtyState()
				end

				renderActionPicker(ctx, selected)
				parts = parseActionLine(selected.line)
				local changedQuick = false

			local paramsText = table.concat(parts.params or {}, " ")
			local paramsChanged
			paramsChanged, paramsText = imgui.InputText(ctx, "Parameters", paramsText)
			if paramsChanged then
				parts.params = tokenizePreservingQuotes(paramsText)
				changedQuick = true
			end

			local storedOsdText = tostring(parts.properties.OSD or "")
			local osdText = storedOsdText
			if osdText == "" or osdText == "?" or osdText:lower() == "no" then
				osdText = getBindingTitle(selected)
			end
			local osdChanged
			osdChanged, osdText = imgui.InputText(ctx, "OSD", osdText)
			if osdChanged then
				parts.properties.OSD = (osdText ~= "") and osdText or nil
				changedQuick = true
			end

			local keyLabelText = tostring(parts.properties.KeyLabel or "")
			local keyLabelChanged
			if imgui.InputTextWithHint then
				local keyLabelHint = osdText ~= "" and osdText or "Uses OSD when empty"
				keyLabelChanged, keyLabelText = imgui.InputTextWithHint(ctx, "KeyLabel", keyLabelHint, keyLabelText)
			else
				keyLabelChanged, keyLabelText = imgui.InputText(ctx, "KeyLabel (OSD default)", keyLabelText)
			end
			if keyLabelChanged then
				parts.properties.KeyLabel = (keyLabelText ~= "") and keyLabelText or nil
				changedQuick = true
			end

			local toggled, enabled = imgui.Checkbox(ctx, "Hold##pseudo_hold", selected.hasHold == true)
			if toggled then
				setHoldEnabled(selected, enabled)
				parts = parseActionLine(selected.line)
			end
			imgui.SameLine(ctx)
			toggled, enabled = imgui.Checkbox(ctx, "DoublePress##pseudo_double", selected.hasDoublePress == true)
			if toggled then
				selected.hasDoublePress = enabled
				updateDirtyState()
			end

			if data.IsRelativeWidget(state.surfaceName, state.widgetName) then
				imgui.SameLine(ctx)
				toggled, enabled = imgui.Checkbox(ctx, "Increase##direction_increase", selected.isIncrease == true)
				if toggled then
					selected.isIncrease = enabled
					if enabled then selected.isDecrease = false end
					updateDirtyState()
				end
				imgui.SameLine(ctx)
				toggled, enabled = imgui.Checkbox(ctx, "Decrease##direction_decrease", selected.isDecrease == true)
				if toggled then
					selected.isDecrease = enabled
					if enabled then selected.isIncrease = false end
					updateDirtyState()
				end
			end

			toggled, enabled = imgui.Checkbox(ctx, "Invert value", selected.isValueInverted == true)
			if toggled then
				selected.isValueInverted = enabled
				updateDirtyState()
			end
			imgui.SameLine(ctx)
			toggled, enabled = imgui.Checkbox(ctx, "Invert feedback", selected.isFeedbackInverted == true)
			if toggled then
				selected.isFeedbackInverted = enabled
				updateDirtyState()
			end
			imgui.SameLine(ctx)
			local feedbackNo = tostring(parts.properties.Feedback or ""):lower() == "no"
			local feedbackChanged
			feedbackChanged, feedbackNo = imgui.Checkbox(ctx, "No Feedback", feedbackNo)
			if feedbackChanged then
				if feedbackNo then parts.properties.Feedback = "No" else parts.properties.Feedback = nil end
				changedQuick = true
			end

			imgui.TextDisabled(ctx, "Modifiers:")
			imgui.SameLine(ctx)
			for idx, modifier in ipairs(MODIFIER_FLAGS) do
				local hasFlag = ((selected.mod or 0) & modifier.bit) ~= 0
				toggled, hasFlag = imgui.Checkbox(ctx, modifier.name .. "##mod_" .. idx, hasFlag)
				if toggled then
					if hasFlag then
						selected.mod = (selected.mod or 0) | modifier.bit
					else
						selected.mod = (selected.mod or 0) & (~modifier.bit)
					end
					updateDirtyState()
				end
				if idx ~= #MODIFIER_FLAGS and idx % 5 ~= 0 then imgui.SameLine(ctx) end
			end

			local runCountVal = tonumber(parts.properties.RunCount) or 1
			local runCountChanged
			imgui.SetNextItemWidth(ctx, 120)
			runCountChanged, runCountVal = imgui.DragInt(ctx, "##run_count", runCountVal, 1, 1, 10, "RunCount = %d")
			if runCountChanged then
				parts.properties.RunCount = (runCountVal > 1) and tostring(runCountVal) or nil
				changedQuick = true
			end

			if selected.hasHold then
				imgui.SameLine(ctx, 0, 5)
				local holdDelayVal = tonumber(parts.properties.HoldDelay) or 0
				local holdDelayChanged
				imgui.SetNextItemWidth(ctx, 145)
				holdDelayChanged, holdDelayVal = imgui.DragInt(ctx, "##hold_delay", holdDelayVal, 100, 0, 10000, "HoldDelay = %d ms")
				if holdDelayChanged then
					parts.properties.HoldDelay = (holdDelayVal > 0) and tostring(holdDelayVal) or nil
					changedQuick = true
				end

				imgui.SameLine(ctx, 0, 5)
				local holdRepeatVal = tonumber(parts.properties.HoldRepeatInterval) or 0
				local holdRepeatChanged
				imgui.SetNextItemWidth(ctx, 145)
				holdRepeatChanged, holdRepeatVal = imgui.DragInt(ctx, "##hold_repeat", holdRepeatVal, 10, 0, 1000, "Repeat = %d ms")
				if holdRepeatChanged then
					parts.properties.HoldRepeatInterval = (holdRepeatVal > 0) and tostring(holdRepeatVal) or nil
					changedQuick = true
				end
			end

			if changedQuick then
				selected.line = buildActionLine(parts)
				refreshBindingDerivedFields(selected)
				updateDirtyState()
			end

				imgui.Separator(ctx)
				local lineChanged
				lineChanged, selected.line = imgui.InputText(ctx, "Raw", selected.line or "")
				if lineChanged then
					refreshBindingDerivedFields(selected)
					updateDirtyState()
				end
			else
				imgui.Text(ctx, "No binding for this widget in the active zone.")
			end
		end
		imgui.EndChild(ctx)
	end

	imgui.End(ctx)
end

return M
