#include "control_panel_action.h"

#include "product_identity.h"
#include "../shared/reaper_plugin_functions.h"
#include "../shared/reascript_action.h"
#include "../shared/utils.h"

#include <string>

namespace ControlPanelAction {
static int commandId = 0;
static int scriptCommandId = 0;
static unsigned long requestCounter = 0;
static std::string actionName;
static custom_action_register_t action = { 0, ProductIdentity::ControlPanelActionId, nullptr, nullptr };

static bool IsOpen() {
    if (std::string(::GetExtState(ProductIdentity::ExtStateControlPanel, "State")) != "Open") return false;
    return scriptCommandId == 0 || ::GetToggleCommandState(scriptCommandId) == 1;
}

static void RefreshToolbar() {
    if (commandId > 0) ::RefreshToolbar2(0, commandId);
}

static void PublishRequest(const char* command, const char* tabName) {
    ++requestCounter;
    std::string payload = "Version=1\nRequestId=" + std::to_string(requestCounter) + "\nCommand=" + command + "\n";
    if (tabName && tabName[0] != '\0') payload += "Tab=" + std::string(tabName) + "\n";
    ::SetExtState(ProductIdentity::ExtStateControlPanel, "Request", payload.c_str(), false);
}

static int ToggleActionCallback(int requestedCommandId) {
    if (requestedCommandId != commandId) return -1;
    return IsOpen() ? 1 : 0;
}

static bool OnAction(KbdSectionInfo* section, int requestedCommandId, int value, int valueHardware, int relativeMode, HWND window) {
    (void) section;
    (void) value;
    (void) valueHardware;
    (void) relativeMode;
    (void) window;
    if (requestedCommandId != commandId) return false;
    Toggle();
    return true;
}

void Toggle() {
    if (IsOpen()) {
        PublishRequest("Close", nullptr);
        RefreshToolbar();
        return;
    }
    OpenOrFocus();
}

void OpenOrFocus(const char* tabName) {
    if (scriptCommandId == 0) scriptCommandId = ReaScriptAction::ResolveCommandId(ProductIdentity::ControlPanelScriptResourcePath, "OpenControlPanel");
    if (scriptCommandId == 0) return;

    const bool scriptIsRunning = ::GetToggleCommandState(scriptCommandId) == 1;
    if (scriptIsRunning) {
        PublishRequest(tabName && tabName[0] != '\0' ? "SelectTab" : "Focus", tabName);
    } else {
        ::SetExtState(ProductIdentity::ExtStateControlPanel, "State", "Closed", false);
        PublishRequest(tabName && tabName[0] != '\0' ? "SelectTab" : "Open", tabName);
        ::Main_OnCommand(scriptCommandId, 0);
    }
    RefreshToolbar();
}

bool Register() {
    if (commandId != 0) return true;
    actionName = std::string(ProductIdentity::DisplayName) + ": Open Control Panel";
    action.name = actionName.c_str();
    commandId = plugin_register("custom_action", &action);
    if (commandId == 0) {
        LogToConsole("[ERROR] Failed to register the %s Control Panel action\n", ProductIdentity::DisplayName);
        return false;
    }
    if (plugin_register("toggleaction", reinterpret_cast<void*>(ToggleActionCallback)) == 0 || plugin_register("hookcommand2", reinterpret_cast<void*>(OnAction)) == 0) {
        LogToConsole("[ERROR] Failed to register callbacks for the %s Control Panel action\n", ProductIdentity::DisplayName);
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
