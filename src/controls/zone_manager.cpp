#include "integrator.h"
#include "zone_parser.h"

extern void WidgetMoved(ZoneManager* zoneManager, Widget* widget, int modifier);

static void collectFilesOfType(const string& type, const string& searchPath, vector<string>& results) {
    filesystem::path zonePath { searchPath };
    if (filesystem::exists(searchPath) && filesystem::is_directory(searchPath))
        for (auto& file : filesystem::recursive_directory_iterator(searchPath))
            if (file.path().extension() == type)
                results.push_back(file.path().string());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZoneManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager::ZoneManager(CSurfIntegrator* const csi, ControlSurface* surface, const string& zoneFolder, const string& fxZoneFolder)
    : csi_(csi), surface_(surface), zoneFolder_(zoneFolder), fxZoneFolder_(fxZoneFolder == "" ? zoneFolder : fxZoneFolder) {
}

Navigator* ZoneManager::GetNavigatorForTrack(MediaTrack* track) { return surface_->GetPage()->GetTrackNavigationManager()->GetNavigatorForTrack(track); }
Navigator* ZoneManager::GetMasterTrackNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetMasterTrackNavigator(); }
Navigator* ZoneManager::GetSelectedTrackNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetSelectedTrackNavigator(); }
Navigator* ZoneManager::GetFocusedFXNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetFocusedFXNavigator(); }
int ZoneManager::GetNumChannels() { return surface_->GetNumChannels(); }

void ZoneManager::Initialize() {
    PreProcessZones();

    if (zoneInfo_.find("Home") == zoneInfo_.end())
        return LogToConsole("[ERROR] Missing Home Zone for %s\n", surface_->GetName());

    homeZone_ = make_unique<Zone>(csi_, this, GetSelectedTrackNavigator(), 0, "Home", "Home", zoneInfo_["Home"].filePath);
    LoadZoneFile(homeZone_.get(), "");
    zoneInfo_["Home"].isLoaded = true;
    zoneInfo_["Home"].isReferenced = true;
    if (zoneInfo_.find("LastTouchedFXParam") != zoneInfo_.end()) {
        lastTouchedFXParamZone_ = make_shared<Zone>(csi_, this, GetFocusedFXNavigator(), 0, "LastTouchedFXParam", "LastTouchedFXParam", zoneInfo_["LastTouchedFXParam"].filePath);
        LoadZoneFile(lastTouchedFXParamZone_.get(), "");
        zoneInfo_["LastTouchedFXParam"].isLoaded = true;
    }

    vector<string> zoneList;

    if (zoneInfo_.find("GoZones") != zoneInfo_.end()) {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE) LogToConsole("[NOTICE] GoZones.zon is DEPRICATED and support of file will be removed in future.\n");
        LoadZoneMetadata(zoneInfo_["GoZones"].filePath.c_str(), zoneList);

        try {
            LoadZones(goZones_, zoneList);
        } catch (const std::exception& e) {
            LogToConsole("[ERROR] %s in GoZones section in file %s\n", e.what(), GetRelativePath(zoneInfo_["GoZones"].filePath.c_str()).c_str());
        }
    } else {
        for (const auto& entry : zoneInfo_) {
            if (IsSameString(entry.first, "FXEpilogue")
                || IsSameString(entry.first, "FXPrologue")
                || IsSameString(entry.first, "FXRowLayout")
                || IsSameString(entry.first, "FXWidgetLayout")
                || IsSameString(entry.first, "GoZones")
            ) continue;
            if (!entry.second.isLoaded && !entry.second.isFxZone) {
                zoneList.push_back(entry.first);
            }
        }
        LoadZones(goZones_, zoneList);

        for (const auto& entry : zoneInfo_) {
            ZoneInfo zoneInfo = entry.second;
            if (zoneInfo.isLoaded && !zoneInfo.isReferenced)
                if (g_debugLevel >= DEBUG_LEVEL_WARNING) LogToConsole("[WARNING] Zone '%s' was loaded but never referenced! %s\n", entry.first.c_str(), GetRelativePath(zoneInfo.filePath.c_str()).c_str());
            if (!zoneInfo.isLoaded && zoneInfo.isReferenced) LogToConsole("[ERROR] Zone '%s' was referenced but not loaded! (%s)\n", entry.first.c_str(), GetRelativePath(zoneInfo.filePath.c_str()).c_str());
        }
    }

    homeZone_->Activate();
}

void ZoneManager::PreProcessZoneFile(const string& filePath) {
    try {
        ifstream file(filePath);

        ZoneInfo info;
        info.filePath = filePath;

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] PreProcessZoneFile: %s\n", GetRelativePath(filePath.c_str()).c_str());

        info.isFxZone = 0 == strncmp(fxZoneFolder_.c_str(), filePath.c_str(), fxZoneFolder_.length());

        for (string line; getline(file, line);) {
            TrimLine(line);

            if (IsCommentedOrEmpty(line)) continue;

            vector<string> tokens;
            GetTokens(tokens, line);

            PropertyList pList;
            GetPropertiesFromTokens(0, (int) tokens.size(), tokens, pList);
            // "AU:", "AUi:", "VST:", "VST3:", "VST3i:", "VSTi:", "JS:", "Rewire:", "CLAP:", "CLAPi:",
            if (tokens[0] == "Zone" && tokens.size() > 1) {
                info.alias = tokens.size() > 2 ? tokens[2] : tokens[1];

                if (const char* propValue = pList.get_prop(PropertyType_NavType)) {
                    NavigatorType type = Navigator::NameToType(propValue);

                    if (type != NavigatorType::Invalid) {
                        info.navigator = propValue;
                    } else {
                        LogToConsole("[ERROR] Invalid value for property NavType=%s (supported: %s) in file %s\n", propValue, JoinStringVector(Navigator::GetSupportedNames(), ", ").c_str(), GetRelativePath(filePath.c_str()).c_str()); //FIXME review logging and overall approach on string vs char* everywhere, this .c_str()).c_str()).c_str() does not look right
                    }
                }
                //TODO: GoSubZone LeaveSubZone GoZone GoHome validity check

                AddZoneFilePath(tokens[1], info);
            }

            break;
        }
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to PreProcessZoneFile in %s\n", filePath.c_str());
        LogToConsole("Exception: %s\n", e.what());
    }
}

static ModifierManager s_modifierManager(NULL);

void ZoneManager::GetWidgetNameAndModifiers(const string& line, string& baseWidgetName, int& modifier, bool& isValueInverted, bool& isFeedbackInverted, bool& hasHoldModifier, bool& HasDoublePressPseudoModifier, bool& isDecrease, bool& isIncrease) {
    vector<string> tokens;
    GetTokens(tokens, line, '+');

    baseWidgetName = tokens[tokens.size() - 1];

    if (tokens.size() > 1) {
        for (int i = 0; i < tokens.size() - 1; ++i) {
            if (tokens[i].find("Touch") != string::npos) modifier += 1;
            else if (tokens[i] == "Toggle") modifier += 2;
            else if (tokens[i] == "Invert") isValueInverted = true;
            else if (tokens[i] == "InvertFB") isFeedbackInverted = true;
            else if (tokens[i] == "Hold") hasHoldModifier = true;
            else if (tokens[i] == "DoublePress") HasDoublePressPseudoModifier = true;
            else if (tokens[i] == "Decrease") isDecrease = true;
            else if (tokens[i] == "Increase") isIncrease = true;
        }
    }

    tokens.erase(tokens.begin() + tokens.size() - 1);

    modifier += s_modifierManager.GetModifierValue(tokens);
}

void ZoneManager::GetNavigatorsForZone(const char* zoneName, const char* navigatorName, vector<Navigator*>& navigators) {
    if (IsSameString(navigatorName, "MasterTrackNavigator") || IsSameString(zoneName, "MasterTrack"))
        navigators.push_back(GetMasterTrackNavigator());
    else if (IsSameString(zoneName, "MasterTrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
            navigators.push_back(GetMasterTrackNavigator());
    else if (IsSameString(navigatorName, "TrackNavigator") || IsSameString(zoneName, "Track") || IsSameString(zoneName, "VCA") || IsSameString(zoneName, "Folder") || IsSameString(zoneName, "SelectedTracks") || IsSameString(zoneName, "TrackSend") || IsSameString(zoneName, "TrackReceive") || IsSameString(zoneName, "TrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i) {
            Navigator* channelNavigator = GetSurface()->GetPage()->GetTrackNavigationManager()->GetNavigatorForChannel(i + GetSurface()->GetChannelOffset());
            if (channelNavigator)
                navigators.push_back(channelNavigator);
        }
    else if (IsSameString(zoneName, "SelectedTrack") || IsSameString(zoneName, "SelectedTrackSend") || IsSameString(zoneName, "SelectedTrackReceive") || IsSameString(zoneName, "SelectedTrackFXMenu"))
        for (int i = 0; i < GetNumChannels(); ++i)
            navigators.push_back(GetSelectedTrackNavigator());
    else if (IsSameString(navigatorName, "FocusedFXNavigator"))
        navigators.push_back(GetFocusedFXNavigator());
    //TODO: what about FixedTrackNavigator?
    else
        navigators.push_back(GetSelectedTrackNavigator());
}

void ZoneManager::LoadZones(vector<unique_ptr<Zone>>& zones, vector<string>& zoneList) {
    string missingZoneNames;

    for (const string& line : zoneList) {
        vector<string> tokens;
        GetTokens(tokens, line);

        if (tokens.empty()) continue;

        const string& zoneName = tokens[0];
        string navigatorName = tokens.size() > 1 ? tokens[1] : "";

        const auto& zoneInfoPair = zoneInfo_.find(zoneName);
        if (zoneInfoPair == zoneInfo_.end()) {
            missingZoneNames += " " + line;
            continue;
        }

        ZoneInfo& zoneInfo = zoneInfoPair->second;
        vector<Navigator*> navigators;
        GetNavigatorsForZone(zoneName.c_str(), navigatorName.c_str(), navigators);

        if (navigators.empty()) continue;
        for (int j = 0; j < navigators.size(); ++j) {
            string alias = zoneInfo.alias;
            string widgetSuffix = "";

            if (navigators.size() == 1) {
                bool alreadyLoaded = false;
                for (int i = 0; i < zones.size(); ++i) {
                    if (zones[i]->GetName() == zoneName && zones[i]->GetNavigator()->GetName() == navigators[j]->GetName()) {
                        alreadyLoaded = true;
                        break;
                    }
                }
                if (alreadyLoaded) continue;
            } else {
                alias += to_string(j + 1);
                widgetSuffix = to_string(j + 1);
            }

            auto zone = make_unique<Zone>(csi_, this, navigators[j], j, zoneName, alias, zoneInfo.filePath);
            LoadZoneFile(zone.get(), widgetSuffix.c_str());
            zones.push_back(std::move(zone));
            zoneInfo.isLoaded = true;
        }
    }

    if (!missingZoneNames.empty())
        LogToConsole("No .zon files found for zones: %s\n", missingZoneNames.c_str());
}

void ZoneManager::LoadZoneFile(Zone* zone, const char* widgetSuffix) {
    LoadZoneFile(zone, zone->GetSourceFilePath(), widgetSuffix);
}

void ZoneManager::LoadZoneFile(Zone* zone, const char* filePath, const char* widgetSuffix) {
    ZoneFileParser::ParseFile(this, zone, filePath, widgetSuffix);
}

void ZoneManager::AddListener(ControlSurface* surface) {
    listeners_.push_back(surface->GetZoneManager());
}

void ZoneManager::SetListenerCategories(PropertyList& pList) {
    if (const char* property = pList.get_prop(PropertyType_GoHome)) if (IsSameString(property, "Yes")) listensToGoHome_ = true;
    if (const char* property = pList.get_prop(PropertyType_SelectedTrackSends)) if (IsSameString(property, "Yes")) listensToSends_ = true;
    if (const char* property = pList.get_prop(PropertyType_SelectedTrackReceives)) if (IsSameString(property, "Yes")) listensToReceives_ = true;
    if (const char* property = pList.get_prop(PropertyType_FXMenu)) if (IsSameString(property, "Yes")) listensToFXMenu_ = true;
    if (const char* property = pList.get_prop(PropertyType_SelectedTrackFX)) if (IsSameString(property, "Yes")) listensToSelectedTrackFX_ = true;
    if (const char* property = pList.get_prop(PropertyType_Modifiers)) if (IsSameString(property, "Yes")) surface_->SetListensToModifiers();
}

void ZoneManager::CheckFocusedFXState() {
    int trackNumber = 0;
    int itemNumber = 0;
    int takeNumber = 0;
    int fxSlot = 0;
    int paramIndex = 0;

    bool retVal = GetTouchedOrFocusedFX(1, &trackNumber, &itemNumber, &takeNumber, &fxSlot, &paramIndex);

    if (!isFocusedFXMappingEnabled_) 
        return;

    if (!retVal || (retVal && (paramIndex & 0x01))) {
        if (focusedFXZone_ != NULL)
            ClearFocusedFX();

        return;
    }

    if (fxSlot < 0) return;

    MediaTrack* focusedTrack = NULL;

    trackNumber++;

    if (retVal && !(paramIndex & 0x01)) {
        if (trackNumber > 0)
            focusedTrack = DAW::GetTrack(trackNumber);
        else if (trackNumber == 0)
            focusedTrack = GetMasterTrack(NULL);
    }

    if (focusedTrack) {
        char fxName[MEDBUF];
        TrackFX_GetFXName(focusedTrack, fxSlot, fxName, sizeof(fxName));

        if (focusedFXZone_ != NULL && focusedFXZone_->GetSlotIndex() == fxSlot && IsSameString(fxName, focusedFXZone_->GetName())) return;
        else ClearFocusedFX();

        if (zoneInfo_.find(fxName) != zoneInfo_.end()) {
            focusedFXZone_ = make_shared<Zone>(csi_, this, GetFocusedFXNavigator(), fxSlot, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath.c_str());
            LoadZoneFile(focusedFXZone_.get(), "");
            focusedFXZone_->Activate();
        }
    }
}

void ZoneManager::GoSelectedTrackFX() {
    selectedTrackFXZones_.clear();

    if (MediaTrack* selectedTrack = surface_->GetPage()->GetTrackNavigationManager()->GetSelectedTrack(true)) {
        for (int i = 0; i < TrackFX_GetCount(selectedTrack); ++i) {
            char fxName[MEDBUF];

            TrackFX_GetFXName(selectedTrack, i, fxName, sizeof(fxName));

            if (zoneInfo_.find(fxName) != zoneInfo_.end()) {
                shared_ptr<Zone> zone = make_shared<Zone>(csi_, this, GetSelectedTrackNavigator(), i, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
                LoadZoneFile(zone.get(), "");
                selectedTrackFXZones_.push_back(zone);
                zone->Activate();
            }
        }
    }
}

void ZoneManager::GoFXSlot(MediaTrack* track, Navigator* navigator, int fxSlot) {
    if (fxSlot > TrackFX_GetCount(track) - 1) return;

    char fxName[MEDBUF];

    TrackFX_GetFXName(track, fxSlot, fxName, sizeof(fxName));

    if (zoneInfo_.find(fxName) != zoneInfo_.end()) {
        ClearFXSlot();
        fxSlotZone_ = make_shared<Zone>(csi_, this, navigator, fxSlot, fxName, zoneInfo_[fxName].alias, zoneInfo_[fxName].filePath);
        LoadZoneFile(fxSlotZone_.get(), "");
        fxSlotZone_->Activate();
    } else
        TrackFX_SetOpen(track, fxSlot, true);
}

void ZoneManager::UpdateCurrentActionContextModifiers() {
    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->UpdateCurrentActionContextModifiers();

    if (lastTouchedFXParamZone_ != NULL)
        lastTouchedFXParamZone_->UpdateCurrentActionContextModifiers();

    if (focusedFXZone_ != NULL)
        focusedFXZone_->UpdateCurrentActionContextModifiers();

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->UpdateCurrentActionContextModifiers();

    if (fxSlotZone_ != NULL)
        fxSlotZone_->UpdateCurrentActionContextModifiers();

    if (homeZone_ != NULL)
        homeZone_->UpdateCurrentActionContextModifiers();

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->UpdateCurrentActionContextModifiers();
    // Re-publish OSK labels so the on-screen keyboard reflects the new modifier state.
    surface_->PublishOSKLabels();
}

void ZoneManager::PreProcessZones() {
    if (zoneFolder_[0] == 0)
        return LogToConsole("[ERROR] Please check your CSI.ini, cannot find Zone folder for %s in: %s/CSI/Zones/", GetSurface()->GetName(), GetResourcePath());

    vector<string> zoneFilesToProcess;
    collectFilesOfType(".zon", zoneFolder_, zoneFilesToProcess);

    if (zoneFilesToProcess.size() == 0)
        return LogToConsole("[ERROR] Cannot find Zone files for %s in: %s", GetSurface()->GetName(), zoneFolder_.c_str());

    collectFilesOfType(".zon", fxZoneFolder_, zoneFilesToProcess);

    for (const string& zoneFile : zoneFilesToProcess)
        PreProcessZoneFile(zoneFile);
}

void ZoneManager::DoAction(Widget* widget, double value) {
    widget->LogInput(value);

    bool isUsed = false;

    DoAction(widget, value, isUsed);
}

void ZoneManager::DoAction(Widget* widget, double value, bool& isUsed) {
    if (!widget->IsVirtual() && value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE && surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoAction(widget, isUsed, value);

    if (isUsed) return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoAction(widget, isUsed, value);

    if (isUsed) return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoAction(widget, isUsed, value);

    if (isUsed) return;

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoAction(widget, isUsed, value);

    if (isUsed) return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoAction(widget, isUsed, value);

    if (isUsed) return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoAction(widget, isUsed, value);

    if (isUsed) return;

    if (homeZone_ != NULL)
        homeZone_->DoAction(widget, isUsed, value);
}

void ZoneManager::DoRelativeAction(Widget* widget, double delta) {
    widget->LogInput(delta);

    bool isUsed = false;

    DoRelativeAction(widget, delta, isUsed);
}

void ZoneManager::DoRelativeAction(Widget* widget, double delta, bool& isUsed) {
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoRelativeAction(widget, isUsed, delta);

    if (isUsed) return;

    if (homeZone_ != NULL)
        homeZone_->DoRelativeAction(widget, isUsed, delta);
}

void ZoneManager::DoRelativeAction(Widget* widget, int accelerationIndex, double delta) {
    widget->LogInput(delta);

    bool isUsed = false;

    DoRelativeAction(widget, accelerationIndex, delta, isUsed);
}

void ZoneManager::DoRelativeAction(Widget* widget, int accelerationIndex, double delta, bool& isUsed) {
    if (surface_->GetModifiers().size() > 0)
        WidgetMoved(this, widget, surface_->GetModifiers()[0]);

    if (learnFocusedFXZone_ != NULL)
        learnFocusedFXZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoRelativeAction(widget, isUsed, accelerationIndex, delta);

    if (isUsed) return;

    if (homeZone_ != NULL)
        homeZone_->DoRelativeAction(widget, isUsed, accelerationIndex, delta);
}

void ZoneManager::DoTouch(Widget* widget, double value) {
    widget->LogInput(value);
    bool isUsed = false;
    DoTouch(widget, value, isUsed);
}

void ZoneManager::DoTouch(Widget* widget, double value, bool& isUsed) {
    surface_->TouchChannel(widget->GetChannelNumber(), value != 0);

    // GAW -- temporary
    //if (surface_->GetModifiers().GetSize() > 0 && value != 0.0) // ignore touch releases for Learn mode
    //WidgetMoved(this, widget, surface_->GetModifiers().Get()[0]);

    //if (learnFocusedFXZone_ != NULL)
    //learnFocusedFXZone_->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed) return;

    if (lastTouchedFXParamZone_ != NULL && isLastTouchedFXParamMappingEnabled_)
        lastTouchedFXParamZone_->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed) return;

    if (focusedFXZone_ != NULL)
        focusedFXZone_->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed) return;

    for (int i = 0; i < selectedTrackFXZones_.size(); ++i)
        selectedTrackFXZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed) return;

    if (fxSlotZone_ != NULL)
        fxSlotZone_->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed) return;

    for (int i = 0; i < goZones_.size(); ++i)
        goZones_[i]->DoTouch(widget, widget->GetName(), isUsed, value);

    if (isUsed) return;

    if (homeZone_ != NULL)
        homeZone_->DoTouch(widget, widget->GetName(), isUsed, value);
}
