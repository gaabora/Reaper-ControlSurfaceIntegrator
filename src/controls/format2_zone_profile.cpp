#include "format2_zone_profile.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
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
    result.diagnostics.push_back({"format2.zone-profile.id.duplicate", message, sources[sourceIndices.front()].location, sourceIndices});
}

static void AddFormat2ZoneProfileDiagnostic(Format2ZoneProfileResolveResult& result, const std::string& code, const std::string& message, const Format2SourceLocation& location, const std::vector<std::size_t>& sourceIndices) {
    result.diagnostics.push_back({code, message, location, sourceIndices});
}

Format2ZoneSource MakeFormat2ZoneSource(Format2ZoneCollection collection, Format2ZoneSourceLayer layer, const Format2ZoneParseResult& parsed) {
    Format2SourceLocation location;
    if (!parsed.document.metadata.entries.empty()) location = parsed.document.metadata.entries.front().nameLocation;
    else if (!parsed.document.lexical.tokens.empty()) location = parsed.document.lexical.tokens.front().location;
    return {collection, layer, parsed.zone.id, parsed.document.lexical.sourcePath, location, parsed.document.metadata.role, parsed.zone.includedZones, parsed.zone.zoneLayers, parsed.zone.navigationReferences, parsed.IsValid()};
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

    std::map<std::string, std::size_t> activeMainSources;
    std::vector<std::size_t> homeSources;
    for (const Format2ActiveZoneSource& activeZone : result.activeZones) {
        if (activeZone.collection != Format2ZoneCollection::Main || !activeZone.activeSourceIndex) continue;
        const std::size_t sourceIndex = *activeZone.activeSourceIndex;
        activeMainSources[activeZone.canonicalId] = sourceIndex;
        if (activeZone.available && sources[sourceIndex].role == Format2ZoneRole::Home) homeSources.push_back(sourceIndex);
    }

    if (homeSources.empty()) {
        AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.home.missing", "Zone profile '" + profileId + "' requires exactly one valid Main zone with Role=Home", {}, {});
    } else if (homeSources.size() > 1) {
        AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.home.duplicate", "Zone profile '" + profileId + "' has more than one valid Main zone with Role=Home: " + JoinFormat2ZoneSourcePaths(sources, homeSources), sources[homeSources.front()].location, homeSources);
    }

    auto validateReference = [&](std::size_t sourceIndex, const Format2ZoneReference& reference, bool layerReference) {
        const std::string canonicalId = FoldFormat2ZoneId(reference.id);
        const auto targetEntry = activeMainSources.find(canonicalId);
        if (targetEntry == activeMainSources.end() || !sources[targetEntry->second].valid) {
            AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.reference.missing", "Zone '" + sources[sourceIndex].id + "' references unavailable Main zone '" + reference.id + "'", reference.location, {sourceIndex});
            return;
        }
        const Format2ZoneSource& target = sources[targetEntry->second];
        if (layerReference && target.role != Format2ZoneRole::Layer) {
            AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.layer.role", "ZoneLayers target '" + reference.id + "' must declare Role=Layer", reference.location, {sourceIndex, targetEntry->second});
        } else if (!layerReference && target.role == Format2ZoneRole::Layer) {
            AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.included.layer", "IncludedZones cannot target Role=Layer zone '" + reference.id + "'", reference.location, {sourceIndex, targetEntry->second});
        }
    };

    for (const auto& activeEntry : activeMainSources) {
        const std::size_t sourceIndex = activeEntry.second;
        if (!sources[sourceIndex].valid) continue;
        for (const Format2ZoneReference& reference : sources[sourceIndex].includedZones) validateReference(sourceIndex, reference, false);
        for (const Format2ZoneReference& reference : sources[sourceIndex].zoneLayers) validateReference(sourceIndex, reference, true);
        for (const Format2ZoneNavigationReference& reference : sources[sourceIndex].navigationReferences) {
            const std::string canonicalId = FoldFormat2ZoneId(reference.id);
            const auto targetEntry = activeMainSources.find(canonicalId);
            if (targetEntry == activeMainSources.end() || !sources[targetEntry->second].valid) {
                AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.navigation.missing", "Zone '" + sources[sourceIndex].id + "' references unavailable Main zone '" + reference.id + "'", reference.location, {sourceIndex});
                continue;
            }
            const Format2ZoneSource& target = sources[targetEntry->second];
            if (reference.kind == Format2ZoneNavigationKind::IndependentZone) {
                if (target.role == Format2ZoneRole::Home) AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.navigation.home", "GoZone cannot target Role=Home zone '" + reference.id + "'; use GoHome", reference.location, {sourceIndex, targetEntry->second});
                else if (target.role == Format2ZoneRole::Layer) AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.navigation.layer", "GoZone cannot target Role=Layer zone '" + reference.id + "'; use EnterZoneLayer", reference.location, {sourceIndex, targetEntry->second});
                continue;
            }
            if (target.role != Format2ZoneRole::Layer) {
                AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.navigation.layer-role", "EnterZoneLayer target '" + reference.id + "' must declare Role=Layer", reference.location, {sourceIndex, targetEntry->second});
                continue;
            }
            const bool declaredByOwner = std::any_of(sources[sourceIndex].zoneLayers.begin(), sources[sourceIndex].zoneLayers.end(), [&](const Format2ZoneReference& declaredReference) { return FoldFormat2ZoneId(declaredReference.id) == canonicalId; });
            if (!declaredByOwner) AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.navigation.layer-not-declared", "EnterZoneLayer target '" + reference.id + "' is not declared in ZoneLayers for zone '" + sources[sourceIndex].id + "'", reference.location, {sourceIndex, targetEntry->second});
        }
    }

    std::map<std::string, int> visitStates;
    std::vector<std::string> visitStack;
    std::set<std::string> reportedCycles;
    std::function<void(const std::string&)> visitZone = [&](const std::string& canonicalId) {
        if (visitStates[canonicalId] == 2) return;
        if (visitStates[canonicalId] == 1) return;
        const auto sourceEntry = activeMainSources.find(canonicalId);
        if (sourceEntry == activeMainSources.end() || !sources[sourceEntry->second].valid) return;
        visitStates[canonicalId] = 1;
        visitStack.push_back(canonicalId);
        const std::size_t sourceIndex = sourceEntry->second;
        auto visitReference = [&](const Format2ZoneReference& reference) {
            const std::string targetId = FoldFormat2ZoneId(reference.id);
            const auto cycleStart = std::find(visitStack.begin(), visitStack.end(), targetId);
            if (cycleStart != visitStack.end()) {
                std::vector<std::string> cycle(cycleStart, visitStack.end());
                cycle.push_back(targetId);
                std::set<std::string> cycleIds(cycle.begin(), cycle.end());
                std::string cycleKey;
                std::string cycleText;
                for (const std::string& cycleId : cycleIds) cycleKey += cycleId + "\n";
                if (!reportedCycles.insert(cycleKey).second) return;
                for (const std::string& cycleId : cycle) {
                    if (!cycleText.empty()) cycleText += " -> ";
                    cycleText += sources[activeMainSources[cycleId]].id;
                }
                AddFormat2ZoneProfileDiagnostic(result, "format2.zone-profile.reference.cycle", "Structural zone dependency cycle: " + cycleText, reference.location, {sourceIndex});
                return;
            }
            visitZone(targetId);
        };
        for (const Format2ZoneReference& reference : sources[sourceIndex].includedZones) visitReference(reference);
        for (const Format2ZoneReference& reference : sources[sourceIndex].zoneLayers) visitReference(reference);
        visitStack.pop_back();
        visitStates[canonicalId] = 2;
    };
    for (const auto& activeEntry : activeMainSources) visitZone(activeEntry.first);

    return result;
}
