local r = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local identity = dofile(scriptDir .. "product_identity.lua")

local M = {}

local imguiModule = nil

local function prependPackagePath(pathEntry)
    if not pathEntry or pathEntry == "" then return end
    if package.path:find(pathEntry, 1, true) then return end
    package.path = pathEntry .. ";" .. package.path
end

function M.RequireImGui(scriptDir)
    if not r.ImGui_GetBuiltinPath then
        r.ShowMessageBox("Script needs ReaImGui.\nPlease install it in the next window.", "MISSING DEPENDENCY", 0)
        r.ReaPack_BrowsePackages('^ReaImGui:')
        return nil
    end

    local builtInPath = r.ImGui_GetBuiltinPath() .. "/?.lua"
    prependPackagePath(scriptDir .. "?.lua")
    prependPackagePath(builtInPath)

    imguiModule = require "imgui" "0.9.3"
    return imguiModule
end

function M.CreateContext(name, fonts)
    if not imguiModule then return nil, {} end

    local ctx = imguiModule.CreateContext(name)
    local createdFonts = {}

    for idx, definition in ipairs(fonts or {}) do
        local family = definition.family or "sans-serif"
        local size = definition.size or definition.px or 13
        local font = imguiModule.CreateFont(family, size)
        imguiModule.Attach(ctx, font)
        createdFonts[definition.key or idx] = font
    end

    return ctx, createdFonts
end

function M.SetToolbarState(value)
    local _, _, sectionId, commandId = r.get_action_context()
    r.SetToggleCommandState(sectionId, commandId, value or 0)
    r.RefreshToolbar2(sectionId, commandId)
end

function M.IsContextValid(ctx)
    if not ctx then return false end
    if r.ImGui_ValidatePtr then
        return r.ImGui_ValidatePtr(ctx, "ImGui_Context*")
    end
    return true
end

function M.OnExit(cleanupFn)
    r.atexit(function()
        if not cleanupFn then return end
        local ok, err = pcall(cleanupFn)
        if not ok and r.ShowConsoleMsg then
            r.ShowConsoleMsg("[" .. identity.displayName .. " script_host] Cleanup failed: " .. tostring(err) .. "\n")
        end
    end)
end

return M
