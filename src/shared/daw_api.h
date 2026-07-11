//
//  daw_api.h (formerly control_surface_integrator_Reaper.h)
//  reaper_csurf_integrator
//
//  DAW abstraction layer — the static DAW class wrapping REAPER API calls.
//  Value types (osd_data, rgba_color, MIDI_event_ex_t, MEDBUF/SMLBUF) have
//  moved to shared/types.h.  Utility functions moved to shared/utils.h.
//

#ifndef control_surface_integrator_Reaper_h
#define control_surface_integrator_Reaper_h

#ifndef WDL_NO_DEFINE_MINMAX
#define WDL_NO_DEFINE_MINMAX
#endif

#include "reaper_plugin_functions.h"
#include "types.h"
#include "handy_functions.h"

#include <string>
#include <vector>

using namespace std;

extern HWND g_hwnd;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DAW
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    static int constexpr AUTOMODE_TRIM = 0;
    static int constexpr AUTOMODE_READ = 1;
    static int constexpr AUTOMODE_TOUCH = 2;
    static int constexpr AUTOMODE_WRITE = 3;
    static int constexpr AUTOMODE_LATCH = 4;

    static int constexpr PANMODE_CLASSIC = 0;
    static int constexpr PANMODE_BALANCE = 3;
    static int constexpr PANMODE_STEREO = 5;
    static int constexpr PANMODE_DUAL = 6;

    static int constexpr QUERY_LAST_TOUCHED_PARAMETER = 0;
    static int constexpr QUERY_CURRENTLY_FOCUSED_FX = 1;

    static void SendCommandMessage(WPARAM wparam) { ::SendMessage(g_hwnd, WM_COMMAND, wparam, 0); }
    
    static bool ValidateTrackPtr(MediaTrack *track) { return ::ValidatePtr(track, "MediaTrack*"); }
    
    static bool CanUndo()
    {
        if (::Undo_CanUndo2(NULL))
           return true;
        else
            return false;
    }
    
    static bool CanRedo()
    {
        if (::Undo_CanRedo2(NULL))
           return true;
        else
            return false;
    }
    
    static void Undo()
    {
        if (CanUndo())
            ::Undo_DoUndo2(NULL);
    }
    
    static void Redo()
    {
        if (CanRedo())
            ::Undo_DoRedo2(NULL);
    }
       
    static MediaTrack *GetTrack(int trackidx)
    {
        trackidx--;
        
        if (trackidx < 0)
            trackidx = 0;
        
        return ::GetTrack(NULL, trackidx) ;
    }
    
    static rgba_color GetTrackColor(MediaTrack *track)
    {
        rgba_color color;
        
        if (ValidateTrackPtr(track))
            ::ColorFromNative(::GetTrackColor(track), &color.r, &color.g, &color.b);
        
        if (color.r == 0 && color.g == 0 && color.b == 0)
        {
            color.r = 64;
            color.g = 64;
            color.b = 64;
        }
        
        return color;
    }
    
    static unsigned int GetTrackGroupMembership(MediaTrack *track, const char *groupname)
    {
        if (ValidateTrackPtr(track))
            return ::GetSetTrackGroupMembership(track, groupname, 0, 0);
        else
            return 0;
    }
    
    static unsigned int GetTrackGroupMembershipHigh(MediaTrack *track, const char *groupname)
    {
        if (ValidateTrackPtr(track))
            return ::GetSetTrackGroupMembershipHigh(track, groupname, 0, 0);
        else
            return 0;
    }
    
    static const char* GetCommandName(int cmdID)
    {
        const char* actionName = ::kbd_getTextFromCmd(cmdID, ::SectionFromUniqueID(1));
        if (actionName)
            return actionName;
        else
            return "NOT FOUND!";
    }
    
    static void ShowOSD(const osd_data osdData)
    {
        static string lastValue;
        static DWORD lastUpdateTs = 0;
        DWORD now = GetTickCount();

        if (lastValue == osdData.lastValue) {
            if (osdData.timeoutMs == -1) return;
            if (osdData.timeoutMs >= 0 && (now - lastUpdateTs) < (DWORD)osdData.timeoutMs) return;
        }

        lastValue = osdData.toString();
        lastUpdateTs = now;
        ::SetExtState("CSI_TMP", "OSD", lastValue.c_str(), false);
    }
    static bool CheckTouchedOrFocusedFX(MediaTrack** outTrack, int* fxSlotNum, int* fxParamNum)
    {
        if (!outTrack || !fxSlotNum || !fxParamNum)
            return false;

        int trackNum, trackIdx, itemIdx, itemTake, slotIdx, paramIdx;

        if (GetTouchedOrFocusedFX) {
            if (GetTouchedOrFocusedFX(QUERY_CURRENTLY_FOCUSED_FX, &trackIdx, &itemIdx, &itemTake, &slotIdx, &paramIdx)) {
                if (paramIdx & 1) // open, but no longer focused
                    *fxParamNum = -1;
            } else {
                if (!GetTouchedOrFocusedFX(QUERY_LAST_TOUCHED_PARAMETER, &trackIdx, &itemIdx, &itemTake, &slotIdx, &paramIdx))
                    return false;
            }
            trackNum = trackIdx + 1;
        } else {
            int type = GetFocusedFX2(&trackNum, fxSlotNum, fxParamNum);
            if (!type || (type & 4)) // closed or open, but no longer focused
                *fxParamNum = -1;
        }

        MediaTrack* track = nullptr;
        if (trackNum > 0)
            track = GetTrack(trackNum);
        else if (trackNum == 0)
            track = GetMasterTrack(nullptr);

        if (!track)
            return false;

        *outTrack = track;
        *fxSlotNum = slotIdx;
        *fxParamNum = paramIdx;
        return true;
    }

    static std::string GetFxParamDescription(MediaTrack *track, int fxSlotNum, int fxParamNum) {
        if (!track)
            return "";
        char fxName[256] = "";
        char paramName[128] = "";

        TrackFX_GetFXName(track, fxSlotNum, fxName, sizeof(fxName));
        TrackFX_GetParamName(track, fxSlotNum, fxParamNum, paramName, sizeof(paramName));

        string fxShortName = DAW::GetShortFXName(fxName);
        return "[" + fxShortName  + "] " + paramName;
    }

    static std::string GetFxParamValue(MediaTrack *track, int fxSlotNum, int fxParamNum) {
        if (!track)
            return "";
        char paramValue[128] = "";
        TrackFX_GetFormattedParamValue(track, fxSlotNum, fxParamNum, paramValue, sizeof(paramValue));
        return string(paramValue);
    }

    static std::string GetTrackName(MediaTrack *track) {
        const char* tn = static_cast<const char*>(GetSetMediaTrackInfo(track, "P_NAME", nullptr));
        if (tn && *tn)
            return std::string(tn);
        int trackNum = static_cast<int>(reinterpret_cast<intptr_t>(GetSetMediaTrackInfo(track, "IP_TRACKNUMBER", nullptr)));
        return (trackNum == -1) ? "Master" : "Track " + std::to_string(trackNum);
    }

    static std::string GetLastTouchedFXParamDescription()
    {
        int trackNum = 0, fxSlotNum = 0, fxParamNum = 0;
        if (!GetLastTouchedFX(&trackNum, &fxSlotNum, &fxParamNum)) return "No FX was touched";

        if (MediaTrack* track = GetTrack(trackNum)) {
            return DAW::GetFxParamDescription(track, fxSlotNum, fxParamNum) + " (" + DAW::GetTrackName(track) + ")";
        }
        return "FAILED to GetLastTouchedFXParamDescription";
    }

    static std::string GetShortFXName(const char* fullName)
    {
        std::string name(fullName);

        size_t colonPos = name.find(": ");
        if (colonPos != std::string::npos)
            name = name.substr(colonPos + 2);

        size_t parenPos = name.find(" (");
        if (parenPos != std::string::npos)
            name = name.substr(0, parenPos);

        return name;
    }

    static bool CheckTrackFxParamHasVolumeCurve(MediaTrack *track, int fxSlotNum, int fxParamNum,
        double* valueOut = nullptr, double* minOut = nullptr, double* maxOut = nullptr, double* midOut = nullptr, double* stepSizeOut = nullptr
    ) {
        double min = 0.0, max = 0.0, mid = 0.0, value = TrackFX_GetParamEx(track, fxSlotNum, fxParamNum, &min, &max, &mid);
        double stepSize = DAW::GetTrackFxParamStepSize(track, fxSlotNum, fxParamNum);
        if (valueOut) *valueOut = value;
        if (minOut) *minOut = min;
        if (maxOut) *maxOut = max;
        if (midOut) *midOut = mid;
        if (stepSizeOut) *stepSizeOut = stepSize;

        return (stepSize == 1.0 && min == 0.0 && max == 2.0);
    }

    static double GetTrackFxParamValue(MediaTrack *track, int fxSlotNum, int fxParamNum)
    {
        double value = 0.0, rawValue = 0.0, min = 0.0, max = 0.0;
        if (CheckTrackFxParamHasVolumeCurve(track, fxSlotNum, fxParamNum, &rawValue, &min, &max)) {
            value = (rawValue == 0.0) ? 0.0 : volToNormalized(rawValue);
        } else {
            #if defined(REAPERAPI_WANT_TrackFX_GetParamNormalized)
                value = TrackFX_GetParamNormalized(track, fxSlotNum, fxParamNum);
            #else
                double range = max - min;
                value = (range > 0.0) ? ((rawValue - min) / range) : 0.0;
            #endif
        }
        return value;
    }

    static void SetTrackFxParamValue(MediaTrack *track, int fxSlotNum, int fxParamNum, double value)
    {
        double newValue = value, rawValue = 0.0, min = 0.0, max = 0.0, mid = 0.0, stepSize = 0.0;
        if (CheckTrackFxParamHasVolumeCurve(track, fxSlotNum, fxParamNum, &rawValue, &min, &max, &mid, &stepSize)) {
            newValue = normalizedToVol(value);
            TrackFX_SetParam(track, fxSlotNum, fxParamNum, newValue);
        } else {
            #if defined(REAPERAPI_WANT_TrackFX_SetParamNormalized)
                TrackFX_SetParamNormalized(track, fxSlotNum, fxParamNum,  newValue);
            #else
                newValue = min + value * (max - min);
                TrackFX_SetParam(track, fxSlotNum, fxParamNum, newValue);
            #endif
        }
    }

    static double GetTrackFxParamStepSize(MediaTrack *track, int fxSlotNum, int fxParamNum)
    {
        double stepSize = 1.0, smallstep, largestep;
        bool isToggle;
        TrackFX_GetParameterStepSizes(track, fxSlotNum, fxParamNum, &stepSize, &smallstep, &largestep, &isToggle);
        return stepSize;
    }

    static double GetTrackVolumeValue(MediaTrack *track)
    {
        double value = 0.0, pan = 0.0;
        GetTrackUIVolPan(track, &value, &pan);
        return volToNormalized(value);
    }

    static void SetTrackVolumeValue(MediaTrack *track, double value)
    {
        CSurf_SetSurfaceVolume(track, CSurf_OnVolumeChange(track, normalizedToVol(value), false), NULL);
    }

    static bool CompareFaderValues(double a, double b, int decimals = 3) {
        double tolerance = std::pow(10.0, -decimals);
        return std::fabs(a - b) < tolerance;
    }

    static double RoundDouble(double value, int decimals = 6) {
        double multiplier = std::pow(10.0, decimals);
        return std::round(value * multiplier) / multiplier;
    }

    static void CycleTrackAutoMode(MediaTrack* track) {
        if (!track) return;
        static const int cycleModes[] = { AUTOMODE_TRIM, AUTOMODE_READ, AUTOMODE_TOUCH, AUTOMODE_LATCH };

        int currentMode = (int)GetMediaTrackInfo_Value(track, "I_AUTOMODE");
        int nextMode = CycleNextValue(cycleModes, currentMode);

        GetSetMediaTrackInfo(track, "I_AUTOMODE", &nextMode);
    }

    static bool GetTrackSolo(MediaTrack* track) {
        auto ass = GetMediaTrackInfo_Value(track, "I_SOLO");
        if (track == GetMasterTrack(NULL)) {
            int muteSoloFlags = GetMasterMuteSoloFlags();
            return (muteSoloFlags & 2);
        } else {
            return GetMediaTrackInfo_Value(track, "I_SOLO") > 0;
        }
    }
    static void SetTrackSolo(MediaTrack* track, bool newState) {
        if (!track)
            return;
        if (track == GetMasterTrack(NULL)) {
            int muteSoloFlags = GetMasterMuteSoloFlags();
            if (muteSoloFlags & 2) {
                muteSoloFlags &= ~2;
            } else {
                muteSoloFlags |= 2;
            }
            CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, muteSoloFlags), NULL);
        } else {
            CSurf_SetSurfaceSolo(track, CSurf_OnSoloChange(track, newState), NULL);
        }
    }

    static bool GetTrackMute(MediaTrack* track) {
        bool mute = false;
        GetTrackUIMute(track, &mute);
        return mute;
    }
    static void SetTrackMute(MediaTrack* track, bool newState) {
        if (track)
            CSurf_SetSurfaceMute(track, CSurf_OnMuteChange(track, newState), NULL);
    }

    static bool GetTrackRecordArm(MediaTrack* track) {
        return GetMediaTrackInfo_Value(track, "I_RECARM") == 0.0 ? false : true;
    }
    static void SetTrackRecordArm(MediaTrack* track, bool newState) {
        if (track)
            CSurf_SetSurfaceRecArm(track, CSurf_OnRecArmChange(track, newState), NULL);
    }

    static bool GetTrackInvertPhase(MediaTrack* track) {
        return GetMediaTrackInfo_Value(track, "B_PHASE") == 0.0 ? false : true;
    }
    static void SetTrackInvertPhase(MediaTrack* track, bool newState) {
        if (track)
            SetMediaTrackInfo_Value(track, "B_PHASE", newState ? 1.0 : 0.0);
    }
    
    static bool GetTrackBypass(MediaTrack* track) {
        return GetMediaTrackInfo_Value(track, "I_FXEN") == 0;
    }
    static void SetTrackBypass(MediaTrack* track, bool newState) {
        if (track)
            SetMediaTrackInfo_Value(track, "I_FXEN", newState ? 0.0 : 1.0);
    }

    static bool GetTrackEffectsBypass(MediaTrack* track) {
        if (DAW::GetTrackBypass(track))
            return true;
        //TODO: containers + parallel fx support

        int fxCount = TrackFX_GetCount(track);
        if (fxCount == 0)
            return false; //TODO: return MultiState::Undefined;

        int instrumentIdx = TrackFX_GetInstrument(track);
        int startIdx = instrumentIdx < 0 ? 0 : instrumentIdx + 1;
        bool anyBypassed = false;

        if (instrumentIdx >= 0) {
            if (!TrackFX_GetEnabled(track, instrumentIdx)) {
                for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                    if (TrackFX_GetOffline(track, fxIdx))
                        continue;
                    if (TrackFX_GetEnabled(track, fxIdx) && IsFxInstrument(track, fxIdx)) {
                        instrumentIdx = fxIdx;
                        startIdx = instrumentIdx + 1;
                        break;
                    }
                }
            }
            bool allInstrumentsBypassed = !TrackFX_GetEnabled(track, instrumentIdx);
            if (allInstrumentsBypassed)
                return true;

            for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                if (TrackFX_GetOffline(track, fxIdx))
                    continue;
                if (TrackFX_GetEnabled(track, fxIdx))
                    return false;
                else if (!IsFxInstrument(track, fxIdx))
                    anyBypassed = true;
            }
            //TODO: if (no enabled non-instrument) return MultiState::Undefined;
        } else {
            for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                if (TrackFX_GetOffline(track, fxIdx))
                    continue;
                if (TrackFX_GetEnabled(track, fxIdx))
                    return false;
                else
                    anyBypassed = true;
            }
        }
        return anyBypassed;
    }
    static void SetTrackEffectsBypass(MediaTrack* track, bool newState) {
        if (!track) return;

        int fxCount = TrackFX_GetCount(track);
        if (fxCount == 0) {
            DAW::SetTrackBypass(track, newState);
            return;
        }

        
        int instrumentIdx = TrackFX_GetInstrument(track);
        int startIdx = instrumentIdx < 0 ? 0 : instrumentIdx + 1;
        bool wasTrackBypassed = DAW::GetTrackBypass(track);
        
        Undo_BeginBlock();
        if (instrumentIdx >= 0) {
            if (wasTrackBypassed)
                DAW::SetTrackBypass(track, !wasTrackBypassed);
            if (!TrackFX_GetEnabled(track, instrumentIdx)) {
                for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                    if (TrackFX_GetOffline(track, fxIdx))
                        continue;
                    if (TrackFX_GetEnabled(track, fxIdx) && IsFxInstrument(track, fxIdx)) {
                        instrumentIdx = fxIdx;
                        startIdx = instrumentIdx + 1;
                        break;
                    }
                }
            }
            bool allInstrumentsBypassed = !TrackFX_GetEnabled(track, instrumentIdx);
            if (allInstrumentsBypassed && !newState)
                TrackFX_SetEnabled(track, instrumentIdx, !newState);

            for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                if (TrackFX_GetOffline(track, fxIdx))
                    continue;
                if (IsFxInstrument(track, fxIdx))
                    continue;
                TrackFX_SetEnabled(track, fxIdx, !newState);
            }
        } else {
            DAW::SetTrackBypass(track, !wasTrackBypassed);

            if (!newState && wasTrackBypassed) {
                bool anyEnabled = false;

                for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                    if (TrackFX_GetOffline(track, fxIdx))
                        continue;
                    if (TrackFX_GetEnabled(track, fxIdx)) {
                        anyEnabled = true;
                        break;
                    }
                }
                if (!anyEnabled) {
                    for (int fxIdx = startIdx; fxIdx < fxCount; ++fxIdx) {
                        if (TrackFX_GetOffline(track, fxIdx))
                            continue;
                        TrackFX_SetEnabled(track, fxIdx, true);
                    }
                }
            }
        }
        Undo_EndBlock("Toggle tracks effects bypass", 0);
    }

    static string GetTrackFxName(MediaTrack* track, int fxIdx) {
        char fxName[256];
        TrackFX_GetFXName(track, fxIdx, fxName, sizeof(fxName));
        return string(fxName);
    }

    static bool IsFxInstrument(MediaTrack* track, int fxIdx) {
        char fxType[128] = {};
        if (TrackFX_GetNamedConfigParm(track, fxIdx, "fx_type", fxType, sizeof(fxType))) {
            return strncmp(fxType, "VSTi", 4) == 0
                || strncmp(fxType, "VST3i", 5) == 0
                || strncmp(fxType, "AUi", 3) == 0
                || strncmp(fxType, "CLAPi", 5) == 0
                || (strncmp(fxType, "JS", 2) == 0 && strstr(fxType, ":i") != nullptr);
        }
        return false;
    }
};

#endif /* control_surface_integrator_Reaper_h */
