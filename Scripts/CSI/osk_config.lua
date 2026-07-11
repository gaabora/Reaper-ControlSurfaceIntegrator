local r = reaper
local imgui = require "imgui" "0.9.3"

local action_line = require("action_line")
local data = require("osk_data")
local osk_config_model = require("osk_config_model")
local osk_config_protocol = require("osk_config_protocol")
local osk_config_view = require("osk_config_view")
local settings_store = require("settings_store")
local theme = require("theme_settings")

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
    searchMode = "all",
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
    windowGeometryApplied = false,
    windowPosition = nil,
    windowSize = nil,
    action_line = action_line,
}

local CONFIG_POSITION_KEY = "WidgetConfigPosition"
local CONFIG_SIZE_KEY = "WidgetConfigSize"
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

local TABLE_FLAGS = imgui.TableFlags_Borders
    | imgui.TableFlags_RowBg
    | imgui.TableFlags_Resizable
    | imgui.TableFlags_ScrollY

local function loadWindowGeometry()
    state.windowPosition = settings_store.ReadPair(data.EXT_SETTINGS, CONFIG_POSITION_KEY)
    state.windowSize = settings_store.ReadPair(data.EXT_SETTINGS, CONFIG_SIZE_KEY)
end

local function saveWindowGeometry(ctx)
    local x, y = imgui.GetWindowPos(ctx)
    local width, height = imgui.GetWindowSize(ctx)
    local oldPosition = state.windowPosition
    local oldSize = state.windowSize

    if not oldPosition or math.abs(oldPosition.x - x) > theme.CONFIG.geometry_epsilon or math.abs(oldPosition.y - y) > theme.CONFIG.geometry_epsilon then
        state.windowPosition = { x = x, y = y }
        settings_store.WritePair(data.EXT_SETTINGS, CONFIG_POSITION_KEY, state.windowPosition)
    end

    if not oldSize or math.abs(oldSize.x - width) > theme.CONFIG.geometry_epsilon or math.abs(oldSize.y - height) > theme.CONFIG.geometry_epsilon then
        state.windowSize = { x = width, y = height }
        settings_store.WritePair(data.EXT_SETTINGS, CONFIG_SIZE_KEY, state.windowSize)
    end
end

local function closeEditor()
    if state.hasLiveChanges or state.pendingOperation == "ApplyLive" then
        osk_config_protocol.RequestRevert(state, data)
    end
    state.isOpen = false
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
    state.windowGeometryApplied = false
    osk_config_model.SyncSearchIndexFromMode(state)

    state.suppressWindowContextMenuUntil = os.clock() + 0.20

    osk_config_protocol.SendConfigQuery(state, data, "", true)
    if #state.csiActions == 0 then
        osk_config_protocol.RequestActionList(data)
    else
        osk_config_model.RefreshSearchResults(state, r)
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

function M.RenderConfigEditor(ctx, font)
    if not state.isOpen then return end

    osk_config_protocol.PollConfigResponses(state, data, osk_config_model)

    local dirtyMarker = state.isDirty and " *" or ""
    local title = "Widget config: [" .. state.widgetName .. "]  @" .. state.surfaceName .. "/" .. state.zoneName .. dirtyMarker .. " ###osk_widget_config"
    if not state.windowGeometryApplied then
        loadWindowGeometry()
        if state.windowPosition then imgui.SetNextWindowPos(ctx, state.windowPosition.x, state.windowPosition.y, imgui.Cond_Appearing) end
        if state.windowSize then
            imgui.SetNextWindowSize(ctx, state.windowSize.x, state.windowSize.y, imgui.Cond_Appearing)
        else
            imgui.SetNextWindowSize(ctx, theme.CONFIG.default_window_width, theme.CONFIG.default_window_height, imgui.Cond_Appearing)
        end
        state.windowGeometryApplied = true
    end

    if font then imgui.PushFont(ctx, font) end
    local visible, open = imgui.Begin(ctx, title, true, CONFIG_WINDOW_FLAGS)
    if open == false then
        closeEditor()
        imgui.End(ctx)
        if font then imgui.PopFont(ctx) end
        return
    end

    if visible then
        saveWindowGeometry(ctx)
        local deps = {
            data = data,
            action_line = action_line,
            model = osk_config_model,
            protocol = osk_config_protocol,
            reaper = r,
            modifierFlags = MODIFIER_FLAGS,
            tableFlags = TABLE_FLAGS,
        }
        osk_config_view.RenderToolbar(ctx, state, deps)
        imgui.Separator(ctx)
        local bodyVisible = imgui.BeginChild(ctx, "##config_body", -1, -1, 0, 0)
        if bodyVisible then
            osk_config_view.RenderBody(ctx, state, deps)
            imgui.EndChild(ctx)
        end
    end

    imgui.End(ctx)
    if font then imgui.PopFont(ctx) end
end

return M
