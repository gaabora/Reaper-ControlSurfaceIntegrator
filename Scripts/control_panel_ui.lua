local imgui = require "imgui" "0.9.3"

local identity = require("product_identity")
local lifecycleProtocol = require("control_panel_protocol")
local pages = require("control_panel_pages")
local settingsProtocol = require("settings_protocol")
local settingsStore = require("settings_store")
local ui = require("ui_components")

local module = {}

local DEFAULT_TAB = "General"
local DEFAULT_WIDTH = 760
local DEFAULT_HEIGHT = 560
local NAVIGATION_WIDTH = 160
local FOOTER_HEIGHT = 38
local CLOSE_POPUP_ID = "Save changes before closing?##ControlPanelClose"

local function readSelectedTab()
    local savedTab = reaper.GetExtState(identity.extState.controlPanel, "SelectedTab")
    return pages.Find(savedTab) and savedTab or DEFAULT_TAB
end

local function newState()
    local state = {
        closePopupPending = false,
        configError = "",
        configMessage = "Checking configuration...",
        configNeedsQuery = true,
        configRequestId = nil,
        configRequestStartedAt = 0,
        configRetryAt = 0,
        focusRequested = true,
        geometry = {
            position = settingsStore.ReadPair(identity.extState.controlPanel, "WindowPosition"),
            size = settingsStore.ReadPair(identity.extState.controlPanel, "WindowSize"),
        },
        lastStatus = "",
        open = true,
        restoreScroll = true,
        scrollByPage = {},
        selectedTab = readSelectedTab(),
    }
    for idx, page in ipairs(pages.All()) do state.scrollByPage[page.id] = tonumber(reaper.GetExtState(identity.extState.controlPanel, "Scroll." .. page.id)) or 0 end
    return state
end

local function startConfigQuery(state)
    if state.configRequestId then return end
    local requestId, requestError = settingsProtocol.Query("Product")
    if requestId then
        state.configRequestId = requestId
        state.configRequestStartedAt = reaper.time_precise()
        state.configNeedsQuery = false
        state.configError = ""
        state.configMessage = "Checking configuration..."
        return
    end
    state.configMessage = "Waiting to check configuration..."
    state.configNeedsQuery = true
    state.configError = requestError or "Cannot request configuration status"
    state.configRetryAt = reaper.time_precise() + 0.5
end

local function pollConfigQuery(state)
    if not state.configRequestId then
        if state.configNeedsQuery and reaper.time_precise() >= state.configRetryAt then startConfigQuery(state) end
        return
    end
    local response, responseError = settingsProtocol.Poll(state.configRequestId)
    if not response and not responseError then
        if reaper.time_precise() - state.configRequestStartedAt >= 3 then
            settingsProtocol.Cancel(state.configRequestId)
            state.configRequestId = nil
            state.configMessage = "Configuration status unavailable"
            state.configError = "No active C++ configuration response"
        end
        return
    end
    state.configRequestId = nil
    if not response then
        state.configMessage = "Configuration status unavailable"
        state.configError = responseError or "Invalid configuration response"
        return
    end
    if not response.ok then
        state.configMessage = "Configuration has an error"
        state.configError = response.message ~= "" and response.message or "Configuration query failed"
        return
    end
    state.configMessage = "Configuration is active"
    state.configError = ""
end

local function selectTab(state, tabId)
    if not pages.Find(tabId) or state.selectedTab == tabId then return false end
    state.selectedTab = tabId
    state.restoreScroll = true
    return true
end

local function pollLifecycleRequests(state)
    local request, requestError = lifecycleProtocol.Poll()
    if requestError then
        state.lastStatus = requestError
        return
    end
    if not request then return end
    if request.command == "Open" or request.command == "Focus" then state.focusRequested = true end
    if request.command == "SelectTab" then
        if pages.Find(request.tab) then
            selectTab(state, request.tab)
            state.focusRequested = true
        else
            state.lastStatus = "Unknown Control Panel tab: " .. tostring(request.tab)
        end
    end
end

local function renderHeader(ctx, state)
    imgui.Text(ctx, identity.displayName)
    imgui.SameLine(ctx)
    imgui.TextDisabled(ctx, state.configMessage)
    if state.configError ~= "" then
        imgui.SameLine(ctx)
        imgui.TextColored(ctx, 0xFF6666FF, state.configError)
    end
    imgui.SameLine(ctx)
    ui.Disabled(ctx, state.configRequestId ~= nil, function()
        if imgui.SmallButton(ctx, "Refresh##ConfigurationStatus") then
            state.configNeedsQuery = true
            state.configRetryAt = 0
            startConfigQuery(state)
        end
    end)
end

local function renderNavigation(ctx, state, height)
    if imgui.BeginChild(ctx, "##ControlPanelNavigation", NAVIGATION_WIDTH, height, 0, 0) then
        for idx, page in ipairs(pages.All()) do
            local selected = page.id == state.selectedTab
            if selected then imgui.PushStyleColor(ctx, imgui.Col_Button, 0x5278A8FF) end
            if imgui.Button(ctx, page.label .. "##Navigation", -1, 34) then selectTab(state, page.id) end
            if selected then imgui.PopStyleColor(ctx) end
        end
    end
    imgui.EndChild(ctx)
end

local function renderPage(ctx, state, width, height)
    local page = pages.Find(state.selectedTab) or pages.Find(DEFAULT_TAB)
    if imgui.BeginChild(ctx, "##ControlPanelPage_" .. page.id, width, height, 0, 0) then
        if state.restoreScroll and imgui.SetScrollY then
            imgui.SetScrollY(ctx, state.scrollByPage[page.id] or 0)
            state.restoreScroll = false
        end
        pages.Render(ctx, page)
        if imgui.GetScrollY then state.scrollByPage[page.id] = imgui.GetScrollY(ctx) end
    end
    imgui.EndChild(ctx)
end

local function renderFooter(ctx, state, page)
    imgui.Separator(ctx)
    if not page.hasDraft then
        imgui.TextDisabled(ctx, "No editable settings are available on this page yet")
        return
    end
    local dirty = pages.IsDirty(page)
    ui.DirtyActionButton(ctx, "Save changes", dirty, function()
        local saved, saveError = pages.Save(page)
        state.lastStatus = saved and "Changes saved" or tostring(saveError or "Cannot save changes")
    end)
    imgui.SameLine(ctx)
    ui.Disabled(ctx, not dirty, function()
        if imgui.Button(ctx, "Revert") then
            pages.Revert(page)
            state.lastStatus = "Draft reverted"
        end
    end)
    imgui.SameLine(ctx)
    imgui.TextDisabled(ctx, dirty and "Unsaved changes" or (state.lastStatus ~= "" and state.lastStatus or "No unsaved changes"))
end

local function renderClosePopup(ctx, state)
    if state.closePopupPending then
        imgui.OpenPopup(ctx, CLOSE_POPUP_ID)
        state.closePopupPending = false
    end
    local visible = imgui.BeginPopupModal(ctx, CLOSE_POPUP_ID, nil, imgui.WindowFlags_AlwaysAutoResize)
    if not visible then return end
    imgui.Text(ctx, "Save changes before closing the Control Panel?")
    imgui.Spacing(ctx)
    if imgui.Button(ctx, "Save", 100, 0) then
        local saved, saveError = pages.SaveAll()
        if saved then
            state.open = false
            imgui.CloseCurrentPopup(ctx)
        else
            state.lastStatus = tostring(saveError or "Cannot save changes")
        end
    end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Don't Save", 100, 0) then
        pages.RevertAll()
        state.open = false
        imgui.CloseCurrentPopup(ctx)
    end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Cancel", 100, 0) then imgui.CloseCurrentPopup(ctx) end
    imgui.EndPopup(ctx)
end

function module.New(ctx)
    local state = newState()
    state.ctx = ctx
    startConfigQuery(state)
    return state
end

function module.SaveWindowState(state)
    if not state then return end
    if state.configRequestId then settingsProtocol.Cancel(state.configRequestId) end
    if state.geometry.position then settingsStore.WritePair(identity.extState.controlPanel, "WindowPosition", state.geometry.position) end
    if state.geometry.size then settingsStore.WritePair(identity.extState.controlPanel, "WindowSize", state.geometry.size) end
    reaper.SetExtState(identity.extState.controlPanel, "SelectedTab", state.selectedTab, true)
    for pageId, scrollPosition in pairs(state.scrollByPage) do reaper.SetExtState(identity.extState.controlPanel, "Scroll." .. pageId, tostring(scrollPosition), true) end
end

function module.Render(state)
    pollLifecycleRequests(state)
    pollConfigQuery(state)

    if state.geometry.position then imgui.SetNextWindowPos(state.ctx, state.geometry.position.x, state.geometry.position.y, imgui.Cond_Appearing) end
    local windowSize = state.geometry.size or { x = DEFAULT_WIDTH, y = DEFAULT_HEIGHT }
    imgui.SetNextWindowSize(state.ctx, windowSize.x, windowSize.y, imgui.Cond_Appearing)
    if state.focusRequested then
        imgui.SetNextWindowFocus(state.ctx)
        state.focusRequested = false
    end

    local windowFlags = imgui.WindowFlags_NoCollapse
    if imgui.WindowFlags_NoDocking then windowFlags = windowFlags | imgui.WindowFlags_NoDocking end
    if imgui.WindowFlags_NoSavedSettings then windowFlags = windowFlags | imgui.WindowFlags_NoSavedSettings end
    local visible, windowOpen = imgui.Begin(state.ctx, identity.displayName .. " Control Panel", true, windowFlags)
    if visible then
        local windowX, windowY = imgui.GetWindowPos(state.ctx)
        local windowWidth, windowHeight = imgui.GetWindowSize(state.ctx)
        state.geometry.position = { x = windowX, y = windowY }
        state.geometry.size = { x = windowWidth, y = windowHeight }
        renderHeader(state.ctx, state)
        imgui.Separator(state.ctx)
        local availableWidth, availableHeight = imgui.GetContentRegionAvail(state.ctx)
        local bodyHeight = availableHeight - FOOTER_HEIGHT
        renderNavigation(state.ctx, state, bodyHeight)
        imgui.SameLine(state.ctx)
        renderPage(state.ctx, state, availableWidth - NAVIGATION_WIDTH - 8, bodyHeight)
        local selectedPage = pages.Find(state.selectedTab) or pages.Find(DEFAULT_TAB)
        renderFooter(state.ctx, state, selectedPage)
    end
    imgui.End(state.ctx)

    if not windowOpen then
        if pages.HasAnyDirty() then state.closePopupPending = true else state.open = false end
    end
    renderClosePopup(state.ctx, state)
    return state.open
end

return module
