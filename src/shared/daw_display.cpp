// daw_display.cpp — Implementation of DAW::ShowOSD.
// static variables `lastValue` and `lastUpdateTs` are defined here so they are true process-wide singletons. 
// If ShowOSD were defined inline in a header, each translation unit that included the header would get its own copy of those statics, causing rate-limiting to fail.

#include "daw_display.h"
#include "reaper_plugin_functions.h"

#include <string>
using std::string;

namespace DAW {
    void ShowOSD(const osd_data& osdData) { //FIXME: why this tiny thing have 2 dedicated files (.h + .cpp) and OSK related stuff is not here but together with many other stuff? maybe move OSK here?
        static string lastValue;
        static DWORD lastUpdateTs = 0;
        static unsigned int eventId = 0;
        DWORD now = GetTickCount();
        const string nextValue = osdData.toString();
        if (lastValue == nextValue) {
            if (osdData.timeoutMs == -1) return;
            if (osdData.timeoutMs >= 0 && (now - lastUpdateTs) < (DWORD) osdData.timeoutMs) return;
        }
        lastValue = nextValue;
        lastUpdateTs = now;
        const string eventIdValue = std::to_string(++eventId);
        ::SetExtState(ProductIdentity::ExtStateOsd, "OSD", lastValue.c_str(), false);
        ::SetExtState(ProductIdentity::ExtStateOsd, "OSD_ID", eventIdValue.c_str(), false);
    }
} // namespace DAW
