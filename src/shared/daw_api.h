#pragma once
//
//  daw_api.h â€” Central aggregate header for the DAW namespace.
//
//  Phase 7 of REFACTORING_PLAN.md converted the monolithic DAW class into a
//  namespace split across focused sub-headers.  Include this file to get the
//  full DAW:: API surface; do not include the sub-headers directly.
//
//  Sub-headers (in dependency order):
//    daw_transport.h  â€” constants (AUTOMODE_*, PANMODE_*, QUERY_*),
//                       SendCommandMessage, ValidateTrackPtr, Undo/Redo,
//                       GetCommandName
//    daw_utils.h      â€” CompareFaderValues, RoundDouble
//    daw_display.h    â€” ShowOSD (defined in daw_display.cpp)
//    daw_fx.h         â€” GetShortFXName, GetFx*/SetTrackFx*, IsFxInstrument
//    daw_tracks.h     â€” GetTrack, GetTrackColor, GetTrackName, Solo/Mute/Arm,
//                       Bypass, CycleTrackAutoMode, CheckTouchedOrFocusedFX,
//                       GetLastTouchedFXParamDescription
//

#include "handy_functions.h"   // backward-compat: transitively pulls in utils.h
#include "daw_transport.h"
#include "daw_utils.h"
#include "daw_display.h"
#include "daw_fx.h"
#include "daw_tracks.h"

// Backward-compatibility macro: everything that previously included daw_api.h
// expecting "class DAW { static ... }" now finds "namespace DAW { inline ... }".
// The DAW:: call syntax is identical for both.
