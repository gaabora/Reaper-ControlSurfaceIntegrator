#pragma once
//
//  preamble.h — Common includes required by all CSI class headers.
//  Each extracted class header includes this first, then adds class-specific includes.
//

#if __cplusplus < 201100
#if !defined(_MSC_VER) || _MSC_VER < 1400
#define override
#endif
#endif

#ifdef _WIN32
#if _MSC_VER <= 1400
#define _CRT_SECURE_NO_DEPRECATE
#endif
#if _MSC_VER >= 1800
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#endif

#include <fstream>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <functional>

using std::string;
using std::vector;
using std::map;
using std::unique_ptr;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;
using std::ifstream;
using std::ofstream;
using std::ostringstream;
using std::istringstream;
using std::function;
using std::min;
using std::max;
namespace filesystem = std::filesystem;

// oscpkt/udp must be included BEFORE windows.h (winsock2 must precede winsock.h).
// WDL's win32_utf8.h pulls in windows.h, so include these first.
#include "../shared/oscpkt.hh"
#include "../shared/udp.hh"

#ifdef USING_CMAKE
  #include "../../lib/WDL/WDL/win32_utf8.h"
  #include "../../lib/WDL/WDL/ptrlist.h"
  #include "../../lib/WDL/WDL/queue.h"
  #include "../../lib/WDL/WDL/mutex.h"
#else
  #ifdef _WIN32
  #ifndef strnicmp
    #define strnicmp _strnicmp
  #endif
  #endif
  #include "../../WDL/win32_utf8.h"
  #include "../../WDL/ptrlist.h"
  #include "../../WDL/queue.h"
  #include "../../WDL/mutex.h"
#endif

// KbdSectionInfo (used by localize-import.h) comes from the REAPER SDK.
// Include it here so it's available before localize-import.h is pulled in.
#include "../shared/reaper_plugin_functions.h"

// localize-import.h MUST come before localize.h — handled here to guarantee ordering.
#ifdef INCLUDE_LOCALIZE_IMPORT_H
  #define LOCALIZE_IMPORT_PREFIX "csi_"
  #ifdef USING_CMAKE
    #include "../../lib/WDL/WDL/localize/localize-import.h"
  #else
    #include "../../WDL/localize/localize-import.h"
  #endif
#endif

#ifdef USING_CMAKE
  #include "../../lib/WDL/WDL/localize/localize.h"
#else
  #include "../../WDL/localize/localize.h"
#endif

#ifdef _WIN32
#include "commctrl.h"
#endif

#define NUM_ELEM(array) (int(sizeof(array)/sizeof(array[0])))

#ifdef _WIN32
#define STRICASECMP _stricmp
#else
#define STRICASECMP strcasecmp
#endif

#include "../shared/daw_api.h"
#include "../shared/utils.h"
#include "../shared/types.h"

#include "fwd.h"

// External helpers declared in integrator.cpp / integrator_ui.cpp
extern void TrimLine(string& line);
extern void ReplaceAllWith(string& output, const char* replaceAny, const char* replacement);
extern int strToHex(string& valueStr);
extern void GetTokens(vector<string>& tokens, const string& line);
extern void GetTokens(vector<string>& tokens, const string& line, char delimiter);
extern void GetPropertiesFromTokens(int start, int finish, const vector<string>& tokens, PropertyList& properties);

extern void RequestFocusedFXDialog(ZoneManager* zoneManager);
extern void CloseFocusedFXDialog();
extern void UpdateLearnWindow(ZoneManager* zoneManager);
extern void InitBlankLearnFocusedFXZone(ZoneManager* zoneManager, Zone* fxZone, MediaTrack* track, int fxSlot);
extern void ShutdownLearn();

extern int g_debugLevel;
extern bool g_surfaceRawInDisplay;
extern bool g_surfaceInDisplay;
extern bool g_surfaceOutDisplay;
extern bool g_fxParamsWrite;
extern REAPER_PLUGIN_HINSTANCE g_hInst;

extern vector<HWND> g_openDialogs;
void CloseAllDialogs();
