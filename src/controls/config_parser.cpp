// config_parser.cpp - CSurfIntegrator configuration application

#include "integrator.h"
#include "integrator_config_parser.h"

static void LogConfigIssue(const string& configPath, const IntegratorConfigIssue& issue) {
    LogToConsole("[ERROR] Configuration issue in %s at line %d: %s\n", configPath.c_str(), issue.lineNumber, issue.message.c_str());
}

static unique_ptr<ControlSurface> CreateConfiguredSurface(CSurfIntegrator* integrator, Page* page, const SurfaceAssignmentConfig& config, const ProductPaths& productPaths, const vector<unique_ptr<Midi_ControlSurfaceIO>>& midiIo, const vector<unique_ptr<OSC_ControlSurfaceIO>>& oscIo, string& errorMessage) {
    std::optional<filesystem::path> surfaceFile;
    try {
        surfaceFile = productPaths.FindSurfaceFile(config.surfaceId);
    } catch (const std::exception& error) {
        errorMessage = "Invalid SurfaceFolder '" + config.surfaceId + "': " + error.what();
        return nullptr;
    }
    if (!surfaceFile) {
        errorMessage = "Missing surface '" + config.surfaceId + "'. Expected " + productPaths.UserSurfacesRoot().string() + "/" + config.surfaceId + ".txt or " + productPaths.VendorSurfacesRoot().string() + "/" + config.surfaceId + ".txt";
        return nullptr;
    }

    std::optional<filesystem::path> mainZoneFolder;
    try {
        mainZoneFolder = productPaths.FindMainZones(config.mainZoneProfileId);
    } catch (const std::exception& error) {
        errorMessage = "Invalid ZoneFolder '" + config.mainZoneProfileId + "': " + error.what();
        return nullptr;
    }
    if (!mainZoneFolder) {
        errorMessage = "Missing Main zone profile '" + config.mainZoneProfileId + "'. Expected " + productPaths.MainZones(ZoneSource::User, config.mainZoneProfileId).string() + " or " + productPaths.MainZones(ZoneSource::Vendor, config.mainZoneProfileId).string();
        return nullptr;
    }

    filesystem::path vendorFxZoneFolder;
    filesystem::path userFxZoneFolder;
    try {
        vendorFxZoneFolder = productPaths.FxZones(ZoneSource::Vendor, config.fxZoneProfileId);
        userFxZoneFolder = productPaths.FxZones(ZoneSource::User, config.fxZoneProfileId);
    } catch (const std::exception& error) {
        errorMessage = "Invalid FXZoneFolder '" + config.fxZoneProfileId + "': " + error.what();
        return nullptr;
    }
    std::error_code createFxFolderError;
    filesystem::create_directories(userFxZoneFolder, createFxFolderError);
    if (createFxFolderError || !filesystem::is_directory(userFxZoneFolder)) {
        errorMessage = "Unable to create User FX zone folder " + userFxZoneFolder.string() + ": " + createFxFolderError.message();
        return nullptr;
    }

    const string surfaceFilePath = surfaceFile->string();
    const string mainZoneFolderPath = mainZoneFolder->string();
    const string vendorFxZoneFolderPath = vendorFxZoneFolder.string();
    const string userFxZoneFolderPath = userFxZoneFolder.string();
    const string& deviceId = config.deviceId.empty() ? config.surfaceName : config.deviceId;
    for (const auto& io : midiIo) {
        if (!IsSameString(deviceId, io->GetName())) continue;
        return make_unique<Midi_ControlSurface>(integrator, page, config.surfaceName.c_str(), config.startChannel, surfaceFilePath.c_str(), mainZoneFolderPath.c_str(), vendorFxZoneFolderPath.c_str(), userFxZoneFolderPath.c_str(), io.get(), config.effectiveSettings, config.settingOverrides);
    }
    for (const auto& io : oscIo) {
        if (!IsSameString(deviceId, io->GetName())) continue;
        return make_unique<OSC_ControlSurface>(integrator, page, config.surfaceName.c_str(), config.startChannel, surfaceFilePath.c_str(), mainZoneFolderPath.c_str(), vendorFxZoneFolderPath.c_str(), userFxZoneFolderPath.c_str(), io.get(), config.effectiveSettings, config.settingOverrides);
    }
    errorMessage = "Surface '" + config.surfaceName + "' references unavailable Device '" + deviceId + "'";
    return nullptr;
}

static ControlSurface* FindConfiguredSurface(Page* page, const string& surfaceName) {
    for (const auto& surface : page->GetSurfaces()) if (surface->GetName() == surfaceName) return surface.get();
    return nullptr;
}

static bool ApplyListenerConfig(Page* page, const ListenerConfig& config, string& errorMessage) {
    ControlSurface* broadcaster = FindConfiguredSurface(page, config.broadcasterName);
    ControlSurface* listener = FindConfiguredSurface(page, config.listenerName);
    if (!broadcaster || !listener) {
        errorMessage = "Listener relationship '" + config.broadcasterName + "' -> '" + config.listenerName + "' requires both surfaces to load successfully";
        return false;
    }

    PropertyList properties;
    if (config.goHome) properties.set_prop(PropertyType_GoHome, "Yes");
    if (config.modifiers) properties.set_prop(PropertyType_Modifiers, "Yes");
    if (config.fxMenu) properties.set_prop(PropertyType_FXMenu, "Yes");
    if (config.selectedTrackFx) properties.set_prop(PropertyType_SelectedTrackFX, "Yes");
    if (config.selectedTrackSends) properties.set_prop(PropertyType_SelectedTrackSends, "Yes");
    if (config.selectedTrackReceives) properties.set_prop(PropertyType_SelectedTrackReceives, "Yes");
    broadcaster->GetZoneManager()->AddListener(listener);
    listener->GetZoneManager()->SetListenerCategories(properties);
    return true;
}

struct ConfigLoadSummary {
    int issueCount = 0;
    int loadedSurfaceCount = 0;
    int skippedSurfaceCount = 0;
};

static void CreateConfiguredIo(CSurfIntegrator* integrator, const IntegratorConfig& config, vector<unique_ptr<Midi_ControlSurfaceIO>>& midiIo, vector<unique_ptr<OSC_ControlSurfaceIO>>& oscIo, const string& configPath, ConfigLoadSummary& summary) {
    for (const MidiIoConfig& io : config.midiIo) {
        try {
            midiIo.push_back(make_unique<Midi_ControlSurfaceIO>(integrator, io.name.c_str(), io.channelCount, io.inputPort, io.outputPort, io.refreshRate, io.maxMessagesPerRun));
        } catch (const std::exception& error) {
            summary.issueCount++;
            LogToConsole("[ERROR] Skipping MIDI SurfaceType '%s' from %s at line %d: %s\n", io.name.c_str(), configPath.c_str(), io.lineNumber, error.what());
        }
    }
    for (const OscIoConfig& io : config.oscIo) {
        try {
            if (IsSameString(io.type, s_OSCSurfaceToken)) oscIo.push_back(make_unique<OSC_ControlSurfaceIO>(integrator, io.name.c_str(), io.channelCount, io.receiveOnPort.c_str(), io.transmitToPort.c_str(), io.transmitToIpAddress.c_str(), io.maxPacketsPerRun));
            else oscIo.push_back(make_unique<OSC_X32ControlSurfaceIO>(integrator, io.name.c_str(), io.channelCount, io.receiveOnPort.c_str(), io.transmitToPort.c_str(), io.transmitToIpAddress.c_str(), io.maxPacketsPerRun));
        } catch (const std::exception& error) {
            summary.issueCount++;
            LogToConsole("[ERROR] Skipping OSC SurfaceType '%s' from %s at line %d: %s\n", io.name.c_str(), configPath.c_str(), io.lineNumber, error.what());
        }
    }
}

static void CreateConfiguredPages(CSurfIntegrator* integrator, const IntegratorConfig& config, vector<unique_ptr<Page>>& pages) {
    for (const PageConfig& page : config.pages) pages.push_back(make_unique<Page>(integrator, page.name.c_str(), page.followsMcp, page.synchPages, page.scrollLink, page.scrollSynch));
    if (pages.empty()) pages.push_back(make_unique<Page>(integrator, "Home", false, false, false, false));
}

static void CreateConfiguredSurfaces(CSurfIntegrator* integrator, const IntegratorConfig& config, const ProductPaths& productPaths, const vector<unique_ptr<Midi_ControlSurfaceIO>>& midiIo, const vector<unique_ptr<OSC_ControlSurfaceIO>>& oscIo, vector<unique_ptr<Page>>& pages, const string& configPath, ConfigLoadSummary& summary) {
    for (size_t pageIdx = 0; pageIdx < config.pages.size(); pageIdx++) {
        Page* page = pages[pageIdx].get();
        for (const SurfaceAssignmentConfig& surfaceConfig : config.pages[pageIdx].surfaces) {
            string errorMessage;
            try {
                unique_ptr<ControlSurface> surface = CreateConfiguredSurface(integrator, page, surfaceConfig, productPaths, midiIo, oscIo, errorMessage);
                if (!surface) {
                    summary.issueCount++;
                    summary.skippedSurfaceCount++;
                    LogToConsole("[ERROR] Skipping Surface '%s' from %s at line %d: %s\n", surfaceConfig.surfaceName.c_str(), configPath.c_str(), surfaceConfig.lineNumber, errorMessage.c_str());
                    continue;
                }
                page->GetSurfaces().push_back(std::move(surface));
                summary.loadedSurfaceCount++;
            } catch (const std::exception& error) {
                summary.issueCount++;
                summary.skippedSurfaceCount++;
                LogToConsole("[ERROR] Skipping Surface '%s' from %s at line %d: %s\n", surfaceConfig.surfaceName.c_str(), configPath.c_str(), surfaceConfig.lineNumber, error.what());
            }
        }
    }
}

static void ApplyConfiguredListeners(const IntegratorConfig& config, vector<unique_ptr<Page>>& pages, const string& configPath, ConfigLoadSummary& summary) {
    for (size_t pageIdx = 0; pageIdx < config.pages.size(); pageIdx++) {
        for (const ListenerConfig& listenerConfig : config.pages[pageIdx].listeners) {
            string errorMessage;
            try {
                if (ApplyListenerConfig(pages[pageIdx].get(), listenerConfig, errorMessage)) continue;
                summary.issueCount++;
                LogToConsole("[ERROR] Skipping Listener from %s at line %d: %s\n", configPath.c_str(), listenerConfig.lineNumber, errorMessage.c_str());
            } catch (const std::exception& error) {
                summary.issueCount++;
                LogToConsole("[ERROR] Skipping Listener from %s at line %d: %s\n", configPath.c_str(), listenerConfig.lineNumber, error.what());
            }
        }
    }
}

static void InitializeConfiguredPages(vector<unique_ptr<Page>>& pages, ConfigLoadSummary& summary) {
    for (auto& page : pages) {
        for (auto& surface : page->GetSurfaces()) {
            try {
                surface->ForceClear();
                surface->OnInitialization();
            } catch (const std::exception& error) {
                summary.issueCount++;
                LogToConsole("[ERROR] Surface '%s' initialization failed on Page '%s': %s\n", surface->GetName(), page->GetName(), error.what());
            }
        }
    }
}

void CSurfIntegrator::ApplyProductRuntimeSettings() {
    const string debugLevel = this->productSettings_.GetString("DebugLevel");
    if (debugLevel == "Debug") g_debugLevel = DEBUG_LEVEL_DEBUG;
    else if (debugLevel == "Info") g_debugLevel = DEBUG_LEVEL_INFO;
    else if (debugLevel == "Notice") g_debugLevel = DEBUG_LEVEL_NOTICE;
    else if (debugLevel == "Warning") g_debugLevel = DEBUG_LEVEL_WARNING;
    else g_debugLevel = DEBUG_LEVEL_ERROR;
    g_surfaceRawInDisplay = this->productSettings_.GetBoolean("SurfaceRawInDisplay");
    g_surfaceInDisplay = this->productSettings_.GetBoolean("SurfaceInDisplay");
    g_surfaceOutDisplay = this->productSettings_.GetBoolean("SurfaceOutDisplay");
}

void CSurfIntegrator::Init() {
    ProductLog::Initialize();
    this->OpenNotificationsPanel();
    this->pages_.clear();
    this->midiSurfacesIO_.clear();
    this->oscSurfacesIO_.clear();
    const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
    const string productRootPath = productPaths.ProductRoot().string();
    if (!filesystem::is_directory(productRootPath)) {
        LogToConsole("[ERROR] Missing %s resource folder. Please check your installation, cannot find %s\n", ProductIdentity::DisplayName, productRootPath.c_str());
        return;
    }

    const string configPath = productPaths.ConfigFile().string();
    if (!filesystem::is_regular_file(configPath)) {
        LogToConsole("[ERROR] Missing %s. Please check your installation, cannot find %s\n", ProductIdentity::ConfigFilename, configPath.c_str());
        return;
    }

    IntegratorConfig config;
    try {
        config = ParseIntegratorConfig(configPath);
    } catch (const std::exception& error) {
        LogToConsole("[ERROR] FAILED to parse %s: %s\n", configPath.c_str(), error.what());
        return;
    }
    if (!config.fatalError.empty()) {
        LogToConsole("[ERROR] FAILED to initialize configuration: %s\n", config.fatalError.c_str());
        return;
    }

    this->productSettings_ = config.productSettings;
    this->productSettingOverrides_ = config.productSettingOverrides;
    this->ApplyProductRuntimeSettings();
    ConfigLoadSummary summary { static_cast<int>(config.issues.size()), 0, config.skippedSurfaceCount };
    for (const IntegratorConfigIssue& issue : config.issues) LogConfigIssue(configPath, issue);
    CreateConfiguredIo(this, config, this->midiSurfacesIO_, this->oscSurfacesIO_, configPath, summary);
    CreateConfiguredPages(this, config, this->pages_);
    CreateConfiguredSurfaces(this, config, productPaths, this->midiSurfacesIO_, this->oscSurfacesIO_, this->pages_, configPath, summary);
    ApplyConfiguredListeners(config, this->pages_, configPath, summary);
    InitializeConfiguredPages(this->pages_, summary);

    LogToConsole("[NOTICE] Configuration loaded: %d page(s), %d surface(s), %d skipped surface(s), %d issue(s)\n", static_cast<int>(this->pages_.size()), summary.loadedSurfaceCount, summary.skippedSurfaceCount, summary.issueCount);
    if (this->HasAnyOSKEnabled()) {
        this->PublishOSKSurfacesList();
        if (this->pages_.size() > this->currentPageIndex_ && this->pages_[this->currentPageIndex_]) {
            for (auto& surface : this->pages_[this->currentPageIndex_]->GetSurfaces()) {
                if (!surface->GetOskEnabled()) continue;
                surface->PublishOSKLayout();
                surface->PublishOSKLabels();
                surface->PublishOSKState();
            }
        }
        this->OpenOSKPanel();
    }
}
