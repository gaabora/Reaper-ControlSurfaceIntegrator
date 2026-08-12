#include "zone_file_creator.h"

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

bool ZoneFileCreator::IsSafeAlias(const string& value) {
    if (value.size() > 200 || value.find_first_of("\r\n\"|") != string::npos) return false;
    for (const unsigned char character : value) if (character < 32 && character != '\t') return false;
    return true;
}

bool ZoneFileCreator::IsSupportedNavigator(const string& value) {
    return value.empty() || value == "TrackNavigator" || value == "MasterTrackNavigator" || value == "SelectedTrackNavigator" || value == "FocusedFXNavigator" || value == "VCANavigator" || value == "FolderNavigator";
}

bool ZoneFileCreator::ResolveRelativeDirectory(const string& scaffoldType, filesystem::path& relativeDirectory, bool& isFx) {
    isFx = false;
    if (scaffoldType == "main") relativeDirectory.clear();
    else if (scaffoldType == "home") relativeDirectory = "HomeZones";
    else if (scaffoldType == "go") relativeDirectory = "GoZones";
    else if (scaffoldType == "subzone") relativeDirectory = "SubZones";
    else if (scaffoldType == "included") relativeDirectory = "IncludedZones";
    else if (scaffoldType == "learn") relativeDirectory = "LearnZones";
    else if (scaffoldType == "fx") {
        relativeDirectory.clear();
        isFx = true;
    } else return false;
    return true;
}

bool ZoneFileCreator::HasCaseInsensitiveFile(const filesystem::path& directory, const string& fileName) {
    if (!filesystem::is_directory(directory)) return false;
    const string lowercaseFileName = ZoneFileCreator::LowerAscii(fileName);
    for (const filesystem::directory_entry& entry : filesystem::directory_iterator(directory)) if (ZoneFileCreator::LowerAscii(entry.path().filename().string()) == lowercaseFileName) return true;
    return false;
}

bool ZoneFileCreator::FindDuplicateZoneName(const filesystem::path& profileRoot, const string& zoneName, filesystem::path& duplicatePath) {
    if (!filesystem::is_directory(profileRoot)) return false;
    const string lowercaseZoneName = ZoneFileCreator::LowerAscii(zoneName);
    for (const filesystem::directory_entry& entry : filesystem::recursive_directory_iterator(profileRoot, filesystem::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file() || ZoneFileCreator::LowerAscii(entry.path().extension().string()) != ".zon") continue;
        ifstream inputFile(entry.path());
        for (string line; getline(inputFile, line);) {
            TrimLine(line);
            if (line.rfind("Zone", 0) != 0) continue;
            vector<string> tokens;
            GetTokens(tokens, line);
            if (tokens.size() >= 2 && tokens[0] == "Zone" && ZoneFileCreator::LowerAscii(tokens[1]) == lowercaseZoneName) {
                duplicatePath = entry.path();
                return true;
            }
            if (!tokens.empty() && tokens[0] == "Zone") break;
        }
    }
    return false;
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
    if (!ZoneFileCreator::IsSafeAlias(request.alias)) {
        result.message = "Alias must not contain quotes, line breaks, or control characters";
        return result;
    }
    if (!ZoneFileCreator::IsSupportedNavigator(request.navigator)) {
        result.message = "Unsupported zone navigator";
        return result;
    }

    filesystem::path relativeDirectory;
    bool isFx = false;
    if (!ZoneFileCreator::ResolveRelativeDirectory(request.scaffoldType, relativeDirectory, isFx)) {
        result.message = "Unsupported zone scaffold type";
        return result;
    }

    const string configuredBase = isFx ? zoneManager->GetFXZoneFolder() : zoneManager->GetZoneFolder();
    if (configuredBase.empty()) {
        result.message = isFx ? "FX zone folder is not configured" : "Main zone folder is not configured";
        return result;
    }

    bool activatedUserProfile = false;
    string editableBase;
    string preparationError;
    if (!zoneManager->PrepareZonePathForWrite(configuredBase, editableBase, activatedUserProfile, preparationError)) {
        result.message = preparationError;
        return result;
    }

    try {
        const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
        const std::optional<string> profileId = productPaths.UserZoneProfileIdForPath(editableBase);
        if (!profileId) {
            result.message = "Configured zone folder is not inside a user zone profile";
            return result;
        }
        const filesystem::path profileRoot = productPaths.ZoneProfileDirectory(ZoneSource::User, *profileId);
        const filesystem::path expectedBase = isFx ? productPaths.FxZones(ZoneSource::User, *profileId) : productPaths.MainZones(ZoneSource::User, *profileId);
        if (filesystem::weakly_canonical(filesystem::absolute(editableBase)) != filesystem::weakly_canonical(filesystem::absolute(expectedBase))) {
            result.message = "Configured zone folder does not match the selected scaffold destination";
            return result;
        }

        filesystem::path duplicatePath;
        if (ZoneFileCreator::FindDuplicateZoneName(profileRoot, request.zoneName, duplicatePath)) {
            result.message = "Zone name already exists in profile: " + duplicatePath.string();
            return result;
        }

        const filesystem::path destinationDirectory = expectedBase / relativeDirectory;
        const filesystem::path targetPath = destinationDirectory / (request.zoneName + ".zon");
        if (!ZoneFileCreator::IsContainedPath(profileRoot, targetPath) || !ZoneFileCreator::IsContainedPath(destinationDirectory, targetPath)) {
            result.message = "Zone destination escapes the user profile";
            return result;
        }
        filesystem::create_directories(destinationDirectory);
        if (ZoneFileCreator::HasCaseInsensitiveFile(destinationDirectory, targetPath.filename().string())) {
            result.message = "A zone file with this name already exists in the destination";
            return result;
        }

        string source = "// @format zone 1\nZone \"" + request.zoneName + "\"";
        if (!request.alias.empty()) source += " \"" + request.alias + "\"";
        if (!request.navigator.empty()) source += " NavType=" + request.navigator;
        source += "\nZoneEnd\n";

        string writeError;
        if (!ZoneFileCreator::WriteCompletedTemporaryFile(targetPath, source, writeError)) {
            result.message = writeError;
            return result;
        }
        result.success = true;
        result.path = targetPath.string();
        result.message = activatedUserProfile ? "Zone created and editable user profile activated" : "Zone created";
        return result;
    } catch (const std::exception& error) {
        result.message = string("Unable to create zone: ") + error.what();
        return result;
    }
}
