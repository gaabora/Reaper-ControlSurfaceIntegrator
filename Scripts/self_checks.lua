local M = {}

local scriptDir = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
if scriptDir ~= "" then
    local pathEntry = scriptDir .. "?.lua"
    if not package.path:find(pathEntry, 1, true) then
        package.path = pathEntry .. ";" .. package.path
    end
end

local CHECK_MODULES = {
    "action_line",
    "settings_schema",
    "settings_protocol",
    "devices_model",
    "layout_parser",
    "label_replacements",
}

function M.RunAll()
    local results = {}
    local allOk = true

    for _, moduleName in ipairs(CHECK_MODULES) do
        local ok, err = pcall(function()
            local module = require(moduleName)
            if module.RunSelfChecks then module.RunSelfChecks() end
        end)
        results[#results + 1] = {
            module = moduleName,
            ok = ok,
            error = ok and "" or tostring(err),
        }
        allOk = allOk and ok
    end

    return allOk, results
end

function M.FormatResults(results)
    local lines = {}
    for _, result in ipairs(results or {}) do
        if result.ok then
            lines[#lines + 1] = "[OK] " .. result.module
        else
            lines[#lines + 1] = "[ERR] " .. result.module .. ": " .. result.error
        end
    end
    return table.concat(lines, "\n")
end

function M.RunAndReport()
    local ok, results = M.RunAll()
    local message = M.FormatResults(results) .. "\n"
    if reaper and reaper.ShowConsoleMsg then
        reaper.ShowConsoleMsg(message)
    else
        print(message)
    end
    return ok
end

return M
