#pragma once

#include <optional>
#include <string>
#include <vector>

#include "format2_lexer.h"
#include "format2_syntax.h"

enum class Format2DocumentKind {
    MainZone,
    FxZone,
    Surface,
    LearnFx,
    Snippet,
};

enum class Format2ZoneRole {
    Home,
    LastTouchedFxParam,
    Layer,
};

enum class Format2ZoneTarget {
    Tracks,
    SelectedTrack,
    MasterTrack,
    FocusedFx,
    Vca,
    Folder,
    SelectedTracks,
};

enum class Format2BankTarget {
    Sends,
    Receives,
    Fx,
};

enum class Format2SurfaceProtocol {
    Midi,
    Osc,
};

struct Format2MetadataEntry {
    std::string name;
    std::string value;
    bool quoted = false;
    Format2SourceLocation nameLocation;
    Format2SourceLocation valueLocation;
};

struct Format2DocumentMetadata {
    int version = 0;
    std::optional<int> channels;
    std::optional<Format2ZoneRole> role;
    std::optional<Format2ZoneTarget> target;
    std::optional<Format2BankTarget> bankTarget;
    std::optional<Format2SurfaceProtocol> protocol;
    std::optional<std::string> name;
    std::optional<std::string> description;
    std::optional<std::string> matchFx;
    std::optional<std::string> alias;
    std::vector<Format2MetadataEntry> entries;
};

struct Format2DocumentParseResult {
    Format2DocumentKind kind = Format2DocumentKind::MainZone;
    Format2LexResult lexical;
    Format2DocumentMetadata metadata;
    std::vector<Format2SyntaxNode> body;
    std::size_t bodyTokenIndex = 0;

    bool IsValid() const { return !HasFormat2DiagnosticErrors(this->lexical.diagnostics); }
};

Format2DocumentParseResult ParseFormat2DocumentSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind);
void ValidateFormat2Delimiters(Format2LexResult& lexical);
