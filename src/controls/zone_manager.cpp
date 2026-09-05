#include "integrator.h"
#include "format2_zone_runtime.h"
#include "zone_parser.h"

extern void WidgetMoved(ZoneManager* zoneManager, Widget* widget, int modifier);

static void collectFilesOfType(const string& type, const string& searchPath, vector<string>& results) {
    const size_t startingResultCount = results.size();
    filesystem::path zonePath { searchPath };
    if (filesystem::exists(searchPath) && filesystem::is_directory(searchPath))
        for (auto& file : filesystem::recursive_directory_iterator(searchPath))
            if (file.path().extension() == type)
                results.push_back(file.path().string());
    sort(results.begin() + startingResultCount, results.end());
}

static bool IsContainedZonePath(const filesystem::path& root, const filesystem::path& candidate) {
    const filesystem::path canonicalRoot = filesystem::weakly_canonical(filesystem::absolute(root));
    const filesystem::path canonicalCandidate = filesystem::weakly_canonical(filesystem::absolute(candidate));
    const filesystem::path relativePath = canonicalCandidate.lexically_relative(canonicalRoot);
    if (relativePath.empty() || relativePath.is_absolute()) return false;
    for (const filesystem::path& pathPart : relativePath) if (pathPart == "..") return false;
    return true;
}

static bool RemapZoneFolderPath(string& configuredPath, const filesystem::path& vendorProfileRoot, const filesystem::path& userProfileRoot) {
    const filesystem::path canonicalConfiguredPath = filesystem::weakly_canonical(filesystem::absolute(configuredPath));
    const filesystem::path canonicalVendorRoot = filesystem::weakly_canonical(filesystem::absolute(vendorProfileRoot));
    const filesystem::path relativePath = canonicalConfiguredPath.lexically_relative(canonicalVendorRoot);
    if (relativePath.empty() || relativePath.is_absolute()) return false;
    for (const filesystem::path& pathPart : relativePath) if (pathPart == "..") return false;
    configuredPath = (filesystem::weakly_canonical(filesystem::absolute(userProfileRoot)) / relativePath).string();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZoneManager
////////////////////////////////////////////////////////////////////////////////////////////////////////
ZoneManager::ZoneManager(CSurfIntegrator* const csi, ControlSurface* surface, const string& zoneFolder, const string& vendorFxZoneFolder, const string& userFxZoneFolder)
    : csi_(csi), surface_(surface), zoneFolder_(zoneFolder), vendorFxZoneFolder_(vendorFxZoneFolder), userFxZoneFolder_(userFxZoneFolder) {
}

Navigator* ZoneManager::GetNavigatorForTrack(MediaTrack* track) { return surface_->GetPage()->GetTrackNavigationManager()->GetNavigatorForTrack(track); }
Navigator* ZoneManager::GetMasterTrackNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetMasterTrackNavigator(); }
Navigator* ZoneManager::GetSelectedTrackNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetSelectedTrackNavigator(); }
Navigator* ZoneManager::GetFocusedFXNavigator() { return surface_->GetPage()->GetTrackNavigationManager()->GetFocusedFXNavigator(); }
int ZoneManager::GetNumChannels() { return surface_->GetNumChannels(); }

static void LogFormat2ZoneDiagnostic(const string& sourcePath, const Format2Diagnostic& diagnostic) {
    LogToConsole("[ERROR] %s:%d:%d: %s: %s\n", GetRelativePath(sourcePath.c_str()).c_str(), diagnostic.location.line, diagnostic.location.column, diagnostic.code.c_str(), diagnostic.message.c_str());
}

static void LogFormat2ProfileDiagnostic(const vector<Format2ZoneSource>& sources, const Format2ZoneProfileDiagnostic& diagnostic) {
    string sourcePath;
    if (!diagnostic.sourceIndices.empty() && diagnostic.sourceIndices.front() < sources.size()) sourcePath = sources[diagnostic.sourceIndices.front()].sourcePath;
    if (sourcePath.empty()) LogToConsole("[ERROR] %s: %s\n", diagnostic.code.c_str(), diagnostic.message.c_str());
    else LogToConsole("[ERROR] %s:%d:%d: %s: %s\n", GetRelativePath(sourcePath.c_str()).c_str(), diagnostic.location.line, diagnostic.location.column, diagnostic.code.c_str(), diagnostic.message.c_str());
}

static string FoldFormat2RuntimeId(const string& value) {
    string folded = value;
    for (char& character : folded) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    return folded;
}

static Navigator* GetFormat2ZoneBaseNavigator(ZoneManager* zoneManager, const Format2DocumentMetadata& metadata) {
    if (metadata.role == Format2ZoneRole::LastTouchedFxParam || metadata.target == Format2ZoneTarget::FocusedFx) return zoneManager->GetFocusedFXNavigator();
    if (metadata.target == Format2ZoneTarget::MasterTrack) return zoneManager->GetMasterTrackNavigator();
    return zoneManager->GetSelectedTrackNavigator();
}

static Format2DocumentMetadata GetFormat2EffectiveMetadata(const Format2DocumentMetadata& metadata) {
    Format2DocumentMetadata effective = metadata;
    if (metadata.role == Format2ZoneRole::Home) effective.target = Format2ZoneTarget::SelectedTrack;
    else if (metadata.role == Format2ZoneRole::LastTouchedFxParam) effective.target = Format2ZoneTarget::FocusedFx;
    effective.role.reset();
    return effective;
}

static ZoneRuntimeTarget GetFormat2ZoneRuntimeTarget(const Format2DocumentMetadata& metadata) {
    if (!metadata.target) return ZoneRuntimeTarget::Global;
    switch (*metadata.target) {
        case Format2ZoneTarget::Tracks: return ZoneRuntimeTarget::Tracks;
        case Format2ZoneTarget::SelectedTrack: return ZoneRuntimeTarget::SelectedTrack;
        case Format2ZoneTarget::MasterTrack: return ZoneRuntimeTarget::MasterTrack;
        case Format2ZoneTarget::FocusedFx: return ZoneRuntimeTarget::FocusedFx;
        case Format2ZoneTarget::Vca: return ZoneRuntimeTarget::Vca;
        case Format2ZoneTarget::Folder: return ZoneRuntimeTarget::Folder;
        case Format2ZoneTarget::SelectedTracks: return ZoneRuntimeTarget::SelectedTracks;
    }
    return ZoneRuntimeTarget::Global;
}

static ZoneRuntimeBankTarget GetFormat2ZoneRuntimeBankTarget(const Format2DocumentMetadata& metadata) {
    if (!metadata.bankTarget) return ZoneRuntimeBankTarget::None;
    if (*metadata.bankTarget == Format2BankTarget::Sends) return ZoneRuntimeBankTarget::Sends;
    if (*metadata.bankTarget == Format2BankTarget::Receives) return ZoneRuntimeBankTarget::Receives;
    return ZoneRuntimeBankTarget::Fx;
}

static unique_ptr<Zone> CreateFormat2RelatedZone(ZoneManager* zoneManager, const Format2ZoneProfileLoadResult& loaded, const map<string, size_t>& activeMainSources, size_t sourceIndex,
    Zone* parentZone, bool isLayer, const Format2DocumentMetadata& parentMetadata);

static bool AddFormat2ZoneRelations(ZoneManager* zoneManager, Zone* zone, size_t sourceIndex, const Format2ZoneProfileLoadResult& loaded, const map<string, size_t>& activeMainSources, const Format2DocumentMetadata& effectiveMetadata) {
    const Format2ZoneDocument& zoneDocument = loaded.documents[sourceIndex].parsed.zone;
    for (const Format2ZoneReference& reference : zoneDocument.includedZones) {
        const auto target = activeMainSources.find(FoldFormat2RuntimeId(reference.id));
        if (target == activeMainSources.end()) return false;
        unique_ptr<Zone> includedZone = CreateFormat2RelatedZone(zoneManager, loaded, activeMainSources, target->second, nullptr, false, effectiveMetadata);
        if (!includedZone) return false;
        zone->AddIncludedZone(std::move(includedZone));
    }
    for (const Format2ZoneReference& reference : zoneDocument.zoneLayers) {
        const auto target = activeMainSources.find(FoldFormat2RuntimeId(reference.id));
        if (target == activeMainSources.end()) return false;
        unique_ptr<Zone> zoneLayer = CreateFormat2RelatedZone(zoneManager, loaded, activeMainSources, target->second, zone, true, effectiveMetadata);
        if (!zoneLayer) return false;
        zone->AddZoneLayer(std::move(zoneLayer));
    }
    return true;
}

static unique_ptr<Zone> CreateFormat2RelatedZone(ZoneManager* zoneManager, const Format2ZoneProfileLoadResult& loaded, const map<string, size_t>& activeMainSources, size_t sourceIndex,
    Zone* parentZone, bool isLayer, const Format2DocumentMetadata& parentMetadata) {
    const Format2LoadedZoneDocument& document = loaded.documents[sourceIndex];
    const Format2DocumentMetadata& metadata = document.parsed.document.metadata;
    const Format2DocumentMetadata effectiveMetadata = isLayer ? parentMetadata : GetFormat2EffectiveMetadata(metadata);
    Navigator* navigator = isLayer && parentZone ? parentZone->GetNavigator() : GetFormat2ZoneBaseNavigator(zoneManager, metadata);
    const int slotIndex = isLayer && parentZone ? parentZone->GetSlotIndex() : 0;
    const string& runtimeName = document.parsed.zone.id;
    const string alias = metadata.alias ? *metadata.alias : runtimeName;
    unique_ptr<Zone> zone = make_unique<Zone>(zoneManager->GetCSI(), zoneManager, navigator, slotIndex, runtimeName, alias, document.parsed.document.lexical.sourcePath);
    const bool deactivatesOnTrackLoss = isLayer && parentZone ? parentZone->DeactivatesOnTrackLoss() : metadata.target == Format2ZoneTarget::SelectedTrack;
    zone->ConfigureFormat2Runtime(GetFormat2ZoneRuntimeTarget(effectiveMetadata), GetFormat2ZoneRuntimeBankTarget(effectiveMetadata), deactivatesOnTrackLoss, isLayer ? parentZone : nullptr);
    const Format2ZoneRuntimeResult runtimeResult = LoadFormat2ZoneRuntimeBindings(zoneManager, zone.get(), document.parsed, isLayer ? &effectiveMetadata : nullptr);
    if (!runtimeResult.IsValid()) {
        for (const Format2Diagnostic& diagnostic : runtimeResult.diagnostics) LogFormat2ZoneDiagnostic(document.parsed.document.lexical.sourcePath, diagnostic);
        return nullptr;
    }
    if (!AddFormat2ZoneRelations(zoneManager, zone.get(), sourceIndex, loaded, activeMainSources, effectiveMetadata)) return nullptr;
    return zone;
}

ZoneManager::Format2InitializationState ZoneManager::InitializeFormat2() {
    const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
    const optional<string> userMainProfileId = productPaths.UserZoneProfileIdForPath(this->zoneFolder_);
    const optional<string> vendorMainProfileId = productPaths.VendorZoneProfileIdForPath(this->zoneFolder_);
    const optional<string> mainProfileId = userMainProfileId ? userMainProfileId : vendorMainProfileId;
    vector<Format2ZoneProfileRoot> roots;
    if (mainProfileId) {
        roots.push_back({productPaths.MainZones(ZoneSource::Vendor, *mainProfileId), Format2ZoneCollection::Main, Format2ZoneSourceLayer::Vendor});
        roots.push_back({productPaths.MainZones(ZoneSource::User, *mainProfileId), Format2ZoneCollection::Main, Format2ZoneSourceLayer::User});
    } else {
        roots.push_back({this->zoneFolder_, Format2ZoneCollection::Main, userMainProfileId ? Format2ZoneSourceLayer::User : Format2ZoneSourceLayer::Vendor});
    }
    roots.push_back({this->vendorFxZoneFolder_, Format2ZoneCollection::Fx, Format2ZoneSourceLayer::Vendor});
    roots.push_back({this->userFxZoneFolder_, Format2ZoneCollection::Fx, Format2ZoneSourceLayer::User});

    Format2ZoneProfileLoadResult loaded = LoadFormat2ZoneProfile(mainProfileId ? *mainProfileId : this->surface_->GetName(), roots);
    if (!loaded.UsesFormat2()) return Format2InitializationState::NotUsed;
    if (!loaded.ContainsOnlyFormat2()) {
        LogToConsole("[ERROR] Zone profile '%s' mixes legacy and format 2 .zon files. Convert the complete profile before loading it.\n", loaded.profile.profileId.c_str());
        for (const Format2LoadedZoneDocument& document : loaded.documents) if (document.parsed.document.metadata.version != 2) LogToConsole("[ERROR] Legacy Zone file in format 2 profile: %s\n", GetRelativePath(document.parsed.document.lexical.sourcePath.c_str()).c_str());
        return Format2InitializationState::Failed;
    }

    for (const Format2LoadedZoneDocument& document : loaded.documents) for (const Format2Diagnostic& diagnostic : document.parsed.document.lexical.diagnostics) LogFormat2ZoneDiagnostic(document.parsed.document.lexical.sourcePath, diagnostic);
    for (const Format2ZoneProfileDiagnostic& diagnostic : loaded.profile.diagnostics) LogFormat2ProfileDiagnostic(loaded.sources, diagnostic);
    if (!loaded.IsValid()) return Format2InitializationState::Failed;

    map<string, size_t> fxSourceByMatch;
    for (const Format2ActiveZoneSource& activeZone : loaded.profile.activeZones) {
        if (activeZone.collection != Format2ZoneCollection::Fx || !activeZone.available || !activeZone.activeSourceIndex) continue;
        const size_t sourceIndex = *activeZone.activeSourceIndex;
        const optional<string>& matchFx = loaded.documents[sourceIndex].parsed.document.metadata.matchFx;
        if (!matchFx) continue;
        const string canonicalMatch = FoldFormat2RuntimeId(*matchFx);
        const auto existing = fxSourceByMatch.find(canonicalMatch);
        if (existing == fxSourceByMatch.end()) {
            fxSourceByMatch[canonicalMatch] = sourceIndex;
            continue;
        }
        LogToConsole("[ERROR] FX zones '%s' and '%s' use the same MatchFX value '%s'. Both are unavailable.\n", GetRelativePath(loaded.sources[existing->second].sourcePath.c_str()).c_str(), GetRelativePath(loaded.sources[sourceIndex].sourcePath.c_str()).c_str(), matchFx->c_str());
        loaded.sources[existing->second].valid = false;
        loaded.sources[sourceIndex].valid = false;
    }

    map<size_t, unique_ptr<Zone>> preparedMainZones;
    for (const Format2ActiveZoneSource& activeZone : loaded.profile.activeZones) {
        if (!activeZone.available || !activeZone.activeSourceIndex) continue;
        const size_t sourceIndex = *activeZone.activeSourceIndex;
        const Format2LoadedZoneDocument& document = loaded.documents[sourceIndex];
        const Format2ZoneDocument& zoneDocument = document.parsed.zone;
        const Format2DocumentMetadata& metadata = document.parsed.document.metadata;
        const Format2DocumentMetadata effectiveMetadata = GetFormat2EffectiveMetadata(metadata);
        const string runtimeName = activeZone.collection == Format2ZoneCollection::Fx && metadata.matchFx ? *metadata.matchFx : zoneDocument.id;
        const string alias = metadata.alias ? *metadata.alias : runtimeName;
        unique_ptr<Zone> zone = make_unique<Zone>(this->csi_, this, GetFormat2ZoneBaseNavigator(this, metadata), 0, runtimeName, alias, document.parsed.document.lexical.sourcePath);
        zone->ConfigureFormat2Runtime(GetFormat2ZoneRuntimeTarget(effectiveMetadata), GetFormat2ZoneRuntimeBankTarget(effectiveMetadata), metadata.target == Format2ZoneTarget::SelectedTrack);
        const Format2ZoneRuntimeResult runtimeResult = LoadFormat2ZoneRuntimeBindings(this, zone.get(), document.parsed);
        if (!runtimeResult.IsValid()) {
            loaded.sources[sourceIndex].valid = false;
            for (const Format2Diagnostic& diagnostic : runtimeResult.diagnostics) LogFormat2ZoneDiagnostic(document.parsed.document.lexical.sourcePath, diagnostic);
            continue;
        }
        if (activeZone.collection == Format2ZoneCollection::Main) preparedMainZones[sourceIndex] = std::move(zone);
    }

    loaded.profile = ResolveFormat2ZoneProfile(loaded.profile.profileId, loaded.sources);
    for (const Format2ZoneProfileDiagnostic& diagnostic : loaded.profile.diagnostics) LogFormat2ProfileDiagnostic(loaded.sources, diagnostic);
    if (!loaded.IsValid()) return Format2InitializationState::Failed;

    map<string, size_t> activeMainSources;
    for (const Format2ActiveZoneSource& activeZone : loaded.profile.activeZones) {
        if (activeZone.collection == Format2ZoneCollection::Main && activeZone.available && activeZone.activeSourceIndex) activeMainSources[activeZone.canonicalId] = *activeZone.activeSourceIndex;
    }
    for (auto& prepared : preparedMainZones) {
        const Format2DocumentMetadata& metadata = loaded.documents[prepared.first].parsed.document.metadata;
        if (metadata.role == Format2ZoneRole::Layer) continue;
        if (!AddFormat2ZoneRelations(this, prepared.second.get(), prepared.first, loaded, activeMainSources, GetFormat2EffectiveMetadata(metadata))) {
            LogToConsole("[ERROR] Failed to create runtime relations for format 2 Zone '%s'.\n", loaded.documents[prepared.first].parsed.zone.id.c_str());
            return Format2InitializationState::Failed;
        }
    }

    this->format2DocumentIndexByPath_.clear();
    for (const Format2ActiveZoneSource& activeZone : loaded.profile.activeZones) {
        if (!activeZone.available || !activeZone.activeSourceIndex) continue;
        const size_t sourceIndex = *activeZone.activeSourceIndex;
        const Format2LoadedZoneDocument& document = loaded.documents[sourceIndex];
        const Format2DocumentMetadata& metadata = document.parsed.document.metadata;
        const string runtimeName = activeZone.collection == Format2ZoneCollection::Fx && metadata.matchFx ? *metadata.matchFx : document.parsed.zone.id;
        ZoneInfo info;
        info.filePath = document.parsed.document.lexical.sourcePath;
        info.isFxZone = activeZone.collection == Format2ZoneCollection::Fx;
        info.isUserZone = document.layer == Format2ZoneSourceLayer::User;
        info.alias = metadata.alias ? *metadata.alias : runtimeName;
        info.isLoaded = activeZone.collection == Format2ZoneCollection::Main;
        info.isReferenced = metadata.role == Format2ZoneRole::Home;
        this->AddZoneFilePath(runtimeName, info);
        this->format2DocumentIndexByPath_[info.filePath] = sourceIndex;

        if (activeZone.collection != Format2ZoneCollection::Main) continue;
        auto prepared = preparedMainZones.find(sourceIndex);
        if (prepared == preparedMainZones.end()) continue;
        if (metadata.role == Format2ZoneRole::Home) this->homeZone_ = std::move(prepared->second);
        else if (metadata.role == Format2ZoneRole::LastTouchedFxParam) this->lastTouchedFXParamZone_ = shared_ptr<Zone>(std::move(prepared->second));
        else if (metadata.role == Format2ZoneRole::Layer) continue;
        else this->goZones_.push_back(std::move(prepared->second));
    }
    if (!this->homeZone_) {
        LogToConsole("[ERROR] Format 2 Zone profile '%s' did not produce a runnable Home zone.\n", loaded.profile.profileId.c_str());
        return Format2InitializationState::Failed;
    }
    this->format2ZoneProfile_ = make_unique<Format2ZoneProfileLoadResult>(std::move(loaded));
    this->homeZone_->Activate();
    return Format2InitializationState::Initialized;
}

void ZoneManager::Initialize() {
    const Format2InitializationState state = this->InitializeFormat2();
    if (state != Format2InitializationState::NotUsed) return;
    this->InitializeLegacy();
}

void ZoneManager::InitializeLegacy() {
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

void ZoneManager::ReloadFromDisk() {
    this->learnFocusedFXZone_.reset();
    this->lastTouchedFXParamZone_.reset();
    this->focusedFXZone_.reset();
    this->selectedTrackFXZones_.clear();
    this->fxSlotZone_.reset();
    this->homeZone_.reset();
    this->goZones_.clear();
    this->zonesToBeDeleted_.clear();
    this->zoneInfo_.clear();
    this->format2DocumentIndexByPath_.clear();
    this->format2ZoneProfile_.reset();
    this->Initialize();
}

void ZoneManager::ReplaceZoneProfileRoot(const filesystem::path& vendorProfileRoot, const filesystem::path& userProfileRoot) {
    RemapZoneFolderPath(this->zoneFolder_, vendorProfileRoot, userProfileRoot);
}

bool ZoneManager::PrepareZonePathForWrite(const string& sourcePath, string& editablePath, bool& activatedUserProfile, string& errorMessage) {
    editablePath = sourcePath;
    activatedUserProfile = false;
    errorMessage.clear();

    try {
        const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
        const std::optional<string> vendorProfileId = productPaths.VendorZoneProfileIdForPath(sourcePath);
        if (!vendorProfileId) return true;

        const filesystem::path vendorFxRoot = productPaths.FxZones(ZoneSource::Vendor, *vendorProfileId);
        if (IsContainedZonePath(vendorFxRoot, sourcePath)) {
            if (!filesystem::is_regular_file(sourcePath)) {
                errorMessage = "Only individual vendor FX zone files can be copied for editing";
                return false;
            }
            const string prompt = "FX zone '" + filesystem::path(sourcePath).filename().string() + "' is provided by the vendor and is read-only. Create an editable user override?";
            if (MessageBox(g_hwnd, prompt.c_str(), ProductIdentity::DisplayName, MB_YESNO) != IDYES) {
                errorMessage = "Operation cancelled. Vendor FX zone was not changed";
                return false;
            }
            editablePath = productPaths.CopyVendorFxZoneToUser(*vendorProfileId, sourcePath).string();
            activatedUserProfile = true;
            return true;
        }

        const filesystem::path vendorMainRoot = productPaths.MainZones(ZoneSource::Vendor, *vendorProfileId);
        if (!IsContainedZonePath(vendorMainRoot, sourcePath)) {
            errorMessage = "Vendor zone path is outside the Main and FX folders for profile '" + *vendorProfileId + "'";
            return false;
        }
        if (!filesystem::is_directory(productPaths.MainZones(ZoneSource::User, *vendorProfileId))) {
            const string prompt = "Main zone configuration '" + *vendorProfileId + "' is provided by the vendor and is read-only. Create an editable user copy?";
            if (MessageBox(g_hwnd, prompt.c_str(), ProductIdentity::DisplayName, MB_YESNO) != IDYES) {
                errorMessage = "Operation cancelled. Vendor Main zones were not changed";
                return false;
            }
            productPaths.CloneVendorMainZonesToUser(*vendorProfileId);
        }
        editablePath = productPaths.UserZonePathForVendorPath(*vendorProfileId, sourcePath).string();
        if (!filesystem::exists(editablePath)) {
            errorMessage = "The matching file or folder does not exist in the User Main zone copy";
            return false;
        }
        this->ReplaceZoneProfileRoot(vendorMainRoot, productPaths.MainZones(ZoneSource::User, *vendorProfileId));
        activatedUserProfile = true;
        return true;
    } catch (const std::exception& error) {
        errorMessage = string("Unable to prepare editable zone profile: ") + error.what();
        return false;
    }
}

void ZoneManager::PreProcessZoneFile(const string& filePath, bool isFxZone, bool isUserZone) {
    try {
        ifstream file(filePath);

        ZoneInfo info;
        info.filePath = filePath;
        info.isFxZone = isFxZone;
        info.isUserZone = isUserZone;

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] PreProcessZoneFile: %s\n", GetRelativePath(filePath.c_str()).c_str());

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
        LogToConsole("[ERROR] Exception: %s\n", e.what());
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
        LogToConsole("[WARNING] No .zon files found for zones: %s\n", missingZoneNames.c_str());
}

void ZoneManager::LoadZoneFile(Zone* zone, const char* widgetSuffix) {
    LoadZoneFile(zone, zone->GetSourceFilePath(), widgetSuffix);
}

void ZoneManager::LoadZoneFile(Zone* zone, const char* filePath, const char* widgetSuffix) {
    const auto format2Document = this->format2DocumentIndexByPath_.find(filePath);
    if (this->format2ZoneProfile_ && format2Document != this->format2DocumentIndexByPath_.end() && format2Document->second < this->format2ZoneProfile_->documents.size()) {
        const Format2LoadedZoneDocument& document = this->format2ZoneProfile_->documents[format2Document->second];
        const Format2ZoneRuntimeResult result = LoadFormat2ZoneRuntimeBindings(this, zone, document.parsed);
        for (const Format2Diagnostic& diagnostic : result.diagnostics) LogFormat2ZoneDiagnostic(document.parsed.document.lexical.sourcePath, diagnostic);
        return;
    }
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
            if (this->format2ZoneProfile_) focusedFXZone_->ConfigureFormat2Runtime(ZoneRuntimeTarget::FocusedFx);
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
                if (this->format2ZoneProfile_) zone->ConfigureFormat2Runtime(ZoneRuntimeTarget::SelectedTrack, ZoneRuntimeBankTarget::None, true);
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
        if (this->format2ZoneProfile_) fxSlotZone_->ConfigureFormat2Runtime(ZoneRuntimeTarget::Global);
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
    if (this->zoneFolder_.empty())
        return LogToConsole("[ERROR] Please check %s. Cannot find the Zone folder for %s under %s", ProductIdentity::ConfigFilename, this->GetSurface()->GetName(), ProductPaths::FromReaperResourcePath().ZonesRoot().string().c_str());

    vector<string> mainZoneFiles;
    collectFilesOfType(".zon", this->zoneFolder_, mainZoneFiles);

    if (mainZoneFiles.empty())
        return LogToConsole("[ERROR] Cannot find Zone files for %s in: %s", this->GetSurface()->GetName(), this->zoneFolder_.c_str());

    const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
    const bool mainZonesAreUserZones = productPaths.UserZoneProfileIdForPath(this->zoneFolder_).has_value();
    for (const string& zoneFile : mainZoneFiles) this->PreProcessZoneFile(zoneFile, false, mainZonesAreUserZones);

    vector<string> vendorFxZoneFiles;
    collectFilesOfType(".zon", this->vendorFxZoneFolder_, vendorFxZoneFiles);
    for (const string& zoneFile : vendorFxZoneFiles) this->PreProcessZoneFile(zoneFile, true, false);

    vector<string> userFxZoneFiles;
    collectFilesOfType(".zon", this->userFxZoneFolder_, userFxZoneFiles);
    for (const string& zoneFile : userFxZoneFiles) this->PreProcessZoneFile(zoneFile, true, true);
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
