local scriptDirectory = debug.getinfo(1, "S").source:match("@(.+[\\/])") or ""
local identityPath = scriptDirectory .. "product_identity.conf"
local identityFile, openError = io.open(identityPath, "r")
if not identityFile then error("Cannot open product identity file " .. identityPath .. ": " .. tostring(openError)) end

local values = {}
for line in identityFile:lines() do
    local trimmedLine = line:match("^%s*(.-)%s*$")
    if trimmedLine ~= "" and not trimmedLine:match("^#") then
        local key, value = trimmedLine:match("^([A-Z0-9_]+)=(.*)$")
        if not key then
            identityFile:close()
            error("Invalid product identity line: " .. trimmedLine)
        end
        if values[key] ~= nil then
            identityFile:close()
            error("Duplicate product identity key: " .. key)
        end
        values[key] = value
    end
end
identityFile:close()

local function RequireValue(key)
    local value = values[key]
    if not value or value == "" then error("Missing product identity value: " .. key) end
    return value
end

local extStatePrefix = RequireValue("PRODUCT_EXTSTATE_PREFIX")

return {
    displayName = RequireValue("PRODUCT_DISPLAY_NAME"),
    productId = RequireValue("PRODUCT_ID"),
    resourceDirectory = RequireValue("PRODUCT_RESOURCE_DIRECTORY"),
    configFilename = RequireValue("PRODUCT_CONFIG_FILENAME"),
    logFilename = RequireValue("PRODUCT_LOG_FILENAME"),
    extStatePrefix = extStatePrefix,
    reaperRegistrationId = RequireValue("PRODUCT_REAPER_REGISTRATION_ID"),
    pluginFilename = RequireValue("PRODUCT_PLUGIN_FILENAME"),
    scriptDirectory = RequireValue("PRODUCT_SCRIPT_DIRECTORY"),
    packagePrefix = RequireValue("PRODUCT_PACKAGE_PREFIX"),
    extState = {
        osk = extStatePrefix .. "_OSK",
        oskCommand = extStatePrefix .. "_OSK_CMD",
        oskSettings = extStatePrefix .. "_OSK_SETTINGS",
        osd = extStatePrefix .. "_OSD",
        osdSettings = extStatePrefix .. "_OSD_SETTINGS",
    },
}
