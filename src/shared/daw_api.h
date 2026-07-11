#pragma once
// daw_api.h central aggregate header for the DAW namespace.
// Include this file to get the full DAW:: API surface; do not include the sub-headers directly.
// Sub-headers (in dependency order):
//    daw_transport.h - constants (AUTOMODE_*, PANMODE_*, QUERY_*), SendCommandMessage, ValidateTrackPtr, Undo/Redo, GetCommandName
//    daw_utils.h     - CompareFaderValues, RoundDouble
//    daw_display.h   - ShowOSD (defined in daw_display.cpp)
//    daw_fx.h        - GetShortFXName, GetFx*/SetTrackFx*, IsFxInstrument
//    daw_tracks.h    - GetTrack, GetTrackColor, GetTrackName, Solo/Mute/Arm, Bypass, CycleTrackAutoMode, CheckTouchedOrFocusedFX, GetLastTouchedFXParamDescription

#include "utils.h"
#include "daw_transport.h"
#include "daw_utils.h"
#include "daw_display.h"
#include "daw_fx.h"
#include "daw_tracks.h"
