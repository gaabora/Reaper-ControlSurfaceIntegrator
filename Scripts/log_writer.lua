local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local identity = dofile(scriptDir .. "product_identity.lua")

local logWriter = {}

function logWriter.Write(level, message)
    local logPath = reaper.GetExtState(identity.extState.log, "File")
    if logPath == "" then return false end
    local logFile = io.open(logPath, "a")
    if not logFile then return false end
    logFile:write(os.date("[%H:%M:%S] "), "[", tostring(level or "INFO"), "] ", tostring(message or ""), "\n")
    logFile:close()
    return true
end

return logWriter
