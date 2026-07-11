--[[
 * ReaScript Name: CSI OSK on-screen keyboard
 * About: On-screen keyboard display for CSI control surfaces.
 *        Shows colorful buttons matching the physical surface layout,
 *        with labels reflecting current zone/modifier bindings.
 * Author: CSI Contributors
 * Licence: GPL v3
 * REAPER: 7.0
 * Version: 1.0.0
--]]

-- Requires ReaImGui
if not reaper.ImGui_GetBuiltinPath then
    reaper.ShowMessageBox("Script needs ReaImGui.\nPlease install it in the next window.", "MISSING DEPENDENCY", 0)
    reaper.ReaPack_BrowsePackages('^ReaImGui:')
    return
end

local r = reaper
package.path = r.ImGui_GetBuiltinPath() .. '/?.lua'
local imgui = require "imgui" "0.9.3"

-- ================================================================
-- Constants
-- ================================================================
local EXT_SECTION     = "CSI_OSK"
local EXT_CMD_SECTION = "CSI_OSK_CMD"
local EXT_SETTINGS    = "CSI_OSK_SETTINGS"

local COLORS = {
    win_bg         = 0x1E1E1Eff,
    button_off     = 0x3A3A3Aff,
    button_on      = 0x5A8A5Aff,
    button_hover   = 0x4A6A9Aff,
    text_normal    = 0x000000ff,
    text_dim       = 0x444444ff,
    tab_bg         = 0x2A2A2Aff,
    tab_active     = 0x3C6191ff,
    round_off      = 0x444444ff,
    round_on_play  = 0x40A040ff,
    round_on_stop  = 0x808080ff,
    round_on_rec   = 0xCC3030ff,
    arrow_off      = 0x505050ff,
    arrow_on       = 0x70B070ff,
}

-- ================================================================
-- State
-- ================================================================
local ctx = nil
local FONT = nil
local FONT_SMALL = nil

local surfaces = {}          -- array of surface names
local currentSurface = 1     -- selected tab index

-- per-surface data: layouts[surfName], states[surfName], labels[surfName]
local layouts     = {}       -- parsed layout: array of rows, each row = array of cells
local states      = {}       -- parsed state: widgetName -> {value=number, color=0xRRGGBBff}
local labels      = {}       -- parsed labels: widgetName -> string (current modifier)
local labelMaps   = {}       -- all modifier bindings: widgetName -> {NoMod="Touch", Shift="Latch", Hold="Trim"}
local rawLayouts  = {}
local rawStates   = {}
local rawLabels   = {}
local rawLabelMaps = {}

local BUTTON_SIZE = 64
local BUTTON_PAD_H = 8  -- horizontal padding between buttons (pixels before zoom)
local BUTTON_PAD_V = 0  -- vertical padding between rows (pixels before zoom)
local ZOOM        = 0.9
local BUTTON_ASPECT = 1.4  -- width/height ratio
local ARROW_ANGLE = 120    -- arrow triangle apex angle in degrees (>90 = obtuse)

-- Configurable extra action buttons shown at top of right-click menu.
-- Each entry: { title = "label", tooltip = "description", action = <REAPER command ID> }
local TOOLBAR_ACTIONS = {
    { action = 42348, title = "reset surfaces", tooltip = "Reset all MIDI control surface devices" },
    { action = 41175, title = "reset MIDI", tooltip = "Reset all MIDI devices" },
    -- Add more: { title = "Label", tooltip = "Description", action = 12345 },
}

-- Tooltip hover tracking
local hoverStartTime = {}  -- [widgetName] = os.clock() when hover started
local TOOLTIP_DELAY = 1.0  -- seconds before tooltip appears

-- OSD (on-screen display) status bar state
-- Data arrives via the same ExtState that the standalone CSI OSD script uses.
local EXT_OSD_SECTION = "CSI_TMP"
local EXT_OSD_KEY     = "OSD"
local osd_text        = ""           -- current message text
local osd_bg_color    = 0x333333ff   -- background colour for status bar
local osd_show_until  = 0            -- r.time_precise() deadline; 0 = permanent
local osd_last_msg    = nil          -- last raw message string (change detection)

-- Default label replacements table  (case-insensitive matching, can be appended from vars.label_replacements)
local LABEL_REPLACEMENTS = {
    ["toggle"] = "",
    ["reaper"] = "",
    ["toolbar"] = "",
    ["move edit"] = "",
    ["cycle"] = "",
    ["previous"] = "prev",
    ["current"] = "curr",
    ["one"] = "1",
    ["and"] = "&",
    ["show"] = "",
    -- Add more: ["SomeWord"] = "Replacement",
} 

-- Settings
local vars = {
    wlen = 500, hlen = 500, xpos = 200, ypos = 200, docked = 0,
    zoom = 0.9, clickable = true, aspect = 1.4,
    pad_h = 6, pad_v = 6,
    transparency = 0.6,      -- window background alpha (0.0 = fully transparent, 1.0 = opaque)
    btn_transparency = 0.9,  -- button alpha (0.2 = very transparent, 1.0 = opaque)
    tooltip_delay = 1.0,     -- seconds before tooltip shows
    arrow_angle = 120,       -- arrow apex angle in degrees
    -- Label replacements: semicolon-separated key=value pairs. Empty value = remove the word
    label_replacements = "",
}

-- ================================================================
-- Utility
-- ================================================================
local function hexToImCol(hex)
    -- Accepts "#RRGGBB" or "RRGGBB" (6 hex digits, optional leading #)
    if not hex then return COLORS.button_off end
    if hex:sub(1,1) == "#" then hex = hex:sub(2) end
    if #hex < 6 then return COLORS.button_off end
    local R = tonumber(hex:sub(1,2), 16) or 0
    local G = tonumber(hex:sub(3,4), 16) or 0
    local B = tonumber(hex:sub(5,6), 16) or 0
    return (R << 24) | (G << 16) | (B << 8) | 0xFF
end

local function dimColor(col, factor)
    local r = ((col >> 24) & 0xFF)
    local g = ((col >> 16) & 0xFF)
    local b = ((col >> 8) & 0xFF)
    r = math.floor(r * factor)
    g = math.floor(g * factor)
    b = math.floor(b * factor)
    return (r << 24) | (g << 16) | (b << 8) | 0xFF
end

local function brightenColor(col, amount)
    local r = math.min(255, ((col >> 24) & 0xFF) + amount)
    local g = math.min(255, ((col >> 16) & 0xFF) + amount)
    local b = math.min(255, ((col >> 8) & 0xFF) + amount)
    return (r << 24) | (g << 16) | (b << 8) | 0xFF
end

--- Lift a color so its perceived luminance is at least minLum (0-255).
--- Applied to *inactive* button backgrounds so black text stays readable.
--- Active/runtime colors are left untouched (they represent real LED colours).
---
--- Uses *additive* gray lift (add the same delta to R, G, B) rather than
--- proportional scaling.  Proportional scaling would bring a saturated
--- single-channel color (e.g. pure red or pure blue at 40% dim) all the way
--- back to its full-brightness value, making active and inactive look identical.
--- Adding a uniform offset raises luminance while keeping the hue darker and
--- desaturated relative to the active color, so they remain visually distinct.
local function ensureMinLuminance(col, minLum)
    minLum = minLum or 80
    local R = (col >> 24) & 0xFF
    local G = (col >> 16) & 0xFF
    local B = (col >> 8) & 0xFF
    local lum = 0.299 * R + 0.587 * G + 0.114 * B
    if lum < minLum then
        -- Because 0.299+0.587+0.114 = 1.0, adding `add` to every channel
        -- raises perceived luminance by exactly `add`.
        local add = minLum - lum
        R = math.min(255, math.floor(R + add))
        G = math.min(255, math.floor(G + add))
        B = math.min(255, math.floor(B + add))
    end
    return (R << 24) | (G << 16) | (B << 8) | 0xFF
end

--- Apply alpha to an ImGui color (0xRRGGBBFF -> 0xRRGGBBAA)
local function applyAlpha(col, alpha)
    return (col & 0xFFFFFF00) | math.floor(alpha * 255)
end

--- Strip "Prefix: " from label text (e.g. "SWS/SM: Command name" -> "Command name")
local function stripLabelPrefix(text)
    local after = text:match("^[^:]+:%s*(.+)$")
    return after or text
end

--- Split PascalCase/camelCase into Title Case
--- e.g. "FastForward" -> "Fast Forward", "RotaryPush" -> "Rotary Push"
local function splitPascalCase(text)
    -- Insert space before each uppercase letter that follows a lowercase letter
    -- or before a run of uppercase followed by a lowercase
    local result = text:gsub("(%l)(%u)", "%1 %2")  -- camelCase boundary
    result = result:gsub("(%u%u)(%u%l)", "%1 %2")   -- ABCDef -> ABC Def
    return result
end

--- Build a case-insensitive Lua pattern from a plain word string.
--- e.g. "toggle" -> "[Tt][Oo][Gg][Gg][Ll][Ee]"
local function caseInsensitivePattern(word)
    return word:gsub("%a", function(c)
        return "[" .. c:upper() .. c:lower() .. "]"
    end)
end

--- Apply configurable word replacements to label text (case-insensitive matching)
local function applyLabelReplacements(text)
    for word, replacement in pairs(LABEL_REPLACEMENTS) do
        local pattern = caseInsensitivePattern(word)
        text = text:gsub("%f[%w]" .. pattern .. "%f[%W]", replacement)
    end
    -- Collapse multiple spaces and trim
    text = text:gsub("%s+", " "):match("^%s*(.-)%s*$")
    return text
end

--- Full label processing pipeline: strip prefix, split PascalCase, apply replacements
local function processLabel(text)
    text = stripLabelPrefix(text)
    text = splitPascalCase(text)
    text = applyLabelReplacements(text)
    return text
end

--- Parse label replacements string "key1=val1;key2=val2" into LABEL_REPLACEMENTS table
local function parseLabelReplacements(str)
    LABEL_REPLACEMENTS = {}
    if not str or str == "" then return end
    for entry in str:gmatch("[^;]+") do
        local k, v = entry:match("^(.-)=(.*)$")
        if k and k ~= "" then
            LABEL_REPLACEMENTS[k] = v
        end
    end
end

--- Poll CSI_TMP/OSD ExtState and update the OSD bar state.
--- Message format (same as stand-alone CSI OSD script): "text;bgColorOrFlag;timeoutMs"
---   bgColorOrFlag: "1" = default on-color, "0"/"" = default off-color, or "#RRGGBB"
---   timeoutMs:     milliseconds to display; 0 = permanent until next message
local function PollOSD()
    local msg = r.GetExtState(EXT_OSD_SECTION, EXT_OSD_KEY)
    if msg ~= osd_last_msg then
        osd_last_msg = msg
        if not msg or msg == "" then
            osd_text       = ""
            osd_show_until = 0
            osd_bg_color   = 0x333333ff
            return
        end
        -- Parse fields (fields may be absent)
        local text, bgStr, timeoutStr = msg:match("([^;]*);?([^;]*);?([^;]*)")
        text = text and text:match("^%s*(.-)%s*$") or ""
        local timeoutMs = tonumber(timeoutStr) or 3000
        if bgStr == "1" then
            osd_bg_color = 0xA4A4A4ff
        elseif bgStr == "" or bgStr == "0" then
            osd_bg_color = 0x333333ff
        else
            osd_bg_color = hexToImCol(bgStr)
        end
        osd_text = text
        osd_show_until = (timeoutMs > 0) and (r.time_precise() + timeoutMs / 1000) or 0
    end
    -- Expire timed-out messages
    if osd_show_until > 0 and r.time_precise() > osd_show_until then
        osd_text       = ""
        osd_show_until = 0
    end
end

--- Render the OSD status bar below the surface buttons.
--- Calls PollOSD() itself; shows nothing when osd_text is empty.
local function RenderOSDBar(ctx)
    PollOSD()
    if osd_text == "" then return end

    imgui.Separator(ctx)
    local drawList = imgui.GetWindowDrawList(ctx)
    local cx, cy   = imgui.GetCursorScreenPos(ctx)
    local avW      = imgui.GetContentRegionAvail(ctx)

    imgui.PushFont(ctx, FONT_SMALL)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local padV = 4
    local barH = lineH + padV * 2

    -- Background rectangle (inherits window transparency)
    local bgCol = applyAlpha(osd_bg_color, vars.transparency)
    imgui.DrawList_AddRectFilled(drawList, cx, cy, cx + avW, cy + barH, bgCol, 0)

    -- Choose text colour for contrast (black on light bg, white on dark bg)
    local lum = 0.299 * ((osd_bg_color >> 24) & 0xFF)
             + 0.587 * ((osd_bg_color >> 16) & 0xFF)
             + 0.114 * ((osd_bg_color >>  8) & 0xFF)
    local textCol = (lum > 128) and 0x000000ff or 0xffffffff

    -- Center the text horizontally
    local tw = imgui.CalcTextSize(ctx, osd_text)
    local tx = cx + (avW - tw) / 2
    local ty = cy + padV
    imgui.DrawList_AddText(drawList, tx, ty, textCol, osd_text)

    -- Advance cursor so ImGui accounts for the bar height
    imgui.Dummy(ctx, avW, barH)
    imgui.PopFont(ctx)
end

--- Show tooltip with delay: only display after hovering for TOOLTIP_DELAY seconds.
--- When a LabelMap is available for the widget, the tooltip shows all modifier bindings
--- (e.g. "Touch\nShift -> Latch\nHold -> Trim") instead of just the default text.
local function ShowDelayedTooltip(ctx, widgetName, text)
    if imgui.IsItemHovered(ctx) then
        local now = os.clock()
        if not hoverStartTime[widgetName] then
            hoverStartTime[widgetName] = now
        end
        if now - hoverStartTime[widgetName] >= TOOLTIP_DELAY then
            -- Build tooltip from LabelMap if available
            local surfName = surfaces[currentSurface]
            local modMap = surfName and labelMaps[surfName] and labelMaps[surfName][widgetName]
            local tooltipText = text
            if modMap and next(modMap) then
                local lines = {}
                -- NoMod first (primary binding)
                if modMap["NoMod"] then
                    lines[#lines + 1] = processLabel(modMap["NoMod"])
                end
                -- All other modifiers sorted alphabetically
                local sortedMods = {}
                for k in pairs(modMap) do
                    if k ~= "NoMod" then sortedMods[#sortedMods + 1] = k end
                end
                table.sort(sortedMods)
                for _, modName in ipairs(sortedMods) do
                    lines[#lines + 1] = modName .. " -> " .. processLabel(modMap[modName])
                end
                if #lines > 0 then
                    tooltipText = table.concat(lines, "\n")
                end
            end
            if imgui.BeginTooltip(ctx) then
                imgui.Text(ctx, tooltipText)
                imgui.EndTooltip(ctx)
            end
        end
    else
        hoverStartTime[widgetName] = nil
    end
end

--- Word-wrap text to fit within maxW pixels, returns array of lines
local function wrapText(ctx, text, maxW)
    local lines = {}
    local words = {}
    for w in text:gmatch("%S+") do words[#words + 1] = w end
    if #words == 0 then return { text } end
    local line = words[1]
    for i = 2, #words do
        local test = line .. " " .. words[i]
        local tw = imgui.CalcTextSize(ctx, test)
        if tw > maxW and line ~= "" then
            lines[#lines + 1] = line
            line = words[i]
        else
            line = test
        end
    end
    lines[#lines + 1] = line
    return lines
end

-- ================================================================
-- Parsing
-- ================================================================
local function ParseLayout(layoutStr)
    local result = {}
    for rowStr in layoutStr:gmatch("[^\n]+") do
        local row = {}
        for cellStr in rowStr:gmatch("[^|]+") do
            local cell = {}
            if cellStr:match("^SPACER:") then
                cell.isSpacer = true
                cell.width = tonumber(cellStr:match("SPACER:([%d%.]+)")) or 0.5
            else
                cell.isSpacer = false
                local name = cellStr:match("^([^:]+)")
                cell.name = name
                cell.shape  = cellStr:match("Shape=([^,]+)") or "Rect"
                cell.width  = tonumber(cellStr:match("Width=([%d%.]+)")) or 1.0
                cell.height = tonumber(cellStr:match("Height=([%d%.]+)")) or 1.0
                cell.group  = cellStr:match("Group=([^,]+)") or ""
                cell.label  = cellStr:match("Label=(.+)$") or ""
                -- Accept "Color=#RRGGBB" or "Color=RRGGBB" (hexToImCol handles both)
                local colorHex    = cellStr:match("Color=(#?%x+)")
                if colorHex    then cell.color    = hexToImCol(colorHex)    end
            end
            row[#row + 1] = cell
        end
        result[#result + 1] = row
    end
    return result
end

local function ParseState(stateStr)
    local result = {}
    if not stateStr or stateStr == "" then return result end
    for entry in stateStr:gmatch("[^;]+") do
        local name, rest = entry:match("^(.-)=(.+)$")
        if name and rest then
            local value = tonumber(rest:match("V:([%d%.%-]+)")) or 0
            local colorHex = rest:match("C:(#%x+)") or "#333333"
            result[name] = {
                value = value,
                color = hexToImCol(colorHex),
            }
        end
    end
    return result
end

local function ParseLabels(labelsStr)
    local result = {}
    if not labelsStr or labelsStr == "" then return result end
    for entry in labelsStr:gmatch("[^;]+") do
        local name, label = entry:match("^(.-)=(.+)$")
        if name then
            result[name] = label
        end
    end
    return result
end

--- Parse LabelMap string from C++: "Touch=NoMod:Touch|Shift:Latch|Hold:Trim;..."
--- Returns widgetName -> {modifierName -> rawLabel} table.
local function ParseLabelMap(str)
    local result = {}
    if not str or str == "" then return result end
    for entry in str:gmatch("[^;]+") do
        local name, modPairs = entry:match("^(.-)=(.+)$")
        if name and modPairs then
            local mods = {}
            for modEntry in modPairs:gmatch("[^|]+") do
                local modName, label = modEntry:match("^([^:]+):(.+)$")
                if modName and label then
                    mods[modName] = label
                end
            end
            result[name] = mods
        end
    end
    return result
end

-- ================================================================
-- ExtState I/O
-- ================================================================
local function LoadSettings()
    for k, v in pairs(vars) do
        if r.HasExtState(EXT_SETTINGS, k) then
            local val = r.GetExtState(EXT_SETTINGS, k)
            if type(v) == "number" then
                vars[k] = tonumber(val) or v
            elseif type(v) == "boolean" then
                vars[k] = val == "true"
            else
                vars[k] = val
            end
        end
    end
    ZOOM = vars.zoom
    BUTTON_ASPECT = vars.aspect
    BUTTON_PAD_H = vars.pad_h
    BUTTON_PAD_V = vars.pad_v
    TOOLTIP_DELAY = vars.tooltip_delay
    ARROW_ANGLE = vars.arrow_angle
    parseLabelReplacements(vars.label_replacements)
end

local function SaveSettings()
    for k, v in pairs(vars) do
        r.SetExtState(EXT_SETTINGS, k, tostring(v), true)
    end
end

local function SetToolbarButtonState(set)
    local _, _, sec, cmd = r.get_action_context()
    r.SetToggleCommandState(sec, cmd, set or 0)
    r.RefreshToolbar2(sec, cmd)
end

-- ================================================================
-- Poll data from C++
-- ================================================================
local function PollData()
    -- Check for close command
    if r.HasExtState(EXT_SECTION, "Command") then
        local cmd = r.GetExtState(EXT_SECTION, "Command")
        if cmd == "Close" then
            r.DeleteExtState(EXT_SECTION, "Command", false)
            return false  -- signal close
        end
        r.DeleteExtState(EXT_SECTION, "Command", false)
    end

    -- Get surface list
    local surfStr = r.GetExtState(EXT_SECTION, "Surfaces")
    if surfStr and surfStr ~= "" then
        local newSurfaces = {}
        for name in surfStr:gmatch("[^|]+") do
            newSurfaces[#newSurfaces + 1] = name
        end
        if #newSurfaces > 0 then
            surfaces = newSurfaces
            if currentSurface > #surfaces then currentSurface = 1 end
        end
    end

    -- For each surface, poll layout/state/labels
    for _, surfName in ipairs(surfaces) do
        -- Layout (only parse when changed)
        local layoutKey = "Layout_" .. surfName
        local raw = r.GetExtState(EXT_SECTION, layoutKey)
        if raw and raw ~= "" and raw ~= rawLayouts[surfName] then
            rawLayouts[surfName] = raw
            layouts[surfName] = ParseLayout(raw)
        end

        -- State
        local stateKey = "State_" .. surfName
        local rawS = r.GetExtState(EXT_SECTION, stateKey)
        if rawS and rawS ~= rawStates[surfName] then
            rawStates[surfName] = rawS
            states[surfName] = ParseState(rawS)
        end

        -- Labels (current modifier)
        local labelKey = "Labels_" .. surfName
        local rawL = r.GetExtState(EXT_SECTION, labelKey)
        if rawL and rawL ~= rawLabels[surfName] then
            rawLabels[surfName] = rawL
            labels[surfName] = ParseLabels(rawL)
        end

        -- LabelMap (all modifier bindings, for hover tooltip)
        local labelMapKey = "LabelMap_" .. surfName
        local rawLM = r.GetExtState(EXT_SECTION, labelMapKey)
        if rawLM and rawLM ~= rawLabelMaps[surfName] then
            rawLabelMaps[surfName] = rawLM
            labelMaps[surfName] = ParseLabelMap(rawLM)
        end
    end

    return true -- keep running
end

-- ================================================================
-- Drawing helpers
-- ================================================================
--- Return the display color for a button.
---
--- Priority for ACTIVE state (value > 0):
---   1. Runtime state color from C++ (RGB surfaces) — non-near-black
---   2. cell.color  — fixed-color LED default active colour from Surface.txt
---   3. COLORS.button_on  — generic fallback
---
--- Priority for INACTIVE state (value == 0):
---   1. dimmed cell.color  — derived from active colour
---   2. COLORS.button_off  — generic dark fallback
---
--- Inactive colours are run through ensureMinLuminance() so that black text
--- remains readable even when the surface uses a dark tint for its LEDs.
---
--- TODO: RGB surfaces report their LED colour via state.color in real-time.
---       Those colours are hardware-mapped and may need a display gamma / gamut
---       correction curve before being used as button backgrounds.  Add a
---       configurable correction pass for buttons that have no Color= in Surface.txt.
local function GetButtonColor(surfName, widgetName)
    -- Helper: find cell info in parsed layout
    local function GetCellInfo(sn, wn)
        local layout = layouts[sn]
        if not layout then return nil end
        for _, row in ipairs(layout) do
            for _, cell in ipairs(row) do
                if not cell.isSpacer and cell.name == wn then return cell end
            end
        end
        return nil
    end

    local st   = states[surfName] and states[surfName][widgetName]
    local cell = GetCellInfo(surfName, widgetName)

    local function colorIsMeaningful(col)
        -- Treat near-black (each channel < 11) as "no colour" from the runtime.
        return ((col >> 24) & 0xFF) > 10
            or ((col >> 16) & 0xFF) > 10
            or ((col >> 8)  & 0xFF) > 10
    end

    if st and st.value > 0 then
        -- ── ACTIVE ──────────────────────────────────────────────────────────
        -- 1. Runtime color (RGB surface sends live colour in state)
        if colorIsMeaningful(st.color) then return st.color end
        -- 2. Default active colour defined on the widget in Surface.txt
        if cell and cell.color then return cell.color end
        -- 3. Generic fallback
        return COLORS.button_on
    else
        -- ── INACTIVE ────────────────────────────────────────────────────────
        -- For non-RGB surfaces the LED simply dims; we mirror that visually.
        -- State color is NOT used for inactive — it reflects the active LED hue.
        local col
        if cell and cell.color then
            col = dimColor(cell.color, 0.40)
        else
            col = COLORS.button_off
        end
        -- Lift darkness so black text is always legible on inactive buttons.
        return ensureMinLuminance(col, 80)
    end
end

local function GetButtonValue(surfName, widgetName)
    local st = states[surfName] and states[surfName][widgetName]
    if st then return st.value end
    return 0
end

local function GetButtonLabel(surfName, cell)
    local lbl = labels[surfName] and labels[surfName][cell.name]
    if lbl and lbl ~= "" then return lbl end
    if cell.label and cell.label ~= "" then return cell.label end
    return cell.name or "?"
end

-- Helper to handle mouse-wheel for rotary widgets (sends WidgetScroll ExtState)
local function HandleRotaryMouseWheel(ctx, cell)
    -- if not cell or not cell.name then return end
    -- local isRotary = false
    -- if cell.group and type(cell.group) == "string" and cell.group:lower():find("rotary") then isRotary = true end
    -- if not isRotary and cell.name and type(cell.name) == "string" and cell.name:lower():find("rotary") then isRotary = true end
    -- if not isRotary then return end
    -- if not imgui.IsItemHovered(ctx) or not vars.clickable then return end
    -- local io = imgui.GetIO()
    -- local wheel = 0
    -- if io then
    --     wheel = io.MouseWheel or io.MouseWheelH or 0
    -- end
    -- if wheel and wheel ~= 0 then
    --     local surfName2 = surfaces[currentSurface]
    --     if surfName2 then
    --         local dir = (wheel > 0) and "Inc" or "Dec"
    --         local msg = surfName2 .. "|" .. cell.name .. "|" .. dir
    --         r.SetExtState(EXT_CMD_SECTION, "WidgetScroll", msg, false)
    --         r.ShowConsoleMsg("[CSI OSK] Scroll: " .. msg .. "\n")
    --     end
    -- end
end

local function DrawRectButton(ctx, drawList, surfName, cell, bw, bh)
    local label = processLabel(GetButtonLabel(surfName, cell))
    local bgCol = applyAlpha(GetButtonColor(surfName, cell.name), vars.btn_transparency)
    local value = GetButtonValue(surfName, cell.name)

    local cx, cy = imgui.GetCursorScreenPos(ctx)

    -- Background
    imgui.DrawList_AddRectFilled(drawList, cx, cy, cx + bw, cy + bh, bgCol, 4)

    -- Label text (multi-line word-wrap)
    imgui.PushFont(ctx, FONT_SMALL)
    local pad = 4
    local lines = wrapText(ctx, label, bw - pad * 2)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local totalH = #lines * lineH
    local startY = cy + (bh - totalH) / 2
    local textCol = applyAlpha(COLORS.text_normal, vars.btn_transparency)
    for li, ln in ipairs(lines) do
        local tw = imgui.CalcTextSize(ctx, ln)
        local tx = cx + (bw - tw) / 2
        local ty = startY + (li - 1) * lineH
        if ty + lineH > cy + bh then break end  -- clip overflow
        imgui.DrawList_AddText(drawList, tx, ty, textCol, ln)
    end
    imgui.PopFont(ctx)

    -- Invisible button for interaction
    local clicked = imgui.InvisibleButton(ctx, "##btn_" .. (cell.name or ""), bw, bh)
    if clicked and vars.clickable and cell.name then
        local surfName2 = surfaces[currentSurface]
        if surfName2 then
            local msg = surfName2 .. "|" .. cell.name
            local col = GetButtonColor(surfName2, cell.name)
            r.SetExtState(EXT_CMD_SECTION, "WidgetPress", msg, false)
            r.ShowConsoleMsg(string.format("[CSI OSK] Click: %s  color=#%06X\n", msg, (col >> 8) & 0xFFFFFF))
        end
    end
    ShowDelayedTooltip(ctx, cell.name or "", label)
    -- Mouse-wheel for rotary widgets
    HandleRotaryMouseWheel(ctx, cell)
end

--- Draw a stadium ("discorectangle") or circle shape for round buttons.
--- Round buttons ignore the aspect ratio for their visual shape but occupy the
--- full grid cell (bw x bh) for layout alignment. The circle/stadium is drawn
--- centered within the cell using the aspect-free dimensions (visualW x bh).
local function DrawRoundButton(ctx, drawList, surfName, cell, bw, bh, visualW)
    local label = processLabel(GetButtonLabel(surfName, cell))
    local bgCol = applyAlpha(GetButtonColor(surfName, cell.name), vars.btn_transparency)
    local value = GetButtonValue(surfName, cell.name)

    local cx, cy = imgui.GetCursorScreenPos(ctx)
    -- Center the visual shape within the grid cell
    local offsetX = (bw - visualW) / 2
    local vx = cx + offsetX  -- visual left edge
    local pad2 = 2  -- inset from edges
    local centerX = vx + visualW / 2
    local centerY = cy + bh / 2
    local segments = 18 -- segments per half-circle

    -- Determine if we need a stadium or a circle
    local innerW = visualW - pad2 * 2
    local innerH = bh - pad2 * 2
    local isCircle = math.abs(innerW - innerH) < 2

    local function drawStadiumPath(inset)
        if isCircle then
            -- Simple circle
            local radius = math.min(innerW, innerH) / 2 - inset
            for i = 0, segments * 2 - 1 do
                local angle = (i / (segments * 2)) * math.pi * 2
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
        elseif innerW > innerH then
            -- Horizontal stadium: left half-circle + rect + right half-circle
            local radius = innerH / 2 - inset
            local bodyLeft = vx + pad2 + radius + inset
            local bodyRight = vx + visualW - pad2 - radius - inset
            for i = 0, segments do
                local angle = -math.pi / 2 + (i / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    bodyRight + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
            for i = 0, segments do
                local angle = math.pi / 2 + (i / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    bodyLeft + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
        else
            -- Vertical stadium: top half-circle + rect + bottom half-circle
            local radius = innerW / 2 - inset
            local bodyTop  = cy + pad2 + radius + inset
            local bodyBot  = cy + bh - pad2 - radius - inset
            for i = 0, segments do
                local angle = math.pi + (i / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    bodyTop + radius * math.sin(angle))
            end
            for i = 0, segments do
                local angle = (i / segments) * math.pi
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    bodyBot + radius * math.sin(angle))
            end
        end
    end

    -- Fill background
    drawStadiumPath(0)
    imgui.DrawList_PathFillConvex(drawList, bgCol)

    -- Label (multi-line word-wrap)
    imgui.PushFont(ctx, FONT_SMALL)
    local textPad = 6
    local lines = wrapText(ctx, label, visualW - textPad * 2)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local totalH = #lines * lineH
    local startY = centerY - totalH / 2
    local textCol = applyAlpha(COLORS.text_normal, vars.btn_transparency)
    for li, ln in ipairs(lines) do
        local tw = imgui.CalcTextSize(ctx, ln)
        local tx = centerX - tw / 2
        local ty = startY + (li - 1) * lineH
        if ty + lineH > cy + bh then break end
        imgui.DrawList_AddText(drawList, tx, ty, textCol, ln)
    end
    imgui.PopFont(ctx)

    -- Invisible button (uses full cell width bw for grid alignment)
    local clicked = imgui.InvisibleButton(ctx, "##btn_" .. (cell.name or ""), bw, bh)
    if clicked and vars.clickable and cell.name then
        local surfName2 = surfaces[currentSurface]
        if surfName2 then
            local msg = surfName2 .. "|" .. cell.name
            local col = GetButtonColor(surfName2, cell.name)
            r.SetExtState(EXT_CMD_SECTION, "WidgetPress", msg, false)
            r.ShowConsoleMsg(string.format("[CSI OSK] Click: %s  color=#%06X\n", msg, (col >> 8) & 0xFFFFFF))
        end
    end
    ShowDelayedTooltip(ctx, cell.name or "", label)
    -- Mouse-wheel for rotary widgets
    HandleRotaryMouseWheel(ctx, cell)
end

--- Draw an arrow-shaped button: rectangular body with a triangular "point" glued
--- to one side. The triangle apex angle is configurable via ARROW_ANGLE (degrees).
--- For obtuse angles (>90°), the triangle base extends beyond the body edges,
--- creating a wider, shallower point. Point depth = (dim/2) / tan(angle/2).
--- Supports left, right, up, down directions.
local function DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, direction)
    local label = processLabel(GetButtonLabel(surfName, cell))
    local bgCol = applyAlpha(GetButtonColor(surfName, cell.name), vars.btn_transparency)
    local value = GetButtonValue(surfName, cell.name)

    if value > 0 then bgCol = applyAlpha(COLORS.arrow_on, vars.btn_transparency) end

    local cx, cy = imgui.GetCursorScreenPos(ctx)

    -- Compute triangle point depth from the configurable apex angle.
    -- For a triangle with base = dimension and apex angle = ARROW_ANGLE:
    --   pointDepth = (base/2) / tan(apex_angle/2)
    -- At 90° this gives depth = base/2 (isoceles right triangle).
    -- At 120° (obtuse) this gives a shallower point.
    local halfAngleRad = math.rad(ARROW_ANGLE / 2)
    local pointDepth
    if direction == "left" or direction == "right" then
        pointDepth = (bh / 2) / math.tan(halfAngleRad)
    else
        pointDepth = (bw / 2) / math.tan(halfAngleRad)
    end
    -- Clamp pointDepth so the body part doesn't disappear
    if direction == "left" or direction == "right" then
        pointDepth = math.min(pointDepth, bw * 0.45)
    else
        pointDepth = math.min(pointDepth, bh * 0.45)
    end

    -- Helper to draw the 5-vertex pentagon path
    local function drawArrowPath()
        if direction == "left" then
            local bodyL = cx + pointDepth
            local bodyR = cx + bw
            imgui.DrawList_PathLineTo(drawList, bodyL, cy)           -- body top-left
            imgui.DrawList_PathLineTo(drawList, bodyR, cy)           -- body top-right
            imgui.DrawList_PathLineTo(drawList, bodyR, cy + bh)      -- body bottom-right
            imgui.DrawList_PathLineTo(drawList, bodyL, cy + bh)      -- body bottom-left
            imgui.DrawList_PathLineTo(drawList, cx, cy + bh / 2)     -- triangle tip
        elseif direction == "right" then
            local bodyL = cx
            local bodyR = cx + bw - pointDepth
            imgui.DrawList_PathLineTo(drawList, bodyL, cy)            -- body top-left
            imgui.DrawList_PathLineTo(drawList, bodyR, cy)            -- body top-right
            imgui.DrawList_PathLineTo(drawList, cx + bw, cy + bh / 2) -- triangle tip
            imgui.DrawList_PathLineTo(drawList, bodyR, cy + bh)      -- body bottom-right
            imgui.DrawList_PathLineTo(drawList, bodyL, cy + bh)      -- body bottom-left
        elseif direction == "up" then
            local bodyT = cy + pointDepth
            local bodyB = cy + bh
            imgui.DrawList_PathLineTo(drawList, cx + bw / 2, cy)     -- triangle tip
            imgui.DrawList_PathLineTo(drawList, cx + bw, bodyT)      -- body top-right
            imgui.DrawList_PathLineTo(drawList, cx + bw, bodyB)      -- body bottom-right
            imgui.DrawList_PathLineTo(drawList, cx, bodyB)            -- body bottom-left
            imgui.DrawList_PathLineTo(drawList, cx, bodyT)            -- body top-left
        elseif direction == "down" then
            local bodyT = cy
            local bodyB = cy + bh - pointDepth
            imgui.DrawList_PathLineTo(drawList, cx, bodyT)            -- body top-left
            imgui.DrawList_PathLineTo(drawList, cx + bw, bodyT)      -- body top-right
            imgui.DrawList_PathLineTo(drawList, cx + bw, bodyB)      -- body bottom-right
            imgui.DrawList_PathLineTo(drawList, cx + bw / 2, cy + bh) -- triangle tip
            imgui.DrawList_PathLineTo(drawList, cx, bodyB)            -- body bottom-left
        end
    end

    -- Fill background
    drawArrowPath()
    imgui.DrawList_PathFillConvex(drawList, bgCol)

    -- Label centered in the body area (excluding triangle)
    imgui.PushFont(ctx, FONT_SMALL)
    local labelCX, labelCY
    if direction == "left" then
        labelCX = cx + pointDepth + (bw - pointDepth) / 2
        labelCY = cy + bh / 2
    elseif direction == "right" then
        labelCX = cx + (bw - pointDepth) / 2
        labelCY = cy + bh / 2
    elseif direction == "up" then
        labelCX = cx + bw / 2
        labelCY = cy + pointDepth + (bh - pointDepth) / 2
    elseif direction == "down" then
        labelCX = cx + bw / 2
        labelCY = cy + (bh - pointDepth) / 2
    else
        labelCX = cx + bw / 2
        labelCY = cy + bh / 2
    end

    local bodyW = (direction == "left" or direction == "right") and (bw - pointDepth) or bw
    local lines = wrapText(ctx, label, bodyW - 8)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local totalH = #lines * lineH
    local startY = labelCY - totalH / 2
    local textCol = applyAlpha(COLORS.text_normal, vars.btn_transparency)
    for li, ln in ipairs(lines) do
        local tw = imgui.CalcTextSize(ctx, ln)
        local tx = labelCX - tw / 2
        local ty = startY + (li - 1) * lineH
        if ty + lineH > cy + bh then break end
        imgui.DrawList_AddText(drawList, tx, ty, textCol, ln)
    end
    imgui.PopFont(ctx)

    local clicked = imgui.InvisibleButton(ctx, "##btn_" .. (cell.name or ""), bw, bh)
    if clicked and vars.clickable and cell.name then
        local surfName2 = surfaces[currentSurface]
        if surfName2 then
            local msg = surfName2 .. "|" .. cell.name
            local col = GetButtonColor(surfName2, cell.name)
            r.SetExtState(EXT_CMD_SECTION, "WidgetPress", msg, false)
            r.ShowConsoleMsg(string.format("[CSI OSK] Click: %s  color=#%06X\n", msg, (col >> 8) & 0xFFFFFF))
        end
    end
    ShowDelayedTooltip(ctx, cell.name or "", label)
    -- Mouse-wheel for rotary widgets
    HandleRotaryMouseWheel(ctx, cell)
end

-- ================================================================
-- Main render
-- ================================================================
local FLT_MIN, FLT_MAX = imgui.NumericLimits_Float()

local function RenderSurface(ctx, surfName)
    local layout = layouts[surfName]
    if not layout then
        imgui.Text(ctx, "Waiting for layout data from " .. surfName .. "...")
        return
    end

    local drawList = imgui.GetWindowDrawList(ctx)
    local baseW = BUTTON_SIZE * ZOOM * BUTTON_ASPECT  -- base width (wider due to aspect ratio)
    local baseH = BUTTON_SIZE * ZOOM                   -- base height
    local padH  = BUTTON_PAD_H * ZOOM                 -- horizontal padding between buttons
    local padV  = BUTTON_PAD_V * ZOOM                 -- vertical padding between rows

    for rowIdx, row in ipairs(layout) do
        -- Compute max cell height for this row (for spacer/row-spacing)
        local rowMaxH = baseH
        for _, cell in ipairs(row) do
            if not cell.isSpacer then
                local cellH = baseH * (cell.height or 1.0)
                if cellH > rowMaxH then rowMaxH = cellH end
            end
        end

        for cellIdx, cell in ipairs(row) do
            if cellIdx > 1 then
                imgui.SameLine(ctx, 0, padH)
            end

            if cell.isSpacer then
                imgui.Dummy(ctx, baseW * (cell.width or 0.5), rowMaxH)
            else
                local bw = baseW * (cell.width or 1.0)  -- all buttons use same grid width
                local bh = baseH * (cell.height or 1.0)
                local shape = (cell.shape or "Rect"):lower()

                if shape == "round" then
                    -- Round shape ignores aspect for visual size, but occupies full grid cell
                    local visualW = baseH * (cell.width or 1.0)  -- aspect-free visual width
                    DrawRoundButton(ctx, drawList, surfName, cell, bw, bh, visualW)
                elseif shape == "leftarrow" then
                    DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "left")
                elseif shape == "rightarrow" then
                    DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "right")
                elseif shape == "uparrow" then
                    DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "up")
                elseif shape == "downarrow" then
                    DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, "down")
                else
                    DrawRectButton(ctx, drawList, surfName, cell, bw, bh)
                end
            end
        end

        -- Spacing between rows
        if rowIdx < #layout then
            imgui.Dummy(ctx, 0, padV)
        end
    end
end

local function RenderContextMenu(ctx)
    if imgui.BeginPopupContextWindow(ctx, "OSK_ContextMenu") then
        -- Extra toolbar action buttons at the top
        if #TOOLBAR_ACTIONS > 0 then
            for _, act in ipairs(TOOLBAR_ACTIONS) do
                if imgui.MenuItem(ctx, act.title) then
                    r.Main_OnCommand(act.action, 0)
                    r.ShowConsoleMsg("[CSI OSK] Action: " .. act.action .. " (" .. (act.tooltip or act.title) .. ")\n")
                end
                if act.tooltip and imgui.IsItemHovered(ctx) then
                    if imgui.BeginTooltip(ctx) then
                        imgui.Text(ctx, act.tooltip)
                        imgui.EndTooltip(ctx)
                    end
                end
            end
            imgui.Separator(ctx)
        end

        -- Surface tabs (if multiple surfaces)
        if #surfaces > 1 then
            for i, name in ipairs(surfaces) do
                local isSelected = (i == currentSurface)
                if imgui.MenuItem(ctx, name, nil, isSelected) then
                    currentSurface = i
                end
            end
            imgui.Separator(ctx)
        end

        -- Settings
        local rv

        rv, ZOOM = imgui.SliderDouble(ctx, "Zoom", ZOOM, 0.5, 3.0, "%.1f")
        if rv then
            vars.zoom = ZOOM
            SaveSettings()
        end

        rv, BUTTON_ASPECT = imgui.SliderDouble(ctx, "Aspect (W/H)", BUTTON_ASPECT, 0.5, 2.0, "%.2f")
        if rv then
            vars.aspect = BUTTON_ASPECT
            SaveSettings()
        end

        rv, BUTTON_PAD_H = imgui.SliderDouble(ctx, "H Padding", BUTTON_PAD_H, 0, 20, "%.0f")
        if rv then
            vars.pad_h = BUTTON_PAD_H
            SaveSettings()
        end

        rv, BUTTON_PAD_V = imgui.SliderDouble(ctx, "V Padding", BUTTON_PAD_V, 0, 20, "%.0f")
        if rv then
            vars.pad_v = BUTTON_PAD_V
            SaveSettings()
        end

        rv, ARROW_ANGLE = imgui.SliderDouble(ctx, "Arrow Angle", ARROW_ANGLE, 60, 150, "%.0f")
        if rv then
            vars.arrow_angle = ARROW_ANGLE
            SaveSettings()
        end

        rv, vars.transparency = imgui.SliderDouble(ctx, "Window Alpha", vars.transparency, 0.2, 1.0, "%.2f")
        if rv then SaveSettings() end

        rv, vars.btn_transparency = imgui.SliderDouble(ctx, "Button Alpha", vars.btn_transparency, 0.2, 1.0, "%.2f")
        if rv then SaveSettings() end

        rv, TOOLTIP_DELAY = imgui.SliderDouble(ctx, "Tooltip Delay", TOOLTIP_DELAY, 0.0, 5.0, "%.1fs")
        if rv then
            vars.tooltip_delay = TOOLTIP_DELAY
            SaveSettings()
        end

        rv, vars.clickable = imgui.Checkbox(ctx, "Clickable buttons", vars.clickable)
        if rv then SaveSettings() end

        imgui.Separator(ctx)
        imgui.Text(ctx, "Label Replacements (word=replacement):")
        local changed
        changed, vars.label_replacements = imgui.InputText(ctx, "##replacements", vars.label_replacements)
        if changed then
            parseLabelReplacements(vars.label_replacements)
            SaveSettings()
        end

        imgui.EndPopup(ctx)
    end
end

local function main()
    -- Poll data
    if not PollData() then
        return -- Close requested
    end

    if #surfaces == 0 then
        r.defer(main)
        return
    end

    imgui.PushStyleColor(ctx, imgui.Col_WindowBg, COLORS.win_bg)
    imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, COLORS.win_bg)

    -- Apply window transparency
    imgui.SetNextWindowBgAlpha(ctx, vars.transparency)

    local visible, p_open = imgui.Begin(ctx, 'CSI On-Screen Keyboard', true,
        imgui.WindowFlags_NoScrollbar
        | imgui.WindowFlags_NoCollapse
        | imgui.WindowFlags_AlwaysAutoResize
    )
    imgui.PopStyleColor(ctx, 2)

    if visible then
        imgui.PushFont(ctx, FONT)

        -- Right-click context menu (replaces top bar)
        RenderContextMenu(ctx)

        -- Render current surface directly (no BeginChild — allows AlwaysAutoResize to work)
        local surfName = surfaces[currentSurface]
        if surfName then
            RenderSurface(ctx, surfName)
        end

        -- OSD status bar (shows CSI_TMP/OSD messages; hidden when empty)
        RenderOSDBar(ctx)

        imgui.PopFont(ctx)
        imgui.End(ctx)
    end

    if p_open then
        r.defer(main)
    else
        -- Cleanup
        SaveSettings()
        SetToolbarButtonState(-1)
    end
end

-- ================================================================
-- Init
-- ================================================================
local function Init()
    SetToolbarButtonState(1)
    LoadSettings()

    ctx = imgui.CreateContext('CSI OSK')
    FONT = imgui.CreateFont('sans-serif', 13)
    FONT_SMALL = imgui.CreateFont('sans-serif', 11)
    imgui.Attach(ctx, FONT)
    imgui.Attach(ctx, FONT_SMALL)

    ZOOM = vars.zoom
    BUTTON_ASPECT = vars.aspect
    BUTTON_PAD_H = vars.pad_h
    BUTTON_PAD_V = vars.pad_v
    TOOLTIP_DELAY = vars.tooltip_delay
    ARROW_ANGLE = vars.arrow_angle

    -- Initial poll
    PollData()

    main()
    r.atexit(function()
        SaveSettings()
        SetToolbarButtonState(-1)
    end)
end

Init()
