local imgui = require "imgui" "0.9.3"

local ui = require("ui_components")
local log_writer = require("log_writer")

local M = {}

local TOOLBAR_ACTIONS = {
    { action = 42348, title = "Reset devices", tooltip = "Reset all MIDI control surface devices" },
    { action = 41175, title = "Reset MIDI", tooltip = "Reset all MIDI devices" },
}

function M.RenderContextMenu(ctx, popupId, surfName, deps)
    deps = deps or {}
    local data = deps.data
    local osd_ui = deps.osd_ui
    if not imgui.BeginPopupContextWindow(ctx, popupId) then return end

    if imgui.MenuItem(ctx, "Settings...") and deps.onOpenSettings then deps.onOpenSettings() end
    ui.ItemTooltip(ctx, "Open Appearance settings")
    imgui.Spacing(ctx)
    for _, action in ipairs(TOOLBAR_ACTIONS) do
        if imgui.MenuItem(ctx, action.title) then
            reaper.Main_OnCommand(action.action, 0)
            log_writer.Write("NOTICE", "[" .. data.productDisplayName .. " OSK] Action: " .. action.action .. " (" .. action.tooltip .. ")")
        end
        ui.ItemTooltip(ctx, action.tooltip)
    end

    imgui.Spacing(ctx)
    osd_ui.RenderOSKPositionToggle(ctx, imgui, surfName)
    imgui.EndPopup(ctx)
end

return M
