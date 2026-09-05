#pragma once

#include <optional>
#include <string>
#include <vector>

#include "format2_action_metadata.h"
#include "format2_document.h"

enum class Format2ZoneSelectorKind {
    Context,
    Input,
};

struct Format2ZoneSelector {
    Format2ZoneSelectorKind kind = Format2ZoneSelectorKind::Context;
    std::string name;
    Format2SourceLocation location;
};

struct Format2ZoneAction {
    std::string action;
    Format2SourceLocation actionLocation;
    std::vector<Format2ScalarSyntax> arguments;
    std::vector<Format2PropertySyntax> properties;
};

struct Format2ZoneBinding {
    Format2SourceLocation location;
    std::vector<Format2ZoneSelector> selectors;
    Format2WidgetSelector widget;
    Format2ZoneAction action;
};

enum class Format2ModifierDeclarationKind {
    Standard,
    Pseudo,
};

enum class Format2ModifierMode {
    Default,
    Momentary,
    Latch,
    Hybrid,
};

struct Format2ModifierDeclaration {
    Format2ModifierDeclarationKind kind = Format2ModifierDeclarationKind::Standard;
    Format2SourceLocation location;
    Format2WidgetSelector widget;
    std::string name;
    Format2SourceLocation nameLocation;
    Format2ModifierMode mode = Format2ModifierMode::Default;
};

struct Format2ZoneReference {
    std::string id;
    Format2SourceLocation location;
};

struct Format2ZoneNavigationReference {
    Format2ZoneNavigationKind kind = Format2ZoneNavigationKind::IndependentZone;
    std::string id;
    Format2SourceLocation location;
};

enum class Format2LifecycleEvent {
    SurfaceInitialization,
    TrackSelection,
    PageEnter,
    PageExit,
    PlaybackStart,
    PlaybackStop,
    RecordStart,
    RecordStop,
    ZoneActivation,
    ZoneDeactivation,
};

struct Format2LifecycleBlock {
    Format2LifecycleEvent event = Format2LifecycleEvent::ZoneActivation;
    Format2SourceLocation location;
    std::vector<Format2ZoneAction> actions;
};

struct Format2ZoneDocument {
    std::string id;
    std::vector<Format2ModifierDeclaration> modifiers;
    std::vector<Format2ZoneBinding> bindings;
    std::vector<Format2ZoneReference> includedZones;
    std::vector<Format2ZoneReference> zoneLayers;
    std::vector<Format2ZoneNavigationReference> navigationReferences;
    std::vector<Format2LifecycleBlock> lifecycleBlocks;
};

struct Format2ZoneParseResult {
    Format2DocumentParseResult document;
    Format2ZoneDocument zone;

    bool IsValid() const { return this->document.IsValid(); }
};

Format2ZoneParseResult ParseFormat2ZoneDocumentSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind);
Format2ZoneParseResult ParseFormat2GeneratedBindings(const std::vector<Format2SyntaxNode>& body);
