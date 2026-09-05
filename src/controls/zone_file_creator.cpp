#include "zone_file_creator.h"

#include "format2_zone_profile_loader.h"
#include "zone_manager.h"

#include <chrono>

string ZoneFileCreator::LowerAscii(const string& value) {
    string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) { return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) : static_cast<char>(character); });
    return result;
}

bool ZoneFileCreator::IsContainedPath(const filesystem::path& root, const filesystem::path& candidate) {
    const filesystem::path canonicalRoot = filesystem::weakly_canonical(filesystem::absolute(root));
    const filesystem::path canonicalCandidate = filesystem::weakly_canonical(filesystem::absolute(candidate));
    auto rootPart = canonicalRoot.begin();
    auto candidatePart = canonicalCandidate.begin();
    while (rootPart != canonicalRoot.end() && candidatePart != canonicalCandidate.end()) {
        if (*rootPart != *candidatePart) return false;
        ++rootPart;
        ++candidatePart;
    }
    return rootPart == canonicalRoot.end();
}

bool ZoneFileCreator::IsSafeZoneName(const string& value) {
    if (value.empty() || value.size() > 128) return false;
    const unsigned char firstCharacter = static_cast<unsigned char>(value.front());
    const bool firstIsLetter = (firstCharacter >= 'A' && firstCharacter <= 'Z') || (firstCharacter >= 'a' && firstCharacter <= 'z');
    if (!firstIsLetter && !(firstCharacter >= '0' && firstCharacter <= '9')) return false;
    for (const unsigned char character : value) {
        const bool isLetter = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        const bool isDigit = character >= '0' && character <= '9';
        if (!isLetter && !isDigit && character != '_' && character != '-') return false;
    }
    return true;
}

bool ZoneFileCreator::IsSafeText(const string& value, size_t maximumLength) {
    if (value.size() > maximumLength || value.find_first_of("\r\n\"|") != string::npos) return false;
    for (const unsigned char character : value) if (character < 32 && character != '\t') return false;
    return true;
}

bool ZoneFileCreator::ResolveMetadata(const ZoneFileCreateRequest& request, bool& isFx, string& metadata, string& errorMessage) {
    isFx = request.documentType == "fx";
    if (request.documentType != "main" && !isFx) {
        errorMessage = "Type must be Main zone or FX zone";
        return false;
    }
    if (isFx) {
        if (request.matchFx.empty()) {
            errorMessage = "FX zone requires a plugin name to match";
            return false;
        }
        if (!ZoneFileCreator::IsSafeText(request.matchFx, 300)) {
            errorMessage = "Plugin name must not contain quotes, |, line breaks, or control characters";
            return false;
        }
        metadata = "MatchFX=\"" + request.matchFx + "\"";
        return true;
    }

    static const map<string, string> metadataByPurpose = {
        {"home", "Role=Home"},
        {"last-touched-fx-param", "Role=LastTouchedFXParam"},
        {"layer", "Role=Layer"},
        {"tracks", "Target=Tracks"},
        {"selected-track", "Target=SelectedTrack"},
        {"master-track", "Target=MasterTrack"},
        {"focused-fx", "Target=FocusedFX"},
        {"tracks-sends", "Target=Tracks BankTarget=Sends"},
        {"tracks-receives", "Target=Tracks BankTarget=Receives"},
        {"tracks-fx", "Target=Tracks BankTarget=FX"},
        {"selected-track-sends", "Target=SelectedTrack BankTarget=Sends"},
        {"selected-track-receives", "Target=SelectedTrack BankTarget=Receives"},
        {"selected-track-fx", "Target=SelectedTrack BankTarget=FX"},
        {"master-track-fx", "Target=MasterTrack BankTarget=FX"},
        {"vca", "Target=VCA"},
        {"folder", "Target=Folder"},
        {"selected-tracks", "Target=SelectedTracks"},
    };
    const auto metadataEntry = metadataByPurpose.find(request.purpose);
    if (metadataEntry == metadataByPurpose.end()) {
        errorMessage = "Choose what the Main zone controls";
        return false;
    }
    metadata = metadataEntry->second;
    return true;
}

bool ZoneFileCreator::HasCaseInsensitiveFile(const filesystem::path& directory, const string& fileName) {
    if (!filesystem::is_directory(directory)) return false;
    const string lowercaseFileName = ZoneFileCreator::LowerAscii(fileName);
    for (const filesystem::directory_entry& entry : filesystem::directory_iterator(directory)) if (ZoneFileCreator::LowerAscii(entry.path().filename().string()) == lowercaseFileName) return true;
    return false;
}

bool ZoneFileCreator::FindCaseInsensitiveZoneId(const filesystem::path& collectionRoot, const string& zoneName, filesystem::path& duplicatePath) {
    if (!filesystem::is_directory(collectionRoot)) return false;
    const string lowercaseZoneName = ZoneFileCreator::LowerAscii(zoneName);
    for (const filesystem::directory_entry& entry : filesystem::recursive_directory_iterator(collectionRoot, filesystem::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file() || ZoneFileCreator::LowerAscii(entry.path().extension().string()) != ".zon") continue;
        if (ZoneFileCreator::LowerAscii(entry.path().stem().string()) == lowercaseZoneName) {
            duplicatePath = entry.path();
            return true;
        }
    }
    return false;
}

bool ZoneFileCreator::ValidateCandidate(const string& profileId, const filesystem::path& targetPath, bool isFx, const string& source, string& errorMessage) {
    const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
    const vector<Format2ZoneProfileRoot> roots = {
        {productPaths.MainZones(ZoneSource::Vendor, profileId), Format2ZoneCollection::Main, Format2ZoneSourceLayer::Vendor},
        {productPaths.MainZones(ZoneSource::User, profileId), Format2ZoneCollection::Main, Format2ZoneSourceLayer::User},
        {productPaths.FxZones(ZoneSource::Vendor, profileId), Format2ZoneCollection::Fx, Format2ZoneSourceLayer::Vendor},
        {productPaths.FxZones(ZoneSource::User, profileId), Format2ZoneCollection::Fx, Format2ZoneSourceLayer::User},
    };
    Format2ZoneProfileLoadResult loaded = LoadFormat2ZoneProfile(profileId, roots);
    if (!loaded.ContainsOnlyFormat2() || !loaded.IsValid()) {
        errorMessage = "The active zone profile must be fully converted to format 2 before OSK can create a zone";
        return false;
    }

    const Format2DocumentKind kind = isFx ? Format2DocumentKind::FxZone : Format2DocumentKind::MainZone;
    Format2ZoneParseResult candidate = ParseFormat2ZoneDocumentSource(source, targetPath.string(), kind);
    if (!candidate.IsValid()) {
        errorMessage = candidate.document.lexical.diagnostics.empty() ? "Generated zone is invalid" : candidate.document.lexical.diagnostics.front().message;
        return false;
    }
    if (isFx && candidate.document.metadata.matchFx) {
        const string candidateMatch = ZoneFileCreator::LowerAscii(*candidate.document.metadata.matchFx);
        for (const Format2ActiveZoneSource& activeZone : loaded.profile.activeZones) {
            if (activeZone.collection != Format2ZoneCollection::Fx || !activeZone.available || !activeZone.activeSourceIndex) continue;
            const Format2LoadedZoneDocument& document = loaded.documents[*activeZone.activeSourceIndex];
            if (!document.parsed.document.metadata.matchFx) continue;
            if (ZoneFileCreator::LowerAscii(*document.parsed.document.metadata.matchFx) == candidateMatch) {
                errorMessage = "Another active FX zone already matches this plugin: " + document.parsed.document.lexical.sourcePath;
                return false;
            }
        }
    }
    loaded.sources.push_back(MakeFormat2ZoneSource(isFx ? Format2ZoneCollection::Fx : Format2ZoneCollection::Main, Format2ZoneSourceLayer::User, candidate));
    const Format2ZoneProfileResolveResult resolved = ResolveFormat2ZoneProfile(profileId, loaded.sources);
    if (!resolved.IsValid()) {
        errorMessage = resolved.diagnostics.front().message;
        return false;
    }
    return true;
}

bool ZoneFileCreator::WriteCompletedTemporaryFile(const filesystem::path& targetPath, const string& source, string& errorMessage) {
    const auto uniqueValue = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const filesystem::path temporaryPath = targetPath.parent_path() / ("." + targetPath.filename().string() + ".tmp." + std::to_string(uniqueValue));
    std::error_code fileError;
    {
        ofstream outputFile(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!outputFile.is_open()) {
            errorMessage = "Unable to open temporary zone file";
            return false;
        }
        outputFile << source;
        outputFile.flush();
        if (!outputFile.good()) {
            errorMessage = "Unable to write complete temporary zone file";
            outputFile.close();
            filesystem::remove(temporaryPath, fileError);
            return false;
        }
        outputFile.close();
        if (!outputFile.good()) {
            errorMessage = "Unable to close temporary zone file";
            filesystem::remove(temporaryPath, fileError);
            return false;
        }
    }
    if (ZoneFileCreator::HasCaseInsensitiveFile(targetPath.parent_path(), targetPath.filename().string())) {
        errorMessage = "A zone file with this name already exists";
        filesystem::remove(temporaryPath, fileError);
        return false;
    }
    filesystem::rename(temporaryPath, targetPath, fileError);
    if (!fileError) return true;
    errorMessage = "Unable to install completed zone file: " + fileError.message();
    filesystem::remove(temporaryPath, fileError);
    return false;
}

ZoneFileCreateResult ZoneFileCreator::Create(ZoneManager* zoneManager, const ZoneFileCreateRequest& request) {
    ZoneFileCreateResult result;
    if (!zoneManager) {
        result.message = "ZoneManager unavailable";
        return result;
    }
    if (!ZoneFileCreator::IsSafeZoneName(request.zoneName)) {
        result.message = "Zone name must start with an ASCII letter or digit and contain only ASCII letters, digits, '_' or '-'";
        return result;
    }
    if (!ZoneFileCreator::IsSafeText(request.alias, 200)) {
        result.message = "Alias must not contain quotes, |, line breaks, or control characters";
        return result;
    }
    if (!zoneManager->UsesFormat2ZoneProfile()) {
        result.message = "Convert this zone profile to format 2 before creating zones from OSK";
        return result;
    }

    bool isFx = false;
    string metadata;
    if (!ZoneFileCreator::ResolveMetadata(request, isFx, metadata, result.message)) return result;

    const string configuredBase = isFx ? zoneManager->GetFXZoneFolder() : zoneManager->GetZoneFolder();
    if (configuredBase.empty()) {
        result.message = isFx ? "FX zone folder is not configured" : "Main zone folder is not configured";
        return result;
    }

    try {
        const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
        const std::optional<string> userProfileId = productPaths.UserZoneProfileIdForPath(configuredBase);
        const std::optional<string> vendorProfileId = productPaths.VendorZoneProfileIdForPath(configuredBase);
        const std::optional<string> profileId = userProfileId ? userProfileId : vendorProfileId;
        if (!profileId) {
            result.message = "Configured zone folder is not inside a known zone profile";
            return result;
        }
        const filesystem::path profileRoot = productPaths.ZoneProfileDirectory(ZoneSource::User, *profileId);
        const filesystem::path expectedBase = isFx ? productPaths.FxZones(ZoneSource::User, *profileId) : productPaths.MainZones(ZoneSource::User, *profileId);

        filesystem::path duplicatePath;
        const filesystem::path vendorCollection = isFx ? productPaths.FxZones(ZoneSource::Vendor, *profileId) : productPaths.MainZones(ZoneSource::Vendor, *profileId);
        const filesystem::path userCollection = isFx ? productPaths.FxZones(ZoneSource::User, *profileId) : productPaths.MainZones(ZoneSource::User, *profileId);
        if (ZoneFileCreator::FindCaseInsensitiveZoneId(userCollection, request.zoneName, duplicatePath) || ZoneFileCreator::FindCaseInsensitiveZoneId(vendorCollection, request.zoneName, duplicatePath)) {
            result.message = "Zone ID already exists: " + duplicatePath.string();
            return result;
        }

        const filesystem::path destinationDirectory = expectedBase;
        const filesystem::path targetPath = destinationDirectory / (request.zoneName + ".zon");
        if (!ZoneFileCreator::IsContainedPath(profileRoot, targetPath) || !ZoneFileCreator::IsContainedPath(destinationDirectory, targetPath)) {
            result.message = "Zone destination escapes the user profile";
            return result;
        }
        string source = "@Meta { Version=2 " + metadata;
        if (!request.alias.empty()) source += " Alias=\"" + request.alias + "\"";
        source += " }\n";
        if (!ZoneFileCreator::ValidateCandidate(*profileId, targetPath, isFx, source, result.message)) return result;

        filesystem::create_directories(destinationDirectory);
        string writeError;
        if (!ZoneFileCreator::WriteCompletedTemporaryFile(targetPath, source, writeError)) {
            result.message = writeError;
            return result;
        }
        result.success = true;
        result.path = targetPath.string();
        result.message = "Format 2 zone created";
        return result;
    } catch (const std::exception& error) {
        result.message = string("Unable to create zone: ") + error.what();
        return result;
    }
}
