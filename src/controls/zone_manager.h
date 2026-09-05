#pragma once
//
//  zone_manager.h — ZoneManager class
//
#include "preamble.h"
#include "format2_zone_profile_loader.h"
#include "zone.h"
#include "widget.h"

class ZoneManager
{
    friend class ZoneFileParser; // zone_parser.h — parses .zon files

private:
    CSurfIntegrator* const csi_;
    ControlSurface* const surface_;
    string zoneFolder_;
    string vendorFxZoneFolder_;
    string userFxZoneFolder_;
    unique_ptr<Format2ZoneProfileLoadResult> format2ZoneProfile_;
    map<string, size_t> format2DocumentIndexByPath_;

    vector<unique_ptr<ActionContext>> emptyContexts_;

    map<const string, ZoneInfo> zoneInfo_;

    shared_ptr<Zone> learnFocusedFXZone_ = NULL;

    unique_ptr<Zone> homeZone_;

    vector<unique_ptr<Zone>> goZones_;

    vector<ZoneManager*> listeners_; // does not own pointers

    vector<shared_ptr<Zone>> zonesToBeDeleted_;

    bool listensToGoHome_ = false;
    bool listensToSends_ = false;
    bool listensToReceives_ = false;
    bool listensToFXMenu_ = false;
    bool usesLocalFXSlot_ = false;
    bool listensToSelectedTrackFX_ = false;

    shared_ptr<Zone> lastTouchedFXParamZone_ = NULL;
    bool isLastTouchedFXParamMappingEnabled_ = false;

    shared_ptr<Zone> focusedFXZone_ = NULL;
    bool isFocusedFXMappingEnabled_ = true;

    vector<shared_ptr<Zone>> selectedTrackFXZones_;
    shared_ptr<Zone> fxSlotZone_ = NULL;

    int trackSendOffset_ = 0;
    int trackReceiveOffset_ = 0;
    int trackFXMenuOffset_ = 0;
    int selectedTrackSendOffset_ = 0;
    int selectedTrackReceiveOffset_ = 0;
    int selectedTrackFXMenuOffset_ = 0;
    int masterTrackFXMenuOffset_ = 0;

    void GoFXSlot(MediaTrack* track, Navigator* navigator, int fxSlot);
    void GoSelectedTrackFX();
    enum class Format2InitializationState { NotUsed, Initialized, Failed };
    Format2InitializationState InitializeFormat2();
    void InitializeLegacy();
    void GetWidgetNameAndModifiers(const string& line, string& baseWidgetName, int& modifier, bool& isValueInverted, bool& isFeedbackInverted, bool& hasHoldModifier, bool& HasDoublePressPseudoModifier, bool& isDecrease, bool& isIncrease);
    void GetNavigatorsForZone(const char* zoneName, const char* navigatorName, vector<Navigator*>& navigators);
    void LoadZones(vector<unique_ptr<Zone>>& zones, vector<string>& zoneList);

    void DoAction(Widget* widget, double value, bool& isUsed);
    void DoRelativeAction(Widget* widget, double delta, bool& isUsed);
    void DoRelativeAction(Widget* widget, int accelerationIndex, double delta, bool& isUsed);
    void DoTouch(Widget* widget, double value, bool& isUsed);

    void LoadZoneMetadata(const char* filePath, vector<string>& metadata) {
        int lineNumber = 0;

        try {
            ifstream file(filePath);

            if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] LoadZoneMetadata: %s\n", GetRelativePath(filePath).c_str());
            for (string line; getline(file, line);) {
                TrimLine(line);
                lineNumber++;
                if (IsCommentedOrEmpty(line)) continue;

                vector<string> tokens;
                GetTokens(tokens, line);

                if (tokens[0] == "Zone" || tokens[0] == "ZoneEnd") continue;

                metadata.push_back(line);
            }
        } catch (const std::exception& e) {
            LogToConsole("[ERROR] FAILED to LoadZoneMetadata in %s, around line %d\n", filePath, lineNumber);
            LogToConsole("[ERROR] Exception: %s\n", e.what());
        }
    }

    void AdjustBank(int& bankOffset, int amount) {
        bankOffset += amount;

        if (bankOffset < 0) bankOffset = 0;
    }

    void ResetOffsets() {
        trackSendOffset_ = 0;
        trackReceiveOffset_ = 0;
        trackFXMenuOffset_ = 0;
        selectedTrackSendOffset_ = 0;
        selectedTrackReceiveOffset_ = 0;
        selectedTrackFXMenuOffset_ = 0;
        masterTrackFXMenuOffset_ = 0;
    }

    void ResetSelectedTrackOffsets() {
        selectedTrackSendOffset_ = 0;
        selectedTrackReceiveOffset_ = 0;
        selectedTrackFXMenuOffset_ = 0;
    }

    bool GetIsListener() {
        return listensToGoHome_ || listensToSends_ || listensToReceives_ || listensToFXMenu_ || listensToSelectedTrackFX_;
    }

    void ListenToGoZone(const char* zoneName) {
        if (IsSameString("SelectedTrackSend", zoneName))
            for (auto& listener : listeners_) {
                if (listener->GetListensToSends())
                    listener->GoZone(zoneName);
            }
        else if (IsSameString("SelectedTrackReceive", zoneName))
            for (auto& listener : listeners_) {
                if (listener->GetListensToReceives())
                    listener->GoZone(zoneName);
            }
        else if (IsSameString("SelectedTrackFX", zoneName))
            for (auto& listener : listeners_) {
                if (listener->GetListensToSelectedTrackFX())
                    listener->GoZone(zoneName);
            }
        else if (IsSameString("SelectedTrackFXMenu", zoneName))
            for (auto& listener : listeners_) {
                if (listener->GetListensToFXMenu())
                    listener->GoZone(zoneName);
            }
        else
            GoZone(zoneName);
    }

    void ListenToClearFXZone(const char* zoneToClear) {
        if (IsSameString("LastTouchedFXParam", zoneToClear))
            for (auto& listener : listeners_)
                listener->ClearLastTouchedFXParam();
        else if (IsSameString("FocusedFX", zoneToClear))
            for (auto& listener : listeners_)
                listener->ClearFocusedFX();
        else if (IsSameString("SelectedTrackFX", zoneToClear))
            for (auto& listener : listeners_) {
                if (listener->GetListensToSelectedTrackFX())
                    listener->GoZone(zoneToClear);
            }
        else if (IsSameString("FXSlot", zoneToClear))
            for (auto& listener : listeners_) {
                if (listener->GetListensToFXMenu())
                    listener->GoZone(zoneToClear);
            }
    }

    void ListenToGoHome() { if (listensToGoHome_) GoHome(); }

    void ListenToGoFXSlot(MediaTrack* track, Navigator* navigator, int fxSlot) { if (listensToFXMenu_) GoFXSlot(track, navigator, fxSlot); }

    void ListenToToggleEnableLastTouchedFXParamMapping() { ToggleEnableLastTouchedFXParamMapping(); }

    void ToggleEnableLastTouchedFXParamMapping() {
        isLastTouchedFXParamMappingEnabled_ = !isLastTouchedFXParamMappingEnabled_;
        if (lastTouchedFXParamZone_ != NULL) {
            if (isLastTouchedFXParamMappingEnabled_) lastTouchedFXParamZone_->Activate();
            else lastTouchedFXParamZone_->Deactivate();
        }
    }

    void ListenToToggleEnableFocusedFXMapping() { ToggleEnableFocusedFXMapping(); }

    void ToggleEnableFocusedFXMapping() { isFocusedFXMappingEnabled_ = !isFocusedFXMappingEnabled_; }

    void ClearLastTouchedFXParam() {
        if (lastTouchedFXParamZone_ != NULL) {
            lastTouchedFXParamZone_->Deactivate();
            zonesToBeDeleted_.push_back(lastTouchedFXParamZone_);
            lastTouchedFXParamZone_ = NULL;
        }
    }

    void ClearFocusedFX() {
        if (focusedFXZone_ != NULL) {
            focusedFXZone_->Deactivate();
            zonesToBeDeleted_.push_back(focusedFXZone_);
            focusedFXZone_ = NULL;
        }
    }

    void ClearSelectedTrackFX() {
        for (auto& selectedTrackFXZone : selectedTrackFXZones_) {
            selectedTrackFXZone->Deactivate();
            zonesToBeDeleted_.push_back(selectedTrackFXZone);
        }
        selectedTrackFXZones_.clear();
    }

    void ClearFXSlot() {
        if (fxSlotZone_ != NULL) {
            fxSlotZone_->Deactivate();
            zonesToBeDeleted_.push_back(fxSlotZone_);
            fxSlotZone_ = NULL;
            ReactivateFXMenuZone();
        }
    }

    void ReactivateFXMenuZone() {
        for (int i = 0; i < goZones_.size(); ++i)
            if (goZones_[i]->GetIsActive() && (IsSameString(goZones_[i]->GetName(), "TrackFXMenu") || IsSameString(goZones_[i]->GetName(), "SelectedTrackFXMenu")))
                goZones_[i]->Activate();
    }

public:
    ZoneManager(CSurfIntegrator* const csi, ControlSurface* surface, const string& zoneFolder, const string& vendorFxZoneFolder, const string& userFxZoneFolder);

    ~ZoneManager() {
        focusedFXZone_ = NULL;
        lastTouchedFXParamZone_ = NULL;
        fxSlotZone_ = NULL;
        learnFocusedFXZone_ = NULL;
        selectedTrackFXZones_.clear();
        goZones_.clear();
        zoneInfo_.clear();
    }

    inline static const char* PIPE_CHARACTER = "|";

    void Initialize();
    void ReloadFromDisk();
    void ReplaceZoneProfileRoot(const filesystem::path& vendorProfileRoot, const filesystem::path& userProfileRoot);
    bool PrepareZonePathForWrite(const string& sourcePath, string& editablePath, bool& activatedUserProfile, string& errorMessage);

    Navigator* GetNavigatorForTrack(MediaTrack* track);
    Navigator* GetMasterTrackNavigator();
    Navigator* GetSelectedTrackNavigator();
    Navigator* GetFocusedFXNavigator();

    bool GetIsBroadcaster() { return !(listeners_.size() == 0); }
    void AddListener(ControlSurface* surface);
    void SetListenerCategories(PropertyList& pList);
    const vector<ZoneManager*>& GetListeners() { return listeners_; }
    void ToggleUseLocalFXSlot() { usesLocalFXSlot_ = !usesLocalFXSlot_; }
    bool GetToggleUseLocalFXSlot() { return usesLocalFXSlot_; }

    bool GetListensToSends() { return listensToSends_; }
    bool GetListensToReceives() { return listensToReceives_; }
    bool GetListensToFXMenu() { return listensToFXMenu_; }
    bool GetListensToSelectedTrackFX() { return listensToSelectedTrackFX_; }

    int GetNumChannels();

    void PreProcessZones();
    void PreProcessZoneFile(const string& filePath, bool isFxZone, bool isUserZone);
    void LoadZoneFile(Zone* zone, const char* widgetSuffix);
    void LoadZoneFile(Zone* zone, const char* filePath, const char* widgetSuffix);

    void UpdateCurrentActionContextModifiers();
    void CheckFocusedFXState();

    void DoAction(Widget* widget, double value);
    void DoRelativeAction(Widget* widget, double delta);
    void DoRelativeAction(Widget* widget, int accelerationIndex, double delta);
    void DoTouch(Widget* widget, double value);

    const string& GetZoneFolder() { return zoneFolder_; }
    const char* GetFXZoneFolder() { return userFxZoneFolder_.c_str(); }
    map<const string, ZoneInfo>& GetZoneInfo() { return zoneInfo_; }

    CSurfIntegrator* GetCSI() { return csi_; }
    ControlSurface* GetSurface() { return surface_; }

    Zone* GetActiveZoneForWidget(Widget* widget) {
        if (!widget) return NULL;

        function<Zone*(Zone*)> findInZone = [&](Zone* zone) -> Zone* {
            map<int, const vector<unique_ptr<ActionContext>>*> out;
            for (auto& zoneLayer : zone->GetZoneLayers()) if (zoneLayer->GetIsActive()) if (Zone* owner = findInZone(zoneLayer.get())) return owner;
            if (zone->GetAllModifierContexts(widget, out)) return zone;
            for (auto& includedZone : zone->GetIncludedZones()) if (includedZone->GetIsActive()) if (Zone* owner = findInZone(includedZone.get())) return owner;
            return NULL;
        };

        for (auto& goZone : goZones_) {
            if (!goZone->GetIsActive()) continue;
            if (Zone* owner = findInZone(goZone.get())) return owner;
        }

        if (homeZone_) {
            if (Zone* owner = findInZone(homeZone_.get())) return owner;
        }

        return NULL;
    }

    void GetActiveZoneInfoForWidget(Widget* widget, string& zoneName, string& zonePath) {
        zoneName.clear();
        zonePath.clear();
        Zone* zone = GetActiveZoneForWidget(widget);
        if (!zone) return;
        zoneName = zone->GetName();
        zonePath = zone->GetSourceFilePath();
    }

    // Collect all (modifier → &contexts) for widget from the first active zone that defines it.
    // Used by PublishOSKLabelMap() to enumerate all possible bindings per widget for tooltip display.
    void CollectAllModifierContextsForWidget(Widget* widget, map<int, const vector<unique_ptr<ActionContext>>*>& out) {
        function<bool(Zone*)> tryZone = [&](Zone* zone) -> bool {
            for (auto& zoneLayer : zone->GetZoneLayers()) if (zoneLayer->GetIsActive() && tryZone(zoneLayer.get())) return true;
            if (zone->GetAllModifierContexts(widget, out)) return true;
            for (auto& includedZone : zone->GetIncludedZones()) if (includedZone->GetIsActive() && tryZone(includedZone.get())) return true;
            return false;
        };
        for (auto& goZone : goZones_)
            if (goZone->GetIsActive() && tryZone(goZone.get())) return;
        if (homeZone_ && tryZone(homeZone_.get())) return;
    }

    const vector<unique_ptr<ActionContext>>& GetCurrentActionContextsForWidget(Widget* widget) {
        function<const vector<unique_ptr<ActionContext>>*(Zone*)> findContexts = [&](Zone* zone) -> const vector<unique_ptr<ActionContext>>* {
            for (auto& zoneLayer : zone->GetZoneLayers()) {
                if (!zoneLayer->GetIsActive()) continue;
                const vector<unique_ptr<ActionContext>>* contexts = findContexts(zoneLayer.get());
                if (contexts) return contexts;
            }
            const auto& contexts = zone->GetActionContexts(widget);
            if (!contexts.empty()) return &contexts;
            for (auto& includedZone : zone->GetIncludedZones()) {
                if (!includedZone->GetIsActive()) continue;
                const vector<unique_ptr<ActionContext>>* includedContexts = findContexts(includedZone.get());
                if (includedContexts) return includedContexts;
            }
            return nullptr;
        };

        // Check active goZones first
        for (auto& goZone : goZones_) {
            if (goZone->GetIsActive()) {
                const vector<unique_ptr<ActionContext>>* contexts = findContexts(goZone.get());
                if (contexts) return *contexts;
            }
        }

        // Fall back to home zone
        if (homeZone_) {
            const vector<unique_ptr<ActionContext>>* contexts = findContexts(homeZone_.get());
            if (contexts) return *contexts;
        }

        return emptyContexts_;
    }

    int GetTrackSendOffset() { return trackSendOffset_; }
    int GetTrackReceiveOffset() { return trackReceiveOffset_; }
    int GetTrackFXMenuOffset() { return trackFXMenuOffset_; }
    int GetSelectedTrackSendOffset() { return selectedTrackSendOffset_; }
    int GetSelectedTrackReceiveOffset() { return selectedTrackReceiveOffset_; }
    int GetSelectedTrackFXMenuOffset() { return selectedTrackFXMenuOffset_; }
    int GetMasterTrackFXMenuOffset() { return masterTrackFXMenuOffset_; }

    bool GetIsFocusedFXMappingEnabled() { return isFocusedFXMappingEnabled_; }
    bool GetIsLastTouchedFXParamMappingEnabled() { return isLastTouchedFXParamMappingEnabled_; }

    Zone* GetLearnedFocusedFXZone() { return learnFocusedFXZone_.get(); }

    const vector<unique_ptr<ActionContext>>& GetLearnFocusedFXActionContexts(Widget* widget, int modifier) {
        if (learnFocusedFXZone_ != NULL)
            return learnFocusedFXZone_->GetActionContexts(widget, modifier);
        else
            return emptyContexts_;
    }

    void GetAlias(const char* fxName, char* alias, int aliassz) {
        static const char* const prefixes[] = { //FIXME: move to config ini or whatever format
            "AU: Tube-Tech ",
            "AU: AU ",
            "AU: UAD UA ",
            "AU: UAD Pultec ",
            "AU: UAD Tube-Tech ",
            "AU: UAD Softube ",
            "AU: UAD Teletronix ",
            "AU: UADx ",
            "AU: UAD ",
            "AU: ",
            "AUi: ",
            "VST: TDR ",
            "VST: UAD UA ",
            "VST: UAD Pultec ",
            "VST: UAD Tube-Tech ",
            "VST: UAD Softube ",
            "VST: UAD Teletronix ",
            "VST: UAD ",
            "VST3: UADx ",
            "VST3i: UADx ",
            "VST: ",
            "VSTi: ",
            "VST3: ",
            "VST3i: ",
            "JS: ",
            "Rewire: ",
            "CLAP: ",
            "CLAPi: ",
        };

        // skip over known prefixes
        for (int i = 0; i < NUM_ELEM(prefixes); ++i) {
            const int l = (int) strlen(prefixes[i]);
            if (!strncmp(fxName, prefixes[i], l)) {
                fxName += l;
                break;
            }
        }

        lstrcpyn_safe(alias, fxName, aliassz);
        char* p = strstr(alias, " (");
        if (p) *p = 0;
    }

    void ClearLearnFocusedFXZone() {
        if (learnFocusedFXZone_ != NULL) {
            learnFocusedFXZone_->Deactivate();
            zonesToBeDeleted_.push_back(learnFocusedFXZone_);
            learnFocusedFXZone_ = NULL;
        }
    }

    void LoadLearnFocusedFXZone(MediaTrack* track, const char* fxName, int fxIndex) {
        if (zoneInfo_.find(fxName) != zoneInfo_.end()) {
            learnFocusedFXZone_ = make_shared<Zone>(csi_, this, GetNavigatorForTrack(track), fxIndex, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
            LoadZoneFile(learnFocusedFXZone_.get(), "");
            learnFocusedFXZone_->Activate();
        } else {
            char alias[BUFSIZ];
            GetAlias(fxName, alias, sizeof(alias));

            ZoneInfo info;
            info.alias = alias;

            string editableFxZoneFolder;
            string preparationError;
            bool activatedUserProfile = false;
            if (!this->PrepareZonePathForWrite(this->userFxZoneFolder_, editableFxZoneFolder, activatedUserProfile, preparationError)) {
                LogToConsole("[ERROR] Unable to prepare FX zone profile for writing: %s\n", preparationError.c_str());
                return;
            }

            char fxFilePath[BUFSIZ];
            snprintf(fxFilePath, sizeof(fxFilePath), "%s/AutoGeneratedFXZones", editableFxZoneFolder.c_str());
            RecursiveCreateDirectory(fxFilePath, 0);
            string fxFileName = fxName;
            ReplaceAllWith(fxFileName, s_BadFileChars, "_");
            char fxFullFilePath[BUFSIZ];
            snprintf(fxFullFilePath, sizeof(fxFullFilePath), "%s/%s.zon", fxFilePath, fxFileName.c_str());
            info.filePath = fxFullFilePath;

            learnFocusedFXZone_ = make_shared<Zone>(csi_, this, GetNavigatorForTrack(track), fxIndex, fxName, info.alias, info.filePath);

            if (learnFocusedFXZone_) {
                InitBlankLearnFocusedFXZone(this, learnFocusedFXZone_.get(), track, fxIndex);
                learnFocusedFXZone_->Activate();
            }
        }
    }

    void DeclareGoZone(const char* zoneName) {
        if (!GetIsBroadcaster() && !GetIsListener()) // No Broadcasters/Listeners relationships defined
            GoZone(zoneName);
        else
            for (auto& listener : listeners_)
                listener->ListenToGoZone(zoneName);
    }

    void GoZone(const char* zoneName); // out-of-line definition after ControlSurface class

    void DeclareClearFXZone(const char* zoneName) {
        if (!GetIsBroadcaster() && !GetIsListener()) // No Broadcasters/Listeners relationships defined
        {
            if (IsSameString("LastTouchedFXParam", zoneName))
                ClearLastTouchedFXParam();
            else if (IsSameString("FocusedFX", zoneName))
                ClearFocusedFX();
            else if (IsSameString("SelectedTrackFX", zoneName))
                ClearSelectedTrackFX();
            else if (IsSameString("FXSlot", zoneName))
                ClearFXSlot();
        } else
            for (auto& listener : listeners_)
                listener->ListenToClearFXZone(zoneName);
    }

    void DeclareGoFXSlot(MediaTrack* track, Navigator* navigator, int fxSlot) {
        if (usesLocalFXSlot_ || (!GetIsBroadcaster() && !GetIsListener())) // No Broadcasters/Listeners relationships defined
            GoFXSlot(track, navigator, fxSlot);
        else
            for (auto& listener : listeners_)
                listener->ListenToGoFXSlot(track, navigator, fxSlot);
    }

    void GetName(MediaTrack* track, int fxIndex, char* name, int namesz) {
        char fxName[MEDBUF];
        TrackFX_GetFXName(track, fxIndex, fxName, sizeof(fxName));

        if (zoneInfo_.find(fxName) != zoneInfo_.end())
            lstrcpyn_safe(name, zoneInfo_[fxName].alias.c_str(), namesz);
        else
            GetAlias(fxName, name, namesz);
    }

    void HideAllFXWindows() {
        for (int i = -1; i < GetNumTracks(); ++i) {
            MediaTrack* tr = i < 0 ? GetMasterTrack(NULL) : GetTrack(NULL, i);
            if (WDL_NOT_NORMALLY(tr == NULL)) continue;

            for (int j = TrackFX_GetCount(tr) - 1; j >= -1; j--)
                TrackFX_Show(tr, j, j < 0 ? 0 : 2);

            for (int j = CountTrackMediaItems(tr) - 1; j >= 0; j--) {
                MediaItem* item = GetTrackMediaItem(tr, j);
                for (int k = CountTakes(item) - 1; k >= 0; k--) {
                    MediaItem_Take* take = GetMediaItemTake(item, k);
                    if (take) {
                        for (int l = TakeFX_GetCount(take) - 1; l >= -1; l--)
                            TakeFX_Show(take, l, l < 0 ? 0 : 2);
                    }
                }
            }
        }
    }

    void GoHome(); // out-of-line definition after ControlSurface class

    void DeclareGoHome() {
        if (!GetIsBroadcaster() && !GetIsListener()) // No Broadcasters/Listeners relationships defined
            GoHome();
        else
            for (auto& listener : listeners_)
                listener->ListenToGoHome();
    }

    void OnTrackSelection() {
        if (fxSlotZone_ != NULL)
            ClearFXSlot();
    }

    void OnTrackDeselection() {
        ResetSelectedTrackOffsets();

        selectedTrackFXZones_.clear();

        for (auto& goZone : goZones_) {
            if (IsSameString(goZone->GetName(), "SelectedTrack") || IsSameString(goZone->GetName(), "SelectedTrackSend") || IsSameString(goZone->GetName(), "SelectedTrackReceive") || IsSameString(goZone->GetName(), "SelectedTrackFXMenu")) {
                goZone->Deactivate();
            }
        }

        homeZone_->OnTrackDeselection();
    }

    void DeclareToggleEnableLastTouchedFXParamMapping() {
        if (!GetIsBroadcaster() && !GetIsListener()) // No Broadcasters/Listeners relationships defined
            ToggleEnableLastTouchedFXParamMapping();
        else
            for (auto& listener : listeners_)
                listener->ListenToToggleEnableLastTouchedFXParamMapping();
    }

    void DisableLastTouchedFXParamMapping() {
        isLastTouchedFXParamMappingEnabled_ = false;
    }

    void DeclareToggleEnableFocusedFXMapping() {
        if (!GetIsBroadcaster() && !GetIsListener()) // No Broadcasters/Listeners relationships defined
            ToggleEnableFocusedFXMapping();
        else
            for (auto& listener : listeners_)
                listener->ListenToToggleEnableFocusedFXMapping();
    }

    void DisableFocusedFXMapping() {
        isFocusedFXMappingEnabled_ = false;
    }

    bool GetIsGoZoneActive(const char* zoneName) {
        for (auto& goZone : goZones_)
            if (IsSameString(zoneName, goZone->GetName()))
                return goZone->GetIsActive();

        return false;
    }

    bool GetIsHomeZoneOnlyActive() {
        for (auto& goZone : goZones_)
            if (goZone->GetIsActive())
                return false;

        return true;
    }

    void ClearFXMapping() {
        ClearLearnFocusedFXZone();
        CloseFocusedFXDialog();
        ClearFocusedFX();
        ClearSelectedTrackFX();
        ClearFXSlot();
    }

    void AdjustBank(const char* zoneName, int amount) {
        if (IsSameString(zoneName, "TrackSend"))
            AdjustBank(trackSendOffset_, amount);
        else if (IsSameString(zoneName, "TrackReceive"))
            AdjustBank(trackReceiveOffset_, amount);
        else if (IsSameString(zoneName, "TrackFXMenu"))
            AdjustBank(trackFXMenuOffset_, amount);
        else if (IsSameString(zoneName, "SelectedTrackSend"))
            AdjustBank(selectedTrackSendOffset_, amount);
        else if (IsSameString(zoneName, "SelectedTrackReceive"))
            AdjustBank(selectedTrackReceiveOffset_, amount);
        else if (IsSameString(zoneName, "SelectedTrackFXMenu"))
            AdjustBank(selectedTrackFXMenuOffset_, amount);
        else if (IsSameString(zoneName, "MasterTrackFXMenu"))
            AdjustBank(masterTrackFXMenuOffset_, amount);
    }

    void AddZoneFilePath(const string& name, ZoneInfo& zoneInfo) {
        if (zoneInfo_.find(name) == zoneInfo_.end()) {
            zoneInfo_[name] = zoneInfo;
        } else {
            ZoneInfo& info = zoneInfo_[name];
            if (!IsSameRelativePath(zoneInfo.filePath.c_str(), zoneInfo_[name].filePath.c_str())) {
                if (info.isFxZone && zoneInfo.isFxZone && info.isUserZone != zoneInfo.isUserZone) {
                    if (zoneInfo.isUserZone) {
                        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] User FX zone '%s' overrides vendor file '%s'\n", GetRelativePath(zoneInfo.filePath.c_str()).c_str(), GetRelativePath(info.filePath.c_str()).c_str());
                        info = zoneInfo;
                    }
                    return;
                }
                if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] Skipping file '%s': A zone named '%s' has already been loaded from the same layer. Duplicate zones are not allowed.\n", GetRelativePath(zoneInfo.filePath.c_str()).c_str(), name.c_str());
                return;
            }
            info.alias = zoneInfo.alias;
        }
    }

    void RequestUpdate() {
        CheckFocusedFXState();

        if (learnFocusedFXZone_ != NULL) {
            UpdateLearnWindow(this);
            learnFocusedFXZone_->RequestUpdate();
        }

        if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
            lastTouchedFXParamZone_->RequestUpdate();

        if (focusedFXZone_ != NULL)
            focusedFXZone_->RequestUpdate();

        for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
            selectedTrackFXZones_[i]->RequestUpdate();

        if (fxSlotZone_ != NULL)
            fxSlotZone_->RequestUpdate();

        for (int i = 0; i < goZones_.size(); ++i)
            goZones_[i]->RequestUpdate();

        if (homeZone_ != NULL)
            homeZone_->RequestUpdate();

        zonesToBeDeleted_.clear();
    }
};
