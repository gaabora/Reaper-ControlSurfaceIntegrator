local imgui = require "imgui" "0.9.3"

local appearance = require("control_panel_appearance")
local general = require("settings_ui")
local logging = require("control_panel_logging")

local module = {}

local pages = {
    {
        id = "Devices",
        label = "Devices",
        description = "Device definitions, Pages, Surface assignments, and listener relationships will move here in later stages. Use the native configuration window for editing now.",
    },
    {
        id = "General",
        label = "General",
    },
    {
        id = "Appearance",
        label = "Appearance",
    },
    {
        id = "Logging",
        label = "Logging",
    },
}

local pagesById = {}
for pageIndex, page in ipairs(pages) do pagesById[page.id] = page end

local state = {
    initialized = false,
    pendingSaveStage = "",
    status = "",
}

local function renderPlaceholder(ctx, page)
    if imgui.TextWrapped then imgui.TextWrapped(ctx, page.description) else imgui.Text(ctx, page.description) end
end

local function finishAppearanceSave()
    local saved, saveError = appearance.Save()
    state.pendingSaveStage = ""
    state.status = saved and "Changes saved" or tostring(saveError or "Cannot save Appearance settings")
end

local function finishPendingSave()
    if state.pendingSaveStage == "" or general.IsBusy() or logging.IsBusy() then return end
    if state.pendingSaveStage == "General" then
        if general.HasError() or general.IsDirty() then
            state.pendingSaveStage = ""
            state.status = general.GetStatus() ~= "" and general.GetStatus() or "General settings were not saved"
            return
        end
        if logging.IsDirty() then
            local accepted, saveError = logging.Save()
            if not accepted then
                state.pendingSaveStage = ""
                state.status = tostring(saveError or "Logging settings were not saved")
                return
            end
            state.pendingSaveStage = "Logging"
            state.status = "Saving Logging settings..."
            return
        end
        finishAppearanceSave()
        return
    end
    if logging.HasError() or logging.IsDirty() then
        state.pendingSaveStage = ""
        state.status = logging.GetStatus() ~= "" and logging.GetStatus() or "Logging settings were not saved"
        return
    end
    finishAppearanceSave()
end

function module.Initialize()
    if state.initialized then return end
    state.initialized = true
    general.Initialize()
    appearance.Initialize()
end

function module.Update()
    module.Initialize()
    general.Update()
    appearance.Update()
    logging.Update()
    finishPendingSave()
end

function module.All()
    return pages
end

function module.Find(pageId)
    return pagesById[pageId]
end

function module.Render(ctx, page, fonts)
    if page.id == "General" then
        general.RenderPage(ctx, fonts)
    elseif page.id == "Appearance" then
        appearance.RenderPage(ctx, fonts)
    elseif page.id == "Logging" then
        logging.RenderPage(ctx)
    else
        renderPlaceholder(ctx, page)
    end
end

function module.IsDirty(page)
    if not page then return false end
    if page.id == "General" then return general.IsDirty() end
    if page.id == "Appearance" then return appearance.IsDirty() end
    if page.id == "Logging" then return logging.IsDirty() end
    return false
end

function module.HasAnyDirty()
    return general.IsDirty() or appearance.IsDirty() or logging.IsDirty()
end

function module.ValidateAll()
    local generalValid, generalError = general.Validate()
    if not generalValid then return false, generalError end
    local loggingValid, loggingError = logging.Validate()
    if not loggingValid then return false, loggingError end
    return appearance.Validate()
end

function module.IsBusy()
    return general.IsBusy() or appearance.IsBusy() or logging.IsBusy() or state.pendingSaveStage ~= ""
end

function module.SaveAll()
    if module.IsBusy() then return false, "Wait for the current settings operation" end
    local valid, validationError = module.ValidateAll()
    if not valid then return false, validationError end
    if not module.HasAnyDirty() then
        state.status = "No unsaved changes"
        return true
    end
    if general.IsDirty() then
        local accepted, saveError = general.Save()
        if not accepted then return false, saveError end
        state.pendingSaveStage = "General"
        state.status = "Saving General settings..."
        return true
    end
    if logging.IsDirty() then
        local accepted, saveError = logging.Save()
        if not accepted then return false, saveError end
        state.pendingSaveStage = "Logging"
        state.status = "Saving Logging settings..."
        return true
    end
    finishAppearanceSave()
    return state.status == "Changes saved", state.status == "Changes saved" and nil or state.status
end

function module.RevertAll()
    if module.IsBusy() then return false, "Wait for the current settings operation" end
    local reverted, revertError = general.Revert()
    if not reverted then return false, revertError end
    reverted, revertError = logging.Revert()
    if not reverted then return false, revertError end
    appearance.Revert()
    state.status = "Draft reverted"
    return true
end

function module.SetGeneralContext(surfaceName, pageName)
    return general.SetContext(surfaceName, pageName)
end

function module.NavigateLogging(sessionId, byteOffset)
    return logging.Navigate(sessionId, byteOffset)
end

function module.Shutdown()
    general.Shutdown()
    appearance.Shutdown()
    logging.Shutdown()
end

function module.GetStatus()
    if state.status ~= "" then return state.status end
    local generalStatus = general.GetStatus()
    if generalStatus ~= "" then return generalStatus end
    local loggingStatus = logging.GetStatus()
    if loggingStatus ~= "" then return loggingStatus end
    return appearance.GetStatus()
end

return module
