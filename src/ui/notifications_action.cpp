#include "notifications_action.h"

#include "product_identity.h"
#include "../shared/reaper_plugin_functions.h"
#include "../shared/reascript_action.h"
#include "../shared/product_log.h"
#include "../shared/utils.h"

#include <filesystem>
#include <string>

namespace NotificationsAction {
static int commandId = 0;
static int scriptCommandId = 0;
static std::string actionName;
static custom_action_register_t action = { 0, ProductIdentity::NotificationsActionId, nullptr, nullptr };

static bool IsRunning() {
    const bool lifecycleOpen = std::string(::GetExtState(ProductIdentity::ExtStateNotifications, "State")) == "Open";
    if (scriptCommandId != 0) return lifecycleOpen && ::GetToggleCommandState(scriptCommandId) == 1;
    return lifecycleOpen;
}

static void RefreshToolbar() {
    if (commandId > 0) ::RefreshToolbar2(0, commandId);
}

static int ToggleActionCallback(int requestedCommandId) {
    if (requestedCommandId != commandId) return -1;
    return IsRunning() ? 1 : 0;
}

static bool OnAction(KbdSectionInfo* section, int requestedCommandId, int value, int valueHardware, int relativeMode, HWND window) {
    (void) section;
    (void) value;
    (void) valueHardware;
    (void) relativeMode;
    (void) window;
    if (requestedCommandId != commandId) return false;
    if (scriptCommandId == 0) scriptCommandId = ReaScriptAction::ResolveCommandId(ProductIdentity::NotificationsScriptResourcePath, "ToggleNotifications");
    const bool scriptIsRunning = scriptCommandId != 0 && ::GetToggleCommandState(scriptCommandId) == 1;
    const std::string lifecycleState = ::GetExtState(ProductIdentity::ExtStateNotifications, "State");
    if (lifecycleState == "Stopping") {
        if (scriptIsRunning) return true;
        ::SetExtState(ProductIdentity::ExtStateNotifications, "State", "Closed", false);
    } else if (lifecycleState == "Open" && !scriptIsRunning) {
        ::SetExtState(ProductIdentity::ExtStateNotifications, "State", "Closed", false);
    }
    if (IsRunning()) {
        ::SetExtState(ProductIdentity::ExtStateNotifications, "Enabled", "0", false);
        ::SetExtState(ProductIdentity::ExtStateNotifications, "Command", "Stop", false);
        ::SetExtState(ProductIdentity::ExtStateNotifications, "State", "Stopping", false);
        RefreshToolbar();
        return true;
    }

    if (scriptCommandId == 0) return true;
    const std::filesystem::path logPath = ProductLog::ActiveFile();
    const auto startOffset = std::filesystem::is_regular_file(logPath) ? std::filesystem::file_size(logPath) : 0;
    const std::string startOffsetValue = std::to_string(startOffset);
    ::SetExtState(ProductIdentity::ExtStateNotifications, "StartOffset", startOffsetValue.c_str(), false);
    ::SetExtState(ProductIdentity::ExtStateNotifications, "Enabled", "1", false);
    ::DeleteExtState(ProductIdentity::ExtStateNotifications, "Command", false);
    ::SetExtState(ProductIdentity::ExtStateNotifications, "State", "Closed", false);
    ::Main_OnCommand(scriptCommandId, 0);
    RefreshToolbar();
    return true;
}

bool Register() {
    if (commandId != 0) return true;
    actionName = std::string(ProductIdentity::DisplayName) + ": Toggle Notifications";
    action.name = actionName.c_str();
    commandId = plugin_register("custom_action", &action);
    if (commandId == 0) {
        LogToConsole("[ERROR] Failed to register the %s Notifications action\n", ProductIdentity::DisplayName);
        return false;
    }
    if (plugin_register("toggleaction", reinterpret_cast<void*>(ToggleActionCallback)) == 0 || plugin_register("hookcommand2", reinterpret_cast<void*>(OnAction)) == 0) {
        LogToConsole("[ERROR] Failed to register callbacks for the %s Notifications action\n", ProductIdentity::DisplayName);
        Unregister();
        return false;
    }
    return true;
}

void Unregister() {
    if (commandId == 0) return;
    plugin_register("-hookcommand2", reinterpret_cast<void*>(OnAction));
    plugin_register("-toggleaction", reinterpret_cast<void*>(ToggleActionCallback));
    plugin_register("-custom_action", &action);
    commandId = 0;
    scriptCommandId = 0;
}
}
