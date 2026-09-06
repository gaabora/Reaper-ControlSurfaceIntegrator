#include "format2_zone_profile.h"
#include "format2_zone_profile_loader.h"
#include "format2_zone_source_editor.h"

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

static bool HasParseDiagnostic(const Format2ZoneParseResult& result, const std::string& code) {
    for (const Format2Diagnostic& diagnostic : result.document.lexical.diagnostics) if (diagnostic.code == code) return true;
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

static void TestLayerOnlyAction() {
    const Format2ZoneParseResult normalZone = ParseFormat2ZoneDocumentSource("@Meta { Version=2 Role=Home }\nButtonA ExitZoneLayer\n", "Home.zon", Format2DocumentKind::MainZone);
    Require(HasParseDiagnostic(normalZone, "format2.zone.action.layer-only"), "ExitZoneLayer normal-zone rejection");

    const Format2ZoneParseResult layer = ParseFormat2ZoneDocumentSource("@Meta { Version=2 Role=Layer }\nButtonA ExitZoneLayer\n", "Pan.zon", Format2DocumentKind::MainZone);
    Require(layer.IsValid(), "ExitZoneLayer layer acceptance");
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
    Require(result.learnFx.has_value(), "profile loader Learn FX document");
    Require(result.learnFx->layer == Format2ZoneSourceLayer::User, "User Learn FX document overrides Vendor");
    Require(result.learnFx->parsed.learnFx.widgets.size() == 1, "active Learn FX document content");
    Require(result.learnFx->parsed.learnFx.widgets.front().selector.source == "Rotary#", "active User Learn FX selector");
}

static void TestExactWidgetSourceEdit() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Play Play", "Stop Stop"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Play", 1, {"[Shift]+Play Reaper 40044"});
    Require(result.success, "exact Widget source edit");
    Require(result.lines == std::vector<std::string>({"@Meta { Version=2 Role=Home }", "", "[Shift]+Play Reaper 40044", "Stop Stop"}), "exact Widget replacement");
}

static void TestChannelFamilySourceEdit() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Fader# TrackVolume", "Play Play"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Fader2", 8, {"Fader2 TrackPan"});
    Require(result.success, "channel-family source edit");
    Require(result.editedChannelFamily, "channel-family edit status");
    Require(result.channelFamilyBaseName == "Fader", "channel-family edit base name");
    Require(result.parsed.zone.bindings.size() == 2, "channel-family edited document parse result");
    Require(result.lines == std::vector<std::string>({"@Meta { Version=2 Role=Home }", "", "Fader# TrackPan", "Play Play"}), "channel-family mapping remains one unit");
}

static void TestMixedWidgetSourceEditRejection() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Fader# TrackVolume", "Fader2 TrackPan"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Fader2", 8, {"Fader2 TrackPanWidth"});
    Require(!result.success, "mixed exact and family Widget edit rejection");
}

static void TestWidgetSourceEditPreservesComment() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Play FixedTextDisplay \"a \\\"quoted\\\" // value\" // transport", "Stop Stop"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Play", 1, {"Play Reaper 40044"});
    Require(result.success, "Widget source edit with inline comment");
    Require(result.lines == std::vector<std::string>({"@Meta { Version=2 Role=Home }", "", "Play Reaper 40044 // transport", "Stop Stop"}), "Widget source comment preservation");
}

static void TestWidgetSourceEditRemovesBindings() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Play Play", "Stop Stop"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Play", 1, {});
    Require(result.success, "Widget source binding removal");
    Require(result.lines == std::vector<std::string>({"@Meta { Version=2 Role=Home }", "", "Stop Stop"}), "Widget source binding removal result");
}

static void TestInvalidWidgetSourceEditRejection() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Play Play"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Play", 1, {"Play"});
    Require(!result.success, "invalid Widget source edit rejection");
    Require(result.lines.empty(), "invalid Widget source edit does not return writable lines");
}

static void TestModifierSourceEdit() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "ShiftButton Modifier Shift", "Stop Stop"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "ShiftButton", 1, {"ShiftButton Modifier Shift Mode=Momentary"});
    Require(result.success, "Modifier source edit");
    Require(result.lines == std::vector<std::string>({"@Meta { Version=2 Role=Home }", "", "ShiftButton Modifier Shift Mode=Momentary", "Stop Stop"}), "Modifier source replacement");
}

static void TestModifierAndChannelFamilySourceEditRejection() {
    const std::vector<std::string> source = {"@Meta { Version=2 Role=Home }", "", "Fader# TrackVolume", "Fader2 Modifier Shift"};
    const Format2ZoneWidgetEditResult result = EditFormat2ZoneWidgetSource("Home.zon", source, "Fader2", 8, {"Fader2 TrackPan"});
    Require(!result.success, "exact Modifier and channel-family source edit rejection");
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
    TestLayerOnlyAction();
    TestProfileLoader();
    TestExactWidgetSourceEdit();
    TestChannelFamilySourceEdit();
    TestMixedWidgetSourceEditRejection();
    TestWidgetSourceEditPreservesComment();
    TestWidgetSourceEditRemovesBindings();
    TestInvalidWidgetSourceEditRejection();
    TestModifierSourceEdit();
    TestModifierAndChannelFamilySourceEditRejection();
    std::cout << "Format2ZoneProfile tests passed\n";
    return 0;
}
