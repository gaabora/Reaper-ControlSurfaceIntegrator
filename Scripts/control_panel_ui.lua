local imgui = require "imgui" "0.9.3"

local identity = require("product_identity")
local lifecycleProtocol = require("control_panel_protocol")
local pages = require("control_panel_pages")
local settingsStore = require("settings_store")
local theme = require("theme_settings")
local ui = require("ui_components")

local module = {}

local DEFAULT_TAB = "General"
local DEFAULT_WIDTH = 1120
local DEFAULT_HEIGHT = 560
local MINIMUM_WIDTH = 1050
local MINIMUM_HEIGHT = 440
local NAVIGATION_WIDTH = 160
local FOOTER_HEIGHT = 38
local CLOSE_POPUP_ID = "Save changes before closing?##ControlPanelClose"

local function readSelectedTab()
    local savedTab = reaper.GetExtState(identity.extState.controlPanel, "SelectedTab")
    return pages.Find(savedTab) and savedTab or DEFAULT_TAB
end

local function newState()
    local state = {
        closeAfterSave = false,
        closePopupPending = false,
        focusRequested = true,
        geometry = {
            position = settingsStore.ReadPair(identity.extState.controlPanel, "WindowPosition"),
            size = settingsStore.ReadPair(identity.extState.controlPanel, "WindowSize"),
        },
        lastStatus = "",
        lastStatusIsError = false,
        open = true,
        restoreScroll = true,
        scrollByPage = {},
        selectedTab = readSelectedTab(),
    }
    for idx, page in ipairs(pages.All()) do state.scrollByPage[page.id] = tonumber(reaper.GetExtState(identity.extState.controlPanel, "Scroll." .. page.id)) or 0 end
    return state
end

local function selectTab(state, tabId)
    if not pages.Find(tabId) or state.selectedTab == tabId then return false end
    state.selectedTab = tabId
    state.restoreScroll = true
    return true
end

local function requestClose(state)
    if pages.IsBusy() then
        state.lastStatus = "Wait for the current settings operation"
    elseif pages.HasAnyDirty() then
        state.closePopupPending = true
    else
        state.open = false
    end
end

local function pollLifecycleRequests(state)
    local request, requestError = lifecycleProtocol.Poll()
    if requestError then
        state.lastStatus = requestError
        return
    end
    if not request then return end
    if request.command == "Close" then requestClose(state) return end
    if request.command == "Open" or request.command == "Focus" then state.focusRequested = true end
    if request.command == "SelectTab" then
        if pages.Find(request.tab) then
            if request.tab == "General" and request.device ~= "" then
                local contextSet, contextError = pages.SetGeneralContext(request.device)
                if not contextSet then state.lastStatus = tostring(contextError or "Cannot change General settings context") end
            end
            if request.tab == "Logging" and request.logSessionId ~= "" and request.logOffset ~= "" then
                local navigated = pages.NavigateLogging(request.logSessionId, request.logOffset)
                if not navigated then state.lastStatus = "The notification source record is not available in the active daily log." end
            end
            selectTab(state, request.tab)
            state.focusRequested = true
        else
            state.lastStatus = "Unknown Control Panel tab: " .. tostring(request.tab)
        end
    end
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
        pages.Render(ctx, page, state.fonts)
        local requestedPage = pages.ConsumeRequestedPage()
        if requestedPage ~= "" then selectTab(state, requestedPage) end
        if imgui.GetScrollY then state.scrollByPage[page.id] = imgui.GetScrollY(ctx) end
    end
    imgui.EndChild(ctx)
end

local function renderFooter(ctx, state)
    local dirty = pages.HasAnyDirty()
    local busy = pages.IsBusy()
    ui.DirtyActionButton(ctx, pages.NeedsConfigurationCreation() and "Create configuration" or "Save changes", dirty and not busy, function()
        local accepted, saveError = pages.SaveAll()
        state.lastStatus = accepted and "Saving changes..." or tostring(saveError or "Cannot save changes")
        state.lastStatusIsError = not accepted
    end)
    imgui.SameLine(ctx)
    ui.Disabled(ctx, not dirty or busy, function()
        if imgui.Button(ctx, "Revert") then
            local reverted, revertError = pages.RevertAll()
            state.lastStatus = reverted and "Draft reverted" or tostring(revertError or "Cannot revert changes")
            state.lastStatusIsError = not reverted
        end
    end)
    imgui.SameLine(ctx)
    local errorStatus = pages.HasError() and pages.GetStatus() or (state.lastStatusIsError and state.lastStatus or "")
    local status = busy and "Working..." or (errorStatus ~= "" and errorStatus or (dirty and "Unsaved changes" or (state.lastStatus ~= "" and state.lastStatus or pages.GetStatus())))
    imgui.TextDisabled(ctx, status ~= "" and status or "No unsaved changes")
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
        local accepted, saveError = pages.SaveAll()
        if accepted then
            state.lastStatusIsError = false
            state.closeAfterSave = pages.IsBusy()
            if not state.closeAfterSave and not pages.HasAnyDirty() then state.open = false end
            imgui.CloseCurrentPopup(ctx)
        else
            state.lastStatus = tostring(saveError or "Cannot save changes")
            state.lastStatusIsError = true
        end
    end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Don't Save", 100, 0) then
        local reverted, revertError = pages.RevertAll()
        if reverted then
            state.lastStatusIsError = false
            state.open = false
            imgui.CloseCurrentPopup(ctx)
        else
            state.lastStatus = tostring(revertError or "Cannot discard changes")
            state.lastStatusIsError = true
        end
    end
    imgui.SameLine(ctx)
    if imgui.Button(ctx, "Cancel", 100, 0) then imgui.CloseCurrentPopup(ctx) end
    imgui.EndPopup(ctx)
end

function module.New(ctx, fonts)
    local state = newState()
    state.ctx = ctx
    state.fonts = fonts or {}
    pages.Initialize()
    return state
end

function module.SaveWindowState(state)
    if not state then return end
    pages.Shutdown()
    if state.geometry.position then settingsStore.WritePair(identity.extState.controlPanel, "WindowPosition", state.geometry.position) end
    if state.geometry.size then settingsStore.WritePair(identity.extState.controlPanel, "WindowSize", state.geometry.size) end
    reaper.SetExtState(identity.extState.controlPanel, "SelectedTab", state.selectedTab, true)
    for pageId, scrollPosition in pairs(state.scrollByPage) do reaper.SetExtState(identity.extState.controlPanel, "Scroll." .. pageId, tostring(scrollPosition), true) end
end

function module.Render(state)
    pollLifecycleRequests(state)
    pages.Update()
    if state.lastStatus == "Saving changes..." and not pages.IsBusy() then
        state.lastStatus = pages.GetStatus()
        state.lastStatusIsError = pages.HasError() or pages.HasAnyDirty()
    end
    if state.closeAfterSave and not pages.IsBusy() then
        if pages.HasAnyDirty() then
            state.closeAfterSave = false
            state.lastStatus = pages.GetStatus()
            state.lastStatusIsError = true
        else
            state.open = false
        end
    end

    if state.geometry.position then imgui.SetNextWindowPos(state.ctx, state.geometry.position.x, state.geometry.position.y, imgui.Cond_Appearing) end
    local windowSize = state.geometry.size or { x = DEFAULT_WIDTH, y = DEFAULT_HEIGHT }
    imgui.SetNextWindowSize(state.ctx, windowSize.x, windowSize.y, imgui.Cond_Appearing)
    if imgui.SetNextWindowSizeConstraints then imgui.SetNextWindowSizeConstraints(state.ctx, MINIMUM_WIDTH, MINIMUM_HEIGHT, 10000, 10000) end
    if state.focusRequested then
        imgui.SetNextWindowFocus(state.ctx)
        state.focusRequested = false
    end

    local windowFlags = imgui.WindowFlags_NoCollapse
    if imgui.WindowFlags_NoDocking then windowFlags = windowFlags | imgui.WindowFlags_NoDocking end
    if imgui.WindowFlags_NoSavedSettings then windowFlags = windowFlags | imgui.WindowFlags_NoSavedSettings end
    imgui.PushStyleVar(state.ctx, imgui.StyleVar_ItemSpacing, theme.common.item_spacing, theme.common.item_spacing)
    imgui.PushStyleVar(state.ctx, imgui.StyleVar_FrameRounding, theme.common.rounding)
    imgui.PushStyleVar(state.ctx, imgui.StyleVar_DisabledAlpha, theme.common.disabled_alpha)
    local visible, windowOpen = imgui.Begin(state.ctx, identity.displayName .. " Control Panel", true, windowFlags)
    if visible then
        local windowX, windowY = imgui.GetWindowPos(state.ctx)
        local windowWidth, windowHeight = imgui.GetWindowSize(state.ctx)
        state.geometry.position = { x = windowX, y = windowY }
        state.geometry.size = { x = windowWidth, y = windowHeight }
        local availableWidth, availableHeight = imgui.GetContentRegionAvail(state.ctx)
        local bodyHeight = availableHeight - FOOTER_HEIGHT
        renderNavigation(state.ctx, state, bodyHeight)
        imgui.SameLine(state.ctx)
        renderPage(state.ctx, state, availableWidth - NAVIGATION_WIDTH - 8, bodyHeight)
        renderFooter(state.ctx, state)
    end
    imgui.End(state.ctx)

    if not windowOpen then
        requestClose(state)
    end
    renderClosePopup(state.ctx, state)
    imgui.PopStyleVar(state.ctx, 3)
    return state.open
end

return module
