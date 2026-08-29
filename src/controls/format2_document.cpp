#include "format2_document.h"

#include <map>
#include <utility>

static std::string Format2AsciiLower(const std::string& value) {
    std::string lowered = value;
    for (char& character : lowered) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return lowered;
}

static bool IsFormat2OpeningDelimiter(Format2TokenKind kind) {
    return kind == Format2TokenKind::LeftBrace || kind == Format2TokenKind::LeftBracket || kind == Format2TokenKind::LeftParenthesis;
}

static bool IsFormat2ClosingDelimiter(Format2TokenKind kind) {
    return kind == Format2TokenKind::RightBrace || kind == Format2TokenKind::RightBracket || kind == Format2TokenKind::RightParenthesis;
}

static Format2TokenKind MatchingFormat2OpeningDelimiter(Format2TokenKind kind) {
    if (kind == Format2TokenKind::RightBrace) return Format2TokenKind::LeftBrace;
    if (kind == Format2TokenKind::RightBracket) return Format2TokenKind::LeftBracket;
    return Format2TokenKind::LeftParenthesis;
}

void ValidateFormat2Delimiters(Format2LexResult& lexical) {
    std::vector<const Format2Token*> openings;
    for (const Format2Token& token : lexical.tokens) {
        if (IsFormat2OpeningDelimiter(token.kind)) {
            openings.push_back(&token);
            continue;
        }
        if (!IsFormat2ClosingDelimiter(token.kind)) continue;
        const Format2TokenKind expectedOpening = MatchingFormat2OpeningDelimiter(token.kind);
        std::size_t matchingIndex = openings.size();
        while (matchingIndex > 0 && openings[matchingIndex - 1]->kind != expectedOpening) matchingIndex--;
        if (matchingIndex == 0) {
            lexical.diagnostics.push_back({"format2.delimiter.unexpected", "Unexpected closing delimiter " + token.text, token.location});
            continue;
        }
        while (openings.size() > matchingIndex) {
            const Format2Token* unclosed = openings.back();
            lexical.diagnostics.push_back({"format2.delimiter.unclosed", "Opening delimiter " + unclosed->text + " is not closed before " + token.text, unclosed->location});
            openings.pop_back();
        }
        openings.pop_back();
    }
    for (const Format2Token* unclosed : openings) lexical.diagnostics.push_back({"format2.delimiter.unclosed", "Opening delimiter " + unclosed->text + " is not closed before EOF", unclosed->location});
}

static bool IsFormat2MetadataKeyAllowed(Format2DocumentKind kind, const std::string& key) {
    if (key == "Version") return true;
    if (kind == Format2DocumentKind::MainZone) return key == "Role" || key == "Target" || key == "BankTarget" || key == "Alias";
    if (kind == Format2DocumentKind::FxZone) return key == "MatchFX" || key == "Alias";
    if (kind == Format2DocumentKind::Surface) return key == "Protocol" || key == "Name" || key == "Description";
    if (kind == Format2DocumentKind::Snippet) return key == "Name" || key == "Description";
    return false;
}

static bool IsFormat2QuotedMetadataKey(const std::string& key) {
    return key == "Name" || key == "Description" || key == "MatchFX" || key == "Alias";
}

class Format2DocumentParser {
public:
    Format2DocumentParser(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind) {
        this->result_.kind = kind;
        this->result_.lexical = LexFormat2Source(source, sourcePath);
    }

    Format2DocumentParseResult Parse() {
        ValidateFormat2Delimiters(this->result_.lexical);
        this->ParseMetadata();
        this->ValidateMetadata();
        this->result_.body = ParseFormat2Syntax(this->result_.lexical, this->result_.bodyTokenIndex);
        return std::move(this->result_);
    }

private:
    Format2DocumentParseResult result_;
    std::size_t tokenIndex_ = 0;

    const Format2Token& CurrentToken() const { return this->result_.lexical.tokens[this->tokenIndex_]; }

    bool AtEnd() const { return this->CurrentToken().kind == Format2TokenKind::EndOfFile; }

    void Advance() {
        if (!this->AtEnd()) this->tokenIndex_++;
    }

    void SkipNewLines() {
        while (this->CurrentToken().kind == Format2TokenKind::NewLine) this->Advance();
    }

    void AddDiagnostic(const std::string& code, const std::string& message, const Format2SourceLocation& location) {
        this->result_.lexical.diagnostics.push_back({ code, message, location });
    }

    void ParseMetadata() {
        this->SkipNewLines();
        if (this->AtEnd() || this->CurrentToken().kind != Format2TokenKind::Bare || this->CurrentToken().text != "@Meta") {
            this->AddDiagnostic("format2.metadata.required", "The first significant element must be @Meta", this->CurrentToken().location);
            this->result_.bodyTokenIndex = this->tokenIndex_;
            return;
        }
        this->Advance();
        this->SkipNewLines();
        if (this->CurrentToken().kind != Format2TokenKind::LeftBrace) {
            this->AddDiagnostic("format2.metadata.block", "@Meta must be followed by a brace block", this->CurrentToken().location);
            this->result_.bodyTokenIndex = this->tokenIndex_;
            return;
        }
        this->Advance();

        std::map<std::string, Format2SourceLocation> keys;
        while (!this->AtEnd() && this->CurrentToken().kind != Format2TokenKind::RightBrace) {
            this->SkipNewLines();
            if (this->AtEnd() || this->CurrentToken().kind == Format2TokenKind::RightBrace) break;
            if (this->CurrentToken().kind != Format2TokenKind::Bare) {
                this->AddDiagnostic("format2.metadata.key", "Expected a metadata key", this->CurrentToken().location);
                this->RecoverMetadataEntry();
                continue;
            }

            Format2MetadataEntry entry;
            entry.name = this->CurrentToken().text;
            entry.nameLocation = this->CurrentToken().location;
            this->Advance();
            if (this->CurrentToken().kind == Format2TokenKind::NewLine || this->CurrentToken().kind != Format2TokenKind::Equals) {
                this->AddDiagnostic("format2.metadata.assignment", "Metadata key " + entry.name + " must use Key=Value on one physical line", entry.nameLocation);
                this->RecoverMetadataEntry();
                continue;
            }
            this->Advance();
            if (this->CurrentToken().kind == Format2TokenKind::NewLine || (this->CurrentToken().kind != Format2TokenKind::Bare && this->CurrentToken().kind != Format2TokenKind::QuotedString)) {
                this->AddDiagnostic("format2.metadata.value", "Metadata key " + entry.name + " requires one value on the same physical line", entry.nameLocation);
                this->RecoverMetadataEntry();
                continue;
            }

            entry.value = this->CurrentToken().text;
            entry.quoted = this->CurrentToken().kind == Format2TokenKind::QuotedString;
            entry.valueLocation = this->CurrentToken().location;
            this->Advance();

            const std::string canonicalKey = Format2AsciiLower(entry.name);
            const auto existing = keys.find(canonicalKey);
            if (existing != keys.end()) {
                this->AddDiagnostic("format2.metadata.duplicate", "Metadata key " + entry.name + " duplicates a key declared earlier", entry.nameLocation);
            } else {
                keys[canonicalKey] = entry.nameLocation;
            }
            if (!IsFormat2MetadataKeyAllowed(this->result_.kind, entry.name)) this->AddDiagnostic("format2.metadata.unknown", "Metadata key " + entry.name + " is not valid for this document type", entry.nameLocation);
            if (IsFormat2QuotedMetadataKey(entry.name) != entry.quoted) {
                const std::string expectedType = IsFormat2QuotedMetadataKey(entry.name) ? "a quoted string" : "an unquoted value";
                this->AddDiagnostic("format2.metadata.value-type", "Metadata key " + entry.name + " requires " + expectedType, entry.valueLocation);
            }
            this->result_.metadata.entries.push_back(entry);
        }

        if (this->CurrentToken().kind != Format2TokenKind::RightBrace) {
            this->AddDiagnostic("format2.metadata.unclosed", "@Meta block is not closed before EOF", this->CurrentToken().location);
            this->result_.bodyTokenIndex = this->tokenIndex_;
            return;
        }
        this->Advance();
        this->result_.bodyTokenIndex = this->tokenIndex_;
    }

    void RecoverMetadataEntry() {
        while (!this->AtEnd() && this->CurrentToken().kind != Format2TokenKind::NewLine && this->CurrentToken().kind != Format2TokenKind::RightBrace) this->Advance();
    }

    const Format2MetadataEntry* FindMetadataEntry(const std::string& name) const {
        for (const Format2MetadataEntry& entry : this->result_.metadata.entries) {
            if (entry.name == name) return &entry;
        }
        return nullptr;
    }

    void ValidateMetadata() {
        const Format2MetadataEntry* version = this->FindMetadataEntry("Version");
        if (!version) {
            const Format2SourceLocation location = this->result_.metadata.entries.empty() ? this->CurrentToken().location : this->result_.metadata.entries.front().nameLocation;
            this->AddDiagnostic("format2.metadata.version.required", "@Meta requires Version=2", location);
        } else if (version->quoted || version->value != "2") {
            this->AddDiagnostic("format2.metadata.version.value", "Version must be the unquoted integer 2", version->valueLocation);
        } else {
            this->result_.metadata.version = 2;
        }

        this->ParseRole();
        this->ParseTarget();
        this->ParseBankTarget();
        this->ParseProtocol();
        this->ParseStringMetadata("Name", this->result_.metadata.name);
        this->ParseStringMetadata("Description", this->result_.metadata.description);
        this->ParseStringMetadata("MatchFX", this->result_.metadata.matchFx);
        this->ParseStringMetadata("Alias", this->result_.metadata.alias);
        this->ValidateMetadataCombinations();
    }

    void ParseRole() {
        const Format2MetadataEntry* entry = this->FindMetadataEntry("Role");
        if (!entry || entry->quoted) return;
        if (entry->value == "Home") this->result_.metadata.role = Format2ZoneRole::Home;
        else if (entry->value == "LastTouchedFXParam") this->result_.metadata.role = Format2ZoneRole::LastTouchedFxParam;
        else if (entry->value == "Layer") this->result_.metadata.role = Format2ZoneRole::Layer;
        else this->AddDiagnostic("format2.metadata.role.value", "Unknown Role value: " + entry->value, entry->valueLocation);
    }

    void ParseTarget() {
        const Format2MetadataEntry* entry = this->FindMetadataEntry("Target");
        if (!entry || entry->quoted) return;
        if (entry->value == "Tracks") this->result_.metadata.target = Format2ZoneTarget::Tracks;
        else if (entry->value == "SelectedTrack") this->result_.metadata.target = Format2ZoneTarget::SelectedTrack;
        else if (entry->value == "MasterTrack") this->result_.metadata.target = Format2ZoneTarget::MasterTrack;
        else if (entry->value == "FocusedFX") this->result_.metadata.target = Format2ZoneTarget::FocusedFx;
        else if (entry->value == "VCA") this->result_.metadata.target = Format2ZoneTarget::Vca;
        else if (entry->value == "Folder") this->result_.metadata.target = Format2ZoneTarget::Folder;
        else if (entry->value == "SelectedTracks") this->result_.metadata.target = Format2ZoneTarget::SelectedTracks;
        else this->AddDiagnostic("format2.metadata.target.value", "Unknown Target value: " + entry->value, entry->valueLocation);
    }

    void ParseBankTarget() {
        const Format2MetadataEntry* entry = this->FindMetadataEntry("BankTarget");
        if (!entry || entry->quoted) return;
        if (entry->value == "Sends") this->result_.metadata.bankTarget = Format2BankTarget::Sends;
        else if (entry->value == "Receives") this->result_.metadata.bankTarget = Format2BankTarget::Receives;
        else if (entry->value == "FX") this->result_.metadata.bankTarget = Format2BankTarget::Fx;
        else this->AddDiagnostic("format2.metadata.bank-target.value", "Unknown BankTarget value: " + entry->value, entry->valueLocation);
    }

    void ParseProtocol() {
        const Format2MetadataEntry* entry = this->FindMetadataEntry("Protocol");
        if (!entry || entry->quoted) return;
        if (entry->value == "MIDI") this->result_.metadata.protocol = Format2SurfaceProtocol::Midi;
        else if (entry->value == "OSC") this->result_.metadata.protocol = Format2SurfaceProtocol::Osc;
        else this->AddDiagnostic("format2.metadata.protocol.value", "Unknown Protocol value: " + entry->value, entry->valueLocation);
    }

    void ParseStringMetadata(const std::string& name, std::optional<std::string>& destination) {
        const Format2MetadataEntry* entry = this->FindMetadataEntry(name);
        if (entry && entry->quoted) destination = entry->value;
    }

    void ValidateMetadataCombinations() {
        if (this->result_.kind == Format2DocumentKind::Surface && !this->result_.metadata.protocol) {
            const Format2SourceLocation location = this->result_.metadata.entries.empty() ? this->CurrentToken().location : this->result_.metadata.entries.front().nameLocation;
            this->AddDiagnostic("format2.metadata.protocol.required", "Surface metadata requires Protocol=MIDI or Protocol=OSC", location);
        }
        if (this->result_.metadata.role && this->result_.metadata.target) {
            const Format2MetadataEntry* target = this->FindMetadataEntry("Target");
            this->AddDiagnostic("format2.metadata.role-target", "Role and Target cannot be used together", target ? target->nameLocation : this->CurrentToken().location);
        }
        if (!this->result_.metadata.bankTarget) return;
        const Format2MetadataEntry* bankTarget = this->FindMetadataEntry("BankTarget");
        if (!this->result_.metadata.target) {
            this->AddDiagnostic("format2.metadata.bank-target.context", "BankTarget requires a compatible Target", bankTarget ? bankTarget->nameLocation : this->CurrentToken().location);
            return;
        }
        const Format2ZoneTarget target = *this->result_.metadata.target;
        const Format2BankTarget child = *this->result_.metadata.bankTarget;
        const bool validTrackChild = (child == Format2BankTarget::Sends || child == Format2BankTarget::Receives) && (target == Format2ZoneTarget::Tracks || target == Format2ZoneTarget::SelectedTrack);
        const bool validFxChild = child == Format2BankTarget::Fx && (target == Format2ZoneTarget::Tracks || target == Format2ZoneTarget::SelectedTrack || target == Format2ZoneTarget::MasterTrack);
        if (!validTrackChild && !validFxChild) this->AddDiagnostic("format2.metadata.bank-target.context", "BankTarget is not compatible with the selected Target", bankTarget ? bankTarget->nameLocation : this->CurrentToken().location);
    }
};

Format2DocumentParseResult ParseFormat2DocumentSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind) {
    Format2DocumentParser parser(source, sourcePath, kind);
    return parser.Parse();
}
