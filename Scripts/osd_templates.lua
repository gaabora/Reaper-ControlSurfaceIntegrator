local osdTemplates = {}

local function getTrackName(context, track)
    if not track then return "" end
    local trackNameAvailable, trackName = context.reaper.GetTrackName(track)
    return trackNameAvailable and trackName or ""
end

local function getAdjacentTrack(context, direction)
    if not context.currentTrack then return nil end
    if context.currentTrackNumber == -1 then return direction > 0 and context.reaper.GetTrack(0, 0) or nil end
    if direction < 0 then
        if context.currentTrackNumber <= 1 then return nil end
        return context.reaper.GetTrack(0, context.currentTrackNumber - 2)
    end
    if context.currentTrackNumber >= context.trackCount then return nil end
    return context.reaper.GetTrack(0, context.currentTrackNumber)
end

local function formatMinSec(positionSeconds)
    local isNegative = positionSeconds < 0
    local totalMilliseconds = math.floor(math.abs(positionSeconds) * 1000 + 0.5)
    local totalMinutes = math.floor(totalMilliseconds / 60000)
    local seconds = math.floor(totalMilliseconds / 1000) % 60
    local milliseconds = totalMilliseconds % 1000
    return string.format("%s%d:%02d.%03d", isNegative and totalMilliseconds > 0 and "-" or "", totalMinutes, seconds, milliseconds)
end

local function formatBarBeat(context)
    local formattedPosition = context.reaper.format_timestr_pos(context.cursorPosition, "", 2)
    local measure, beat = tostring(formattedPosition or ""):match("^%s*(-?%d+)[%.:](%d+)")
    if measure and beat then return measure .. "/" .. beat end
    local beatsInMeasure, measureIndex = context.reaper.TimeMap2_timeToBeats(0, context.cursorPosition)
    return tostring((measureIndex or 0) + 1) .. "/" .. tostring(math.floor((beatsInMeasure or 0) + 0.000000000001) + 1)
end

local DEFINITIONS = {
    {
        name = "currTrackName",
        description = "First selected track name",
        resolve = function(context) return getTrackName(context, context.currentTrack) end,
    },
    {
        name = "prevTrackName",
        description = "Track name before the first selected track",
        resolve = function(context) return getTrackName(context, getAdjacentTrack(context, -1)) end,
    },
    {
        name = "nextTrackName",
        description = "Track name after the first selected track",
        resolve = function(context) return getTrackName(context, getAdjacentTrack(context, 1)) end,
    },
    {
        name = "currMinSec",
        description = "Edit cursor position as M:SS.mmm",
        resolve = function(context) return formatMinSec(context.cursorPosition) end,
    },
    {
        name = "currBarBeat",
        description = "Edit cursor position as bar/beat",
        resolve = formatBarBeat,
    },
}

local definitionsByName = {}
for definitionIdx, definition in ipairs(DEFINITIONS) do definitionsByName[definition.name] = definition end

local function createContext(reaperApi)
    local currentTrack = reaperApi.GetSelectedTrack2(0, 0, true)
    return {
        reaper = reaperApi,
        currentTrack = currentTrack,
        currentTrackNumber = currentTrack and math.floor(reaperApi.GetMediaTrackInfo_Value(currentTrack, "IP_TRACKNUMBER")) or 0,
        trackCount = reaperApi.CountTracks(0),
        cursorPosition = reaperApi.GetCursorPosition(),
    }
end

function osdTemplates.GetDefinitions()
    local result = {}
    for definitionIdx, definition in ipairs(DEFINITIONS) do result[#result + 1] = { name = definition.name, description = definition.description } end
    return result
end

function osdTemplates.GetVariableTokens()
    local result = {}
    for definitionIdx, definition in ipairs(DEFINITIONS) do result[#result + 1] = "{" .. definition.name .. "}" end
    return result
end

function osdTemplates.FindUnknownVariables(text)
    local unknown = {}
    local seen = {}
    for variableName in tostring(text or ""):gmatch("{([%a_][%w_]*)}") do
        if not definitionsByName[variableName] and not seen[variableName] then
            unknown[#unknown + 1] = "{" .. variableName .. "}"
            seen[variableName] = true
        end
    end
    table.sort(unknown)
    return unknown
end

function osdTemplates.Expand(text, reaperApi)
    local source = tostring(text or "")
    local output = {}
    local context = nil
    local resolvedValues = {}
    local resolvedNames = {}
    local position = 1
    while position <= #source do
        if source:sub(position, position + 2) == "\\\\n" then
            output[#output + 1] = "\n"
            position = position + 3
        elseif source:sub(position, position + 1) == "\\n" then
            output[#output + 1] = "\n"
            position = position + 2
        elseif source:sub(position, position) == "{" then
            local closingBrace = source:find("}", position + 1, true)
            if not closingBrace then
                output[#output + 1] = source:sub(position)
                break
            end
            local variableName = source:sub(position + 1, closingBrace - 1)
            local definition = definitionsByName[variableName]
            if definition then
                if not resolvedNames[variableName] then
                    context = context or createContext(reaperApi or reaper)
                    local success, value = pcall(definition.resolve, context)
                    resolvedValues[variableName] = success and tostring(value or "") or "{" .. variableName .. "}"
                    resolvedNames[variableName] = true
                end
                output[#output + 1] = resolvedValues[variableName]
            else
                output[#output + 1] = source:sub(position, closingBrace)
            end
            position = closingBrace + 1
        else
            output[#output + 1] = source:sub(position, position)
            position = position + 1
        end
    end
    return table.concat(output)
end

return osdTemplates
