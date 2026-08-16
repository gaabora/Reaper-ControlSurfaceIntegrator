#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class SurfaceSource {
    Vendor,
    User,
};

enum class ZoneSource {
    Vendor,
    User,
};

class ProductPaths
{
private:
    std::filesystem::path reaperResourceRoot_;

    static std::string LowerAscii(const std::string& value);
    static void AddSurfaceIds(const std::filesystem::path& root, std::unordered_map<std::string, std::string>& idsByLowercase);
    static bool IsContainedPath(const std::filesystem::path& root, const std::filesystem::path& candidate);
    static bool IsStableId(const std::string& value);
    static std::optional<std::string> ZoneProfileIdForPath(const std::filesystem::path& root, const std::filesystem::path& zonePath);
    static std::filesystem::path StableIdChild(const std::filesystem::path& root, const std::string& stableId);
    static std::filesystem::path StableIdFile(const std::filesystem::path& root, const std::string& stableId, const std::string& extension);

public:
    explicit ProductPaths(std::filesystem::path reaperResourceRoot);

    static ProductPaths FromReaperResourcePath();
    static std::filesystem::path ResolveLegacyImportRoot(const std::filesystem::path& selectedRoot);

    const std::filesystem::path& ReaperResourceRoot() const;
    std::filesystem::path ProductRoot() const;
    std::filesystem::path ConfigFile() const;
    std::filesystem::path LogFile() const;
    std::filesystem::path SurfacesRoot() const;
    std::filesystem::path VendorSurfacesRoot() const;
    std::filesystem::path UserSurfacesRoot() const;
    std::filesystem::path SurfaceFile(SurfaceSource source, const std::string& surfaceId) const;
    std::optional<std::filesystem::path> FindSurfaceFile(const std::string& surfaceId) const;
    std::vector<std::string> ListSurfaceIds() const;
    std::filesystem::path ZonesRoot() const;
    std::filesystem::path VendorZonesRoot() const;
    std::filesystem::path UserZonesRoot() const;
    std::filesystem::path ZoneProfileDirectory(ZoneSource source, const std::string& profileId) const;
    std::optional<std::filesystem::path> FindMainZones(const std::string& profileId) const;
    std::filesystem::path MainZones(ZoneSource source, const std::string& profileId) const;
    std::filesystem::path FxZones(ZoneSource source, const std::string& profileId) const;
    std::optional<std::string> UserZoneProfileIdForPath(const std::filesystem::path& zonePath) const;
    std::optional<std::string> VendorZoneProfileIdForPath(const std::filesystem::path& zonePath) const;
    std::filesystem::path UserZonePathForVendorPath(const std::string& profileId, const std::filesystem::path& vendorZonePath) const;
    void CloneVendorMainZonesToUser(const std::string& profileId) const;
    std::filesystem::path CopyVendorFxZoneToUser(const std::string& profileId, const std::filesystem::path& vendorZonePath) const;
    std::filesystem::path SnippetsRoot() const;
    std::filesystem::path BuiltInSnippetsRoot() const;
    std::filesystem::path UserSnippetsRoot() const;
    std::filesystem::path BackupsRoot() const;
    std::filesystem::path BackupRoot(const std::string& operationId) const;
    std::filesystem::path RawFxFilesRoot() const;
    void EnsureUserDirectories() const;
};
