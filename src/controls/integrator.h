//
//  control_surface_integrator.h
//  reaper_control_surface_integrator
//
//

#ifndef control_surface_integrator
#define control_surface_integrator

// Phase 1: All major classes extracted to focused headers.
// Include them in dependency order.
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



static const int s_tickCounts_[] = { 250, 235, 220, 205, 190, 175, 160, 145, 130, 115, 100, 90, 80, 70, 60, 50, 45, 40, 35, 30, 25, 20, 20, 20 };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CSurfIntegrator : public IReaperControlSurface
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    char configtmp[1024];
   
    vector<unique_ptr<Midi_ControlSurfaceIO>> midiSurfacesIO_;
    vector<unique_ptr<OSC_ControlSurfaceIO>> oscSurfacesIO_;

    map<const string, unique_ptr<Action>> actions_;

    vector<unique_ptr<Page>> pages_;

    int currentPageIndex_ = 0;
    
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

    double GetPrivateProfileDouble(const char *key)
    {
        char tmp[512];
        memset(tmp, 0, sizeof(tmp));
        
        GetPrivateProfileString("REAPER", key, "", tmp, sizeof(tmp), get_ini_file());
        
        return strtod (tmp, NULL);
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

    virtual int Extended(int call, void *parm1, void *parm2, void *parm3) override;
    const char *GetTypeString() override;
    const char *GetDescString() override;
    const char *GetConfigString() override; // string of configuration data

    void ResetWidgets()
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->ForceClear();
    }

    void Shutdown()
    {
        // GAW -- IMPORTANT
        
        // We want to stop polling
        shouldRun_ = false;
        
        ResetWidgets();

        ShutdownLearn();
    }
    
    void Init();

    double GetFaderMaxDB() { return GetPrivateProfileDouble("slidermaxv"); }
    double GetFaderMinDB() { return GetPrivateProfileDouble("sliderminv"); }
    double GetVUMaxDB() { return GetPrivateProfileDouble("vumaxvol"); }
    double GetVUMinDB() { return GetPrivateProfileDouble("vuminvol"); }
    
    int *GetTimeModePtr() { return (int *) projectconfig_var_addr(NULL,timeModeOffs_); }
    int *GetTimeMode2Ptr() { return (int *) projectconfig_var_addr(NULL,timeMode2Offs_); }
    int *GetMeasOffsPtr() { return (int *) projectconfig_var_addr(NULL,measOffsOffs_); }
    double *GetTimeOffsPtr() { return (double *) projectconfig_var_addr(NULL,timeOffsOffs_); }
    int GetProjectPanMode() { int *p = (int *) projectconfig_var_addr(NULL,projectPanModeOffs_); return p ? *p : 0; }
   
    double *GetMetronomePrimaryVolumePtr()
    {
      void *ret = projectconfig_var_addr(NULL,projectMetronomePrimaryVolumeOffs_);
      if (ret) return (double *)ret;
      // REAPER 7.09 and earlier require this:
      int size=0;
      ret = get_config_var("projmetrov1", &size);
      if (size==8) return (double *)ret;
      return NULL;
    }
    
    double *GetMetronomeSecondaryVolumePtr()
    { 
      void *ret = projectconfig_var_addr(NULL,projectMetronomeSecondaryVolumeOffs_);
      if (ret) return (double *)ret;
      // REAPER 7.09 and earlier require this:
      int size=0;
      ret = get_config_var("projmetrov2", &size);
      if (size==8) return (double *)ret;
      return NULL;
    }

    void Speak(const char *phrase)
    {
        static void (*osara_outputMessage)(const char *message);
        static bool chk;
    
        if (!chk)
        {
            *(void **)&osara_outputMessage = plugin_getapi("osara_outputMessage");
            chk = true;
        }

        if (osara_outputMessage)
            osara_outputMessage(phrase);
    }
    
    osd_data QueuedOSD;
    void OpenOSDPanel() {
        string scriptsPath = string(GetResourcePath()) + REASCRIPT_PATH__CSI_OSD;
        int commandId = NamedCommandLookup(REASCRIPT_HASH__CSI_OSD);
        if (commandId == 0) {
            commandId = AddRemoveReaScript(true, 0, scriptsPath.c_str(), true);
            if (commandId == 0) {
                LogToConsole("[ERROR] FAILED to OpenOSDPanel. AddRemoveReaScript failed for '%s'\n", REASCRIPT_PATH__CSI_OSD);
                return;
            }
            commandId = NamedCommandLookup(REASCRIPT_HASH__CSI_OSD);
            LogToConsole("[NOTICE] ReaScript %s was loaded: %s (%d)\n", REASCRIPT_PATH__CSI_OSD, REASCRIPT_HASH__CSI_OSD, commandId);
        }
        int runningState;
        for (int attempt = 1; attempt <= 2; ++attempt) {
            runningState = GetToggleCommandState(commandId);
            if (runningState == 1) return;
            if (attempt == 2) {
                commandId = AddRemoveReaScript(true, 0, scriptsPath.c_str(), true);
                if (commandId == 0) {
                    LogToConsole("[ERROR] FAILED to OpenOSDPanel. AddRemoveReaScript failed for '%s'\n", REASCRIPT_PATH__CSI_OSD);
                    return;
                }
                const char* commandHash = ReverseNamedCommandLookup(commandId);
                if (!IsSameString(REASCRIPT_HASH__CSI_OSD + 1, commandHash))
                    LogToConsole("[ERROR] Command ID changed for '%s': '%s' >>> '_%s'\n", REASCRIPT_PATH__CSI_OSD, REASCRIPT_HASH__CSI_OSD, commandHash);
            }
            DAW::SendCommandMessage(commandId);
        }
        runningState = GetToggleCommandState(commandId);
        LogToConsole("[ERROR] FAILED to OpenOSDPanel. ReaScript: '%s' command ID: %s (%d) state: %d\n", 
            REASCRIPT_PATH__CSI_OSD, REASCRIPT_HASH__CSI_OSD, commandId, runningState);
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
            LogToConsole("[NOTICE] ReaScript %s was loaded: commandId=%d\n", REASCRIPT_PATH__CSI_OSK, oskCommandId_);
        }
        int runningState = GetToggleCommandState(oskCommandId_);
        if (runningState == 1) return; // already running
        DAW::SendCommandMessage(oskCommandId_);
    }

    void CloseOSKPanel() {
        ::SetExtState("CSI_OSK", "Command", "Close", false);
    }

    void PublishOSKSurfacesList() {
        string surfaces;
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            for (auto &surface : pages_[currentPageIndex_]->GetSurfaces()) {
                if (surface->GetOskEnabled()) {
                    if (!surfaces.empty()) surfaces += "|";
                    surfaces += surface->GetName();
                }
            }
        }
        ::SetExtState("CSI_OSK", "Surfaces", surfaces.c_str(), false);
    }

    Action *GetFXParamAction(char *FXName)
    {
       if (strstr(FXName, "JS: "))
           return actions_["JSFXParam"].get();
       else
           return actions_["FXParam"].get();
    }
    
    Action *GetAction(const char *actionName)
    {
        if (actions_.find(actionName) != actions_.end())
            return actions_[actionName].get();
        else
            return actions_["InvalidAction"].get();
    }

    void OnTrackSelection(MediaTrack *track) override
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->OnTrackSelection(track);
    }
    
    void SetTrackListChange() override
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->OnTrackListChange();
    }
    
    void NextTimeDisplayMode()
    {
        int *tmodeptr = GetTimeMode2Ptr();
        if (tmodeptr && *tmodeptr>=0)
        {
            (*tmodeptr)++;
            if ((*tmodeptr)>5)
                (*tmodeptr)=0;
        }
        else
        {
            tmodeptr = GetTimeModePtr();
            
            if (tmodeptr)
            {
                (*tmodeptr)++;
                if ((*tmodeptr)>5)
                    (*tmodeptr)=0;
            }
        }
    }
    
    void SetTrackOffset(int offset)
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            pages_[currentPageIndex_]->SetTrackOffset(offset);
    }
    
    void AdjustBank(Page *sendingPage, const char *zoneName, int amount)
    {
        if (! sendingPage->GetSynchPages())
            sendingPage->AdjustBank(zoneName, amount);
        else
            for (int i = 0; i < pages_.size(); ++i)
                if (pages_[currentPageIndex_]->GetSynchPages())
                    pages_[currentPageIndex_]->AdjustBank(zoneName, amount);
    }
       
    void NextPage()
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
        {
            pages_[currentPageIndex_]->LeavePage();
            currentPageIndex_ = currentPageIndex_ == pages_.size() - 1 ? 0 : (currentPageIndex_ + 1);
            if (pages_[currentPageIndex_])
                pages_[currentPageIndex_]->EnterPage();
        }
    }
    
    void GoToPage(const char *pageName)
    {
        for (int i = 0; i < pages_.size(); ++i)
        {
            if (IsSameString(pages_[i]->GetName(), pageName))
            {
                if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
                    pages_[currentPageIndex_]->LeavePage();
                
                currentPageIndex_ = i;
                
                if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
                    pages_[currentPageIndex_]->EnterPage();
                break;
            }
        }
    }
    
    bool GetTouchState(MediaTrack *track, int touchedControl) override
    {
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_])
            return pages_[currentPageIndex_]->GetTouchState(track, touchedControl);
        else
            return false;
    }
    
    void TrackFXListChanged(MediaTrack *track)
    {
        for (auto &page : pages_)
            page->TrackFXListChanged(track);
        
        if (g_fxParamsWrite)
        {
            char fxName[MEDBUF];
            
            for (int i = 0; i < TrackFX_GetCount(track); ++i)
            {
                TrackFX_GetFXName(track, i, fxName, sizeof(fxName));
                FILE *fxFile = NULL;
                                
                if (g_fxParamsWrite)
                {
                    string fxNameNoBadChars(fxName);
                    ReplaceAllWith(fxNameNoBadChars, s_BadFileChars, "_");

                    fxFile = fopenUTF8((string(GetResourcePath()) + "/CSI/ZoneRawFXFiles/" + fxNameNoBadChars + ".txt").c_str(), "wb");
                    
                    if (fxFile)
                        fprintf(fxFile, "Zone \"%s\"\n", fxName);
                }

                for (int j = 0; j < TrackFX_GetNumParams(track, i); ++j)
                {
                    char fxParamName[MEDBUF];
                    TrackFX_GetParamName(track, i, j, fxParamName, sizeof(fxParamName));
 
                    if (fxFile)
                        fprintf(fxFile, "\tFXParam %d \"%s\"\n", j, fxParamName);
                        
                    /* step sizes
                    double stepOut = 0;
                    double smallstepOut = 0;
                    double largestepOut = 0;
                    bool istoggleOut = false;
                    TrackFX_GetParameterStepSizes(track, i, j, &stepOut, &smallstepOut, &largestepOut, &istoggleOut);

                    ShowConsoleMsg(("\n\n" + to_string(j) + " - \"" + string(fxParamName) + "\"\t\t\t\t Step = " +  to_string(stepOut) + " Small Step = " + to_string(smallstepOut)  + " LargeStep = " + to_string(largestepOut)  + " Toggle Out = " + (istoggleOut == 0 ? "false" : "true")).c_str());
                    */
                }
                
                if (fxFile)
                {
                    fprintf(fxFile,"ZoneEnd");
                    fclose(fxFile);
                }
            }
        }
    }
    
    const char *GetTCPFXParamName(MediaTrack *track, int fxIndex, int paramIndex, char *buf, int bufsz)
    {
        buf[0]=0;
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
    
    void ShowErrorOSD(string text) {
        ForceOSD(text, osd_data::COLOR_ERROR);
    }
    void ForceOSD(string text, string bgColor = "") {
        osd_data osdData = osd_data(text);
        osdData.bgColor = bgColor;
        QueuedOSD = osdData;
    }
    void EnqueueOSD(osd_data osdData_) { QueuedOSD = osdData_; }

    void Run() override
    {
        //int start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        ReaProject* currentProject = (*EnumProjects)(-1, NULL, 0);

        if (currentProject_ != currentProject)
        {
            currentProject_ = currentProject;
            DAW::SendCommandMessage(REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES);
        }
        
        if (shouldRun_ && pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            if (!QueuedOSD.isEmpty() && !QueuedOSD.IsAwaitFeedback()) {
                if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] OSD: %s\n", QueuedOSD.toString().c_str());
                OpenOSDPanel();
                DAW::ShowOSD(QueuedOSD);
                QueuedOSD = osd_data();
            }
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


        /*
         repeats++;
         
         if (repeats > 50)
         {
         repeats = 0;
         
         int duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
         
         LogToConsole("%d microseconds\n", duration);
         }
        */
    }
};

/*
 int start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
 
 
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 // code you wish to time goes here
 
 
 
 int duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - start;
 
 LogToConsole("%d microseconds\n", duration);
 
 */

#endif /* control_surface_integrator.h */
