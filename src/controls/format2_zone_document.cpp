#include "format2_zone_document.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

static std::string Format2ZoneAsciiLower(const std::string& value) {
    std::string lowered = value;
    for (char& character : lowered) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return lowered;
}

static std::string Format2ZoneIdFromPath(const std::string& sourcePath) {
    const std::size_t separator = sourcePath.find_last_of("/\\");
    const std::size_t start = separator == std::string::npos ? 0 : separator + 1;
    const std::size_t extension = sourcePath.find_last_of('.');
    const std::size_t end = extension == std::string::npos || extension < start ? sourcePath.size() : extension;
    return sourcePath.substr(start, end - start);
}

static bool IsFormat2LowercaseId(const std::string& value) {
    if (value.empty()) return false;
    for (char character : value) {
        if (!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_' || character == '-')) return false;
    }
    return true;
}

static bool IsFormat2StandardModifier(const std::string& name) {
    return name == "Shift" || name == "Option" || name == "Control" || name == "Alt" || name == "Flip" || name == "Global" || name == "Marker" || name == "Nudge" || name == "Zoom" || name == "Scrub";
}

static bool IsFormat2ChannelStateSelector(const std::string& name) {
    return name == "Touch" || name == "Toggle";
}

static bool IsFormat2ButtonEvent(const std::string& name) {
    return name == "Press" || name == "Tap" || name == "Release" || name == "Hold" || name == "LongHold" || name == "DoublePress";
}

static bool IsFormat2DirectionEvent(const std::string& name) {
    return name == "Increase" || name == "Decrease";
}

static bool IsFormat2Transform(const std::string& name) {
    return name == "Invert" || name == "InvertFB";
}

static std::optional<Format2LifecycleEvent> ParseFormat2LifecycleEvent(const std::string& name) {
    if (name == "SurfaceInitialization") return Format2LifecycleEvent::SurfaceInitialization;
    if (name == "TrackSelection") return Format2LifecycleEvent::TrackSelection;
    if (name == "PageEnter") return Format2LifecycleEvent::PageEnter;
    if (name == "PageExit") return Format2LifecycleEvent::PageExit;
    if (name == "PlaybackStart") return Format2LifecycleEvent::PlaybackStart;
    if (name == "PlaybackStop") return Format2LifecycleEvent::PlaybackStop;
    if (name == "RecordStart") return Format2LifecycleEvent::RecordStart;
    if (name == "RecordStop") return Format2LifecycleEvent::RecordStop;
    if (name == "ZoneActivation") return Format2LifecycleEvent::ZoneActivation;
    if (name == "ZoneDeactivation") return Format2LifecycleEvent::ZoneDeactivation;
    return std::nullopt;
}

class Format2ZoneDocumentParser {
public:
    Format2ZoneDocumentParser(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind) : kind_(kind) {
        this->result_.document = ParseFormat2DocumentSource(source, sourcePath, kind);
        this->result_.zone.id = Format2ZoneIdFromPath(sourcePath);
    }

    Format2ZoneDocumentParser(const std::vector<Format2SyntaxNode>& body) : kind_(Format2DocumentKind::FxZone), generatedBindings_(true) {
        this->result_.document.kind = Format2DocumentKind::FxZone;
        this->result_.document.body = body;
    }

    Format2ZoneParseResult Parse() {
        if (!this->generatedBindings_) this->ValidateKindAndIdentity();
        this->ParseBody();
        this->ValidateDeclarationsAndSelectors();
        if (!this->generatedBindings_) this->ValidateZoneRules();
        return std::move(this->result_);
    }

private:
    Format2DocumentKind kind_;
    Format2ZoneParseResult result_;
    bool includedZonesSeen_ = false;
    bool zoneLayersSeen_ = false;
    bool generatedBindings_ = false;

    void AddDiagnostic(const std::string& code, const std::string& message, const Format2SourceLocation& location) {
        this->result_.document.lexical.diagnostics.push_back({ code, message, location });
    }

    Format2SourceLocation DocumentLocation() const {
        if (!this->result_.document.metadata.entries.empty()) return this->result_.document.metadata.entries.front().nameLocation;
        if (!this->result_.document.lexical.tokens.empty()) return this->result_.document.lexical.tokens.front().location;
        return {};
    }

    void ValidateKindAndIdentity() {
        if (this->kind_ != Format2DocumentKind::MainZone && this->kind_ != Format2DocumentKind::FxZone && this->kind_ != Format2DocumentKind::Snippet) {
            this->AddDiagnostic("format2.zone.document-kind", "The Zone semantic parser accepts only Main zone, FX zone, or snippet documents", this->DocumentLocation());
            return;
        }
        if (!this->result_.zone.id.empty()) {
            const bool validId = this->kind_ == Format2DocumentKind::Snippet ? IsFormat2LowercaseId(this->result_.zone.id) : IsValidFormat2Identifier(this->result_.zone.id);
            if (!validId) this->AddDiagnostic("format2.zone.id", "The filename stem is not a valid " + std::string(this->kind_ == Format2DocumentKind::Snippet ? "lowercase snippet ID" : "zone ID") + ": " + this->result_.zone.id, this->DocumentLocation());
        }
        if (this->kind_ == Format2DocumentKind::FxZone && !this->result_.document.metadata.matchFx) this->AddDiagnostic("format2.zone.fx.match.required", "An FX zone requires quoted MatchFX metadata", this->DocumentLocation());
    }

    void ParseBody() {
        for (const Format2SyntaxNode& node : this->result_.document.body) {
            if (node.kind == Format2SyntaxNodeKind::Block) this->ParseBlock(node);
            else this->ParseBindingOrDeclaration(node);
        }
    }

    void ParseBlock(const Format2SyntaxNode& node) {
        if (node.positionalTokens.empty() || node.positionalTokens[0].kind != Format2TokenKind::Bare) {
            this->AddDiagnostic("format2.zone.block.name", "A Zone block requires an unquoted block name", node.location);
            return;
        }
        const std::string& name = node.positionalTokens[0].text;
        if (this->generatedBindings_ && name != "On") {
            this->AddDiagnostic("format2.learn-fx.generated.block", "GeneratedBindings accepts only normal bindings and lifecycle blocks", node.positionalTokens[0].location);
            return;
        }
        if (name == "IncludedZones") {
            if (this->includedZonesSeen_) this->AddDiagnostic("format2.zone.reference.block.duplicate", "IncludedZones can occur only once", node.location);
            this->includedZonesSeen_ = true;
            this->ParseReferenceBlock(node, this->result_.zone.includedZones);
            return;
        }
        if (name == "ZoneLayers") {
            if (this->zoneLayersSeen_) this->AddDiagnostic("format2.zone.reference.block.duplicate", "ZoneLayers can occur only once", node.location);
            this->zoneLayersSeen_ = true;
            this->ParseReferenceBlock(node, this->result_.zone.zoneLayers);
            return;
        }
        if (name == "On") {
            this->ParseLifecycleBlock(node);
            return;
        }
        this->AddDiagnostic("format2.zone.block.unknown", "Unknown Zone block: " + name, node.positionalTokens[0].location);
    }

    void ParseReferenceBlock(const Format2SyntaxNode& node, std::vector<Format2ZoneReference>& destination) {
        const std::string blockName = node.positionalTokens.empty() ? "Zone reference" : node.positionalTokens[0].text;
        if (node.positionalTokens.size() != 1 || !node.properties.empty()) this->AddDiagnostic("format2.zone.reference.header", blockName + " does not accept an ID or properties", node.location);
        if (this->kind_ == Format2DocumentKind::FxZone) this->AddDiagnostic("format2.zone.reference.fx", blockName + " is not valid in an FX zone", node.location);
        std::map<std::string, Format2SourceLocation> references;
        for (const Format2ZoneReference& reference : destination) references[Format2ZoneAsciiLower(reference.id)] = reference.location;
        int validEntryCount = 0;
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind != Format2SyntaxNodeKind::Line || child.positionalTokens.size() != 1 || child.positionalTokens[0].kind != Format2TokenKind::Bare || !child.properties.empty()) {
                this->AddDiagnostic("format2.zone.reference.entry", blockName + " entries must contain one zone ID", child.location);
                continue;
            }
            const Format2Token& token = child.positionalTokens[0];
            if (!IsValidFormat2Identifier(token.text)) {
                this->AddDiagnostic("format2.zone.reference.id", blockName + " entry is not a valid zone ID: " + token.text, token.location);
                continue;
            }
            validEntryCount++;
            const std::string canonicalId = Format2ZoneAsciiLower(token.text);
            if (references.find(canonicalId) != references.end()) {
                this->AddDiagnostic("format2.zone.reference.duplicate", blockName + " repeats zone ID " + token.text, token.location);
                continue;
            }
            references[canonicalId] = token.location;
            destination.push_back({ token.text, token.location });
        }
        if (validEntryCount == 0) this->AddDiagnostic("format2.zone.reference.required", blockName + " requires at least one zone ID", node.location);
    }

    void ParseLifecycleBlock(const Format2SyntaxNode& node) {
        if (node.positionalTokens.size() != 2 || node.positionalTokens[1].kind != Format2TokenKind::Bare || !node.properties.empty()) {
            this->AddDiagnostic("format2.zone.lifecycle.header", "A lifecycle block requires On followed by one event name", node.location);
            return;
        }
        const std::optional<Format2LifecycleEvent> event = ParseFormat2LifecycleEvent(node.positionalTokens[1].text);
        if (!event) {
            this->AddDiagnostic("format2.zone.lifecycle.event", "Unknown lifecycle event: " + node.positionalTokens[1].text, node.positionalTokens[1].location);
            return;
        }
        Format2LifecycleBlock block;
        block.event = *event;
        block.location = node.location;
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind != Format2SyntaxNodeKind::Line) {
                this->AddDiagnostic("format2.zone.lifecycle.action", "Lifecycle blocks contain action lines, not nested blocks", child.location);
                continue;
            }
            const std::optional<Format2ZoneAction> action = this->ParseActionLine(child, true);
            if (action) block.actions.push_back(*action);
        }
        if (block.actions.empty()) this->AddDiagnostic("format2.zone.lifecycle.action.required", "A lifecycle block requires at least one action", node.location);
        this->result_.zone.lifecycleBlocks.push_back(std::move(block));
    }

    void ParseBindingOrDeclaration(const Format2SyntaxNode& node) {
        std::size_t tokenIdx = 0;
        std::vector<Format2ZoneSelector> selectors;
        while (tokenIdx < node.positionalTokens.size() && (node.positionalTokens[tokenIdx].kind == Format2TokenKind::LeftBracket || node.positionalTokens[tokenIdx].kind == Format2TokenKind::LeftParenthesis)) {
            const Format2TokenKind openingKind = node.positionalTokens[tokenIdx].kind;
            const Format2TokenKind closingKind = openingKind == Format2TokenKind::LeftBracket ? Format2TokenKind::RightBracket : Format2TokenKind::RightParenthesis;
            if (tokenIdx + 3 >= node.positionalTokens.size() || node.positionalTokens[tokenIdx + 1].kind != Format2TokenKind::Bare || node.positionalTokens[tokenIdx + 2].kind != closingKind || node.positionalTokens[tokenIdx + 3].kind != Format2TokenKind::Plus) {
                this->AddDiagnostic("format2.zone.binding.selector", "A binding selector must use [Name]+ or (Name)+ before the Widget", node.positionalTokens[tokenIdx].location);
                return;
            }
            const Format2Token& name = node.positionalTokens[tokenIdx + 1];
            selectors.push_back({ openingKind == Format2TokenKind::LeftBracket ? Format2ZoneSelectorKind::Context : Format2ZoneSelectorKind::Input, name.text, name.location });
            tokenIdx += 4;
        }
        if (tokenIdx >= node.positionalTokens.size() || node.positionalTokens[tokenIdx].kind != Format2TokenKind::Bare) {
            this->AddDiagnostic("format2.zone.binding.widget", "A binding requires one Widget after its selectors", node.location);
            return;
        }
        const Format2WidgetSelectorParseResult widgetResult = ParseFormat2WidgetSelector(node.positionalTokens[tokenIdx]);
        for (const Format2Diagnostic& diagnostic : widgetResult.diagnostics) this->AddDiagnostic(diagnostic.code, diagnostic.message, diagnostic.location);
        if (!widgetResult.selector) return;
        tokenIdx++;
        if (tokenIdx >= node.positionalTokens.size() || node.positionalTokens[tokenIdx].kind != Format2TokenKind::Bare) {
            this->AddDiagnostic("format2.zone.binding.action", "A binding requires one unquoted action after the Widget", node.location);
            return;
        }

        Format2ZoneAction action;
        action.action = node.positionalTokens[tokenIdx].text;
        action.actionLocation = node.positionalTokens[tokenIdx].location;
        if (!IsValidFormat2Identifier(action.action)) this->AddDiagnostic("format2.zone.binding.action.name", "Action name is not a valid identifier: " + action.action, action.actionLocation);
        action.properties = node.properties;
        tokenIdx++;
        for (; tokenIdx < node.positionalTokens.size(); tokenIdx++) {
            const Format2Token& token = node.positionalTokens[tokenIdx];
            if (token.kind != Format2TokenKind::Bare && token.kind != Format2TokenKind::QuotedString) {
                this->AddDiagnostic("format2.zone.binding.argument", "Action positional parameters must be bare values or quoted strings", token.location);
                continue;
            }
            action.arguments.push_back({ token.text, token.kind == Format2TokenKind::QuotedString, token.location });
        }
        this->ValidatePropertyPlacementAndDuplicates(node, action.properties);

        if (action.action == "Modifier" || action.action == "PseudoModifier") {
            if (this->generatedBindings_) {
                this->AddDiagnostic("format2.learn-fx.generated.modifier", "GeneratedBindings does not accept modifier declarations", action.actionLocation);
                return;
            }
            this->ParseModifierDeclaration(node, *widgetResult.selector, selectors, action);
            return;
        }

        Format2ZoneBinding binding;
        binding.location = node.location;
        binding.selectors = std::move(selectors);
        binding.widget = *widgetResult.selector;
        binding.action = std::move(action);
        this->ValidateBindingSelectors(binding);
        this->result_.zone.bindings.push_back(std::move(binding));
    }

    std::optional<Format2ZoneAction> ParseActionLine(const Format2SyntaxNode& node, bool lifecycle) {
        if (node.positionalTokens.empty() || node.positionalTokens[0].kind != Format2TokenKind::Bare) {
            this->AddDiagnostic("format2.zone.action.name", "An action line requires one unquoted action name", node.location);
            return std::nullopt;
        }
        Format2ZoneAction action;
        action.action = node.positionalTokens[0].text;
        action.actionLocation = node.positionalTokens[0].location;
        if (!IsValidFormat2Identifier(action.action)) this->AddDiagnostic("format2.zone.action.name.invalid", "Action name is not a valid identifier: " + action.action, action.actionLocation);
        action.properties = node.properties;
        for (std::size_t tokenIdx = 1; tokenIdx < node.positionalTokens.size(); tokenIdx++) {
            const Format2Token& token = node.positionalTokens[tokenIdx];
            if (token.kind != Format2TokenKind::Bare && token.kind != Format2TokenKind::QuotedString) {
                this->AddDiagnostic("format2.zone.action.argument", std::string(lifecycle ? "Lifecycle" : "Zone") + " action parameters cannot contain binding selectors", token.location);
                continue;
            }
            action.arguments.push_back({ token.text, token.kind == Format2TokenKind::QuotedString, token.location });
        }
        this->ValidatePropertyPlacementAndDuplicates(node, action.properties);
        return action;
    }

    void ValidatePropertyPlacementAndDuplicates(const Format2SyntaxNode& node, const std::vector<Format2PropertySyntax>& properties) {
        std::set<std::string> propertyNames;
        std::size_t firstPropertyOffset = node.endLocation.offset + 1;
        for (const Format2PropertySyntax& property : properties) {
            firstPropertyOffset = (std::min)(firstPropertyOffset, property.nameLocation.offset);
            if (!IsValidFormat2Identifier(property.name)) this->AddDiagnostic("format2.zone.property.name", "Named property is not a valid identifier: " + property.name, property.nameLocation);
            if (!propertyNames.insert(property.name).second) this->AddDiagnostic("format2.zone.property.duplicate", "Named property is duplicated: " + property.name, property.nameLocation);
        }
        for (const Format2Token& token : node.positionalTokens) {
            if (token.location.offset > firstPropertyOffset) {
                this->AddDiagnostic("format2.zone.property.order", "Positional parameters must come before named properties", token.location);
                break;
            }
        }
    }

    void ParseModifierDeclaration(const Format2SyntaxNode& node, const Format2WidgetSelector& widget, const std::vector<Format2ZoneSelector>& selectors, const Format2ZoneAction& action) {
        if (!selectors.empty()) this->AddDiagnostic("format2.zone.modifier.selector", "A modifier declaration cannot have binding selectors", selectors.front().location);
        if (widget.kind != Format2WidgetSelectorKind::Exact) this->AddDiagnostic("format2.zone.modifier.widget", "A modifier declaration requires one exact Widget", widget.location);
        const bool standard = action.action == "Modifier";
        if ((standard && action.arguments.size() != 1) || (!standard && !action.arguments.empty())) {
            this->AddDiagnostic("format2.zone.modifier.argument", standard ? "Modifier requires one standard modifier name" : "PseudoModifier does not accept a separate modifier name", action.actionLocation);
            return;
        }
        Format2ModifierDeclaration declaration;
        declaration.kind = standard ? Format2ModifierDeclarationKind::Standard : Format2ModifierDeclarationKind::Pseudo;
        declaration.location = node.location;
        declaration.widget = widget;
        declaration.name = standard ? action.arguments[0].text : widget.baseName;
        declaration.nameLocation = standard ? action.arguments[0].location : widget.location;
        if (standard && (action.arguments[0].quoted || !IsFormat2StandardModifier(declaration.name))) this->AddDiagnostic("format2.zone.modifier.name", "Unknown standard modifier name: " + declaration.name, declaration.nameLocation);
        if (!standard && (IsFormat2StandardModifier(declaration.name) || IsFormat2ChannelStateSelector(declaration.name))) this->AddDiagnostic("format2.zone.modifier.name.reserved", "PseudoModifier cannot use reserved selector name " + declaration.name, declaration.nameLocation);
        for (const Format2PropertySyntax& property : action.properties) {
            if (property.name != "Mode") {
                this->AddDiagnostic("format2.zone.modifier.property", "Unknown modifier declaration property: " + property.name, property.nameLocation);
                continue;
            }
            if (property.value.list || property.value.scalar.quoted) {
                this->AddDiagnostic("format2.zone.modifier.mode", "Modifier Mode must be Momentary, Latch, or Hybrid", property.value.location);
            } else if (property.value.scalar.text == "Momentary") {
                declaration.mode = Format2ModifierMode::Momentary;
            } else if (property.value.scalar.text == "Latch") {
                declaration.mode = Format2ModifierMode::Latch;
            } else if (property.value.scalar.text == "Hybrid") {
                declaration.mode = Format2ModifierMode::Hybrid;
            } else {
                this->AddDiagnostic("format2.zone.modifier.mode", "Unknown modifier Mode: " + property.value.scalar.text, property.value.location);
            }
        }
        this->result_.zone.modifiers.push_back(std::move(declaration));
    }

    void ValidateBindingSelectors(const Format2ZoneBinding& binding) {
        std::set<std::string> contextSelectors;
        std::set<std::string> inputSelectors;
        int buttonEventCount = 0;
        int directionCount = 0;
        for (const Format2ZoneSelector& selector : binding.selectors) {
            std::set<std::string>& seen = selector.kind == Format2ZoneSelectorKind::Context ? contextSelectors : inputSelectors;
            if (!seen.insert(selector.name).second) this->AddDiagnostic("format2.zone.binding.selector.duplicate", "Binding selector is repeated: " + selector.name, selector.location);
            if (selector.kind == Format2ZoneSelectorKind::Context) {
                if (!IsValidFormat2Identifier(selector.name)) this->AddDiagnostic("format2.zone.binding.context", "Context selector requires an identifier", selector.location);
                continue;
            }
            if (!IsFormat2ButtonEvent(selector.name) && !IsFormat2DirectionEvent(selector.name) && !IsFormat2Transform(selector.name)) this->AddDiagnostic("format2.zone.binding.input", "Unknown input selector: " + selector.name, selector.location);
            if (IsFormat2ButtonEvent(selector.name)) buttonEventCount++;
            if (IsFormat2DirectionEvent(selector.name)) directionCount++;
        }
        if (buttonEventCount > 1) this->AddDiagnostic("format2.zone.binding.button-event", "A binding cannot contain more than one button event", binding.location);
        if (directionCount > 1) this->AddDiagnostic("format2.zone.binding.direction", "A binding cannot contain both Increase and Decrease", binding.location);
        if (buttonEventCount > 0 && directionCount > 0) this->AddDiagnostic("format2.zone.binding.event-direction", "A button event cannot be combined with a relative direction", binding.location);
    }

    void ValidateDeclarationsAndSelectors() {
        std::map<std::string, Format2SourceLocation> modifierNames;
        std::set<std::string> exactModifierNames;
        std::map<std::string, Format2SourceLocation> modifierWidgets;
        for (const Format2ModifierDeclaration& declaration : this->result_.zone.modifiers) {
            const std::string canonicalName = Format2ZoneAsciiLower(declaration.name);
            const std::string canonicalWidget = Format2ZoneAsciiLower(declaration.widget.baseName);
            if (modifierNames.find(canonicalName) != modifierNames.end()) this->AddDiagnostic("format2.zone.modifier.name.duplicate", "Modifier name is declared more than once: " + declaration.name, declaration.nameLocation);
            else modifierNames[canonicalName] = declaration.nameLocation;
            exactModifierNames.insert(declaration.name);
            if (modifierWidgets.find(canonicalWidget) != modifierWidgets.end()) this->AddDiagnostic("format2.zone.modifier.widget.duplicate", "Widget is used by more than one modifier declaration: " + declaration.widget.baseName, declaration.widget.location);
            else modifierWidgets[canonicalWidget] = declaration.widget.location;
        }
        for (const Format2ZoneBinding& binding : this->result_.zone.bindings) {
            for (const Format2ZoneSelector& selector : binding.selectors) {
                if (selector.kind != Format2ZoneSelectorKind::Context || IsFormat2StandardModifier(selector.name) || IsFormat2ChannelStateSelector(selector.name)) continue;
                if (exactModifierNames.find(selector.name) == exactModifierNames.end()) this->AddDiagnostic("format2.zone.binding.context.unknown", "Unknown or incorrectly cased context selector: " + selector.name, selector.location);
            }
        }
    }

    void ValidateZoneRules() {
        if (this->kind_ != Format2DocumentKind::MainZone || !this->result_.document.metadata.role || *this->result_.document.metadata.role != Format2ZoneRole::Layer) return;
        if (!this->result_.zone.includedZones.empty()) this->AddDiagnostic("format2.zone.layer.included", "A Role=Layer zone cannot declare IncludedZones", this->result_.zone.includedZones.front().location);
    }
};

Format2ZoneParseResult ParseFormat2ZoneDocumentSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind) {
    Format2ZoneDocumentParser parser(source, sourcePath, kind);
    return parser.Parse();
}

Format2ZoneParseResult ParseFormat2GeneratedBindings(const std::vector<Format2SyntaxNode>& body) {
    Format2ZoneDocumentParser parser(body);
    return parser.Parse();
}
