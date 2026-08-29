#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "format2_zone_document.h"

enum class Format2ZoneCollection {
    Main,
    Fx,
};

enum class Format2ZoneSourceLayer {
    Vendor,
    User,
};

struct Format2ZoneSource {
    Format2ZoneCollection collection = Format2ZoneCollection::Main;
    Format2ZoneSourceLayer layer = Format2ZoneSourceLayer::Vendor;
    std::string id;
    std::string sourcePath;
    Format2SourceLocation location;
    bool valid = false;
};

struct Format2ActiveZoneSource {
    Format2ZoneCollection collection = Format2ZoneCollection::Main;
    std::string canonicalId;
    std::optional<std::size_t> activeSourceIndex;
    std::vector<std::size_t> overriddenVendorSourceIndices;
    bool available = false;
};

struct Format2ZoneProfileDiagnostic {
    std::string code;
    std::string message;
    std::vector<std::size_t> sourceIndices;
};

struct Format2ZoneProfileResolveResult {
    std::string profileId;
    std::vector<Format2ActiveZoneSource> activeZones;
    std::vector<Format2ZoneProfileDiagnostic> diagnostics;

    bool IsValid() const { return this->diagnostics.empty(); }
};

Format2ZoneSource MakeFormat2ZoneSource(Format2ZoneCollection collection, Format2ZoneSourceLayer layer, const Format2ZoneParseResult& parsed);
Format2ZoneProfileResolveResult ResolveFormat2ZoneProfile(const std::string& profileId, const std::vector<Format2ZoneSource>& sources);
