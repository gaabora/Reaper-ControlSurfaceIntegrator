// types.h

//  Standalone value types, enums, and constants with no major-class dependencies.

#ifndef types_h
#define types_h

#ifndef WDL_NO_DEFINE_MINMAX
  #define WDL_NO_DEFINE_MINMAX
#endif

#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <functional>

// WDL utilities needed by PropertyList (lstrcpyn_safe, snprintf_append, WDL_NOT_NORMALLY)
#ifdef USING_CMAKE
  #include "../../lib/WDL/WDL/wdltypes.h"
  #include "../../lib/WDL/WDL/wdlcstring.h"
#else
  #include "../../WDL/wdltypes.h"
  #include "../../WDL/wdlcstring.h"
#endif

// Forward-declare REAPER types needed by MIDI_event_ex_t
#include "reaper_plugin_functions.h"

using std::string;
using std::vector;
using std::to_string;

// -------------------------------------------------------------------------
// Buffer size constants
// -------------------------------------------------------------------------
const int MEDBUF = 512;
const int SMLBUF = 256;

// -------------------------------------------------------------------------
// REAPER command IDs and CSI script paths
// -------------------------------------------------------------------------
inline constexpr const char* REASCRIPT_PATH__CSI_OSD = "/Scripts/CSI/CSI OSD on-screen display.lua";
inline constexpr const char* REASCRIPT_HASH__CSI_OSD = "_RSba74d8dbb9258d14b5305a183a5f20e8a6e0f64f";
inline constexpr const char* REASCRIPT_PATH__CSI_OSK = "/Scripts/CSI/CSI OSK on-screen keyboard.lua";
inline constexpr int REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES = 41743;
inline constexpr int REAPER__RESET_ALL_MIDI_CONTROL_SURFACE_DEVICES = 42348;
inline constexpr int REAPER__FILE_NEW_PROJECT = 40023;
inline constexpr int REAPER__FILE_OPEN_PROJECT = 40025;
inline constexpr int REAPER__TRACK_INSERT_TRACK_FROM_TEMPLATE = 46000;
inline constexpr int REAPER__CLOSE_CURRENT_PROJECT_TAB = 40860;
inline constexpr int REAPER__SWITCH_TO_NEXT_PROJECT_TAB = 40862;
inline constexpr int REAPER__SWITCH_TO_PREVIOUS_PROJECT_TAB = 40861;

// -------------------------------------------------------------------------
// CSI identification strings
// -------------------------------------------------------------------------
inline constexpr const char* s_CSIName = "CSI";
inline constexpr const char* s_CSIVersionDisplay = "v7.0";
inline constexpr const char* s_MajorVersionToken = "7.0";
inline constexpr const char* s_PageToken = "Page";
inline constexpr const char* s_MidiSurfaceToken = "MIDI";
inline constexpr const char* s_OSCSurfaceToken = "OSC";
inline constexpr const char* s_OSCX32SurfaceToken = "OSCX32";

inline constexpr const char* s_BadFileChars = " \\:*?<>|.,()/";
inline constexpr const char* s_BeginAutoSection = "#Begin auto generated section";
inline constexpr const char* s_EndAutoSection = "#End auto generated section";

// -------------------------------------------------------------------------
// struct osd_data
// -------------------------------------------------------------------------
inline vector<string> ExplodeString(const char separator, const string& value) {
    vector<string> result;
    size_t start = 0;
    size_t end = value.find(separator);
    while (end != string::npos) {
        result.push_back(value.substr(start, end - start));
        start = end + 1;
        end = value.find(separator, start);
    }
    result.push_back(value.substr(start));
    return result;
}

struct osd_data {
    inline static const string COLOR_ERROR = "#FF0000";
    string origValue;
    string message;
    int timeoutMs = 3000;
    vector<string> bgColors;
    string bgColor;
    string lastValue;

    DWORD startWaitFeedback = 0;

    osd_data() = default;

    osd_data(const string& osdValue) {
        origValue = osdValue;

        string value = osdValue;
        if (value.empty()) return;
        if (value.front() == '\"') value.erase(0, 1);
        if (value.back() == '\"') value.pop_back();

        vector<string> osdParams = ExplodeString(';', value);

        message = osdParams[0];
        if (osdParams.size() >= 2 && !osdParams[1].empty()) bgColors = ExplodeString(' ', osdParams[1]);
        if (osdParams.size() >= 3 && !osdParams[2].empty()) timeoutMs = atoi(osdParams[2].c_str());
    }

    const string toString() const {
        return message + ";" + bgColor + ";" + to_string(timeoutMs);
    }
    const bool isEmpty() const {
        return message.empty();
    }
    bool IsAwaitFeedback() const {
        DWORD now = GetTickCount();
        return startWaitFeedback != 0 && (now - startWaitFeedback <= 100);
    }
    void SetAwaitFeedback(bool value) {
        startWaitFeedback = value ? GetTickCount() : 0;
    }
};

// -------------------------------------------------------------------------
// struct rgba_color
// -------------------------------------------------------------------------
struct rgba_color {
    int r;
    int g;
    int b;
    int a;

    bool operator == (const rgba_color& other) const { return r == other.r && g == other.g && b == other.b && a == other.a; }

    bool operator != (const rgba_color& other) const { return r != other.r || g != other.g || b != other.b || a != other.a; }

    const char* rgba_to_string(char* buf) const { // buf must be at least 10 bytes
        snprintf(buf, 10, "#%02x%02x%02x%02x", r, g, b, a);
        return buf;
    }

    rgba_color() {
        r = 0;
        g = 0;
        b = 0;
        a = 255;
    }
};

// -------------------------------------------------------------------------
// struct MIDI_event_ex_t
// -------------------------------------------------------------------------
struct MIDI_event_ex_t : MIDI_event_t {
    MIDI_event_ex_t() {
        frame_offset = 0;
        size = 3;
        midi_message[0] = 0x00;
        midi_message[1] = 0x00;
        midi_message[2] = 0x00;
        midi_message[3] = 0x00;
    };

    MIDI_event_ex_t(const unsigned char first, const unsigned char second, const unsigned char third) {
        size = 3;
        midi_message[0] = first;
        midi_message[1] = second;
        midi_message[2] = third;
        midi_message[3] = 0x00;
    };

    bool IsEqualTo(const MIDI_event_ex_t* other) const {
        if (this->size != other->size)
            return false;

        for (int i = 0; i < size; ++i)
            if (this->midi_message[i] != other->midi_message[i])
                return false;

        return true;
    }
};

// -------------------------------------------------------------------------
// class ReloadPluginException
// -------------------------------------------------------------------------
class ReloadPluginException : public std::runtime_error
{
public:
    explicit ReloadPluginException(const std::string& message) : std::runtime_error(message) {}
};

// -------------------------------------------------------------------------
// enum PropertyType + class PropertyList
// -------------------------------------------------------------------------

// PropertyList::prop_from_string uses IsSameString; include utils.h to get
// the canonical inline definition rather than forward-declaring it static
// (which conflicts when utils.h is included first in Phase 7 sub-headers).
#include "utils.h"

enum PropertyType {

#define DECLARE_PROPERTY_TYPES(D) \
  D(Font) \
  D(TopMargin) \
  D(BottomMargin) \
  D(BackgroundColorOff) \
  D(TextColorOff) \
  D(BackgroundColorOn) \
  D(TextColorOn) \
  D(DisplayText) \
  D(BackgroundColor) \
  D(TextColor) \
  D(RingStyle) \
  D(Push) \
  D(PushColor) \
  D(LEDRingColor) \
  D(LEDRingColors) \
  D(BarStyle) \
  D(TextAlign) \
  D(TextInvert) \
  D(Mode) \
  D(OffColor) \
  D(OnColor) \
  D(Background) \
  D(Foreground) \
  D(Feedback) \
  D(Blink) \
  D(HoldDelay) \
  D(HoldRepeatInterval) \
  D(RunCount) \
  D(OSD) \
  D(Version) \
  D(SurfaceType) \
  D(SurfaceName) \
  D(SurfaceChannelCount) \
  D(MidiInput) \
  D(MidiOutput) \
  D(MIDISurfaceRefreshRate) \
  D(MaxMIDIMesssagesPerRun) \
  D(ReceiveOnPort) \
  D(TransmitToPort) \
  D(TransmitToIPAddress) \
  D(MaxPacketsPerRun) \
  D(PageName) \
  D(PageFollowsMCP) \
  D(SynchPages) \
  D(ScrollLink) \
  D(ScrollSynch) \
  D(Broadcaster) \
  D(Listener) \
  D(Surface) \
  D(StartChannel) \
  D(GoHome) \
  D(Modifiers) \
  D(FXMenu) \
  D(SelectedTrackFX) \
  D(SelectedTrackSends) \
  D(SelectedTrackReceives) \
  D(SurfaceFolder) \
  D(ZoneFolder) \
  D(FXZoneFolder) \
  D(NavType) \
  D(MeterMode) \
  D(KeyLabel) \

  PropertyType_Unknown = 0, // in this case, string is type=value pair
#define DEFPT(x) PropertyType_##x ,
  DECLARE_PROPERTY_TYPES(DEFPT)
#undef DEFPT
};

class PropertyList
{
    enum { MAX_PROP=24, RECLEN=10 };
    int nprops_;
    PropertyType props_[MAX_PROP];
    char vals_[MAX_PROP][RECLEN]; // if last byte is nonzero, pointer, otherwise, string

    static char* get_item_ptr(char* vp) { // returns a strdup'd string
        if (!vp[RECLEN - 1]) return NULL;
        char* ret;
        memcpy(&ret, vp, sizeof(char*));
        return ret;
    }

public:
    PropertyList() : nprops_(0) {}
    ~PropertyList() {
        for (int x = 0; x < nprops_; ++x)
            free(get_item_ptr(&vals_[x][0]));
    }

    void delete_props() {
        for (int x = 0; x < nprops_; ++x)
            free(get_item_ptr(&vals_[x][0]));
        nprops_ = 0;
    }

    void set_prop(PropertyType prop, const char* val) {
        int x;
        if (prop == PropertyType_Unknown)
            x = nprops_;
        else
            for (x = 0; x < nprops_ && props_[x] != prop; ++x)
                ;

        if (WDL_NOT_NORMALLY(x >= MAX_PROP))
            return;

        char* rec = &vals_[x][0];
        if (x == nprops_) {
            nprops_++;
            props_[x] = prop;
        } else {
            free(get_item_ptr(rec));
        }

        if (strlen(val) < RECLEN) {
            lstrcpyn_safe(rec, val, RECLEN);
            rec[RECLEN - 1] = 0;
        } else {
#ifdef WIN32
            char* v = _strdup(val);
#else
            char* v = strdup(val);
#endif
            memcpy(rec, &v, sizeof(v));
            rec[RECLEN - 1] = 1;
        }
    }
    void set_prop_int(PropertyType prop, int v) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%d", v);
        set_prop(prop, tmp);
    }

    const char* get_prop(PropertyType prop) const {
        for (int x = 0; x < nprops_; ++x)
            if (props_[x] == prop) {
                char* p = get_item_ptr((char*) (&vals_[x][0]));
                return p ? p : &vals_[x][0];
            }
        return NULL;
    }

    const char* enum_props(int x, PropertyType& type) const {
        if (x < 0 || x >= nprops_) return NULL;
        type = props_[x];
        return get_item_ptr((char*) &vals_[x][0]);
    }

    static PropertyType prop_from_string(const char* str) {
#define CHK(x)                 \
    if (IsSameString(str, #x)) \
        return PropertyType_##x;
        DECLARE_PROPERTY_TYPES(CHK)
#undef CHK
        return PropertyType_Unknown;
    }
    static const char* string_from_prop(PropertyType type) {
#define CHK(x)                    \
    if (type == PropertyType_##x) \
        return #x;
        DECLARE_PROPERTY_TYPES(CHK)
#undef CHK
        return NULL;
    }

    void save_list(FILE* fxFile) const {
        for (int x = 0; x < nprops_; ++x) {
            const char* value = get_prop(props_[x]);
            const char* key = string_from_prop(props_[x]);

            if (key && value)
                fprintf(fxFile, "%s=%s ", key, value);
        }
    }

    void print_to_buf(char* buf, int buf_size, PropertyType prop) {
        const char* key = string_from_prop(prop);
        const char* value = get_prop(prop);

        if (key && value)
            snprintf_append(buf, buf_size, "%s=%s ", key, value);
    }
};

// -------------------------------------------------------------------------
// ACTION_TYPE_LIST macro + enum class ActionType
// -------------------------------------------------------------------------
#define ACTION_TYPE_LIST(X) \
/* Transport and Timeline */ \
X(MoveCursor, "MoveEditCursor") \
X(Rewind, "Rewind") \
X(FastForward, "FastForward") \
X(Play, "Play") \
X(Stop, "Stop") \
X(Record, "Record") \
X(CycleTimeline, "CycleTimeline") \
X(MCUTimeDisplay, "MCUTimeDisplay") \
X(OSCTimeDisplay, "OSCTimeDisplay") \
X(CycleTimeDisplayModes, "CycleTimeDisplayModes") \
/* Tracks */ \
X(TrackVolume, "TrackVolume") \
X(SoftTakeover7BitTrackVolume, "SoftTakeover7BitTrackVolume") \
X(SoftTakeover14BitTrackVolume, "SoftTakeover14BitTrackVolume") \
X(TrackPanAutoLeft, "TrackPanAutoLeft") \
X(TrackPanAutoRight, "TrackPanAutoRight") \
X(TrackPan, "TrackPan") \
X(TrackPanWidth, "TrackPanWidth") \
X(TrackPanL, "TrackPanL") \
X(TrackPanR, "TrackPanR") \
X(TrackSelect, "TrackSelect") \
X(TrackUniqueSelect, "TrackUniqueSelect") \
X(TrackRangeSelect, "TrackRangeSelect") \
X(TrackEffectsBypass, "TrackEffectsBypass") \
X(TrackSolo, "TrackSolo") \
X(TrackMute, "TrackMute") \
X(TrackRecordArm, "TrackRecordArm") \
X(TrackRecordArmDisplay, "TrackRecordArmDisplay") \
X(TrackInvertPolarity, "TrackInvertPolarity") \
X(TrackInvertPolarityDisplay, "TrackInvertPolarityDisplay") \
X(CycleTrackInputMonitor, "CycleTrackInputMonitor") \
X(TrackInputMonitorDisplay, "TrackInputMonitorDisplay") \
X(TrackNameDisplay, "TrackNameDisplay") \
X(TrackNumberDisplay, "TrackNumberDisplay") \
X(TrackVolumeDisplay, "TrackVolumeDisplay") \
X(TrackPanAutoLeftDisplay, "TrackPanAutoLeftDisplay") \
X(TrackPanAutoRightDisplay, "TrackPanAutoRightDisplay") \
X(TrackPanDisplay, "TrackPanDisplay") \
X(TrackPanWidthDisplay, "TrackPanWidthDisplay") \
X(TrackPanLeftDisplay, "TrackPanLeftDisplay") \
X(TrackPanRightDisplay, "TrackPanRightDisplay") \
X(TrackOutputMeter, "TrackOutputMeter") \
X(TrackOutputMeterAverageLR, "TrackOutputMeterAverageLR") \
X(TrackOutputMeterMaxPeakLR, "TrackOutputMeterMaxPeakLR") \
X(TrackRecordInputDisplay, "TrackRecordInputDisplay") \
X(TrackVolumeDB, "TrackVolumeDB") \
X(TrackPanPercent, "TrackPanPercent") \
X(TrackPanWidthPercent, "TrackPanWidthPercent") \
X(TrackPanLPercent, "TrackPanLPercent") \
X(TrackPanRPercent, "TrackPanRPercent") \
X(TrackVolumeWithMeterAverageLR, "TrackVolumeWithMeterAverageLR") \
X(TrackVolumeWithMeterMaxPeakLR, "TrackVolumeWithMeterMaxPeakLR") \
/* Track Sends */ \
X(TrackSendNameDisplay, "TrackSendNameDisplay") \
X(TrackSendVolume, "TrackSendVolume") \
X(TrackSendVolumeDisplay, "TrackSendVolumeDisplay") \
X(TrackSendPan, "TrackSendPan") \
X(TrackSendPanDisplay, "TrackSendPanDisplay") \
X(TrackSendPrePost, "TrackSendPrePost") \
X(TrackSendPrePostDisplay, "TrackSendPrePostDisplay") \
X(TrackSendMute, "TrackSendMute") \
X(TrackSendStereoMonoDisplay, "TrackSendStereoMonoDisplay") \
X(TrackSendStereoMonoToggle, "TrackSendStereoMonoToggle") \
X(TrackSendInvertPolarity, "TrackSendInvertPolarity") \
X(TrackSendVolumeDB, "TrackSendVolumeDB") \
X(TrackSendPanPercent, "TrackSendPanPercent") \
/* Track Receives */ \
X(TrackReceiveNameDisplay, "TrackReceiveNameDisplay") \
X(TrackReceiveVolume, "TrackReceiveVolume") \
X(TrackReceiveVolumeDisplay, "TrackReceiveVolumeDisplay") \
X(TrackReceivePan, "TrackReceivePan") \
X(TrackReceivePanDisplay, "TrackReceivePanDisplay") \
X(TrackReceivePrePost, "TrackReceivePrePost") \
X(TrackReceivePrePostDisplay, "TrackReceivePrePostDisplay") \
X(TrackReceiveMute, "TrackReceiveMute") \
X(TrackReceiveStereoMonoToggle, "TrackReceiveStereoMonoToggle") \
X(TrackReceiveStereoMonoDisplay, "TrackReceiveStereoMonoDisplay") \
X(TrackReceiveInvertPolarity, "TrackReceiveInvertPolarity") \
X(TrackReceiveVolumeDB, "TrackReceiveVolumeDB") \
X(TrackReceivePanPercent, "TrackReceivePanPercent") \
/* FX */ \
X(FXParam, "FXParam") \
X(FXNameDisplay, "FXNameDisplay") \
X(FXParamNameDisplay, "FXParamNameDisplay") \
X(FXParamValueDisplay, "FXParamValueDisplay") \
X(FXMenuNameDisplay, "FXMenuNameDisplay") \
X(ToggleEnableFocusedFXMapping, "ToggleEnableFocusedFXMapping") \
X(ToggleFXBypass, "ToggleFXBypass") \
X(FXBypassDisplay, "FXBypassDisplay") \
X(ToggleFXOffline, "ToggleFXOffline") \
X(FXOfflineDisplay, "FXOfflineDisplay") \
X(FXGainReductionMeter, "FXGainReductionMeter") \
X(GoFXSlot, "GoFXSlot") \
X(ShowFXSlot, "ShowFXSlot") \
X(HideFXSlot, "HideFXSlot") \
X(TCPFXParam, "TCPFXParam") \
X(TCPFXParamNameDisplay, "TCPFXParamNameDisplay") \
X(TCPFXParamValueDisplay, "TCPFXParamValueDisplay") \
X(JSFXParam, "JSFXParam") \
\
X(LearnFocusedFX, "LearnFocusedFX") \
X(LastTouchedFXParam, "LastTouchedFXParam") \
X(LastTouchedFXParamNameDisplay, "LastTouchedFXParamNameDisplay") \
X(LastTouchedFXParamValueDisplay, "LastTouchedFXParamValueDisplay") \
X(ClearLastTouchedFXParam, "ClearLastTouchedFXParam") \
X(DisableFocusedFXMapping, "DisableFocusedFXMapping") \
X(DisableLastTouchedFXParamMapping, "DisableLastTouchedFXParamMapping") \
X(ToggleEnableLastTouchedFXParamMapping, "ToggleEnableLastTouchedFXParamMapping") \
X(ToggleUseLocalFXSlot, "ToggleUseLocalFXSlot") \
/* Navigation */ \
X(Bank, "Bank") \
X(GoHome, "GoHome") \
X(AllSurfacesGoHome, "AllSurfacesGoHome") \
X(GoZone, "GoZone") \
X(GoSubZone, "GoSubZone") \
X(LeaveSubZone, "LeaveSubZone") \
X(GoPage, "GoPage") \
 X(GoNextPage, "NextPage") \
X(PageNameDisplay, "PageNameDisplay") \
X(ToggleSynchPageBanking, "ToggleSynchPageBanking") \
X(ToggleScrollLink, "ToggleScrollLink") \
X(ToggleFollowMCP, "ToggleFollowMCP") \
X(ClearFXSlot, "ClearFXSlot") \
X(ClearFocusedFX, "ClearFocusedFX") \
X(ClearSelectedTrackFX, "ClearSelectedTrackFX") \
\
X(ToggleFolderView, "ToggleFolderView") \
X(TrackEnterFolder, "TrackEnterFolder") \
X(ExitCurrentFolder, "ExitCurrentFolder") \
/* Project Actions */ \
X(SaveProject, "SaveProject") \
X(Undo, "Undo") \
X(Redo, "Redo") \
/* VCA and Folder */ \
X(TrackToggleVCASpill, "TrackToggleVCASpill") \
X(TrackVCALeaderDisplay, "TrackVCALeaderDisplay") \
X(TrackToggleFolderSpill, "TrackToggleFolderSpill") \
X(TrackFolderParentDisplay, "TrackFolderParentDisplay") \
/* Automation */ \
X(TrackAutoMode, "TrackAutoMode") \
X(TrackAutoModeDisplay, "TrackAutoModeDisplay") \
X(GlobalAutoMode, "GlobalAutoMode") \
X(GlobalAutoModeDisplay, "GlobalAutoModeDisplay") \
X(CycleTrackAutoMode, "CycleTrackAutoMode") \
/* Other */ \
X(EnableOSD, "EnableOSD") \
 X(ReaperAction, "Reaper") \
X(NoAction, "NoAction") \
X(InvalidAction, "InvalidAction") \
X(FixedTextDisplay, "FixedTextDisplay") \
X(FixedRGBColorDisplay, "FixedRGBColorDisplay") \
X(ClearAllSolo, "ClearAllSolo") \
X(SetToggleChannel, "ToggleChannel") \
X(SendMIDIMessage, "SendMIDIMessage") \
X(SendOSCMessage, "SendOSCMessage") \
X(SetXTouchDisplayColors, "SetXTouchDisplayColors") \
X(RestoreXTouchDisplayColors, "RestoreXTouchDisplayColors") \
 X(SpeakOSARAMessage, "Speak") \
X(SpeakFXMenuName, "SpeakFXMenuName") \
X(SpeakTrackSendDestination, "SpeakTrackSendDestination") \
X(SpeakTrackReceiveSource, "SpeakTrackReceiveSource") \
X(ToggleRestrictTextLength, "ToggleRestrictTextLength") \
X(ToggleUseLocalModifiers, "ToggleUseLocalModifiers") \
X(CSINameDisplay, "CSINameDisplay") \
X(CSIVersionDisplay, "CSIVersionDisplay") \
/* Modifiers */ \
 X(SetShift, "Shift") \
 X(SetOption, "Option") \
 X(SetControl, "Control") \
 X(SetAlt, "Alt") \
 X(SetFlip, "Flip") \
 X(SetMarker, "Marker") \
 X(SetNudge, "Nudge") \
 X(SetScrub, "Scrub") \
 X(SetZoom, "Zoom") \
 X(SetGlobal, "Global") \
X(GlobalModeDisplay, "GlobalModeDisplay") \
X(ClearModifier, "ClearModifier") \
X(ClearModifiers, "ClearModifiers") \
/* Global settings */ \
X(SetBlinkTime, "SetBlinkTime") \
X(SetDoublePressTime, "SetDoublePressTime") \
X(SetHoldTime, "SetHoldTime") \
X(SetLatchTime, "SetLatchTime") \
X(SetDebugLevel, "SetDebugLevel") \
X(CycleDebugLevel, "CycleDebugLevel") \
X(SetOSDTime, "SetOSDTime") \
X(ToggleOSK, "ToggleOSK")
/* Invert, Hold, DoublePress - are pseudo modifiers */

enum class ActionType {
#define X(enumName, strName) enumName,
    ACTION_TYPE_LIST(X)
#undef X
    Abstract,
    Invalid
};

// -------------------------------------------------------------------------
// NAVIGATOR_TYPE_LIST macro + enum class NavigatorType
// -------------------------------------------------------------------------
#define NAVIGATOR_TYPE_LIST(X) \
X(TrackNavigator, "Track") \
X(FixedTrackNavigator, "FixedTrack") \
X(MasterTrackNavigator, "MasterTrack") \
X(SelectedTrackNavigator, "SelectedTrack") \
X(FocusedFXNavigator, "FocusedFX")

enum class NavigatorType {
#define X(enumName, strName) enumName,
    NAVIGATOR_TYPE_LIST(X)
#undef X
    Abstract,
    Invalid
};

#endif /* types_h */
