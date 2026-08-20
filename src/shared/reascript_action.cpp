#include "reascript_action.h"

#include "reaper_plugin_functions.h"
#include "utils.h"

#include <filesystem>
#include <string>

extern int g_debugLevel;

namespace ReaScriptAction {
static int FindRegisteredCommandId(const std::filesystem::path& scriptPath) {
    KbdSectionInfo* mainActionSection = ::SectionFromUniqueID(0);
    if (!mainActionSection) return 0;

    const std::string scriptFilename = scriptPath.filename().string();
    int matchingCommandId = 0;
    for (int actionIdx = 0;; ++actionIdx) {
        const char* actionIdentifier = NULL;
        const int commandId = ::kbd_enumerateActions(mainActionSection, actionIdx, &actionIdentifier);
        (void) actionIdentifier;
        if (commandId == 0) break;
        const char* actionText = ::kbd_getTextFromCmd(commandId, mainActionSection);
        if (!actionText) continue;
        const std::string actionName = actionText;
        if (actionName.size() < scriptFilename.size() || actionName.compare(actionName.size() - scriptFilename.size(), scriptFilename.size(), scriptFilename) != 0) continue;
        if (matchingCommandId != 0) {
            LogToConsole("[ERROR] More than one registered ReaScript action ends with '%s'; cannot choose a command ID safely\n", scriptFilename.c_str());
            return -1;
        }
        matchingCommandId = commandId;
    }
    return matchingCommandId;
}

int ResolveCommandId(const char* relativeScriptPath, const char* operationName) {
    std::string normalizedRelativePath = relativeScriptPath ? relativeScriptPath : "";
    while (!normalizedRelativePath.empty() && (normalizedRelativePath[0] == '/' || normalizedRelativePath[0] == '\\')) normalizedRelativePath.erase(0, 1);
    const std::filesystem::path scriptPath = std::filesystem::path(GetResourcePath()) / normalizedRelativePath;
    if (!std::filesystem::is_regular_file(scriptPath)) {
        LogToConsole("[ERROR] FAILED to %s. ReaScript file does not exist: '%s'\n", operationName, scriptPath.string().c_str());
        return 0;
    }

    const std::string scriptPathString = scriptPath.string();
    const int registeredCommandId = ::AddRemoveReaScript(true, 0, scriptPathString.c_str(), true);
    if (registeredCommandId != 0) {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] ReaScript registered: '%s', commandId=%d\n", scriptPathString.c_str(), registeredCommandId);
        return registeredCommandId;
    }

    const int existingCommandId = FindRegisteredCommandId(scriptPath);
    if (existingCommandId > 0) {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] Reusing registered ReaScript: '%s', commandId=%d\n", scriptPathString.c_str(), existingCommandId);
        return existingCommandId;
    }
    if (existingCommandId == 0) LogToConsole("[ERROR] FAILED to %s. AddRemoveReaScript failed and no registered action matches '%s'\n", operationName, scriptPathString.c_str());
    return 0;
}
}
