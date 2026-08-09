local M = {}

M.FADER_VALUE_EPSILON = 0.0005

function M.GetInteractionStateKey(surfaceName, widgetName)
    if not surfaceName or surfaceName == "" or not widgetName or widgetName == "" then return nil end
    return surfaceName .. "|" .. widgetName
end

function M.ClampNormalized(value)
    return math.max(0.0, math.min(1.0, tonumber(value) or 0.0))
end

function M.DbToNormalized(dbValue)
    dbValue = tonumber(dbValue) or 0.0
    if reaper.DB2SLIDER then return M.ClampNormalized(reaper.DB2SLIDER(dbValue) / 1000.0) end
    if dbValue <= -144.0 then return 0.0 end
    return M.ClampNormalized((dbValue + 144.0) / 168.0)
end

function M.NormalizedToDb(value)
    value = M.ClampNormalized(value)
    if reaper.SLIDER2DB then return reaper.SLIDER2DB(value * 1000.0) end
    if value <= 0.0 then return -144.0 end
    return value * 168.0 - 144.0
end

return M
