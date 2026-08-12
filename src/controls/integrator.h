//
//  control_surface_integrator.h
//  reaper_control_surface_integrator
//
//

#ifndef control_surface_integrator
#define control_surface_integrator

// includes in dependency order.
#include "../actions/action.h"
#include "navigator.h"
#include "../actions/action_context.h"
#include "zone.h"
#include "feedback.h"
#include "widget.h"
#include "zone_manager.h"
#include "message_generator.h"
#include "modifier_manager.h"
#include "control_surface.h"
#include "midi/midi_surface.h"
#include "osc/osc_surface.h"
#include "track_nav_manager.h"
#include "page.h"

inline constexpr int s_tickCounts_[] = { 250, 235, 220, 205, 190, 175, 160, 145, 130, 115, 100, 90, 80, 70, 60, 50, 45, 40, 35, 30, 25, 20, 20, 20 };

class CSurfIntegrator : public IReaperControlSurface
{
private:
    // Single mutex serializing all REAPER-initiated entry points (Run, callbacks).
    // WDL_Mutex is backed by CRITICAL_SECTION on Windows — reentrant for the
    // owning thread, so nested REAPER API calls that re-enter CSI are safe.
    WDL_Mutex csiMutex_;

    vector<unique_ptr<Midi_ControlSurfaceIO>> midiSurfacesIO_;
    vector<unique_ptr<OSC_ControlSurfaceIO>> oscSurfacesIO_;

    map<const string, unique_ptr<Action>> actions_;

    vector<unique_ptr<Page>> pages_;

    std::atomic<int> currentPageIndex_{ 0 }; // atomic: read safely from audio thread in GetTouchState() (Phase C)

    bool shouldRun_ = true;

    ReaProject* currentProject_ = NULL;

    // these are offsets to be passed to projectconfig_var_addr() when needed in order to get the actual pointers
    int timeModeOffs_;
    int timeMode2Offs_;
    int measOffsOffs_;
    int timeOffsOffs_; // for a double
    int projectPanModeOffs_;

    int projectMetronomePrimaryVolumeOffs_; // for double -- if invalid, use fallbacks
    int projectMetronomeSecondaryVolumeOffs_; // for double -- if invalid, use fallbacks

    void InitActionsDictionary();

    void PollMidiDevices() {
        for (auto& midiSurfaceIO : this->midiSurfacesIO_) {
            if (!midiSurfaceIO->PollForDeviceReconnect()) continue;

            for (auto& page : this->pages_) {
                for (auto& surface : page->GetSurfaces()) {
                    Midi_ControlSurface* midiSurface = dynamic_cast<Midi_ControlSurface*>(surface.get());
                    if (midiSurface && midiSurface->UsesIO(midiSurfaceIO.get()))
                        midiSurface->OnMidiIOReconnected();
                }
            }
        }
    }

    double GetPrivateProfileDouble(const char* key) {
        char tmp[512];
        memset(tmp, 0, sizeof(tmp));

        GetPrivateProfileString("REAPER", key, "", tmp, sizeof(tmp), get_ini_file());

        return strtod(tmp, NULL);
    }
    inline static std::vector<int> reloadingCommandIds_ = {
        REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES,
        REAPER__RESET_ALL_MIDI_CONTROL_SURFACE_DEVICES,
        REAPER__FILE_NEW_PROJECT,
        REAPER__FILE_OPEN_PROJECT,
        REAPER__CLOSE_CURRENT_PROJECT_TAB,
        REAPER__SWITCH_TO_NEXT_PROJECT_TAB,
        REAPER__SWITCH_TO_PREVIOUS_PROJECT_TAB,
        REAPER__TRACK_INSERT_TRACK_FROM_TEMPLATE
    };

public:
    CSurfIntegrator();

    ~CSurfIntegrator();

    virtual int Extended(int call, void* parm1, void* parm2, void* parm3) override;
    const char* GetTypeString() override { return ProductIdentity::ReaperRegistrationId; }
    const char* GetDescString() override;
    const char* GetConfigString() override; // string of configuration data

    void ResetWidgets() {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->ForceClear();
    }

    void Shutdown() {
        shouldRun_ = false;
        ResetWidgets();
        ShutdownLearn();
    }

    void Init();

    double GetFaderMaxDB() { return GetPrivateProfileDouble("slidermaxv"); }
    double GetFaderMinDB() { return GetPrivateProfileDouble("sliderminv"); }
    double GetVUMaxDB() { return GetPrivateProfileDouble("vumaxvol"); }
    double GetVUMinDB() { return GetPrivateProfileDouble("vuminvol"); }

    int* GetTimeModePtr() { return (int*) projectconfig_var_addr(NULL, timeModeOffs_); }
    int* GetTimeMode2Ptr() { return (int*) projectconfig_var_addr(NULL, timeMode2Offs_); }
    int* GetActiveTimeModePtr() {
        int* tmodeptr = GetTimeMode2Ptr();
        if (tmodeptr && *tmodeptr >= TIMEMODE_DEFAULT)
            return tmodeptr;
        return GetTimeModePtr();
    }
    int GetResolvedTimeMode() {
        int* tmodeptr = GetActiveTimeModePtr();
        return tmodeptr ? *tmodeptr : TIMEMODE_DEFAULT;
    }
    int* GetMeasOffsPtr() { return (int*) projectconfig_var_addr(NULL, measOffsOffs_); }
    double* GetTimeOffsPtr() { return (double*) projectconfig_var_addr(NULL, timeOffsOffs_); }
    int GetProjectPanMode() {
        int* p = (int*) projectconfig_var_addr(NULL, projectPanModeOffs_);
        return p ? *p : 0;
    }

    double* GetMetronomePrimaryVolumePtr() {
        void* ret = projectconfig_var_addr(NULL, projectMetronomePrimaryVolumeOffs_);
        if (ret)
            return (double*) ret;
        // REAPER 7.09 and earlier require this:
        int size = 0;
        ret = get_config_var("projmetrov1", &size);
        if (size == 8)
            return (double*) ret;
        return NULL;
    }

    double* GetMetronomeSecondaryVolumePtr() {
        void* ret = projectconfig_var_addr(NULL, projectMetronomeSecondaryVolumeOffs_);
        if (ret)
            return (double*) ret;
        // REAPER 7.09 and earlier require this:
        int size = 0;
        ret = get_config_var("projmetrov2", &size);
        if (size == 8)
            return (double*) ret;
        return NULL;
    }

    void Speak(const char* phrase) {
        static void (*osara_outputMessage)(const char* message);
        static bool chk;

        if (!chk) {
            *(void**) &osara_outputMessage = plugin_getapi("osara_outputMessage");
            chk = true;
        }

        if (osara_outputMessage)
            osara_outputMessage(phrase);
    }

    osd_data QueuedOSD;
    int osdCommandId_ = 0;
    void OpenOSDPanel() {
        string scriptsPath = string(GetResourcePath()) + REASCRIPT_PATH__CSI_OSD;
        if (this->osdCommandId_ == 0) {
            this->osdCommandId_ = AddRemoveReaScript(true, 0, scriptsPath.c_str(), true);
            if (this->osdCommandId_ == 0) {
                LogToConsole("[ERROR] FAILED to OpenOSDPanel. AddRemoveReaScript failed for '%s'\n", REASCRIPT_PATH__CSI_OSD);
                return;
            }
            if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] ReaScript %s was loaded: commandId=%d\n", REASCRIPT_PATH__CSI_OSD, this->osdCommandId_);
        }
        int runningState;
        for (int attempt = 1; attempt <= 2; ++attempt) {
            runningState = GetToggleCommandState(this->osdCommandId_);
            if (runningState == 1) return;
            if (attempt == 2) {
                this->osdCommandId_ = AddRemoveReaScript(true, 0, scriptsPath.c_str(), true);
                if (this->osdCommandId_ == 0) {
                    LogToConsole("[ERROR] FAILED to OpenOSDPanel. AddRemoveReaScript failed for '%s'\n", REASCRIPT_PATH__CSI_OSD);
                    return;
                }
            }
            DAW::SendCommandMessage(this->osdCommandId_);
        }
        runningState = GetToggleCommandState(this->osdCommandId_);
        LogToConsole("[ERROR] FAILED to OpenOSDPanel. ReaScript: '%s' command ID: %d state: %d\n", REASCRIPT_PATH__CSI_OSD, this->osdCommandId_, runningState);
    }

    // -----------------------------------------------------------------------
    // OSK command bridge — poll Lua-written ExtState commands and dispatch to
    // the surface that owns the named widget.  Called from Run().
    // -----------------------------------------------------------------------
    void DispatchOSKWidgetPressDown(const string& surfName, const string& widgetName) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->InjectOSKPressDown(widgetName);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKWidgetPressDown: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKWidgetPressUp(const string& surfName, const string& widgetName) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->InjectOSKPressUp(widgetName);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKWidgetPressUp: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKWidgetScroll(const string& surfName, const string& widgetName, int accelerationIndex, double delta, int eventCount) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->InjectOSKScroll(widgetName, accelerationIndex, delta, eventCount);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKWidgetScroll: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKWidgetValue(const string& surfName, const string& widgetName, double value) {
        if (!(this->pages_.size() > this->currentPageIndex_ && this->pages_[this->currentPageIndex_])) return;
        for (auto& surface : this->pages_[this->currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->InjectOSKValue(widgetName, value);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKWidgetValue: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKWidgetTouch(const string& surfName, const string& widgetName, double value) {
        if (!(this->pages_.size() > this->currentPageIndex_ && this->pages_[this->currentPageIndex_])) return;
        for (auto& surface : this->pages_[this->currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->InjectOSKTouch(widgetName, value);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKWidgetTouch: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKSurfaceEnabled(const string& surfName, bool enabled) {
        if (!(this->pages_.size() > this->currentPageIndex_ && this->pages_[this->currentPageIndex_])) return;
        for (auto& surface : this->pages_[this->currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->SetOskEnabled(enabled);
                PublishOSKSurfacesList();
                if (enabled) {
                    surface->PublishOSKLayout();
                    surface->PublishOSKLabels();
                    surface->PublishOSKState();
                    OpenOSKPanel();
                } else if (!HasAnyOSKEnabled()) {
                    CloseOSKPanel();
                }
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKSurfaceEnabled: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKConfigQuery(const string& surfName, const string& widgetName) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->HandleOSKConfigQuery(widgetName);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKConfigQuery: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKConfigApplyLive(const string& surfName, const string& widgetName, const string& bindingData) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->HandleOSKConfigApplyLive(widgetName, bindingData);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKConfigApplyLive: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKConfigSave(const string& surfName, const string& widgetName) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->HandleOSKConfigSave(widgetName);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKConfigSave: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKConfigRevert(const string& surfName, const string& widgetName) {
        if (!(pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])) return;
        for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->HandleOSKConfigRevert(widgetName);
                return;
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKConfigRevert: surface '%s' not found\n", surfName.c_str());
    }

    void DispatchOSKZoneCreate(const string& surfName, const string& scaffoldType, const string& zoneName, const string& alias, const string& navigator) {
        if (!(this->pages_.size() > this->currentPageIndex_ && this->pages_[this->currentPageIndex_])) return;
        for (auto& surface : this->pages_[this->currentPageIndex_]->GetSurfaces()) {
            if (surfName == surface->GetName()) {
                surface->HandleOSKZoneCreate(scaffoldType, zoneName, alias, navigator);
                return;
            }
        }
        const string responseKey = "ZoneCreateStatus_" + surfName;
        ::SetExtState(ProductIdentity::ExtStateOsk, responseKey.c_str(), "ERR||Surface not found", false);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] DispatchOSKZoneCreate: surface '%s' not found\n", surfName.c_str());
    }

    void PublishOSKActionList() {
        const auto names = Action::GetSupportedNames();
        string csv;
        for (const auto& name : names) {
            if (!csv.empty()) csv += ",";
            csv += name;
        }
        ::SetExtState(ProductIdentity::ExtStateOsk, "ActionList", csv.c_str(), false);
    }

    void PollAndHandleOSKCommands() {
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "WidgetPressDown")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "WidgetPressDown");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "WidgetPressDown", false);
            auto sep = payload.find('|');
            if (sep != string::npos)
                DispatchOSKWidgetPressDown(payload.substr(0, sep), payload.substr(sep + 1));
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "WidgetPressUp")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "WidgetPressUp");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "WidgetPressUp", false);
            auto sep = payload.find('|');
            if (sep != string::npos)
                DispatchOSKWidgetPressUp(payload.substr(0, sep), payload.substr(sep + 1));
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "WidgetScroll")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "WidgetScroll");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "WidgetScroll", false);
            auto sep1 = payload.find('|');
            if (sep1 != string::npos) {
                auto sep2 = payload.find('|', sep1 + 1);
                if (sep2 != string::npos) {
                    const string surfaceName = payload.substr(0, sep1);
                    const string widgetName = payload.substr(sep1 + 1, sep2 - sep1 - 1);
                    const string scrollData = payload.substr(sep2 + 1);
                    const auto sep3 = scrollData.find('|');
                    if (sep3 != string::npos) {
                        const string accelerationText = scrollData.substr(0, sep3);
                        const string eventCountText = scrollData.substr(sep3 + 1);
                        char* accelerationEnd = nullptr;
                        char* eventCountEnd = nullptr;
                        const long parsedAccelerationIndex = strtol(accelerationText.c_str(), &accelerationEnd, 10);
                        const long parsedSignedEventCount = strtol(eventCountText.c_str(), &eventCountEnd, 10);
                        if (accelerationEnd && *accelerationEnd == '\0' && eventCountEnd && *eventCountEnd == '\0' && parsedSignedEventCount != 0) {
                            const int eventCount = parsedSignedEventCount > 8 || parsedSignedEventCount < -8 ? 8 : (int) (parsedSignedEventCount > 0 ? parsedSignedEventCount : -parsedSignedEventCount);
                            const double delta = parsedSignedEventCount > 0 ? 1.0 : -1.0;
                            const int accelerationIndex = (int) (std::max)(0L, (std::min)(parsedAccelerationIndex, 64L));
                            DispatchOSKWidgetScroll(surfaceName, widgetName, accelerationIndex, delta, eventCount);
                        } else if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] Invalid WidgetScroll payload: '%s'\n", payload.c_str());
                    } else if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] Invalid WidgetScroll payload: '%s'\n", payload.c_str());
                }
            }
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "WidgetValue")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "WidgetValue");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "WidgetValue", false);
            auto sep1 = payload.find('|');
            if (sep1 != string::npos) {
                auto sep2 = payload.find('|', sep1 + 1);
                if (sep2 != string::npos) {
                    const string surfaceName = payload.substr(0, sep1);
                    const string widgetName = payload.substr(sep1 + 1, sep2 - sep1 - 1);
                    const string valueText = payload.substr(sep2 + 1);
                    char* valueEnd = nullptr;
                    const double parsedValue = strtod(valueText.c_str(), &valueEnd);
                    if (valueEnd && valueEnd != valueText.c_str() && *valueEnd == '\0')
                        this->DispatchOSKWidgetValue(surfaceName, widgetName, parsedValue);
                    else if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] Invalid WidgetValue payload: '%s'\n", payload.c_str());
                }
            }
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "WidgetTouch")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "WidgetTouch");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "WidgetTouch", false);
            auto sep1 = payload.find('|');
            if (sep1 != string::npos) {
                auto sep2 = payload.find('|', sep1 + 1);
                if (sep2 != string::npos) {
                    const string surfaceName = payload.substr(0, sep1);
                    const string widgetName = payload.substr(sep1 + 1, sep2 - sep1 - 1);
                    const string valueText = payload.substr(sep2 + 1);
                    char* valueEnd = nullptr;
                    const double parsedValue = strtod(valueText.c_str(), &valueEnd);
                    if (valueEnd && valueEnd != valueText.c_str() && *valueEnd == '\0')
                        this->DispatchOSKWidgetTouch(surfaceName, widgetName, parsedValue);
                    else if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] Invalid WidgetTouch payload: '%s'\n", payload.c_str());
                }
            }
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "SurfaceEnabled")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "SurfaceEnabled");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "SurfaceEnabled", false);
            auto sep = payload.find('|');
            if (sep != string::npos)
                DispatchOSKSurfaceEnabled(payload.substr(0, sep), payload.substr(sep + 1) == "1");
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "ConfigQuery")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "ConfigQuery");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "ConfigQuery", false);
            auto sep = payload.find('|');
            if (sep != string::npos)
                DispatchOSKConfigQuery(payload.substr(0, sep), payload.substr(sep + 1));
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "ConfigApplyLive")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "ConfigApplyLive");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "ConfigApplyLive", false);
            auto sep1 = payload.find('|');
            if (sep1 != string::npos) {
                auto sep2 = payload.find('|', sep1 + 1);
                if (sep2 != string::npos) {
                    DispatchOSKConfigApplyLive(
                        payload.substr(0, sep1),
                        payload.substr(sep1 + 1, sep2 - sep1 - 1),
                        payload.substr(sep2 + 1)
                    );
                }
            }
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "ConfigSave")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "ConfigSave");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "ConfigSave", false);
            auto sep = payload.find('|');
            if (sep != string::npos)
                DispatchOSKConfigSave(payload.substr(0, sep), payload.substr(sep + 1));
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "ConfigRevert")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "ConfigRevert");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "ConfigRevert", false);
            auto sep = payload.find('|');
            if (sep != string::npos)
                DispatchOSKConfigRevert(payload.substr(0, sep), payload.substr(sep + 1));
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "ActionListQuery")) {
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "ActionListQuery", false);
            PublishOSKActionList();
        }
        if (::HasExtState(ProductIdentity::ExtStateOskCommand, "ZoneCreate")) {
            string payload = ::GetExtState(ProductIdentity::ExtStateOskCommand, "ZoneCreate");
            ::DeleteExtState(ProductIdentity::ExtStateOskCommand, "ZoneCreate", false);
            vector<string> fields;
            size_t fieldStart = 0;
            for (int fieldIndex = 0; fieldIndex < 4; ++fieldIndex) {
                const size_t separator = payload.find('|', fieldStart);
                if (separator == string::npos) break;
                fields.push_back(payload.substr(fieldStart, separator - fieldStart));
                fieldStart = separator + 1;
            }
            if (fields.size() == 4) fields.push_back(payload.substr(fieldStart));
            if (fields.size() == 5) this->DispatchOSKZoneCreate(fields[0], fields[1], fields[2], fields[3], fields[4]);
            else if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] Invalid ZoneCreate payload: '%s'\n", payload.c_str());
        }
    }

    int oskCommandId_ = 0;
    void OpenOSKPanel() {
        string scriptsPath = string(GetResourcePath()) + REASCRIPT_PATH__CSI_OSK;
        if (oskCommandId_ == 0) {
            oskCommandId_ = AddRemoveReaScript(true, 0, scriptsPath.c_str(), true);
            if (oskCommandId_ == 0) {
                LogToConsole("[ERROR] FAILED to OpenOSKPanel. AddRemoveReaScript failed for '%s'\n", REASCRIPT_PATH__CSI_OSK);
                return;
            }
            if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] ReaScript %s was loaded: commandId=%d\n", REASCRIPT_PATH__CSI_OSK, oskCommandId_);
        }
        int runningState = GetToggleCommandState(oskCommandId_);
        if (runningState == 1) return;
        DAW::SendCommandMessage(oskCommandId_);
    }

    void CloseOSKPanel() {
        ::SetExtState(ProductIdentity::ExtStateOsk, "Command", "Close", false);
    }

    void PublishOSKSurfacesList() {
        string surfaces;
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
                if (surface->GetOskEnabled()) {
                    if (!surfaces.empty())
                        surfaces += "|";
                    surfaces += surface->GetName();
                }
            }
        }
        ::SetExtState(ProductIdentity::ExtStateOsk, "Surfaces", surfaces.c_str(), false);
    }

    bool HasAnyOSKEnabled() const {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
                if (surface->GetOskEnabled())
                    return true;
            }
        }
        return false;
    }

    Action* GetFXParamAction(char* FXName) {
        if (strstr(FXName, "JS: ")) return actions_["JSFXParam"].get();
        else return actions_["FXParam"].get();
    }

    Action* GetAction(const char* actionName) {
        if (actions_.find(actionName) != actions_.end())
            return actions_[actionName].get();
        else
            return actions_["InvalidAction"].get();
    }

    void OnTrackSelection(MediaTrack* track) override {
        WDL_MutexLock lock(&csiMutex_);
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->OnTrackSelection(track);
    }

    void SetTrackListChange() override {
        WDL_MutexLock lock(&csiMutex_);
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->OnTrackListChange();
    }

    void NextTimeDisplayMode() {
        int* tmodeptr = GetActiveTimeModePtr();
        if (tmodeptr) {
            (*tmodeptr)++;
            if ((*tmodeptr) > TIMEMODE_LAST)
                (*tmodeptr) = TIMEMODE_DEFAULT;
        }
    }

    void SetTrackOffset(int offset) {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->GetTrackNavigationManager()->SetTrackOffset(offset);
    }

    void AdjustBank(IPageContext* sendingPage, const char* zoneName, int amount) {
        if (!sendingPage->GetTrackNavigationManager()->GetSynchPages())
            sendingPage->AdjustBank(zoneName, amount);
        else
            for (int i = 0; i < pages_.size(); ++i)
                if (pages_[currentPageIndex_]->GetTrackNavigationManager()->GetSynchPages())
                    pages_[currentPageIndex_]->AdjustBank(zoneName, amount);
    }

    void NextPage() {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            pages_[currentPageIndex_]->LeavePage();
            int idx = currentPageIndex_.load();
            currentPageIndex_.store(idx == (int)pages_.size() - 1 ? 0 : idx + 1);
            if (pages_[currentPageIndex_])
                pages_[currentPageIndex_]->EnterPage();
        }
    }

    void GoToPage(const char* pageName) {
        for (int i = 0; i < pages_.size(); ++i) {
            if (IsSameString(pages_[i]->GetName(), pageName)) {
                if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
                    pages_[currentPageIndex_]->LeavePage();

                currentPageIndex_.store(i);

                if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
                    pages_[currentPageIndex_]->EnterPage();
                break;
            }
        }
    }

    bool GetTouchState(MediaTrack* track, int touchedControl) override {
        // Phase C: csiMutex_ is deliberately NOT held here so the audio thread
        // never blocks on the coarse 30 Hz run-loop lock.
        // Safety instead comes from:
        //   currentPageIndex_ — std::atomic<int>, safe to read without a lock.
        //   pages_            — stable vector (never mutated after Init()).
        //   GetIsControlTouched() inside — acquires shared tracksMutex_,
        //                                  excluding concurrent Rebuild* writers.
        const int idx = currentPageIndex_.load(std::memory_order_relaxed);
        if (idx >= 0 && (size_t)idx < pages_.size() && pages_[idx])
            return pages_[idx]->GetTouchState(track, touchedControl);
        return false;
    }

    void TrackFXListChanged(MediaTrack* track) {
        for (auto& page : pages_)
            page->TrackFXListChanged(track);

        if (g_fxParamsWrite) {
            char fxName[MEDBUF];
            const filesystem::path rawFxFilesRoot = ProductPaths::FromReaperResourcePath().RawFxFilesRoot();
            std::error_code directoryError;
            filesystem::create_directories(rawFxFilesRoot, directoryError);
            if (directoryError) {
                LogToConsole("[ERROR] Cannot create raw FX output folder %s: %s\n", rawFxFilesRoot.string().c_str(), directoryError.message().c_str());
                return;
            }

            for (int i = 0; i < TrackFX_GetCount(track); ++i) {
                TrackFX_GetFXName(track, i, fxName, sizeof(fxName));
                FILE* fxFile = NULL;

                if (g_fxParamsWrite) {
                    string fxNameNoBadChars(fxName);
                    ReplaceAllWith(fxNameNoBadChars, s_BadFileChars, "_");
                    fxFile = fopenUTF8((rawFxFilesRoot / (fxNameNoBadChars + ".txt")).string().c_str(), "wb");
                    if (fxFile)
                        fprintf(fxFile, "Zone \"%s\"\n", fxName);
                }

                for (int j = 0; j < TrackFX_GetNumParams(track, i); ++j) {
                    char fxParamName[MEDBUF];
                    TrackFX_GetParamName(track, i, j, fxParamName, sizeof(fxParamName));
                    if (fxFile)
                        fprintf(fxFile, "\tFXParam %d \"%s\"\n", j, fxParamName);
                }

                if (fxFile) {
                    fprintf(fxFile, "ZoneEnd");
                    fclose(fxFile);
                }
            }
        }
    }

    const char* GetTCPFXParamName(MediaTrack* track, int fxIndex, int paramIndex, char* buf, int bufsz) {
        buf[0] = 0;
        TrackFX_GetParamName(track, fxIndex, paramIndex, buf, bufsz);
        return buf;
    }

    void AddReloadingCommandId(int commandId) {
        if (std::find(reloadingCommandIds_.begin(), reloadingCommandIds_.end(), commandId) == reloadingCommandIds_.end()) {
            reloadingCommandIds_.push_back(commandId);
        }
    }

    const std::vector<int>& GetReloadingCommandIds() {
        return reloadingCommandIds_;
    }

    void ShowErrorOSD(const string& text) {
        ForceOSD(text, osd_data::COLOR_ERROR);
    }
    void ForceOSD(const string& text, const string& bgColor = "") {
        osd_data osdData = osd_data(text);
        osdData.bgColor = bgColor;
        QueuedOSD = osdData;
    }
    void EnqueueOSD(const osd_data& osdData_) { QueuedOSD = osdData_; }

    void Run() override {
        WDL_MutexLock lock(&csiMutex_);

        ReaProject* currentProject = (*EnumProjects)(-1, NULL, 0);

        if (currentProject_ != currentProject) {
            currentProject_ = currentProject;
            DAW::SendCommandMessage(REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES);
        }
        if (shouldRun_ && pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            PollMidiDevices();
            if (!QueuedOSD.isEmpty() && !QueuedOSD.IsAwaitFeedback()) {
                if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] OSD: %s\n", QueuedOSD.toString().c_str());
                OpenOSDPanel();
                DAW::ShowOSD(QueuedOSD);
                QueuedOSD = osd_data();
            }
            PollAndHandleOSKCommands();
            try {
                pages_[currentPageIndex_]->Run();
            } catch (const ReloadPluginException& e) {
                if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] RELOADING: %s\n", e.what());
                ResetWidgets();
                ShutdownLearn();
                CloseAllDialogs();
            } catch (const std::exception& e) {
                LogToConsole("[ERROR] # CSurfIntegrator::RUN: %s\n", e.what());
                LogStackTraceToConsole();
            }
        }
    }
};

#endif /* control_surface_integrator.h */
