#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "format2_learn_fx_document.h"
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

struct Format2LoadedLearnFxDocument {
    Format2ZoneSourceLayer layer = Format2ZoneSourceLayer::Vendor;
    Format2LearnFxParseResult parsed;
};

struct Format2ZoneProfileLoadResult {
    std::vector<Format2LoadedZoneDocument> documents;
    std::vector<Format2ZoneSource> sources;
    std::optional<Format2LoadedLearnFxDocument> learnFx;
    Format2ZoneProfileResolveResult profile;

    bool IsValid() const { return this->profile.IsValid() && (!this->learnFx || this->learnFx->parsed.IsValid()); }
    bool UsesFormat2() const;
    bool ContainsOnlyFormat2() const;
};

Format2ZoneProfileLoadResult LoadFormat2ZoneProfile(const std::string& profileId, const std::vector<Format2ZoneProfileRoot>& roots);
