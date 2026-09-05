#pragma once

#include <filesystem>
#include <vector>

#include "format2_zone_profile.h"

struct Format2ZoneProfileRoot {
    std::filesystem::path path;
    Format2ZoneCollection collection = Format2ZoneCollection::Main;
    Format2ZoneSourceLayer layer = Format2ZoneSourceLayer::Vendor;
};

struct Format2LoadedZoneDocument {
    Format2ZoneCollection collection = Format2ZoneCollection::Main;
    Format2ZoneSourceLayer layer = Format2ZoneSourceLayer::Vendor;
    Format2ZoneParseResult parsed;
};

struct Format2ZoneProfileLoadResult {
    std::vector<Format2LoadedZoneDocument> documents;
    std::vector<Format2ZoneSource> sources;
    Format2ZoneProfileResolveResult profile;

    bool IsValid() const { return this->profile.IsValid(); }
    bool UsesFormat2() const;
    bool ContainsOnlyFormat2() const;
};

Format2ZoneProfileLoadResult LoadFormat2ZoneProfile(const std::string& profileId, const std::vector<Format2ZoneProfileRoot>& roots);
