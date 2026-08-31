#include "integrator.h"

void CSurfIntegrator::PollAndHandleLogCommands() {
    ProductLog::Refresh();
    if (!::HasExtState(ProductIdentity::ExtStateLogCommand, "Request")) return;
    const string command = ::GetExtState(ProductIdentity::ExtStateLogCommand, "Request");
    ::DeleteExtState(ProductIdentity::ExtStateLogCommand, "Request", false);
    if (command == "OpenFile") {
        if (!ProductLog::OpenActiveFile()) LogToConsole("[ERROR] Failed to open the active product log file\n");
    } else if (command == "OpenFolder") {
        if (!ProductLog::OpenActiveDirectory()) LogToConsole("[ERROR] Failed to open the product log folder\n");
    } else {
        LogToConsole("[WARNING] Unknown log command: %s\n", command.c_str());
    }
}
