local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local identity = dofile(scriptDir .. "product_identity.lua")

local logWriter = {}

function logWriter.Write(level, message)
    local record = os.date("[%H:%M:%S] ") .. "[" .. tostring(level or "INFO") .. "] " .. tostring(message or "") .. "\n"
    local written = false
    if reaper.GetExtState(identity.extState.log, "WriteFile") ~= "0" then
        local logPath = reaper.GetExtState(identity.extState.log, "File")
        if logPath ~= "" then
            local logFile = io.open(logPath, "a")
            if logFile then
                logFile:write(record)
                logFile:close()
                written = true
            end
        end
    end
    if reaper.GetExtState(identity.extState.log, "ShowConsole") == "1" and reaper.ShowConsoleMsg then
        reaper.ShowConsoleMsg(record)
        written = true
    end
    return written
end

return logWriter
