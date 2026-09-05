#include "format2_zone_profile_loader.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

static std::vector<std::filesystem::path> CollectFormat2ZoneFiles(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::is_directory(root)) return files;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root)) if (entry.is_regular_file() && entry.path().extension() == ".zon") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    return files;
}

static Format2ZoneParseResult ReadFormat2ZoneFile(const std::filesystem::path& path, Format2DocumentKind kind) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Format2ZoneParseResult result;
        result.zone.id = path.stem().string();
        result.document.kind = kind;
        result.document.lexical.sourcePath = path.string();
        result.document.lexical.diagnostics.push_back({"format2.zone.file.open", "Cannot open Zone file", {0, 1, 1}});
        return result;
    }
    const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return ParseFormat2ZoneDocumentSource(source, path.string(), kind);
}

Format2ZoneProfileLoadResult LoadFormat2ZoneProfile(const std::string& profileId, const std::vector<Format2ZoneProfileRoot>& roots) {
    Format2ZoneProfileLoadResult result;
    for (const Format2ZoneProfileRoot& root : roots) {
        const Format2DocumentKind kind = root.collection == Format2ZoneCollection::Main ? Format2DocumentKind::MainZone : Format2DocumentKind::FxZone;
        for (const std::filesystem::path& path : CollectFormat2ZoneFiles(root.path)) {
            Format2LoadedZoneDocument document;
            document.collection = root.collection;
            document.layer = root.layer;
            document.parsed = ReadFormat2ZoneFile(path, kind);
            result.sources.push_back(MakeFormat2ZoneSource(document.collection, document.layer, document.parsed));
            result.documents.push_back(std::move(document));
        }
    }
    result.profile = ResolveFormat2ZoneProfile(profileId, result.sources);
    return result;
}

bool Format2ZoneProfileLoadResult::UsesFormat2() const {
    return std::any_of(this->documents.begin(), this->documents.end(), [](const Format2LoadedZoneDocument& document) { return document.parsed.document.metadata.version == 2; });
}

bool Format2ZoneProfileLoadResult::ContainsOnlyFormat2() const {
    return !this->documents.empty() && std::all_of(this->documents.begin(), this->documents.end(), [](const Format2LoadedZoneDocument& document) { return document.parsed.document.metadata.version == 2; });
}
