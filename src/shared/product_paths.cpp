#include "product_paths.h"

#include "product_identity.h"
#include "reaper_plugin_functions.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

std::string ProductPaths::LowerAscii(const std::string& value) {
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) { return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) : static_cast<char>(character); });
    return result;
}

void ProductPaths::AddSurfaceIds(const std::filesystem::path& root, std::unordered_map<std::string, std::string>& idsByLowercase) {
    if (!std::filesystem::exists(root)) return;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".txt") continue;
        const std::string surfaceId = entry.path().stem().string();
        if (!ProductPaths::IsStableId(surfaceId)) throw std::runtime_error("Invalid surface file ID '" + surfaceId + "' in " + root.string());
        ProductPaths::StableIdFile(root, surfaceId, ".txt");
        const std::string lowercaseId = ProductPaths::LowerAscii(surfaceId);
        auto existing = idsByLowercase.find(lowercaseId);
        if (existing != idsByLowercase.end() && existing->second != surfaceId) throw std::runtime_error("Surface IDs differ only by letter case: " + existing->second + " and " + surfaceId);
        idsByLowercase[lowercaseId] = surfaceId;
    }
}

bool ProductPaths::IsContainedPath(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    while (rootPart != root.end() && candidatePart != candidate.end()) {
        if (*rootPart != *candidatePart) return false;
        ++rootPart;
        ++candidatePart;
    }
    return rootPart == root.end();
}

ProductPaths::ProductPaths(std::filesystem::path reaperResourceRoot) : reaperResourceRoot_(std::move(reaperResourceRoot)) {
    if (this->reaperResourceRoot_.empty()) throw std::invalid_argument("REAPER resource root must not be empty");
    this->reaperResourceRoot_ = this->reaperResourceRoot_.lexically_normal();
}

ProductPaths ProductPaths::FromReaperResourcePath() { return ProductPaths(std::filesystem::path(GetResourcePath())); }

std::filesystem::path ProductPaths::ResolveLegacyImportRoot(const std::filesystem::path& selectedRoot) {
    if (selectedRoot.empty()) throw std::invalid_argument("Legacy import root must not be empty");
    return std::filesystem::weakly_canonical(std::filesystem::absolute(selectedRoot));
}

bool ProductPaths::IsStableId(const std::string& value) {
    if (value.empty()) return false;
    const unsigned char firstCharacter = static_cast<unsigned char>(value.front());
    if (!(firstCharacter >= 'a' && firstCharacter <= 'z') && !(firstCharacter >= '0' && firstCharacter <= '9')) return false;
    for (unsigned char character : value) {
        const bool isLowercaseLetter = character >= 'a' && character <= 'z';
        const bool isDigit = character >= '0' && character <= '9';
        if (!isLowercaseLetter && !isDigit && character != '_' && character != '-') return false;
    }
    return true;
}

std::filesystem::path ProductPaths::StableIdChild(const std::filesystem::path& root, const std::string& stableId) {
    if (!ProductPaths::IsStableId(stableId)) throw std::invalid_argument("Invalid stable ID '" + stableId + "'. Expected lowercase ASCII letters, digits, '_' or '-'");
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
    const std::filesystem::path canonicalChild = std::filesystem::weakly_canonical(canonicalRoot / stableId);
    if (!ProductPaths::IsContainedPath(canonicalRoot, canonicalChild)) throw std::invalid_argument("Stable ID path escapes its configured root: " + stableId);
    if (std::filesystem::exists(canonicalChild) && canonicalChild.filename().string() != stableId) throw std::invalid_argument("Stable ID path uses different letter case: " + stableId);
    return canonicalChild;
}

std::filesystem::path ProductPaths::StableIdFile(const std::filesystem::path& root, const std::string& stableId, const std::string& extension) {
    if (!ProductPaths::IsStableId(stableId)) throw std::invalid_argument("Invalid stable ID '" + stableId + "'. Expected lowercase ASCII letters, digits, '_' or '-'");
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
    const std::filesystem::path canonicalFile = std::filesystem::weakly_canonical(canonicalRoot / (stableId + extension));
    if (!ProductPaths::IsContainedPath(canonicalRoot, canonicalFile)) throw std::invalid_argument("Stable ID path escapes its configured root: " + stableId);
    if (std::filesystem::exists(canonicalFile) && canonicalFile.filename().string() != stableId + extension) throw std::invalid_argument("Stable ID file uses different letter case: " + stableId);
    return canonicalFile;
}

const std::filesystem::path& ProductPaths::ReaperResourceRoot() const { return this->reaperResourceRoot_; }
std::filesystem::path ProductPaths::ProductRoot() const { return this->reaperResourceRoot_ / ProductIdentity::ResourceInstallDirectory; }
std::filesystem::path ProductPaths::ConfigFile() const { return this->ProductRoot() / ProductIdentity::ConfigFilename; }
std::filesystem::path ProductPaths::LogFile() const { return this->ProductRoot() / ProductIdentity::LogFilename; }
std::filesystem::path ProductPaths::SurfacesRoot() const { return this->ProductRoot() / "Surfaces"; }
std::filesystem::path ProductPaths::VendorSurfacesRoot() const { return this->SurfacesRoot() / "Vendor"; }
std::filesystem::path ProductPaths::UserSurfacesRoot() const { return this->SurfacesRoot() / "User"; }

std::filesystem::path ProductPaths::SurfaceFile(SurfaceSource source, const std::string& surfaceId) const {
    const std::filesystem::path root = source == SurfaceSource::Vendor ? this->VendorSurfacesRoot() : this->UserSurfacesRoot();
    return ProductPaths::StableIdFile(root, surfaceId, ".txt");
}

std::optional<std::filesystem::path> ProductPaths::FindSurfaceFile(const std::string& surfaceId) const {
    const std::filesystem::path userFile = this->SurfaceFile(SurfaceSource::User, surfaceId);
    const std::filesystem::path vendorFile = this->SurfaceFile(SurfaceSource::Vendor, surfaceId);
    const bool userExists = std::filesystem::is_regular_file(userFile);
    const bool vendorExists = std::filesystem::is_regular_file(vendorFile);
    if (userExists) return userFile;
    if (vendorExists) return vendorFile;
    return std::nullopt;
}

std::vector<std::string> ProductPaths::ListSurfaceIds() const {
    std::unordered_map<std::string, std::string> idsByLowercase;
    ProductPaths::AddSurfaceIds(this->VendorSurfacesRoot(), idsByLowercase);
    ProductPaths::AddSurfaceIds(this->UserSurfacesRoot(), idsByLowercase);
    std::vector<std::string> surfaceIds;
    surfaceIds.reserve(idsByLowercase.size());
    for (const auto& entry : idsByLowercase) surfaceIds.push_back(entry.second);
    std::sort(surfaceIds.begin(), surfaceIds.end());
    return surfaceIds;
}

std::filesystem::path ProductPaths::ZonesRoot() const { return this->ProductRoot() / "Zones"; }
std::filesystem::path ProductPaths::VendorZonesRoot() const { return this->ZonesRoot() / "Vendor"; }
std::filesystem::path ProductPaths::UserZonesRoot() const { return this->ZonesRoot() / "User"; }

std::filesystem::path ProductPaths::ZoneProfileDirectory(ZoneSource source, const std::string& profileId) const {
    const std::filesystem::path root = source == ZoneSource::Vendor ? this->VendorZonesRoot() : this->UserZonesRoot();
    return ProductPaths::StableIdChild(root, profileId);
}

std::optional<std::filesystem::path> ProductPaths::FindZoneProfileDirectory(const std::string& profileId) const {
    const std::filesystem::path userDirectory = this->ZoneProfileDirectory(ZoneSource::User, profileId);
    const std::filesystem::path vendorDirectory = this->ZoneProfileDirectory(ZoneSource::Vendor, profileId);
    if (std::filesystem::is_directory(userDirectory)) return userDirectory;
    if (std::filesystem::is_directory(vendorDirectory)) return vendorDirectory;
    return std::nullopt;
}

std::filesystem::path ProductPaths::MainZones(ZoneSource source, const std::string& profileId) const { return this->ZoneProfileDirectory(source, profileId) / "Main"; }
std::filesystem::path ProductPaths::FxZones(ZoneSource source, const std::string& profileId) const { return this->ZoneProfileDirectory(source, profileId) / "FX"; }

std::optional<std::string> ProductPaths::ZoneProfileIdForPath(const std::filesystem::path& root, const std::filesystem::path& zonePath) {
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
    const std::filesystem::path canonicalZonePath = std::filesystem::weakly_canonical(std::filesystem::absolute(zonePath));
    if (!ProductPaths::IsContainedPath(canonicalRoot, canonicalZonePath)) return std::nullopt;
    const std::filesystem::path relativePath = canonicalZonePath.lexically_relative(canonicalRoot);
    if (relativePath.empty() || relativePath.begin() == relativePath.end()) return std::nullopt;
    const std::string profileId = (*relativePath.begin()).string();
    if (!ProductPaths::IsStableId(profileId)) return std::nullopt;
    const std::filesystem::path profileRoot = canonicalRoot / profileId;
    if (!ProductPaths::IsContainedPath(profileRoot, canonicalZonePath)) return std::nullopt;
    return profileId;
}

std::optional<std::string> ProductPaths::UserZoneProfileIdForPath(const std::filesystem::path& zonePath) const { return ProductPaths::ZoneProfileIdForPath(this->UserZonesRoot(), zonePath); }
std::optional<std::string> ProductPaths::VendorZoneProfileIdForPath(const std::filesystem::path& zonePath) const { return ProductPaths::ZoneProfileIdForPath(this->VendorZonesRoot(), zonePath); }

std::filesystem::path ProductPaths::UserZonePathForVendorPath(const std::string& profileId, const std::filesystem::path& vendorZonePath) const {
    const std::filesystem::path vendorProfile = this->ZoneProfileDirectory(ZoneSource::Vendor, profileId);
    const std::filesystem::path canonicalVendorPath = std::filesystem::weakly_canonical(std::filesystem::absolute(vendorZonePath));
    if (!ProductPaths::IsContainedPath(vendorProfile, canonicalVendorPath)) throw std::invalid_argument("Vendor zone path is outside profile '" + profileId + "'");
    const std::filesystem::path relativePath = canonicalVendorPath.lexically_relative(vendorProfile);
    const std::filesystem::path userProfile = this->ZoneProfileDirectory(ZoneSource::User, profileId);
    const std::filesystem::path userZonePath = std::filesystem::weakly_canonical(userProfile / relativePath);
    if (!ProductPaths::IsContainedPath(userProfile, userZonePath)) throw std::invalid_argument("User zone path escapes profile '" + profileId + "'");
    return userZonePath;
}

void ProductPaths::CloneVendorZoneProfileToUser(const std::string& profileId) const {
    const std::filesystem::path vendorProfile = this->ZoneProfileDirectory(ZoneSource::Vendor, profileId);
    const std::filesystem::path userProfile = this->ZoneProfileDirectory(ZoneSource::User, profileId);
    if (std::filesystem::exists(userProfile)) {
        if (!std::filesystem::is_directory(userProfile)) throw std::runtime_error("User zone profile path is not a directory: " + userProfile.string());
        return;
    }
    if (!std::filesystem::is_directory(vendorProfile)) throw std::runtime_error("Vendor zone profile does not exist: " + vendorProfile.string());

    std::filesystem::create_directories(this->UserZonesRoot());
    const auto uniqueValue = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path temporaryProfile = this->UserZonesRoot() / (profileId + ".tmp." + std::to_string(uniqueValue));

    try {
        if (!std::filesystem::create_directory(temporaryProfile)) throw std::runtime_error("Unable to create temporary zone profile: " + temporaryProfile.string());
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(vendorProfile)) {
            if (entry.is_symlink()) throw std::runtime_error("Vendor zone profile contains a symlink: " + entry.path().string());
            const std::filesystem::path relativePath = entry.path().lexically_relative(vendorProfile);
            const std::filesystem::path destinationPath = temporaryProfile / relativePath;
            if (entry.is_directory()) std::filesystem::create_directories(destinationPath);
            else if (entry.is_regular_file()) std::filesystem::copy_file(entry.path(), destinationPath);
            else throw std::runtime_error("Vendor zone profile contains an unsupported file: " + entry.path().string());
        }
        std::filesystem::rename(temporaryProfile, userProfile);
    } catch (...) {
        std::error_code cleanupError;
        std::filesystem::remove_all(temporaryProfile, cleanupError);
        throw;
    }
}

std::filesystem::path ProductPaths::SnippetsRoot() const { return this->ProductRoot() / "Snippets"; }
std::filesystem::path ProductPaths::BuiltInSnippetsRoot() const { return this->SnippetsRoot() / "BuiltIn"; }
std::filesystem::path ProductPaths::UserSnippetsRoot() const { return this->SnippetsRoot() / "User"; }
std::filesystem::path ProductPaths::BackupsRoot() const { return this->ProductRoot() / "Backups"; }
std::filesystem::path ProductPaths::BackupRoot(const std::string& operationId) const { return ProductPaths::StableIdChild(this->BackupsRoot(), operationId); }
std::filesystem::path ProductPaths::RawFxFilesRoot() const { return this->ProductRoot() / "Generated" / "ZoneRawFXFiles"; }

void ProductPaths::EnsureUserDirectories() const {
    std::filesystem::create_directories(this->UserSurfacesRoot());
    std::filesystem::create_directories(this->UserZonesRoot());
    std::filesystem::create_directories(this->UserSnippetsRoot());
    std::filesystem::create_directories(this->BackupsRoot());
}
