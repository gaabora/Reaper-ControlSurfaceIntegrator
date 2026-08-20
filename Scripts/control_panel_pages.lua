local imgui = require "imgui" "0.9.3"

local module = {}

local pages = {
    {
        id = "Devices",
        label = "Devices",
        title = "Devices",
        description = "Device definitions, Pages, Surface assignments, and listener relationships will move here in later stages. Use the native configuration window for editing now.",
        hasDraft = false,
    },
    {
        id = "General",
        label = "General",
        title = "General",
        description = "Product and Surface behavior settings will be available here in Phase 2.",
        hasDraft = true,
    },
    {
        id = "Appearance",
        label = "Appearance",
        title = "Appearance",
        description = "Common, OSK, OSD, and Notifications appearance settings will be available here in Phase 2.",
        hasDraft = true,
    },
    {
        id = "Logging",
        label = "Logging",
        title = "Logging",
        description = "Logging settings and the log viewer will be available here in Phase 3.",
        hasDraft = true,
    },
}

local pagesById = {}
for idx, page in ipairs(pages) do
    page.dirty = false
    pagesById[page.id] = page
end

local function renderPlaceholder(ctx, page)
    imgui.Text(ctx, page.title)
    imgui.Separator(ctx)
    imgui.Spacing(ctx)
    if imgui.TextWrapped then imgui.TextWrapped(ctx, page.description) else imgui.Text(ctx, page.description) end
end

function module.All()
    return pages
end

function module.Find(pageId)
    return pagesById[pageId]
end

function module.Render(ctx, page)
    renderPlaceholder(ctx, page)
end

function module.IsDirty(page)
    return page and page.dirty == true
end

function module.HasAnyDirty()
    for idx, page in ipairs(pages) do
        if page.dirty then return true end
    end
    return false
end

function module.Save(page)
    if not page then return false, "No page is selected" end
    page.dirty = false
    return true
end

function module.SaveAll()
    for idx, page in ipairs(pages) do
        local saved, saveError = module.Save(page)
        if not saved then return false, saveError end
    end
    return true
end

function module.Revert(page)
    if page then page.dirty = false end
end

function module.RevertAll()
    for idx, page in ipairs(pages) do module.Revert(page) end
end

return module
