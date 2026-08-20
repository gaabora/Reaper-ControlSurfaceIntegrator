local imgui = require "imgui" "0.9.3"

local appearance = require("control_panel_appearance")
local general = require("settings_ui")

local module = {}

local pages = {
    {
        id = "Devices",
        label = "Devices",
        title = "Devices",
        description = "Device definitions, Pages, Surface assignments, and listener relationships will move here in later stages. Use the native configuration window for editing now.",
    },
    {
        id = "General",
        label = "General",
        title = "General",
    },
    {
        id = "Appearance",
        label = "Appearance",
        title = "Appearance",
    },
    {
        id = "Logging",
        label = "Logging",
        title = "Logging",
        description = "Logging settings and the log viewer will be available here in Phase 3.",
    },
}

local pagesById = {}
for pageIndex, page in ipairs(pages) do pagesById[page.id] = page end

local state = {
    initialized = false,
    pendingSaveAll = false,
    status = "",
}

local function renderPlaceholder(ctx, page)
    imgui.Text(ctx, page.title)
    imgui.Separator(ctx)
    imgui.Spacing(ctx)
    if imgui.TextWrapped then imgui.TextWrapped(ctx, page.description) else imgui.Text(ctx, page.description) end
end

local function finishPendingSave()
    if not state.pendingSaveAll or general.IsBusy() then return end
    if general.HasError() or general.IsDirty() then
        state.pendingSaveAll = false
        state.status = general.GetStatus() ~= "" and general.GetStatus() or "General settings were not saved"
        return
    end
    local saved, saveError = appearance.Save()
    state.pendingSaveAll = false
    state.status = saved and "Changes saved" or tostring(saveError or "Cannot save Appearance settings")
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
    finishPendingSave()
end

function module.All()
    return pages
end

function module.Find(pageId)
    return pagesById[pageId]
end

function module.Render(ctx, page)
    if page.id == "General" then
        imgui.Text(ctx, page.title)
        imgui.Separator(ctx)
        general.RenderPage(ctx)
    elseif page.id == "Appearance" then
        appearance.RenderPage(ctx)
    else
        renderPlaceholder(ctx, page)
    end
end

function module.IsDirty(page)
    if not page then return false end
    if page.id == "General" then return general.IsDirty() end
    if page.id == "Appearance" then return appearance.IsDirty() end
    return false
end

function module.HasAnyDirty()
    return general.IsDirty() or appearance.IsDirty()
end

function module.ValidateAll()
    local generalValid, generalError = general.Validate()
    if not generalValid then return false, generalError end
    return appearance.Validate()
end

function module.IsBusy()
    return general.IsBusy() or appearance.IsBusy() or state.pendingSaveAll
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
        state.pendingSaveAll = true
        state.status = "Saving General settings..."
        return true
    end
    local saved, saveError = appearance.Save()
    state.status = saved and "Changes saved" or tostring(saveError or "Cannot save Appearance settings")
    return saved, saveError
end

function module.RevertAll()
    if module.IsBusy() then return false, "Wait for the current settings operation" end
    local reverted, revertError = general.Revert()
    if not reverted then return false, revertError end
    appearance.Revert()
    state.status = "Draft reverted"
    return true
end

function module.RefreshGeneral()
    return general.Refresh()
end

function module.SetGeneralContext(surfaceName, pageName)
    return general.SetContext(surfaceName, pageName)
end

function module.Shutdown()
    general.Shutdown()
    appearance.Shutdown()
end

function module.GetConfigurationStatus()
    return general.GetConfigurationStatus()
end

function module.GetStatus()
    if state.status ~= "" then return state.status end
    local generalStatus = general.GetStatus()
    if generalStatus ~= "" then return generalStatus end
    return appearance.GetStatus()
end

return module
