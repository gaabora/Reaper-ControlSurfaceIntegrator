#pragma once
// daw_transport.h — DAW namespace: constants, transport commands, undo/redo.
// Included by daw_api.h — do not include directly.

#ifndef WDL_NO_DEFINE_MINMAX
  #define WDL_NO_DEFINE_MINMAX
#endif

#include "reaper_plugin_functions.h"

extern HWND g_hwnd;

namespace DAW {
    constexpr int AUTOMODE_TRIM = 0;
    constexpr int AUTOMODE_READ = 1;
    constexpr int AUTOMODE_TOUCH = 2;
    constexpr int AUTOMODE_WRITE = 3;
    constexpr int AUTOMODE_LATCH = 4;

    constexpr int PANMODE_CLASSIC = 0;
    constexpr int PANMODE_BALANCE = 3;
    constexpr int PANMODE_STEREO = 5;
    constexpr int PANMODE_DUAL = 6;

    // GetTouchedOrFocusedFX query constants
    constexpr int QUERY_LAST_TOUCHED_PARAMETER = 0;
    constexpr int QUERY_CURRENTLY_FOCUSED_FX = 1;

    // Transport / window messaging

    inline void SendCommandMessage(WPARAM wparam) { ::SendMessage(g_hwnd, WM_COMMAND, wparam, 0); }

    inline bool ValidateTrackPtr(MediaTrack* track) { return ::ValidatePtr(track, "MediaTrack*"); }

    inline bool CanUndo() { return ::Undo_CanUndo2(NULL) != nullptr; }
    inline bool CanRedo() { return ::Undo_CanRedo2(NULL) != nullptr; }
    inline void Undo() { if (CanUndo()) ::Undo_DoUndo2(NULL); }
    inline void Redo() { if (CanRedo()) ::Undo_DoRedo2(NULL); }

    inline const char* GetCommandName(int cmdID) {
        const char* actionName = ::kbd_getTextFromCmd(cmdID, ::SectionFromUniqueID(1));
        return actionName ? actionName : "NOT FOUND!";
    }

} // namespace DAW
