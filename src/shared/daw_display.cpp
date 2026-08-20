// daw_display.cpp - Implementation of DAW::ShowOSD.
// The event id is defined here so it is a true process-wide singleton.

#include "daw_display.h"
#include "reaper_plugin_functions.h"

#include <string>
using std::string;

namespace DAW {
    void ShowOSD(const osd_data& osdData) { //FIXME: why this tiny thing have 2 dedicated files (.h + .cpp) and OSK related stuff is not here but together with many other stuff? maybe move OSK here?
        static unsigned int eventId = 0;
        const string value = osdData.toString();
        const string eventIdValue = std::to_string(++eventId);
        ::SetExtState(ProductIdentity::ExtStateOsd, "OSD", value.c_str(), false);
        ::SetExtState(ProductIdentity::ExtStateOsd, "OSD_ID", eventIdValue.c_str(), false);
    }
} // namespace DAW
