#pragma once
//
//  daw_tracks.h — DAW namespace: track state access/mutation, auto-mode, VCA, solo,
//                  mute, arm, bypass, and FX-touched/focused queries.
//
//  Also contains CheckTouchedOrFocusedFX (needs DAW::GetTrack defined below) and
//  GetLastTouchedFXParamDescription (needs both GetTrackName and GetFxParamDescription).
//
//  Part of the Phase 7 decomposition of the DAW class.
//  Included by daw_api.h — do not include directly.
//

#include "daw_transport.h"
#include "daw_utils.h"
#include "daw_fx.h"
#include "utils.h"

#include <string>
using std::string;

namespace DAW
{
    // -----------------------------------------------------------------------
    // Track access
    // -----------------------------------------------------------------------

    // Get a MediaTrack* by 1-based track number (0 = master track).
    inline MediaTrack *GetTrack(int trackidx)
    {
        trackidx--;
        if (trackidx < 0) trackidx = 0;
        return ::GetTrack(NULL, trackidx);
    }

    inline rgba_color GetTrackColor(MediaTrack *track)
    {
        rgba_color color;
        if (DAW::ValidateTrackPtr(track))
            ::ColorFromNative(::GetTrackColor(track), &color.r, &color.g, &color.b);
        if (color.r == 0 && color.g == 0 && color.b == 0) {
            color.r = 64; color.g = 64; color.b = 64;
        }
        return color;
    }

    inline unsigned int GetTrackGroupMembership(MediaTrack *track, const char *groupname)
    {
        return DAW::ValidateTrackPtr(track)
            ? ::GetSetTrackGroupMembership(track, groupname, 0, 0) : 0;
    }

    inline unsigned int GetTrackGroupMembershipHigh(MediaTrack *track, const char *groupname)
    {
        return DAW::ValidateTrackPtr(track)
            ? ::GetSetTrackGroupMembershipHigh(track, groupname, 0, 0) : 0;
    }

    inline std::string GetTrackName(MediaTrack *track)
    {
        const char *tn = static_cast<const char*>(GetSetMediaTrackInfo(track, "P_NAME", nullptr));
        if (tn && *tn) return std::string(tn);
        int trackNum = static_cast<int>(
            reinterpret_cast<intptr_t>(GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", nullptr)));
        return (trackNum == -1) ? "Master" : "Track " + std::to_string(trackNum);
    }

    // -----------------------------------------------------------------------
    // FX focused/touched queries (placed here because they return MediaTrack*)
    // -----------------------------------------------------------------------

    inline bool CheckTouchedOrFocusedFX(MediaTrack **outTrack, int *fxSlotNum, int *fxParamNum)
    {
        if (!outTrack || !fxSlotNum || !fxParamNum) return false;

        int trackNum, trackIdx, itemIdx, itemTake, slotIdx, paramIdx;

        if (GetTouchedOrFocusedFX) {
            if (GetTouchedOrFocusedFX(QUERY_CURRENTLY_FOCUSED_FX,
                    &trackIdx, &itemIdx, &itemTake, &slotIdx, &paramIdx)) {
                if (paramIdx & 1) // open but no longer focused
                    *fxParamNum = -1;
            } else {
                if (!GetTouchedOrFocusedFX(QUERY_LAST_TOUCHED_PARAMETER,
                        &trackIdx, &itemIdx, &itemTake, &slotIdx, &paramIdx))
                    return false;
            }
            trackNum = trackIdx + 1;
        } else {
            int type = GetFocusedFX2(&trackNum, fxSlotNum, fxParamNum);
            if (!type || (type & 4)) // closed or open but no longer focused
                *fxParamNum = -1;
        }

        MediaTrack *track = nullptr;
        if (trackNum > 0)
            track = DAW::GetTrack(trackNum);
        else if (trackNum == 0)
            track = ::GetMasterTrack(nullptr);

        if (!track) return false;

        *outTrack  = track;
        *fxSlotNum = slotIdx;
        *fxParamNum = paramIdx;
        return true;
    }

    inline std::string GetLastTouchedFXParamDescription()
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum))
            return "No FX was touched";
        if (MediaTrack *track = DAW::GetTrack(trackNum))
            return DAW::GetFxParamDescription(track, fxSlotNum, fxParamNum)
                 + " (" + DAW::GetTrackName(track) + ")";
        return "FAILED to GetLastTouchedFXParamDescription";
    }

    // -----------------------------------------------------------------------
    // Track volume
    // -----------------------------------------------------------------------

    inline double GetTrackVolumeValue(MediaTrack *track)
    {
        double value = 0.0, pan = 0.0;
        GetTrackUIVolPan(track, &value, &pan);
        return volToNormalized(value);
    }

    inline void SetTrackVolumeValue(MediaTrack *track, double value)
    {
        CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }

    // -----------------------------------------------------------------------
    // Track automation
    // -----------------------------------------------------------------------

    inline void CycleTrackAutoMode(MediaTrack *track)
    {
        if (!track) return;
        static const int cycleModes[] = {
            AUTOMODE_TRIM, AUTOMODE_READ, AUTOMODE_TOUCH, AUTOMODE_LATCH
        };
        int currentMode = (int)GetMediaTrackInfo_Value(track, "I_AUTOMODE");
        int nextMode    = CycleNextValue(cycleModes, currentMode);
        GetSetMediaTrackInfo(track, "I_AUTOMODE", &nextMode);
    }

    // -----------------------------------------------------------------------
    // Solo
    // -----------------------------------------------------------------------

    inline bool GetTrackSolo(MediaTrack *track)
    {
        if (track == GetMasterTrack(NULL))
            return (GetMasterMuteSoloFlags() & 2) != 0;
        return GetMediaTrackInfo_Value(track, "I_SOLO") > 0;
    }

    inline void SetTrackSolo(MediaTrack *track, bool newState)
    {
        if (!track) return;
        if (track == GetMasterTrack(NULL)) {
            int flags = GetMasterMuteSoloFlags();
            if (flags & 2) flags &= ~2; else flags |= 2;
            CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, flags), NULL);
        } else {
            CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, newState), NULL);
        }
    }

    // -----------------------------------------------------------------------
    // Mute
    // -----------------------------------------------------------------------

    inline bool GetTrackMute(MediaTrack *track)
    {
        bool mute = false;
        GetTrackUIMute(track, &mute);
        return mute;
    }

    inline void SetTrackMute(MediaTrack *track, bool newState)
    {
        if (track)
            CSurf_SetSurfaceMute(track, CSurf_OnMuteChange(track, newState), NULL);
    }

    // -----------------------------------------------------------------------
    // Record arm
    // -----------------------------------------------------------------------

    inline bool GetTrackRecordArm(MediaTrack *track)
    {
        return GetMediaTrackInfo_Value(track, "I_RECARM") != 0.0;
    }

    inline void SetTrackRecordArm(MediaTrack *track, bool newState)
    {
        if (track)
            CSurf_SetSurfaceRecArm(track, CSurf_OnRecArmChange(track, newState), NULL);
    }

    // -----------------------------------------------------------------------
    // Phase inversion
    // -----------------------------------------------------------------------

    inline bool GetTrackInvertPhase(MediaTrack *track)
    {
        return GetMediaTrackInfo_Value(track, "B_PHASE") != 0.0;
    }

    inline void SetTrackInvertPhase(MediaTrack *track, bool newState)
    {
        if (track)
            SetMediaTrackInfo_Value(track, "B_PHASE", newState ? 1.0 : 0.0);
    }

    // -----------------------------------------------------------------------
    // FX chain bypass (whole track)
    // -----------------------------------------------------------------------

    inline bool GetTrackBypass(MediaTrack *track)
    {
        return GetMediaTrackInfo_Value(track, "I_FXEN") == 0;
    }

    inline void SetTrackBypass(MediaTrack *track, bool newState)
    {
        if (track)
            SetMediaTrackInfo_Value(track, "I_FXEN", newState ? 0.0 : 1.0);
    }

    // -----------------------------------------------------------------------
    // Selective effects bypass (skips instruments on instrument tracks)
    // -----------------------------------------------------------------------

    inline bool GetTrackEffectsBypass(MediaTrack *track)
    {
        if (DAW::GetTrackBypass(track)) return true;

        int fxCount = TrackFX_GetCount(track);
        if (fxCount == 0) return false;

        int  instrumentIdx = TrackFX_GetInstrument(track);
        int  startIdx      = instrumentIdx < 0 ? 0 : instrumentIdx + 1;
        bool anyBypassed   = false;

        if (instrumentIdx >= 0) {
            if (!TrackFX_GetEnabled(track, instrumentIdx)) {
                for (int i = startIdx; i < fxCount; ++i) {
                    if (TrackFX_GetOffline(track, i)) continue;
                    if (TrackFX_GetEnabled(track, i) && DAW::IsFxInstrument(track, i)) {
                        instrumentIdx = i;
                        startIdx = instrumentIdx + 1;
                        break;
                    }
                }
            }
            if (!TrackFX_GetEnabled(track, instrumentIdx)) return true;
            for (int i = startIdx; i < fxCount; ++i) {
                if (TrackFX_GetOffline(track, i)) continue;
                if (TrackFX_GetEnabled(track, i))  return false;
                else if (!DAW::IsFxInstrument(track, i)) anyBypassed = true;
            }
        } else {
            for (int i = startIdx; i < fxCount; ++i) {
                if (TrackFX_GetOffline(track, i)) continue;
                if (TrackFX_GetEnabled(track, i))  return false;
                else anyBypassed = true;
            }
        }
        return anyBypassed;
    }

    inline void SetTrackEffectsBypass(MediaTrack *track, bool newState)
    {
        if (!track) return;

        int fxCount = TrackFX_GetCount(track);
        if (fxCount == 0) { DAW::SetTrackBypass(track, newState); return; }

        int  instrumentIdx    = TrackFX_GetInstrument(track);
        int  startIdx         = instrumentIdx < 0 ? 0 : instrumentIdx + 1;
        bool wasTrackBypassed = DAW::GetTrackBypass(track);

        Undo_BeginBlock();
        if (instrumentIdx >= 0) {
            if (wasTrackBypassed)
                DAW::SetTrackBypass(track, !wasTrackBypassed);
            if (!TrackFX_GetEnabled(track, instrumentIdx)) {
                for (int i = startIdx; i < fxCount; ++i) {
                    if (TrackFX_GetOffline(track, i)) continue;
                    if (TrackFX_GetEnabled(track, i) && DAW::IsFxInstrument(track, i)) {
                        instrumentIdx = i;
                        startIdx = instrumentIdx + 1;
                        break;
                    }
                }
            }
            bool allInstrumentsBypassed = !TrackFX_GetEnabled(track, instrumentIdx);
            if (allInstrumentsBypassed && !newState)
                TrackFX_SetEnabled(track, instrumentIdx, !newState);
            for (int i = startIdx; i < fxCount; ++i) {
                if (TrackFX_GetOffline(track, i)) continue;
                if (DAW::IsFxInstrument(track, i)) continue;
                TrackFX_SetEnabled(track, i, !newState);
            }
        } else {
            DAW::SetTrackBypass(track, !wasTrackBypassed);
            if (!newState && wasTrackBypassed) {
                bool anyEnabled = false;
                for (int i = startIdx; i < fxCount; ++i) {
                    if (TrackFX_GetOffline(track, i)) continue;
                    if (TrackFX_GetEnabled(track, i)) { anyEnabled = true; break; }
                }
                if (!anyEnabled) {
                    for (int i = startIdx; i < fxCount; ++i) {
                        if (TrackFX_GetOffline(track, i)) continue;
                        TrackFX_SetEnabled(track, i, true);
                    }
                }
            }
        }
        Undo_EndBlock("Toggle tracks effects bypass", 0);
    }

} // namespace DAW
