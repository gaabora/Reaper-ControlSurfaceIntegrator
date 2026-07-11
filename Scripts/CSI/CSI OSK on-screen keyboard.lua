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

local processedLabelCache = {}

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
    show_all_surfaces = true,-- render all detected surfaces in one OSK window
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

local function getProcessedLabel(text)
    if not text or text == "" then return "" end
    local cached = processedLabelCache[text]
    if cached then return cached end
    cached = processLabel(text)
    processedLabelCache[text] = cached
    return cached
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
local function RenderOSDBar(ctx)
    PollOSD()

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
local function ShowDelayedTooltip(ctx, surfName, widgetName, text)
    if imgui.IsItemHovered(ctx) then
        local now = os.clock()
        if not hoverStartTime[widgetName] then
            hoverStartTime[widgetName] = now
        end
        if now - hoverStartTime[widgetName] >= TOOLTIP_DELAY then
            -- Build tooltip from LabelMap if available
            local modMap = surfName and labelMaps[surfName] and labelMaps[surfName][widgetName]
            local tooltipText = text
            if modMap and next(modMap) then
                local lines = {}
                -- NoMod first (primary binding)
                if modMap["NoMod"] then
                    lines[#lines + 1] = getProcessedLabel(modMap["NoMod"])
                end
                -- All other modifiers sorted alphabetically
                local sortedMods = {}
                for k in pairs(modMap) do
                    if k ~= "NoMod" then sortedMods[#sortedMods + 1] = k end
                end
                table.sort(sortedMods)
                for _, modName in ipairs(sortedMods) do
                    lines[#lines + 1] = modName .. " -> " .. getProcessedLabel(modMap[modName])
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

local function PollExtStateEntry(surfName, suffix, rawStore, parsedStore, parser)
    local key = suffix .. "_" .. surfName
    local raw = r.GetExtState(EXT_SECTION, key)
    if raw and raw ~= rawStore[surfName] then
        rawStore[surfName] = raw
        parsedStore[surfName] = parser(raw)
    end
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

local function ParseKeyValueList(str, entryParser)
    local result = {}
    if not str or str == "" then return result end
    for entry in str:gmatch("[^;]+") do
        local key, value = entry:match("^(.-)=(.*)$")
        if key then
            entryParser(result, key, value)
        end
    end
    return result
end

local function ParseState(stateStr)
    return ParseKeyValueList(stateStr, function(result, name, rest)
        local value = tonumber(rest:match("V:([%d%.%-]+)")) or 0
        local colorHex = rest:match("C:(#%x+)") or "#333333"
        result[name] = {
            value = value,
            color = hexToImCol(colorHex),
        }
    end)
end

local function ParseLabels(labelsStr)
    return ParseKeyValueList(labelsStr, function(result, name, label)
        result[name] = label
    end)
end

--- Parse LabelMap string from C++: "Touch=NoMod:Touch|Shift:Latch|Hold:Trim;..."
--- Returns widgetName -> {modifierName -> rawLabel} table.
local function ParseLabelMap(str)
    return ParseKeyValueList(str, function(result, name, modPairs)
        if not modPairs or modPairs == "" then return end
        local mods = {}
        for modEntry in modPairs:gmatch("[^|]+") do
            local modName, label = modEntry:match("^([^:]+):(.+)$")
            if modName and label then
                mods[modName] = label
            end
        end
        result[name] = mods
    end)
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
-- Poll data from C++ via ExtState and update internal state tables.
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
        PollExtStateEntry(surfName, "Layout", rawLayouts, layouts, function(raw)
            if raw and raw ~= "" then return ParseLayout(raw) end
            return layouts[surfName]
        end)
        PollExtStateEntry(surfName, "State", rawStates, states, ParseState)
        PollExtStateEntry(surfName, "Labels", rawLabels, labels, ParseLabels)
        PollExtStateEntry(surfName, "LabelMap", rawLabelMaps, labelMaps, ParseLabelMap)
    end

    return true -- keep running
end

-- ================================================================
-- Interaction helpers
-- ================================================================

local function GetWheelDirection(ctx)
    local v = imgui.GetMouseWheel(ctx)
    if v > 0 then return 1 end
    if v < 0 then return -1 end
    return 0
end

local function HandleButtonClick(surfName, cell)
    if not vars.clickable or not cell.name then return end
    if not surfName then return end
    local msg = surfName .. "|" .. cell.name
    r.SetExtState(EXT_CMD_SECTION, "WidgetPress", msg, false)
end

local function HandleRotaryMouseWheel(ctx, surfName, cell)
    if not cell or not cell.name then return end

    local name = tostring(cell.name):lower()
    local group = tostring(cell.group or ""):lower()
    local isRotary = name:find("rotary") or group:find("rotary")
    if not isRotary then return end

    if not imgui.IsItemHovered(ctx) then return end
    if not vars.clickable then return end

    local dir = GetWheelDirection(ctx)
    if dir == 0 then return end

    if not surfName then return end

    local action = (dir > 0) and "Inc" or "Dec"
    local msg = surfName .. "|" .. cell.name .. "|" .. action
    r.SetExtState(EXT_CMD_SECTION, "WidgetScroll", msg, false)
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
local function GetCellInfo(surfName, widgetName)
    local layout = layouts[surfName]
    if not layout then return nil end
    for _, row in ipairs(layout) do
        for _, cell in ipairs(row) do
            if not cell.isSpacer and cell.name == widgetName then return cell end
        end
    end
    return nil
end

local function GetButtonColor(surfName, widgetName)
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


local function DrawButtonInteraction(ctx, surfName, cell, bw, bh, label)
    local id = "##btn_" .. (cell.name or "")
    imgui.InvisibleButton(ctx, id, bw, bh)
    if imgui.IsItemHovered(ctx) then
        ShowDelayedTooltip(ctx, surfName, cell.name or "", label)
        HandleRotaryMouseWheel(ctx, surfName, cell)
    end
    if imgui.IsItemClicked(ctx) then HandleButtonClick(surfName, cell) end
end

local function RenderCenteredWrappedText(ctx, drawList, text, centerX, centerY, maxW, maxH)
    imgui.PushFont(ctx, FONT_SMALL)
    local lines = wrapText(ctx, text, maxW)
    local _, lineH = imgui.CalcTextSize(ctx, "M")
    local totalH = #lines * lineH
    local startY = centerY - totalH / 2
    local textCol = applyAlpha(COLORS.text_normal, vars.btn_transparency)
    for li, ln in ipairs(lines) do
        local tw = imgui.CalcTextSize(ctx, ln)
        local tx = centerX - tw / 2
        local ty = startY + (li - 1) * lineH
        if maxH and ty + lineH > centerY + maxH / 2 then break end
        imgui.DrawList_AddText(drawList, tx, ty, textCol, ln)
    end
    imgui.PopFont(ctx)
end


local function DrawRectButton(ctx, drawList, surfName, cell, bw, bh)
    local label = getProcessedLabel(GetButtonLabel(surfName, cell))
    local bgCol = applyAlpha(GetButtonColor(surfName, cell.name), vars.btn_transparency)
    local cx, cy = imgui.GetCursorScreenPos(ctx)

    imgui.DrawList_AddRectFilled(drawList, cx, cy, cx + bw, cy + bh, bgCol, 4)
    RenderCenteredWrappedText(ctx, drawList, label, cx + bw / 2, cy + bh / 2, bw - 8, bh)
    DrawButtonInteraction(ctx, surfName, cell, bw, bh, label)
end

--- Draw a stadium ("discorectangle") or circle shape for round buttons.
--- Round buttons ignore the aspect ratio for their visual shape but occupy the
--- full grid cell (bw x bh) for layout alignment. The circle/stadium is drawn
--- centered within the cell using the aspect-free dimensions (visualW x bh).
local function DrawRoundButton(ctx, drawList, surfName, cell, bw, bh, visualW)
    local label = getProcessedLabel(GetButtonLabel(surfName, cell))
    local bgCol = applyAlpha(GetButtonColor(surfName, cell.name), vars.btn_transparency)
    local cx, cy = imgui.GetCursorScreenPos(ctx)

    local offsetX = (bw - visualW) / 2
    local vx = cx + offsetX
    local pad2 = 2
    local centerX = vx + visualW / 2
    local centerY = cy + bh / 2
    local segments = 18

    local innerW = visualW - pad2 * 2
    local innerH = bh - pad2 * 2
    local isCircle = math.abs(innerW - innerH) < 2

    local function drawStadiumPath(inset)
        if isCircle then
            local radius = math.min(innerW, innerH) / 2 - inset
            for i = 0, segments * 2 - 1 do
                local angle = (i / (segments * 2)) * math.pi * 2
                imgui.DrawList_PathLineTo(drawList,
                    centerX + radius * math.cos(angle),
                    centerY + radius * math.sin(angle))
            end
        elseif innerW > innerH then
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

    drawStadiumPath(0)
    imgui.DrawList_PathFillConvex(drawList, bgCol)
    RenderCenteredWrappedText(ctx, drawList, label, centerX, centerY, visualW - 12, bh)
    DrawButtonInteraction(ctx, surfName, cell, bw, bh, label)
end

--- Draw an arrow-shaped button: rectangular body with a triangular "point" glued
--- to one side. The triangle apex angle is configurable via ARROW_ANGLE (degrees).
--- For obtuse angles (>90°), the triangle base extends beyond the body edges,
--- creating a wider, shallower point. Point depth = (dim/2) / tan(angle/2).
--- Supports left, right, up, down directions.
local function DrawArrowButton(ctx, drawList, surfName, cell, bw, bh, direction)
    local label = getProcessedLabel(GetButtonLabel(surfName, cell))
    local bgCol = applyAlpha(GetButtonColor(surfName, cell.name), vars.btn_transparency)
    local value = GetButtonValue(surfName, cell.name)

    if value > 0 then bgCol = applyAlpha(COLORS.arrow_on, vars.btn_transparency) end

    local cx, cy = imgui.GetCursorScreenPos(ctx)
    local halfAngleRad = math.rad(ARROW_ANGLE / 2)
    local pointDepth
    if direction == "left" or direction == "right" then
        pointDepth = (bh / 2) / math.tan(halfAngleRad)
    else
        pointDepth = (bw / 2) / math.tan(halfAngleRad)
    end
    if direction == "left" or direction == "right" then
        pointDepth = math.min(pointDepth, bw * 0.45)
    else
        pointDepth = math.min(pointDepth, bh * 0.45)
    end

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
    RenderCenteredWrappedText(ctx, drawList, label, labelCX, labelCY, bodyW - 8, bh)
    DrawButtonInteraction(ctx, surfName, cell, bw, bh, label)
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

local function RenderAllSurfaces(ctx)
    if #surfaces == 0 then return end
    for i, surfName in ipairs(surfaces) do
        imgui.PushFont(ctx, FONT_SMALL)
        imgui.Text(ctx, surfName)
        imgui.PopFont(ctx)
        RenderSurface(ctx, surfName)
        if i < #surfaces then
            imgui.Separator(ctx)
        end
    end
end

local function SliderSetting(ctx, label, currentValue, storeKey, min, max, fmt)
    local rv, value = imgui.SliderDouble(ctx, label, currentValue, min, max, fmt)
    if rv then
        vars[storeKey] = value
        SaveSettings()
    end
    return rv, value
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
            local toggled
            toggled, vars.show_all_surfaces = imgui.Checkbox(ctx, "Show all surfaces", vars.show_all_surfaces)
            if toggled then SaveSettings() end

            if not vars.show_all_surfaces then
                imgui.Separator(ctx)
                for i, name in ipairs(surfaces) do
                    local isSelected = (i == currentSurface)
                    if imgui.MenuItem(ctx, name, nil, isSelected) then
                        currentSurface = i
                    end
                end
            end
            imgui.Separator(ctx)
        end

        -- Settings
        local rv

        rv, ZOOM = SliderSetting(ctx, "Zoom", ZOOM, "zoom", 0.5, 3.0, "%.1f")
        rv, BUTTON_ASPECT = SliderSetting(ctx, "Aspect (W/H)", BUTTON_ASPECT, "aspect", 0.5, 2.0, "%.2f")
        rv, BUTTON_PAD_H = SliderSetting(ctx, "H Padding", BUTTON_PAD_H, "pad_h", 0, 20, "%.0f")
        rv, BUTTON_PAD_V = SliderSetting(ctx, "V Padding", BUTTON_PAD_V, "pad_v", 0, 20, "%.0f")
        rv, ARROW_ANGLE = SliderSetting(ctx, "Arrow Angle", ARROW_ANGLE, "arrow_angle", 60, 150, "%.0f")
        rv, vars.transparency = SliderSetting(ctx, "Window Alpha", vars.transparency, "transparency", 0.2, 1.0, "%.2f")
        rv, vars.btn_transparency = SliderSetting(ctx, "Button Alpha", vars.btn_transparency, "btn_transparency", 0.2, 1.0, "%.2f")
        rv, TOOLTIP_DELAY = SliderSetting(ctx, "Tooltip Delay", TOOLTIP_DELAY, "tooltip_delay", 0.0, 5.0, "%.1fs")

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
    if not PollData() then
        return -- Close requested
    end

    if #surfaces == 0 then
        r.defer(main)
        return
    end

    imgui.PushStyleColor(ctx, imgui.Col_WindowBg, COLORS.win_bg)
    imgui.PushStyleColor(ctx, imgui.Col_TitleBgActive, COLORS.win_bg)
    imgui.SetNextWindowBgAlpha(ctx, vars.transparency)

    local visible, p_open = imgui.Begin(ctx, 'CSI On-Screen Keyboard', true,
        imgui.WindowFlags_NoScrollbar
        | imgui.WindowFlags_NoCollapse
        | imgui.WindowFlags_AlwaysAutoResize
    )
    imgui.PopStyleColor(ctx, 2)

    if visible then
        imgui.PushFont(ctx, FONT)

        RenderContextMenu(ctx)

        -- Render surface(s) directly (no BeginChild — allows AlwaysAutoResize to work)
        if vars.show_all_surfaces and #surfaces > 1 then
            RenderAllSurfaces(ctx)
        else
            local surfName = surfaces[currentSurface]
            if surfName then RenderSurface(ctx, surfName) end
        end

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
