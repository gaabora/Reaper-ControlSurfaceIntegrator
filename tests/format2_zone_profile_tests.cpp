#include "format2_zone_profile.h"
#include "format2_zone_profile_loader.h"

#include <cstdlib>
#include <iostream>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << "\n";
    std::exit(1);
}

static Format2ZoneSource ParseSource(const std::string& source, const std::string& path, Format2ZoneSourceLayer layer = Format2ZoneSourceLayer::Vendor) {
    return MakeFormat2ZoneSource(Format2ZoneCollection::Main, layer, ParseFormat2ZoneDocumentSource(source, path, Format2DocumentKind::MainZone));
}

static bool HasDiagnostic(const Format2ZoneProfileResolveResult& result, const std::string& code) {
    for (const Format2ZoneProfileDiagnostic& diagnostic : result.diagnostics) if (diagnostic.code == code) return true;
    return false;
}

static void TestValidProfile() {
    std::vector<Format2ZoneSource> sources;
    sources.push_back(ParseSource("@Meta { Version=2 Role=Home }\n\nIncludedZones {\n  Track\n}\nZoneLayers {\n  Pan\n}\nPlay Play\n", "Home.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Target=Tracks }\n\nFader# TrackVolume\n", "Track.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Role=Layer }\n\nRotary# TrackPan\n", "Pan.zon"));
    Require(ResolveFormat2ZoneProfile("test", sources).IsValid(), "valid profile");
}

static void TestHomeRules() {
    std::vector<Format2ZoneSource> missingHome = {ParseSource("@Meta { Version=2 Target=Tracks }\n", "Track.zon")};
    Require(HasDiagnostic(ResolveFormat2ZoneProfile("test", missingHome), "format2.zone-profile.home.missing"), "missing Home role");

    std::vector<Format2ZoneSource> duplicateHome = {ParseSource("@Meta { Version=2 Role=Home }\n", "Home.zon"), ParseSource("@Meta { Version=2 Role=Home }\n", "Other.zon")};
    Require(HasDiagnostic(ResolveFormat2ZoneProfile("test", duplicateHome), "format2.zone-profile.home.duplicate"), "duplicate Home role");
}

static void TestReferenceRules() {
    std::vector<Format2ZoneSource> missingReference = {ParseSource("@Meta { Version=2 Role=Home }\n\nIncludedZones { Missing }\n", "Home.zon")};
    Require(HasDiagnostic(ResolveFormat2ZoneProfile("test", missingReference), "format2.zone-profile.reference.missing"), "missing structural reference");

    std::vector<Format2ZoneSource> wrongLayer = {ParseSource("@Meta { Version=2 Role=Home }\n\nZoneLayers { Track }\n", "Home.zon"), ParseSource("@Meta { Version=2 Target=Tracks }\n", "Track.zon")};
    Require(HasDiagnostic(ResolveFormat2ZoneProfile("test", wrongLayer), "format2.zone-profile.layer.role"), "ZoneLayers role");

    std::vector<Format2ZoneSource> includedLayer = {ParseSource("@Meta { Version=2 Role=Home }\n\nIncludedZones { Pan }\n", "Home.zon"), ParseSource("@Meta { Version=2 Role=Layer }\n", "Pan.zon")};
    Require(HasDiagnostic(ResolveFormat2ZoneProfile("test", includedLayer), "format2.zone-profile.included.layer"), "IncludedZones layer rejection");
}

static void TestStructuralCycle() {
    std::vector<Format2ZoneSource> sources;
    sources.push_back(ParseSource("@Meta { Version=2 Role=Home }\n\nIncludedZones { Track }\n", "Home.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Target=Tracks }\n\nIncludedZones { Home }\n", "Track.zon"));
    Require(HasDiagnostic(ResolveFormat2ZoneProfile("test", sources), "format2.zone-profile.reference.cycle"), "structural cycle");
}

static void TestInvalidOptionalUserOverrideDoesNotDisableProfile() {
    std::vector<Format2ZoneSource> sources;
    sources.push_back(ParseSource("@Meta { Version=2 Role=Home }\n\nPlay Play\n", "Home.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Target=Tracks }\n\nFader# TrackVolume\n", "Track.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Target=Tracks }\n\nFader#\n", "Track.zon", Format2ZoneSourceLayer::User));
    const Format2ZoneProfileResolveResult result = ResolveFormat2ZoneProfile("test", sources);
    Require(result.IsValid(), "invalid optional User override is skipped");
    for (const Format2ActiveZoneSource& activeZone : result.activeZones) if (activeZone.canonicalId == "track") Require(!activeZone.available, "invalid User override blocks Vendor fallback");
}

static void TestMissingNavigationTarget() {
    std::vector<Format2ZoneSource> sources = {ParseSource("@Meta { Version=2 Role=Home }\nButtonA GoZone Mixer\n", "Home.zon")};
    const Format2ZoneProfileResolveResult result = ResolveFormat2ZoneProfile("test", sources);
    Require(HasDiagnostic(result, "format2.zone-profile.navigation.missing"), "missing navigation reference");
    for (const Format2ZoneProfileDiagnostic& diagnostic : result.diagnostics) if (diagnostic.code == "format2.zone-profile.navigation.missing") Require(diagnostic.location.line == 2, "navigation diagnostic line");
}

static void TestNavigationRoleRules() {
    std::vector<Format2ZoneSource> sources;
    sources.push_back(ParseSource("@Meta { Version=2 Role=Home }\nButtonA GoZone Home\nButtonB GoZone Pan\nButtonC EnterZoneLayer Mixer\nButtonD EnterZoneLayer Pan\n", "Home.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Target=Tracks }\nFader# TrackVolume\n", "Mixer.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Role=Layer }\nRotary# TrackPan\n", "Pan.zon"));
    const Format2ZoneProfileResolveResult result = ResolveFormat2ZoneProfile("test", sources);
    Require(HasDiagnostic(result, "format2.zone-profile.navigation.home"), "GoZone Home rejection");
    Require(HasDiagnostic(result, "format2.zone-profile.navigation.layer"), "GoZone Layer rejection");
    Require(HasDiagnostic(result, "format2.zone-profile.navigation.layer-role"), "EnterZoneLayer role rejection");
    Require(HasDiagnostic(result, "format2.zone-profile.navigation.layer-not-declared"), "undeclared layer navigation rejection");
}

static void TestDeclaredLayerNavigation() {
    std::vector<Format2ZoneSource> sources;
    sources.push_back(ParseSource("@Meta { Version=2 Role=Home }\nZoneLayers {\n Pan\n}\nButtonA EnterZoneLayer Pan\n", "Home.zon"));
    sources.push_back(ParseSource("@Meta { Version=2 Role=Layer }\nRotary# TrackPan\n", "Pan.zon"));
    Require(ResolveFormat2ZoneProfile("test", sources).IsValid(), "declared layer navigation");
}

static void TestProfileLoader() {
    const std::filesystem::path fixtureRoot = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "format2-zone-profile";
    const std::vector<Format2ZoneProfileRoot> roots {
        {fixtureRoot / "Vendor" / "Main", Format2ZoneCollection::Main, Format2ZoneSourceLayer::Vendor},
        {fixtureRoot / "User" / "Main", Format2ZoneCollection::Main, Format2ZoneSourceLayer::User},
    };
    const Format2ZoneProfileLoadResult result = LoadFormat2ZoneProfile("test", roots);
    Require(result.IsValid(), "profile loader result");
    Require(result.documents.size() == 2, "profile loader document count");
    Require(result.sources.size() == result.documents.size(), "profile loader source alignment");
}

int main() {
    TestValidProfile();
    TestHomeRules();
    TestReferenceRules();
    TestStructuralCycle();
    TestInvalidOptionalUserOverrideDoesNotDisableProfile();
    TestMissingNavigationTarget();
    TestNavigationRoleRules();
    TestDeclaredLayerNavigation();
    TestProfileLoader();
    std::cout << "Format2ZoneProfile tests passed\n";
    return 0;
}
