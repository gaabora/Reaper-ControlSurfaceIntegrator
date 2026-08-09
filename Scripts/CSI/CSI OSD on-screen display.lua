
--[[
 * ReaScript Name: @PRODUCT_DISPLAY_NAME@ OSD on-screen display
 * About: Show OSD text box with data from @PRODUCT_DISPLAY_NAME@ control surfaces using ImGui.
 *        Displays at top or bottom of screen, with configurable size and transparency.
 * Author: @PRODUCT_DISPLAY_NAME@ Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 2.0.0
 * Notes: Requires ReaImGui. Shares OSD UI logic with @PRODUCT_DISPLAY_NAME@ OSK via osd_ui module.
--]]

local r = reaper
local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local host = dofile(scriptDir .. "script_host.lua")
local imgui = host.RequireImGui(scriptDir)
if not imgui then return end

local identity = require("product_identity")
local font_cache = require("font_cache")
local osd_ui = require("osd_ui")
local theme = require("theme_settings")

local ctx = nil
local FONT_SMALL = nil
local fontCache = nil

local function main()
    if not host.IsContextValid(ctx) then
        host.SetToolbarState(-1)
        return
    end

    osd_ui.PollOSD()

    local screenW, screenH = theme.OSD.fallback_screen_width, theme.OSD.fallback_screen_height
    local originX, originY = 0, 0

    -- Prefer REAPER client rectangle to avoid covering app title bar.
    if r.APIExists and r.APIExists("JS_Window_GetClientRect") and r.APIExists("JS_Window_ClientToScreen") then
        local hwnd = r.GetMainHwnd()
        local ok, cl, ct, cr, cb = r.JS_Window_GetClientRect(hwnd)
        if ok and cr and cb and cr > cl and cb > ct then
            local sx, sy = r.JS_Window_ClientToScreen(hwnd, 0, 0)
            if sx and sy then
                originX, originY = sx, sy
                screenW = cr - cl
                screenH = cb - ct
            end
        end
    elseif r.my_getViewport then
        local left, top, right, bottom = r.my_getViewport(0, 0, 0, 0, 0, 0, 0, 0, true)
        if left and top and right and bottom and right > left and bottom > top then
            originX = left
            originY = top
            screenW = right - left
            screenH = bottom - top
        end
    end

    osd_ui.RenderOSDWindow(ctx, imgui, screenW, screenH, screenW, screenH, originX, originY)
    
    r.defer(main)
end

local function Init()
    host.SetToolbarState(1)
    osd_ui.LoadSettings()

    ctx = host.CreateContext(identity.displayName .. " OSD")
    fontCache = font_cache.New(imgui, ctx)
    local fonts = fontCache:Build(theme.DEFAULT_FONT_DEFINITIONS)
    FONT_SMALL = fonts.small
    osd_ui.SetFont(FONT_SMALL)
    osd_ui.SetFontCache(fontCache)

    host.OnExit(function()
        osd_ui.SaveSettings()
        host.SetToolbarState(-1)
    end)

    main()
end

Init()
