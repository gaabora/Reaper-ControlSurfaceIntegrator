#include "format2_zone_profile.h"

#include <map>
#include <utility>

struct Format2ZoneSourceGroup {
    std::vector<std::size_t> vendorSourceIndices;
    std::vector<std::size_t> userSourceIndices;
};

static std::string FoldFormat2ZoneId(const std::string& value) {
    std::string folded = value;
    for (char& character : folded) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return folded;
}

static const char* Format2ZoneCollectionName(Format2ZoneCollection collection) {
    return collection == Format2ZoneCollection::Main ? "Main" : "FX";
}

static const char* Format2ZoneSourceLayerName(Format2ZoneSourceLayer layer) {
    return layer == Format2ZoneSourceLayer::Vendor ? "Vendor" : "User";
}

static std::string JoinFormat2ZoneSourcePaths(const std::vector<Format2ZoneSource>& sources, const std::vector<std::size_t>& sourceIndices) {
    std::string paths;
    for (std::size_t sourceListIndex = 0; sourceListIndex < sourceIndices.size(); sourceListIndex++) {
        if (sourceListIndex > 0) paths += ", ";
        const Format2ZoneSource& source = sources[sourceIndices[sourceListIndex]];
        paths += source.sourcePath + " (ID '" + source.id + "')";
    }
    return paths;
}

static void AddFormat2DuplicateZoneDiagnostic(Format2ZoneProfileResolveResult& result, const std::vector<Format2ZoneSource>& sources, Format2ZoneCollection collection, Format2ZoneSourceLayer layer, const std::string& canonicalId, const std::vector<std::size_t>& sourceIndices) {
    const std::string message = "Duplicate " + std::string(Format2ZoneSourceLayerName(layer)) + " " + Format2ZoneCollectionName(collection) + " zone ID '" + canonicalId + "' in profile '" + result.profileId + "': " + JoinFormat2ZoneSourcePaths(sources, sourceIndices);
    result.diagnostics.push_back({"format2.zone-profile.id.duplicate", message, sourceIndices});
}

Format2ZoneSource MakeFormat2ZoneSource(Format2ZoneCollection collection, Format2ZoneSourceLayer layer, const Format2ZoneParseResult& parsed) {
    Format2SourceLocation location;
    if (!parsed.document.metadata.entries.empty()) location = parsed.document.metadata.entries.front().nameLocation;
    else if (!parsed.document.lexical.tokens.empty()) location = parsed.document.lexical.tokens.front().location;
    return {collection, layer, parsed.zone.id, parsed.document.lexical.sourcePath, location, parsed.IsValid()};
}

Format2ZoneProfileResolveResult ResolveFormat2ZoneProfile(const std::string& profileId, const std::vector<Format2ZoneSource>& sources) {
    Format2ZoneProfileResolveResult result;
    result.profileId = profileId;
    std::map<std::pair<Format2ZoneCollection, std::string>, Format2ZoneSourceGroup> groups;

    for (std::size_t sourceIndex = 0; sourceIndex < sources.size(); sourceIndex++) {
        const Format2ZoneSource& source = sources[sourceIndex];
        Format2ZoneSourceGroup& group = groups[{source.collection, FoldFormat2ZoneId(source.id)}];
        if (source.layer == Format2ZoneSourceLayer::Vendor) group.vendorSourceIndices.push_back(sourceIndex);
        else group.userSourceIndices.push_back(sourceIndex);
    }

    for (const auto& groupEntry : groups) {
        const Format2ZoneCollection collection = groupEntry.first.first;
        const std::string& canonicalId = groupEntry.first.second;
        const Format2ZoneSourceGroup& group = groupEntry.second;
        Format2ActiveZoneSource activeZone;
        activeZone.collection = collection;
        activeZone.canonicalId = canonicalId;

        if (group.vendorSourceIndices.size() > 1) AddFormat2DuplicateZoneDiagnostic(result, sources, collection, Format2ZoneSourceLayer::Vendor, canonicalId, group.vendorSourceIndices);
        if (group.userSourceIndices.size() > 1) AddFormat2DuplicateZoneDiagnostic(result, sources, collection, Format2ZoneSourceLayer::User, canonicalId, group.userSourceIndices);

        if (!group.userSourceIndices.empty()) {
            activeZone.overriddenVendorSourceIndices = group.vendorSourceIndices;
            if (group.userSourceIndices.size() == 1) activeZone.activeSourceIndex = group.userSourceIndices.front();
        } else if (group.vendorSourceIndices.size() == 1) {
            activeZone.activeSourceIndex = group.vendorSourceIndices.front();
        }

        if (activeZone.activeSourceIndex) activeZone.available = sources[*activeZone.activeSourceIndex].valid;
        result.activeZones.push_back(std::move(activeZone));
    }

    return result;
}
