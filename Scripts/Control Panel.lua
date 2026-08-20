--[[
 * ReaScript Name: ReaControlSurface Control Panel
 * About: Open the shared ReaControlSurface settings window.
 * Author: Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 1.0.0
--]]

local reaperApi = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local host = dofile(scriptDir .. "script_host.lua")
local imgui = host.RequireImGui(scriptDir)
if not imgui then return end

local identity = require("product_identity")
local lifecycleProtocol = require("control_panel_protocol")
local controlPanel = require("control_panel_ui")

local ctx = host.CreateContext(identity.displayName .. " Control Panel")
local state = controlPanel.New(ctx)

local function main()
    if not host.IsContextValid(ctx) then return end
    if not controlPanel.Render(state) then return end
    reaperApi.defer(main)
end

host.OnExit(function()
    controlPanel.SaveWindowState(state)
    lifecycleProtocol.SetOpen(false)
    host.SetToolbarState(-1)
end)
host.SetToolbarState(1)
lifecycleProtocol.SetOpen(true)
main()
