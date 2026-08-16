// config_parser.cpp — CSurfIntegrator::Init()
//
// Reads the product configuration file and wires up MIDI/OSC surfaces to pages.

#include "integrator.h"

// Free functions defined in integrator.cpp
extern void TrimLine(string& line);
extern void GetTokens(vector<string>& tokens, const string& line);
extern void GetPropertiesFromTokens(int start, int finish, const vector<string>& tokens, PropertyList& properties);

void CSurfIntegrator::Init() {
    pages_.clear();
    midiSurfacesIO_.clear();
    oscSurfacesIO_.clear();
    string currentBroadcaster;
    Page* currentPage = NULL;
    const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
    const string productRootPath = productPaths.ProductRoot().string();

    if (!filesystem::exists(productRootPath)) {
        LogToConsole("[ERROR] Missing %s resource folder. Please check your installation, cannot find %s\n", ProductIdentity::DisplayName, productRootPath.c_str());
        return;
    }

    const string iniFilePath = productPaths.ConfigFile().string();

    if (!filesystem::exists(iniFilePath)) {
        LogToConsole("[ERROR] Missing %s. Please check your installation, cannot find %s\n", ProductIdentity::ConfigFilename, iniFilePath.c_str());
        return;
    }

    int lineNumber = 0;

    try {
        ifstream iniFile(iniFilePath);

        for (string line; getline(iniFile, line);) {
            TrimLine(line);

            if (lineNumber == 0) {
                PropertyList pList;
                vector<string> properties;
                properties.push_back(line.c_str());
                GetPropertiesFromTokens(0, 1, properties, pList);

                const char* versionProp = pList.get_prop(PropertyType_Version);
                if (versionProp) {
                    if (!IsSameString(versionProp, s_MajorVersionToken)) {
                        LogToConsole("[ERROR] %s version mismatch. The configuration version is not %s.\n", ProductIdentity::ConfigFilename, s_MajorVersionToken);
                        //FIXME: so what? make backup and generate new, or at least prompt to confirm
                        iniFile.close();
                        return;
                    } else {
                        lineNumber++;
                        continue;
                    }
                } else {
                    LogToConsole("[ERROR] %s has no version.\n", ProductIdentity::ConfigFilename);
                    //FIXME: so what? generate new, or at least prompt to confirm
                    iniFile.close();
                    return;
                }
            }

            if (!line.empty() && line[0] == '#') continue;
            if (IsCommentedOrEmpty(line)) continue;

            vector<string> tokens;
            GetTokens(tokens, line.c_str());

            if (tokens.size() > 0) {
                PropertyList pList;
                GetPropertiesFromTokens(0, (int) tokens.size(), tokens, pList);

                if (const char* typeProp = pList.get_prop(PropertyType_SurfaceType)) {
                    if (const char* nameProp = pList.get_prop(PropertyType_SurfaceName)) {
                        if (const char* channelCountProp = pList.get_prop(PropertyType_SurfaceChannelCount)) {
                            int channelCount = atoi(channelCountProp);

                            if (IsSameString(typeProp, s_MidiSurfaceToken) && tokens.size() == 7) {
                                if (pList.get_prop(PropertyType_MidiInput) != NULL && pList.get_prop(PropertyType_MidiOutput) != NULL && pList.get_prop(PropertyType_MIDISurfaceRefreshRate) != NULL && pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun) != NULL) {
                                    int midiIn = atoi(pList.get_prop(PropertyType_MidiInput));
                                    int midiOut = atoi(pList.get_prop(PropertyType_MidiOutput));
                                    int surfaceRefreshRate = atoi(pList.get_prop(PropertyType_MIDISurfaceRefreshRate));
                                    int maxMIDIMesssagesPerRun = atoi(pList.get_prop(PropertyType_MaxMIDIMesssagesPerRun));

                                    midiSurfacesIO_.push_back(make_unique<Midi_ControlSurfaceIO>(this, nameProp, channelCount, midiIn, midiOut, surfaceRefreshRate, maxMIDIMesssagesPerRun));
                                }
                            } else if ((IsSameString(typeProp, s_OSCSurfaceToken) || IsSameString(typeProp, s_OSCX32SurfaceToken)) && tokens.size() == 7) {
                                if (pList.get_prop(PropertyType_ReceiveOnPort) != NULL && pList.get_prop(PropertyType_TransmitToPort) != NULL && pList.get_prop(PropertyType_TransmitToIPAddress) != NULL && pList.get_prop(PropertyType_MaxPacketsPerRun) != NULL) {
                                    const char* receiveOnPort = pList.get_prop(PropertyType_ReceiveOnPort);
                                    const char* transmitToPort = pList.get_prop(PropertyType_TransmitToPort);
                                    const char* transmitToIPAddress = pList.get_prop(PropertyType_TransmitToIPAddress);
                                    int maxPacketsPerRun = atoi(pList.get_prop(PropertyType_MaxPacketsPerRun));

                                    if (IsSameString(typeProp, s_OSCSurfaceToken))
                                        oscSurfacesIO_.push_back(make_unique<OSC_ControlSurfaceIO>(this, nameProp, channelCount, receiveOnPort, transmitToPort, transmitToIPAddress, maxPacketsPerRun));
                                    else if (IsSameString(typeProp, s_OSCX32SurfaceToken))
                                        oscSurfacesIO_.push_back(make_unique<OSC_X32ControlSurfaceIO>(this, nameProp, channelCount, receiveOnPort, transmitToPort, transmitToIPAddress, maxPacketsPerRun));
                                }
                            }
                        }
                    }
                } else if (const char* pageNameProp = pList.get_prop(PropertyType_PageName)) {
                    bool followMCP = true;
                    bool synchPages = true;
                    bool isScrollLinkEnabled = false;
                    bool isScrollSynchEnabled = false;

                    currentPage = NULL;

                    if (tokens.size() > 1) {
                        if (const char* pageFollowsMCPProp = pList.get_prop(PropertyType_PageFollowsMCP)) if (IsSameString(pageFollowsMCPProp, "No")) followMCP = false;
                        if (const char* synchPagesProp = pList.get_prop(PropertyType_SynchPages)) if (IsSameString(synchPagesProp, "No")) synchPages = false;
                        if (const char* scrollLinkProp = pList.get_prop(PropertyType_ScrollLink)) if (IsSameString(scrollLinkProp, "Yes")) isScrollLinkEnabled = true;
                        if (const char* scrollSynchProp = pList.get_prop(PropertyType_ScrollSynch)) if (IsSameString(scrollSynchProp, "Yes")) isScrollSynchEnabled = true;
                        pages_.push_back(make_unique<Page>(this, pageNameProp, followMCP, synchPages, isScrollLinkEnabled, isScrollSynchEnabled));
                        currentPage = pages_.back().get();
                    }
                } else if (const char* broadcasterProp = pList.get_prop(PropertyType_Broadcaster)) {
                    currentBroadcaster = broadcasterProp;
                } else if (currentPage && tokens.size() > 2 && currentBroadcaster != "" && pList.get_prop(PropertyType_Listener) != NULL) {
                    if (currentPage && tokens.size() > 2 && currentBroadcaster != "") {
                        ControlSurface* broadcaster = NULL;
                        ControlSurface* listener = NULL;

                        const vector<unique_ptr<ControlSurface>>& surfaces = currentPage->GetSurfaces();

                        string currentSurface = string(pList.get_prop(PropertyType_Listener));

                        for (int i = 0; i < surfaces.size(); ++i) {
                            if (surfaces[i]->GetName() == currentBroadcaster)
                                broadcaster = surfaces[i].get();
                            if (surfaces[i]->GetName() == currentSurface)
                                listener = surfaces[i].get();
                        }

                        if (broadcaster != NULL && listener != NULL) {
                            broadcaster->GetZoneManager()->AddListener(listener);
                            listener->GetZoneManager()->SetListenerCategories(pList);
                        }
                    }
                } else if (currentPage && tokens.size() == 5) {
                    if (const char* surfaceProp = pList.get_prop(PropertyType_Surface)) {
                        if (const char* surfaceFolderProp = pList.get_prop(PropertyType_SurfaceFolder)) {
                            if (const char* startChannelProp = pList.get_prop(PropertyType_StartChannel)) {
                                int startChannel = atoi(startChannelProp);

                                if (surfaceFolderProp[0] == '\0') {
                                    LogToConsole("[ERROR] SurfaceFolder must contain a stable surface ID\n");
                                    return;
                                }

                                std::optional<filesystem::path> resolvedSurfaceFile;
                                try {
                                    resolvedSurfaceFile = productPaths.FindSurfaceFile(surfaceFolderProp);
                                } catch (const std::exception& error) {
                                    LogToConsole("[ERROR] Invalid SurfaceFolder '%s': %s\n", surfaceFolderProp, error.what());
                                    return;
                                }

                                if (!resolvedSurfaceFile) {
                                    LogToConsole("[ERROR] Missing surface '%s'. Expected %s/%s.txt or %s/%s.txt\n", surfaceFolderProp, productPaths.UserSurfacesRoot().string().c_str(), surfaceFolderProp, productPaths.VendorSurfacesRoot().string().c_str(), surfaceFolderProp);
                                    return;
                                }
                                const string surfaceFile = resolvedSurfaceFile->string();

                                string zoneProfileId = surfaceFolderProp;
                                if (const char* zoneFolderProp = pList.get_prop(PropertyType_ZoneFolder)) if (zoneFolderProp[0] != '\0') zoneProfileId = zoneFolderProp;
                                std::optional<filesystem::path> resolvedZoneProfile;
                                try {
                                    resolvedZoneProfile = productPaths.FindZoneProfileDirectory(zoneProfileId);
                                } catch (const std::exception& error) {
                                    LogToConsole("[ERROR] Invalid ZoneFolder '%s': %s\n", zoneProfileId.c_str(), error.what());
                                    return;
                                }
                                if (!resolvedZoneProfile) {
                                    LogToConsole("[ERROR] Missing zone profile '%s'. Expected under %s or %s\n", zoneProfileId.c_str(), productPaths.UserZonesRoot().string().c_str(), productPaths.VendorZonesRoot().string().c_str());
                                    return;
                                }
                                const string zoneFolder = (*resolvedZoneProfile / "Main").string();
                                if (!filesystem::is_directory(zoneFolder)) {
                                    LogToConsole("[ERROR] Missing Main zone folder %s\n", zoneFolder.c_str());
                                    return;
                                }

                                string fxZoneProfileId = surfaceFolderProp;
                                if (const char* fxZoneFolderProp = pList.get_prop(PropertyType_FXZoneFolder)) if (fxZoneFolderProp[0] != '\0') fxZoneProfileId = fxZoneFolderProp;
                                std::optional<filesystem::path> resolvedFxZoneProfile;
                                try {
                                    resolvedFxZoneProfile = productPaths.FindZoneProfileDirectory(fxZoneProfileId);
                                } catch (const std::exception& error) {
                                    LogToConsole("[ERROR] Invalid FXZoneFolder '%s': %s\n", fxZoneProfileId.c_str(), error.what());
                                    return;
                                }
                                if (!resolvedFxZoneProfile) {
                                    LogToConsole("[ERROR] Missing FX zone profile '%s'. Expected under %s or %s\n", fxZoneProfileId.c_str(), productPaths.UserZonesRoot().string().c_str(), productPaths.VendorZonesRoot().string().c_str());
                                    return;//FIXME: create folder in user no matter if it exist in vendor, but ALSO in other places where using, to not overduplicate but still use vendor fx zones together with user fx zones, if user did not try make changes to vendor fx zone 
                                }
                                const string fxZoneFolder = (*resolvedFxZoneProfile / "FX").string();
                                if (!filesystem::is_directory(fxZoneFolder)) {
                                    LogToConsole("[ERROR] Missing FX zone folder %s\n", fxZoneFolder.c_str());
                                    return;
                                }

                                bool foundIt = false;

                                for (auto& io : midiSurfacesIO_) {
                                    if (IsSameString(surfaceProp, io->GetName())) {
                                        foundIt = true;
                                        currentPage->GetSurfaces().push_back(make_unique<Midi_ControlSurface>(this, currentPage, surfaceProp, startChannel, surfaceFile.c_str(), zoneFolder.c_str(), fxZoneFolder.c_str(), io.get()));
                                        break;
                                    }
                                }

                                if (!foundIt) {
                                    for (auto& io : oscSurfacesIO_) {
                                        if (IsSameString(surfaceProp, io->GetName())) {
                                            foundIt = true;
                                            currentPage->GetSurfaces().push_back(make_unique<OSC_ControlSurface>(this, currentPage, surfaceProp, startChannel, surfaceFile.c_str(), zoneFolder.c_str(), fxZoneFolder.c_str(), io.get()));
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            lineNumber++;
        }
    } catch (const std::exception& e) {
        LogToConsole("[ERROR] FAILED to Init in %s, around line %d\n", iniFilePath.c_str(), lineNumber);
        LogToConsole("Exception: %s\n", e.what());
    }

    if (pages_.size() == 0)
        pages_.push_back(make_unique<Page>(this, "Home", false, false, false, false));

    for (auto& page : pages_) {
        for (auto& surface : page->GetSurfaces())
            surface->ForceClear();
        page->OnInitialization();
    }

    if (HasAnyOSKEnabled()) {
        PublishOSKSurfacesList();
        if (pages_.size() > currentPageIndex_ && pages_[currentPageIndex_]) {
            for (auto& surface : pages_[currentPageIndex_]->GetSurfaces()) {
                if (!surface->GetOskEnabled()) continue;
                surface->PublishOSKLayout();
                surface->PublishOSKLabels();
                surface->PublishOSKState();
            }
        }
        OpenOSKPanel();
    }
}
