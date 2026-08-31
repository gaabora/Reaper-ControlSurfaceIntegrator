#include "format2_surface_document.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <set>
#include <system_error>
#include <utility>

#include "format2_property_constraints.h"
#include "format2_value_validation.h"

static bool IsFormat2ProfileBlock(const std::string& type) {
    return type == "EncoderProfile" || type == "ValueProfile" || type == "ColorProfile" || type == "RingProfile" || type == "BarProfile" || type == "MeterProfile" || type == "TextProfile";
}

static bool ParsePositiveFormat2Integer(const Format2ValueSyntax& value, int& parsedValue) {
    if (value.list || value.scalar.quoted || value.scalar.text.empty()) return false;
    const char* begin = value.scalar.text.data();
    const char* end = begin + value.scalar.text.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsedValue);
    return result.ec == std::errc() && result.ptr == end && parsedValue > 0;
}

static const Format2PropertySyntax* FindFormat2PrimitiveProperty(const Format2SurfacePrimitive& primitive, const std::string& name) {
    for (const Format2PropertySyntax& property : primitive.properties) {
        if (property.name == name) return &property;
    }
    return nullptr;
}

static bool HasFormat2PrimitiveNestedBlock(const Format2SurfacePrimitive& primitive, const std::string& name) {
    for (const Format2SyntaxNode& block : primitive.nestedBlocks) {
        if (!block.positionalTokens.empty() && block.positionalTokens[0].text == name) return true;
    }
    return false;
}

static void AddFormat2Capability(std::vector<Format2Capability>& capabilities, Format2Capability capability) {
    for (Format2Capability existing : capabilities) {
        if (existing == capability) return;
    }
    capabilities.push_back(capability);
}

static bool Format2PrimitiveMatchesRepresentation(const Format2SurfacePrimitive& primitive, const Format2RepresentationDefinition& definition) {
    for (const std::string& required : definition.requiredProperties) {
        if (!FindFormat2PrimitiveProperty(primitive, required)) return false;
    }
    for (const Format2PropertySyntax& property : primitive.properties) {
        if (!Format2RepresentationAllowsProperty(definition, property.name)) return false;
    }
    for (const Format2SyntaxNode& block : primitive.nestedBlocks) {
        if (block.positionalTokens.empty() || !Format2RepresentationAllowsNestedBlock(definition, block.positionalTokens[0].text)) return false;
    }
    return true;
}

static bool Format2PrimitiveMatchesCapabilityCondition(const Format2SurfacePrimitive& primitive, const Format2CapabilityCondition& condition) {
    for (const std::string& property : condition.allProperties) {
        if (!FindFormat2PrimitiveProperty(primitive, property)) return false;
    }
    if (!condition.equalProperty.empty()) {
        const Format2PropertySyntax* property = FindFormat2PrimitiveProperty(primitive, condition.equalProperty);
        if (!property || property->value.list || property->value.scalar.quoted || property->value.scalar.text != condition.equalValue) return false;
    }
    if (!condition.nestedBlock.empty() && !HasFormat2PrimitiveNestedBlock(primitive, condition.nestedBlock)) return false;
    if (!condition.listProperty.empty()) {
        const Format2PropertySyntax* property = FindFormat2PrimitiveProperty(primitive, condition.listProperty);
        if (!property || !property->value.list) return false;
        bool found = false;
        for (const Format2ScalarSyntax& item : property->value.items) {
            for (const std::string& expected : condition.anyListItems) {
                if (!item.quoted && item.text == expected) found = true;
            }
        }
        if (!found) return false;
    }
    return true;
}

struct Format2ParsedProfileBody {
    std::map<std::string, const Format2PropertySyntax*> properties;
    std::map<std::string, std::vector<const Format2SyntaxNode*>> lines;
};

static const Format2PropertySyntax* FindFormat2NodeProperty(const Format2SyntaxNode& node, const std::string& name) {
    for (const Format2PropertySyntax& property : node.properties) {
        if (property.name == name) return &property;
    }
    return nullptr;
}

class Format2SurfaceDocumentParser {
public:
    Format2SurfaceDocumentParser(const std::string& source, const std::string& sourcePath) {
        this->result_.document = ParseFormat2DocumentSource(source, sourcePath, Format2DocumentKind::Surface);
    }

    Format2SurfaceParseResult Parse() {
        this->ParseTopLevel();
        this->ValidateProfileReferences();
        this->ValidateFeedbackGroups();
        this->ValidateColorCalibration();
        this->ValidateOskLayout();
        if (this->result_.surface.widgets.empty()) {
            const Format2SourceLocation location = this->result_.document.metadata.entries.empty() ? Format2SourceLocation{} : this->result_.document.metadata.entries.front().nameLocation;
            this->AddDiagnostic("format2.surface.widget.required", "A Surface document requires at least one Widget block", location);
        }
        return std::move(this->result_);
    }

private:
    Format2SurfaceParseResult result_;
    std::map<std::string, Format2SourceLocation> widgetIds_;
    std::map<std::string, Format2SourceLocation> namedBlockIds_;
    std::map<std::string, Format2SourceLocation> encoderProfileIds_;
    std::map<std::string, Format2SourceLocation> valueProfileIds_;
    std::map<std::string, Format2ValueProfileDirection> valueProfileDirections_;
    std::map<std::string, Format2SourceLocation> colorProfileIds_;
    std::map<std::string, bool> colorProfileMidiSafe_;
    std::map<std::string, Format2SourceLocation> ringProfileIds_;
    std::map<std::string, std::optional<int>> ringProfileSegments_;
    std::map<std::string, bool> ringProfileMidiSafe_;
    std::map<std::string, Format2SourceLocation> barProfileIds_;
    std::map<std::string, Format2SourceLocation> meterProfileIds_;
    std::map<std::string, bool> meterProfileMidiSafe_;
    std::map<std::string, Format2SourceLocation> textProfileIds_;
    std::map<std::string, Format2TextEncoding> textProfileEncodings_;
    std::map<std::string, std::optional<int>> textProfileWidths_;

    void AddDiagnostic(const std::string& code, const std::string& message, const Format2SourceLocation& location) {
        this->result_.document.lexical.diagnostics.push_back({ code, message, location });
    }

    void ParseTopLevel() {
        for (const Format2SyntaxNode& node : this->result_.document.body) {
            if (node.kind != Format2SyntaxNodeKind::Block) {
                this->AddDiagnostic("format2.surface.top-level", "Surface top-level declarations must be brace blocks", node.location);
                continue;
            }
            if (node.positionalTokens.empty() || node.positionalTokens[0].kind != Format2TokenKind::Bare) {
                this->AddDiagnostic("format2.surface.block.header", "Surface block requires an unquoted block type", node.location);
                continue;
            }

            const std::string& blockType = node.positionalTokens[0].text;
            if (blockType == "Widget") this->ParseWidget(node);
            else if (blockType == "EncoderProfile") this->ParseEncoderProfile(node);
            else if (blockType == "ValueProfile") this->ParseValueProfile(node);
            else if (blockType == "ColorProfile") this->ParseColorProfile(node);
            else if (blockType == "RingProfile") this->ParseRingProfile(node);
            else if (blockType == "BarProfile") this->ParseBarProfile(node);
            else if (blockType == "MeterProfile") this->ParseMeterProfile(node);
            else if (blockType == "TextProfile") this->ParseTextProfile(node);
            else if (IsFormat2ProfileBlock(blockType)) this->ParseNamedBlock(node, this->result_.surface.profiles);
            else if (blockType == "FeedbackGroup") this->ParseFeedbackGroup(node);
            else if (blockType == "ColorCalibration") this->ParseColorCalibration(node);
            else if (blockType == "Initialize") this->ParseInitialization(node);
            else if (blockType == "OSKLayout") this->ParseOskLayout(node);
            else this->AddDiagnostic("format2.surface.block.unknown", "Unknown Surface block: " + blockType, node.location);
        }
    }

    void ParseNamedBlock(const Format2SyntaxNode& node, std::vector<Format2SurfaceNamedBlock>& destination) {
        if (node.positionalTokens.size() != 2 || node.positionalTokens[1].kind != Format2TokenKind::Bare || !node.properties.empty()) {
            this->AddDiagnostic("format2.surface.named-block.header", node.positionalTokens[0].text + " requires exactly one identifier", node.location);
            return;
        }
        const std::string& blockType = node.positionalTokens[0].text;
        const std::string& blockId = node.positionalTokens[1].text;
        if (!IsValidFormat2Identifier(blockId)) {
            this->AddDiagnostic("format2.surface.named-block.id", blockType + " ID is not a valid identifier: " + blockId, node.positionalTokens[1].location);
            return;
        }

        const std::string scopedId = blockType + "\n" + blockId;
        const auto existing = this->namedBlockIds_.find(scopedId);
        if (existing != this->namedBlockIds_.end()) {
            this->AddDiagnostic("format2.surface.named-block.duplicate", blockType + " ID is duplicated: " + blockId, node.positionalTokens[1].location);
            return;
        }
        this->namedBlockIds_[scopedId] = node.positionalTokens[1].location;
        destination.push_back({ blockType, blockId, node.location, node });
    }

    void ParseEncoderProfile(const Format2SyntaxNode& node) {
        const std::size_t profileCount = this->result_.surface.profiles.size();
        this->ParseNamedBlock(node, this->result_.surface.profiles);
        if (this->result_.surface.profiles.size() == profileCount) return;

        Format2EncoderProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->encoderProfileIds_[profile.id] = node.positionalTokens[1].location;
        const Format2ProfileDefinition* definition = FindFormat2ProfileDefinition("EncoderProfile");
        if (!definition) {
            this->AddDiagnostic("format2.surface.schema.profile", "The Surface I/O schema has no EncoderProfile definition", profile.location);
            return;
        }
        std::map<std::string, const Format2PropertySyntax*> properties;
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind == Format2SyntaxNodeKind::Block) {
                this->AddDiagnostic("format2.surface.encoder-profile.block", "EncoderProfile cannot contain nested blocks", child.location);
                continue;
            }
            if (!child.positionalTokens.empty()) this->AddDiagnostic("format2.surface.encoder-profile.line", "EncoderProfile lines can contain only named properties", child.positionalTokens.front().location);
            for (const Format2PropertySyntax& property : child.properties) {
                if (properties.find(property.name) != properties.end()) {
                    this->AddDiagnostic("format2.surface.encoder-profile.property.duplicate", "EncoderProfile property is duplicated: " + property.name, property.nameLocation);
                    continue;
                }
                properties[property.name] = &property;
                bool allowed = false;
                for (const std::string& required : definition->requiredProperties) {
                    if (required == property.name) allowed = true;
                }
                for (const std::string& optional : definition->optionalProperties) {
                    if (optional == property.name) allowed = true;
                }
                if (!allowed) {
                    this->AddDiagnostic("format2.surface.encoder-profile.property.unknown", "Unknown EncoderProfile property: " + property.name, property.nameLocation);
                    continue;
                }
                const Format2PropertyRule* rule = FindFormat2PropertyRule(*definition, property.name);
                if (!rule) {
                    this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for EncoderProfile property " + property.name, property.nameLocation);
                    continue;
                }
                const std::string expected = ValidateFormat2ValueRule(property, rule->valueRule);
                if (!expected.empty()) {
                    this->AddDiagnostic("format2.surface.encoder-profile.property.value", "EncoderProfile property " + property.name + " requires " + expected, property.value.location);
                    continue;
                }
                if (property.name == "Increase") this->ParseEncoderByteList(property, profile.increase);
                else if (property.name == "Decrease") this->ParseEncoderByteList(property, profile.decrease);
                else if (property.name == "Delta") this->ParseEncoderDelta(property, profile.delta);
                else if (property.name == "AccelerationDeltas") this->ParseEncoderDeltaList(property, profile.accelerationDeltas);
            }
        }
        for (const std::string& required : definition->requiredProperties) {
            if (properties.find(required) == properties.end()) this->AddDiagnostic("format2.surface.encoder-profile.property.required", "EncoderProfile requires " + required, profile.location);
        }
        for (int increase : profile.increase) {
            for (int decrease : profile.decrease) {
                if (increase == decrease) this->AddDiagnostic("format2.surface.encoder-profile.overlap", "EncoderProfile Increase and Decrease cannot contain the same MIDI value", profile.location);
            }
        }
        this->result_.surface.encoderProfiles.push_back(std::move(profile));
    }

    void ParseEncoderByteList(const Format2PropertySyntax& property, std::vector<int>& destination) {
        if (!property.value.list || property.value.items.empty()) {
            this->AddDiagnostic("format2.surface.encoder-profile.byte-list", "EncoderProfile " + property.name + " requires a non-empty MIDI data-byte list", property.value.location);
            return;
        }
        std::set<int> values;
        for (const Format2ScalarSyntax& item : property.value.items) {
            int value = 0;
            if (!ParseFormat2IntegerScalar(item, value) || value < 0 || value > 0x7F) {
                this->AddDiagnostic("format2.surface.encoder-profile.byte", "EncoderProfile " + property.name + " values must be MIDI data bytes from 0 through 0x7F", item.location);
                continue;
            }
            if (!values.insert(value).second) {
                this->AddDiagnostic("format2.surface.encoder-profile.byte.duplicate", "EncoderProfile " + property.name + " contains a duplicate MIDI value", item.location);
                continue;
            }
            destination.push_back(value);
        }
    }

    void ParseEncoderDelta(const Format2PropertySyntax& property, std::optional<double>& destination) {
        double value = 0.0;
        if (property.value.list || !ParseFormat2FiniteScalar(property.value.scalar, value) || value <= 0.0) {
            this->AddDiagnostic("format2.surface.encoder-profile.delta", "EncoderProfile Delta requires one positive finite number", property.value.location);
            return;
        }
        destination = value;
    }

    void ParseEncoderDeltaList(const Format2PropertySyntax& property, std::vector<double>& destination) {
        if (!property.value.list || property.value.items.empty()) {
            this->AddDiagnostic("format2.surface.encoder-profile.acceleration", "EncoderProfile AccelerationDeltas requires a non-empty list", property.value.location);
            return;
        }
        for (const Format2ScalarSyntax& item : property.value.items) {
            double value = 0.0;
            if (!ParseFormat2FiniteScalar(item, value) || value <= 0.0) {
                this->AddDiagnostic("format2.surface.encoder-profile.acceleration.value", "EncoderProfile AccelerationDeltas values must be positive finite numbers", item.location);
                continue;
            }
            destination.push_back(value);
        }
    }

    bool ParseProfileBody(const Format2SyntaxNode& node, const std::string& profileName, Format2ParsedProfileBody& body) {
        const Format2ProfileDefinition* definition = FindFormat2ProfileDefinition(profileName);
        if (!definition) {
            this->AddDiagnostic("format2.surface.schema.profile", "The Surface I/O schema has no " + profileName + " definition", node.location);
            return false;
        }
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind == Format2SyntaxNodeKind::Block) {
                this->AddDiagnostic("format2.surface.profile.block", profileName + " cannot contain nested blocks", child.location);
                continue;
            }
            if (child.positionalTokens.empty()) {
                for (const Format2PropertySyntax& property : child.properties) this->ValidateProfileProperty(profileName, *definition, property, body);
                continue;
            }
            if (child.positionalTokens.empty() || child.positionalTokens[0].kind != Format2TokenKind::Bare) {
                this->AddDiagnostic("format2.surface.profile.line", profileName + " line requires an unquoted line type before its properties", child.location);
                continue;
            }
            const std::string& lineName = child.positionalTokens[0].text;
            const Format2ProfileLineDefinition* lineDefinition = FindFormat2ProfileLineDefinition(profileName, lineName);
            if (!lineDefinition) {
                this->AddDiagnostic("format2.surface.profile.line.unknown", "Unknown " + profileName + " line: " + lineName, child.positionalTokens[0].location);
                continue;
            }
            const std::size_t expectedTokenCount = lineDefinition->argumentRule.empty() ? 1 : 2;
            if (child.positionalTokens.size() != expectedTokenCount || (expectedTokenCount == 2 && child.positionalTokens[1].kind != Format2TokenKind::Bare)) {
                const std::string argumentDescription = lineDefinition->argumentRule.empty() ? "no positional argument" : "one unquoted positional argument";
                this->AddDiagnostic("format2.surface.profile.line.argument", profileName + " " + lineName + " requires " + argumentDescription, child.location);
                continue;
            }
            if (!lineDefinition->argumentRule.empty()) this->ValidateProfileLineArgument(profileName, child, *lineDefinition);
            this->ValidateProfileLine(profileName, child, *lineDefinition);
            body.lines[lineName].push_back(&child);
        }
        for (const std::string& required : definition->requiredProperties) {
            if (body.properties.find(required) == body.properties.end()) this->AddDiagnostic("format2.surface.profile.property.required", profileName + " requires property " + required, node.location);
        }
        for (const Format2ProfileLineDefinition* lineDefinition : FindFormat2ProfileLineDefinitions(profileName)) {
            if (static_cast<int>(body.lines[lineDefinition->name].size()) < lineDefinition->minimumCount) this->AddDiagnostic("format2.surface.profile.line.required", profileName + " requires at least " + std::to_string(lineDefinition->minimumCount) + " " + lineDefinition->name + " lines", node.location);
        }
        return true;
    }

    void ValidateProfileLineArgument(const std::string& profileName, const Format2SyntaxNode& line, const Format2ProfileLineDefinition& definition) {
        Format2PropertySyntax argument;
        argument.name = "Argument";
        argument.nameLocation = line.positionalTokens[1].location;
        argument.value.location = line.positionalTokens[1].location;
        argument.value.scalar = { line.positionalTokens[1].text, false, line.positionalTokens[1].location };
        const std::string expected = ValidateFormat2ValueRule(argument, definition.argumentRule);
        if (!expected.empty()) this->AddDiagnostic("format2.surface.profile.line.argument.value", profileName + " " + definition.name + " argument requires " + expected, line.positionalTokens[1].location);
    }

    void ValidateProfileProperty(const std::string& profileName, const Format2ProfileDefinition& definition, const Format2PropertySyntax& property, Format2ParsedProfileBody& body) {
        if (body.properties.find(property.name) != body.properties.end()) {
            this->AddDiagnostic("format2.surface.profile.property.duplicate", profileName + " property is duplicated: " + property.name, property.nameLocation);
            return;
        }
        body.properties[property.name] = &property;
        bool allowed = false;
        for (const std::string& required : definition.requiredProperties) {
            if (required == property.name) allowed = true;
        }
        for (const std::string& optional : definition.optionalProperties) {
            if (optional == property.name) allowed = true;
        }
        if (!allowed) {
            this->AddDiagnostic("format2.surface.profile.property.unknown", "Unknown " + profileName + " property: " + property.name, property.nameLocation);
            return;
        }
        const Format2PropertyRule* rule = FindFormat2PropertyRule(definition, property.name);
        if (!rule) {
            this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for " + profileName + " property " + property.name, property.nameLocation);
            return;
        }
        const std::string expected = ValidateFormat2ValueRule(property, rule->valueRule);
        if (!expected.empty()) this->AddDiagnostic("format2.surface.profile.property.value", profileName + " property " + property.name + " requires " + expected, property.value.location);
    }

    void ValidateProfileLine(const std::string& profileName, const Format2SyntaxNode& line, const Format2ProfileLineDefinition& definition) {
        std::set<std::string> properties;
        for (const Format2PropertySyntax& property : line.properties) {
            if (!properties.insert(property.name).second) {
                this->AddDiagnostic("format2.surface.profile.line.property.duplicate", profileName + " " + definition.name + " property is duplicated: " + property.name, property.nameLocation);
                continue;
            }
            bool allowed = false;
            for (const std::string& required : definition.requiredProperties) {
                if (required == property.name) allowed = true;
            }
            for (const std::string& optional : definition.optionalProperties) {
                if (optional == property.name) allowed = true;
            }
            if (!allowed) {
                this->AddDiagnostic("format2.surface.profile.line.property.unknown", "Unknown " + profileName + " " + definition.name + " property: " + property.name, property.nameLocation);
                continue;
            }
            const Format2PropertyRule* rule = FindFormat2PropertyRule(definition, property.name);
            if (!rule) {
                this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for " + profileName + " " + definition.name + " property " + property.name, property.nameLocation);
                continue;
            }
            const std::string expected = ValidateFormat2ValueRule(property, rule->valueRule);
            if (!expected.empty()) this->AddDiagnostic("format2.surface.profile.line.property.value", profileName + " " + definition.name + " property " + property.name + " requires " + expected, property.value.location);
        }
        for (const std::string& required : definition.requiredProperties) {
            if (properties.find(required) == properties.end()) this->AddDiagnostic("format2.surface.profile.line.property.required", profileName + " " + definition.name + " requires property " + required, line.location);
        }
    }

    bool ParseSurfaceBlockBody(const Format2SyntaxNode& node, const std::string& blockName, Format2ParsedProfileBody& body) {
        const Format2SurfaceBlockDefinition* definition = FindFormat2SurfaceBlockDefinition(blockName);
        if (!definition) {
            this->AddDiagnostic("format2.surface.schema.block", "The Surface I/O schema has no " + blockName + " definition", node.location);
            return false;
        }
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind == Format2SyntaxNodeKind::Block) {
                this->AddDiagnostic("format2.surface.block.nested", blockName + " cannot contain nested blocks", child.location);
                continue;
            }
            if (child.positionalTokens.empty()) {
                for (const Format2PropertySyntax& property : child.properties) this->ValidateSurfaceBlockProperty(blockName, *definition, property, body);
                continue;
            }
            if (child.positionalTokens[0].kind != Format2TokenKind::Bare) {
                this->AddDiagnostic("format2.surface.block.line", blockName + " line requires an unquoted line type", child.location);
                continue;
            }
            const std::string& lineName = child.positionalTokens[0].text;
            const Format2SurfaceLineDefinition* lineDefinition = FindFormat2SurfaceLineDefinition(blockName, lineName);
            if (!lineDefinition) {
                this->AddDiagnostic("format2.surface.block.line.unknown", "Unknown " + blockName + " line: " + lineName, child.positionalTokens[0].location);
                continue;
            }
            const std::size_t expectedTokenCount = lineDefinition->argumentRule.empty() ? 1 : 2;
            if (child.positionalTokens.size() != expectedTokenCount || (expectedTokenCount == 2 && child.positionalTokens[1].kind != Format2TokenKind::Bare)) {
                this->AddDiagnostic("format2.surface.block.line.argument", blockName + " " + lineName + " has an invalid positional argument", child.location);
                continue;
            }
            this->ValidateSurfaceLine(blockName, child, *lineDefinition);
            body.lines[lineName].push_back(&child);
        }
        for (const std::string& required : definition->requiredProperties) {
            if (body.properties.find(required) == body.properties.end()) this->AddDiagnostic("format2.surface.block.property.required", blockName + " requires property " + required, node.location);
        }
        for (const Format2SurfaceLineDefinition* lineDefinition : FindFormat2SurfaceLineDefinitions(blockName)) {
            if (static_cast<int>(body.lines[lineDefinition->name].size()) < lineDefinition->minimumCount) this->AddDiagnostic("format2.surface.block.line.required", blockName + " requires at least " + std::to_string(lineDefinition->minimumCount) + " " + lineDefinition->name + " lines", node.location);
        }
        return true;
    }

    void ValidateSurfaceBlockProperty(const std::string& blockName, const Format2SurfaceBlockDefinition& definition, const Format2PropertySyntax& property, Format2ParsedProfileBody& body) {
        if (body.properties.find(property.name) != body.properties.end()) {
            this->AddDiagnostic("format2.surface.block.property.duplicate", blockName + " property is duplicated: " + property.name, property.nameLocation);
            return;
        }
        body.properties[property.name] = &property;
        bool allowed = false;
        for (const std::string& required : definition.requiredProperties) {
            if (required == property.name) allowed = true;
        }
        for (const std::string& optional : definition.optionalProperties) {
            if (optional == property.name) allowed = true;
        }
        if (!allowed) {
            this->AddDiagnostic("format2.surface.block.property.unknown", "Unknown " + blockName + " property: " + property.name, property.nameLocation);
            return;
        }
        const Format2PropertyRule* rule = FindFormat2PropertyRule(definition, property.name);
        if (!rule) {
            this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for " + blockName + " property " + property.name, property.nameLocation);
            return;
        }
        const std::string expected = ValidateFormat2ValueRule(property, rule->valueRule);
        if (!expected.empty()) this->AddDiagnostic("format2.surface.block.property.value", blockName + " property " + property.name + " requires " + expected, property.value.location);
    }

    void ValidateSurfaceLine(const std::string& blockName, const Format2SyntaxNode& line, const Format2SurfaceLineDefinition& definition) {
        if (!definition.argumentRule.empty()) {
            Format2PropertySyntax argument;
            argument.name = "Argument";
            argument.nameLocation = line.positionalTokens[1].location;
            argument.value.location = line.positionalTokens[1].location;
            argument.value.scalar = { line.positionalTokens[1].text, false, line.positionalTokens[1].location };
            const std::string expected = ValidateFormat2ValueRule(argument, definition.argumentRule);
            if (!expected.empty()) this->AddDiagnostic("format2.surface.block.line.argument.value", blockName + " " + definition.name + " argument requires " + expected, line.positionalTokens[1].location);
        }
        std::set<std::string> properties;
        for (const Format2PropertySyntax& property : line.properties) {
            if (!properties.insert(property.name).second) {
                this->AddDiagnostic("format2.surface.block.line.property.duplicate", blockName + " " + definition.name + " property is duplicated: " + property.name, property.nameLocation);
                continue;
            }
            bool allowed = false;
            for (const std::string& required : definition.requiredProperties) {
                if (required == property.name) allowed = true;
            }
            for (const std::string& optional : definition.optionalProperties) {
                if (optional == property.name) allowed = true;
            }
            if (!allowed) {
                this->AddDiagnostic("format2.surface.block.line.property.unknown", "Unknown " + blockName + " " + definition.name + " property: " + property.name, property.nameLocation);
                continue;
            }
            const Format2PropertyRule* rule = FindFormat2PropertyRule(definition, property.name);
            if (!rule) {
                this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for " + blockName + " " + definition.name + " property " + property.name, property.nameLocation);
                continue;
            }
            const std::string expected = ValidateFormat2ValueRule(property, rule->valueRule);
            if (!expected.empty()) this->AddDiagnostic("format2.surface.block.line.property.value", blockName + " " + definition.name + " property " + property.name + " requires " + expected, property.value.location);
        }
        for (const std::string& required : definition.requiredProperties) {
            if (properties.find(required) == properties.end()) this->AddDiagnostic("format2.surface.block.line.property.required", blockName + " " + definition.name + " requires property " + required, line.location);
        }
    }

    void ParseValueProfile(const Format2SyntaxNode& node) {
        const std::size_t profileCount = this->result_.surface.profiles.size();
        this->ParseNamedBlock(node, this->result_.surface.profiles);
        if (this->result_.surface.profiles.size() == profileCount) return;
        Format2ParsedProfileBody body;
        if (!this->ParseProfileBody(node, "ValueProfile", body)) return;

        Format2ValueProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->valueProfileIds_[profile.id] = profile.location;
        const auto inputUnitProperty = body.properties.find("InputUnit");
        const auto outputUnitProperty = body.properties.find("OutputUnit");
        const auto directionProperty = body.properties.find("Direction");
        const auto interpolationProperty = body.properties.find("Interpolation");
        if (inputUnitProperty == body.properties.end() || outputUnitProperty == body.properties.end() || directionProperty == body.properties.end() || interpolationProperty == body.properties.end()) return;
        const std::string& inputUnit = inputUnitProperty->second->value.scalar.text;
        const std::string& outputUnit = outputUnitProperty->second->value.scalar.text;
        const std::string& direction = directionProperty->second->value.scalar.text;
        const std::string& interpolation = interpolationProperty->second->value.scalar.text;
        profile.inputUnit = inputUnit == "Integer" ? Format2ValueUnit::Integer : inputUnit == "Decibels" ? Format2ValueUnit::Decibels : Format2ValueUnit::Normalized;
        profile.outputUnit = outputUnit == "Integer" ? Format2ValueUnit::Integer : outputUnit == "Decibels" ? Format2ValueUnit::Decibels : Format2ValueUnit::Normalized;
        profile.direction = direction == "Decode" ? Format2ValueProfileDirection::Decode : direction == "Encode" ? Format2ValueProfileDirection::Encode : Format2ValueProfileDirection::Both;
        profile.interpolation = interpolation == "Step" ? Format2Interpolation::Step : Format2Interpolation::Linear;
        for (const Format2SyntaxNode* point : body.lines["Point"]) {
            const Format2PropertySyntax* input = FindFormat2NodeProperty(*point, "Input");
            const Format2PropertySyntax* output = FindFormat2NodeProperty(*point, "Output");
            if (!input || !output) continue;
            Format2ValueProfilePoint parsedPoint;
            parsedPoint.location = point->location;
            if (!ParseFormat2FiniteScalar(input->value.scalar, parsedPoint.input) || !ParseFormat2FiniteScalar(output->value.scalar, parsedPoint.output)) continue;
            profile.points.push_back(parsedPoint);
        }
        for (std::size_t pointIdx = 1; pointIdx < profile.points.size(); ++pointIdx) {
            if (profile.points[pointIdx].input <= profile.points[pointIdx - 1].input) this->AddDiagnostic("format2.surface.value-profile.input-order", "ValueProfile Point Input values must increase strictly", profile.points[pointIdx].location);
        }
        if ((profile.direction == Format2ValueProfileDirection::Encode || profile.direction == Format2ValueProfileDirection::Both) && profile.interpolation != Format2Interpolation::Linear) this->AddDiagnostic("format2.surface.value-profile.inverse.interpolation", "ValueProfile Direction=Encode or Both requires Interpolation=Linear", profile.location);
        if (profile.direction == Format2ValueProfileDirection::Encode || profile.direction == Format2ValueProfileDirection::Both) this->ValidateValueProfileOutputOrder(profile);
        this->valueProfileDirections_[profile.id] = profile.direction;
        this->result_.surface.valueProfiles.push_back(std::move(profile));
    }

    void ValidateValueProfileOutputOrder(const Format2ValueProfile& profile) {
        if (profile.points.size() < 2) return;
        const bool increasing = profile.points[1].output > profile.points[0].output;
        if (profile.points[1].output == profile.points[0].output) {
            this->AddDiagnostic("format2.surface.value-profile.output-order", "ValueProfile Point Output values must be strictly monotonic for Encode or Both", profile.points[1].location);
            return;
        }
        for (std::size_t pointIdx = 2; pointIdx < profile.points.size(); ++pointIdx) {
            const bool ordered = increasing ? profile.points[pointIdx].output > profile.points[pointIdx - 1].output : profile.points[pointIdx].output < profile.points[pointIdx - 1].output;
            if (!ordered) this->AddDiagnostic("format2.surface.value-profile.output-order", "ValueProfile Point Output values must be strictly monotonic for Encode or Both", profile.points[pointIdx].location);
        }
    }

    void ParseColorProfile(const Format2SyntaxNode& node) {
        const std::size_t profileCount = this->result_.surface.profiles.size();
        this->ParseNamedBlock(node, this->result_.surface.profiles);
        if (this->result_.surface.profiles.size() == profileCount) return;
        Format2ParsedProfileBody body;
        if (!this->ParseProfileBody(node, "ColorProfile", body)) return;

        Format2ColorProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->colorProfileIds_[profile.id] = profile.location;
        const auto matchProperty = body.properties.find("Match");
        if (matchProperty == body.properties.end()) return;
        const std::string& match = matchProperty->second->value.scalar.text;
        profile.match = match == "Exact" ? Format2ColorMatch::Exact : match == "HueRanges" ? Format2ColorMatch::HueRanges : Format2ColorMatch::Nearest;
        const auto defaultValue = body.properties.find("Default");
        if (defaultValue != body.properties.end()) ParseFormat2IntegerScalar(defaultValue->second->value.scalar, profile.defaultValue);
        const auto minimumBrightness = body.properties.find("MinimumBrightness");
        if (minimumBrightness != body.properties.end()) {
            double value = 0.0;
            if (ParseFormat2FiniteScalar(minimumBrightness->second->value.scalar, value)) profile.minimumBrightness = value;
        }
        const auto maximumNeutralSaturation = body.properties.find("MaximumNeutralSaturation");
        if (maximumNeutralSaturation != body.properties.end()) {
            double value = 0.0;
            if (ParseFormat2FiniteScalar(maximumNeutralSaturation->second->value.scalar, value)) profile.maximumNeutralSaturation = value;
        }
        this->ParseColorProfileEntries(body, profile);
        this->ParseColorProfileHueRanges(body, profile);
        this->ValidateColorProfileMode(profile);
        this->colorProfileMidiSafe_[profile.id] = this->IsColorProfileMidiSafe(profile);
        this->result_.surface.colorProfiles.push_back(std::move(profile));
    }

    void ParseColorProfileEntries(const Format2ParsedProfileBody& body, Format2ColorProfile& profile) {
        std::set<std::uint32_t> colors;
        std::set<int> values;
        const auto lines = body.lines.find("Entry");
        if (lines == body.lines.end()) return;
        for (const Format2SyntaxNode* entry : lines->second) {
            const Format2PropertySyntax* color = FindFormat2NodeProperty(*entry, "Color");
            const Format2PropertySyntax* value = FindFormat2NodeProperty(*entry, "Value");
            if (!color || !value) continue;
            Format2ColorProfileEntry parsedEntry;
            parsedEntry.location = entry->location;
            if (!ParseFormat2ColorScalar(color->value.scalar, parsedEntry.color) || !ParseFormat2IntegerScalar(value->value.scalar, parsedEntry.value)) continue;
            if (!colors.insert(parsedEntry.color).second) this->AddDiagnostic("format2.surface.color-profile.color.duplicate", "ColorProfile Entry Color is duplicated", color->value.location);
            if (!values.insert(parsedEntry.value).second) this->AddDiagnostic("format2.surface.color-profile.value.duplicate", "ColorProfile Entry Value is duplicated", value->value.location);
            profile.entries.push_back(parsedEntry);
        }
    }

    void ParseColorProfileHueRanges(const Format2ParsedProfileBody& body, Format2ColorProfile& profile) {
        std::set<int> values;
        const auto lines = body.lines.find("HueRange");
        if (lines == body.lines.end()) return;
        for (const Format2SyntaxNode* range : lines->second) {
            const Format2PropertySyntax* minimum = FindFormat2NodeProperty(*range, "Minimum");
            const Format2PropertySyntax* maximum = FindFormat2NodeProperty(*range, "Maximum");
            const Format2PropertySyntax* value = FindFormat2NodeProperty(*range, "Value");
            if (!minimum || !maximum || !value) continue;
            Format2HueRange parsedRange;
            parsedRange.location = range->location;
            if (!ParseFormat2FiniteScalar(minimum->value.scalar, parsedRange.minimum) || !ParseFormat2FiniteScalar(maximum->value.scalar, parsedRange.maximum) || !ParseFormat2IntegerScalar(value->value.scalar, parsedRange.value)) continue;
            if (parsedRange.minimum == parsedRange.maximum) this->AddDiagnostic("format2.surface.color-profile.hue-range.empty", "ColorProfile HueRange Minimum and Maximum must differ", range->location);
            if (!values.insert(parsedRange.value).second) this->AddDiagnostic("format2.surface.color-profile.hue-value.duplicate", "ColorProfile HueRange Value is duplicated", value->value.location);
            profile.hueRanges.push_back(parsedRange);
        }
    }

    void ValidateColorProfileMode(const Format2ColorProfile& profile) {
        if (profile.match == Format2ColorMatch::Nearest || profile.match == Format2ColorMatch::Exact) {
            if (profile.entries.empty()) this->AddDiagnostic("format2.surface.color-profile.entry.required", "ColorProfile Match=Nearest or Exact requires at least one Entry", profile.location);
            if (!profile.hueRanges.empty()) this->AddDiagnostic("format2.surface.color-profile.hue-range.forbidden", "ColorProfile HueRange is valid only with Match=HueRanges", profile.hueRanges.front().location);
            if (profile.minimumBrightness || profile.maximumNeutralSaturation) this->AddDiagnostic("format2.surface.color-profile.hue-property.forbidden", "MinimumBrightness and MaximumNeutralSaturation are valid only with Match=HueRanges", profile.location);
            return;
        }
        if (!profile.entries.empty()) this->AddDiagnostic("format2.surface.color-profile.entry.forbidden", "ColorProfile Match=HueRanges does not accept Entry lines", profile.entries.front().location);
        if (profile.hueRanges.empty()) this->AddDiagnostic("format2.surface.color-profile.hue-range.required", "ColorProfile Match=HueRanges requires at least one HueRange", profile.location);
        if (!profile.minimumBrightness) this->AddDiagnostic("format2.surface.color-profile.brightness.required", "ColorProfile Match=HueRanges requires MinimumBrightness", profile.location);
        if (!profile.maximumNeutralSaturation) this->AddDiagnostic("format2.surface.color-profile.saturation.required", "ColorProfile Match=HueRanges requires MaximumNeutralSaturation", profile.location);
        this->ValidateHueCoverage(profile);
    }

    void ValidateHueCoverage(const Format2ColorProfile& profile) {
        struct Interval {
            double minimum = 0.0;
            double maximum = 0.0;
        };
        std::vector<Interval> intervals;
        for (const Format2HueRange& range : profile.hueRanges) {
            if (range.minimum < range.maximum) intervals.push_back({ range.minimum, range.maximum });
            else if (range.minimum > range.maximum) {
                intervals.push_back({ range.minimum, 360.0 });
                intervals.push_back({ 0.0, range.maximum });
            }
        }
        std::sort(intervals.begin(), intervals.end(), [](const Interval& left, const Interval& right) { return left.minimum < right.minimum; });
        const double tolerance = 0.000000001;
        double coveredUntil = 0.0;
        bool valid = !intervals.empty();
        for (const Interval& interval : intervals) {
            if (std::abs(interval.minimum - coveredUntil) > tolerance) valid = false;
            if (interval.maximum < coveredUntil - tolerance) valid = false;
            if (interval.maximum > coveredUntil) coveredUntil = interval.maximum;
        }
        if (std::abs(coveredUntil - 360.0) > tolerance) valid = false;
        if (!valid) this->AddDiagnostic("format2.surface.color-profile.hue-coverage", "ColorProfile HueRange lines must cover 0 through 360 exactly once without gaps or overlap", profile.location);
    }

    bool IsColorProfileMidiSafe(const Format2ColorProfile& profile) const {
        if (profile.defaultValue < 0 || profile.defaultValue > 0x7F) return false;
        for (const Format2ColorProfileEntry& entry : profile.entries) {
            if (entry.value < 0 || entry.value > 0x7F) return false;
        }
        for (const Format2HueRange& range : profile.hueRanges) {
            if (range.value < 0 || range.value > 0x7F) return false;
        }
        return true;
    }

    bool BeginTypedProfile(const Format2SyntaxNode& node, const std::string& profileName, Format2ParsedProfileBody& body) {
        const std::size_t profileCount = this->result_.surface.profiles.size();
        this->ParseNamedBlock(node, this->result_.surface.profiles);
        return this->result_.surface.profiles.size() != profileCount && this->ParseProfileBody(node, profileName, body);
    }

    void ParseRingProfile(const Format2SyntaxNode& node) {
        Format2ParsedProfileBody body;
        if (!this->BeginTypedProfile(node, "RingProfile", body)) return;
        Format2RingProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->ringProfileIds_[profile.id] = profile.location;
        const auto quantize = body.properties.find("Quantize");
        const auto valueOffset = body.properties.find("ValueOffset");
        if (quantize == body.properties.end() || valueOffset == body.properties.end()) return;
        profile.quantize = quantize->second->value.scalar.text == "Round" ? Format2Quantize::Round : Format2Quantize::Floor;
        ParseFormat2IntegerScalar(valueOffset->second->value.scalar, profile.valueOffset);
        const auto segments = body.properties.find("Segments");
        if (segments != body.properties.end()) {
            int value = 0;
            if (ParseFormat2IntegerScalar(segments->second->value.scalar, value) && value > 0) profile.segments = value;
        }
        const auto defaultColor = body.properties.find("DefaultColor");
        if (defaultColor != body.properties.end()) {
            ParseFormat2ColorScalar(defaultColor->second->value.scalar, profile.defaultColor);
            if (!profile.segments) this->AddDiagnostic("format2.surface.ring-profile.default-color.segments", "RingProfile DefaultColor requires Segments", defaultColor->second->nameLocation);
        }
        this->ParseRingStyles(body, profile);
        this->ringProfileSegments_[profile.id] = profile.segments;
        this->ringProfileMidiSafe_[profile.id] = this->IsRingProfileMidiSafe(profile);
        this->result_.surface.ringProfiles.push_back(std::move(profile));
    }

    void ParseRingStyles(const Format2ParsedProfileBody& body, Format2RingProfile& profile) {
        std::set<Format2RingStyle> styles;
        std::set<int> codes;
        const auto lines = body.lines.find("Style");
        if (lines == body.lines.end()) return;
        for (const Format2SyntaxNode* line : lines->second) {
            const Format2PropertySyntax* code = FindFormat2NodeProperty(*line, "Code");
            const Format2PropertySyntax* steps = FindFormat2NodeProperty(*line, "Steps");
            if (line->positionalTokens.size() != 2 || !code || !steps) continue;
            Format2RingStyleEntry entry;
            entry.style = this->ParseRingStyle(line->positionalTokens[1].text);
            entry.location = line->location;
            if (!ParseFormat2IntegerScalar(code->value.scalar, entry.code) || !ParseFormat2IntegerScalar(steps->value.scalar, entry.steps)) continue;
            if (!styles.insert(entry.style).second) this->AddDiagnostic("format2.surface.ring-profile.style.duplicate", "RingProfile Style is duplicated: " + line->positionalTokens[1].text, line->positionalTokens[1].location);
            if (!codes.insert(entry.code).second) this->AddDiagnostic("format2.surface.ring-profile.code.duplicate", "RingProfile Style Code is duplicated", code->value.location);
            if (profile.segments && entry.steps > *profile.segments) this->AddDiagnostic("format2.surface.ring-profile.steps.segments", "RingProfile Style Steps cannot be greater than Segments", steps->value.location);
            profile.styles.push_back(entry);
        }
    }

    Format2RingStyle ParseRingStyle(const std::string& value) const {
        if (value == "Fill") return Format2RingStyle::Fill;
        if (value == "BoostCut") return Format2RingStyle::BoostCut;
        if (value == "Spread") return Format2RingStyle::Spread;
        return Format2RingStyle::Dot;
    }

    bool IsRingProfileMidiSafe(const Format2RingProfile& profile) const {
        for (const Format2RingStyleEntry& entry : profile.styles) {
            const long long maximumValue = static_cast<long long>(profile.valueOffset) + static_cast<long long>(entry.steps) - 1;
            if (entry.code < 0 || entry.code > 0x7F || profile.valueOffset < 0 || maximumValue > 0x7F) return false;
        }
        return true;
    }

    void ParseBarProfile(const Format2SyntaxNode& node) {
        Format2ParsedProfileBody body;
        if (!this->BeginTypedProfile(node, "BarProfile", body)) return;
        Format2BarProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->barProfileIds_[profile.id] = profile.location;
        const auto defaultStyle = body.properties.find("Default");
        if (defaultStyle == body.properties.end()) return;
        profile.defaultStyle = this->ParseBarStyle(defaultStyle->second->value.scalar.text);
        std::set<Format2BarStyle> styles;
        std::set<int> codes;
        const auto lines = body.lines.find("Style");
        if (lines != body.lines.end()) {
            for (const Format2SyntaxNode* line : lines->second) {
                const Format2PropertySyntax* code = FindFormat2NodeProperty(*line, "Code");
                if (line->positionalTokens.size() != 2 || !code) continue;
                Format2BarStyleEntry entry;
                entry.style = this->ParseBarStyle(line->positionalTokens[1].text);
                entry.location = line->location;
                if (!ParseFormat2IntegerScalar(code->value.scalar, entry.code)) continue;
                if (!styles.insert(entry.style).second) this->AddDiagnostic("format2.surface.bar-profile.style.duplicate", "BarProfile Style is duplicated: " + line->positionalTokens[1].text, line->positionalTokens[1].location);
                if (!codes.insert(entry.code).second) this->AddDiagnostic("format2.surface.bar-profile.code.duplicate", "BarProfile Style Code is duplicated", code->value.location);
                if (entry.code > 0x7F) this->AddDiagnostic("format2.surface.bar-profile.code.midi-range", "BarProfile Style Code must fit one MIDI data byte from 0 through 0x7F", code->value.location);
                profile.styles.push_back(entry);
            }
        }
        if (styles.find(profile.defaultStyle) == styles.end()) this->AddDiagnostic("format2.surface.bar-profile.default.missing", "BarProfile Default must have a matching Style entry", defaultStyle->second->value.location);
        if (styles.find(Format2BarStyle::Off) == styles.end()) this->AddDiagnostic("format2.surface.bar-profile.off.required", "BarProfile requires an Off Style entry", profile.location);
        this->result_.surface.barProfiles.push_back(std::move(profile));
    }

    Format2BarStyle ParseBarStyle(const std::string& value) const {
        if (value == "Normal") return Format2BarStyle::Normal;
        if (value == "Bipolar") return Format2BarStyle::Bipolar;
        if (value == "Fill") return Format2BarStyle::Fill;
        if (value == "Spread") return Format2BarStyle::Spread;
        return Format2BarStyle::Off;
    }

    void ParseMeterProfile(const Format2SyntaxNode& node) {
        Format2ParsedProfileBody body;
        if (!this->BeginTypedProfile(node, "MeterProfile", body)) return;
        Format2MeterProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->meterProfileIds_[profile.id] = profile.location;
        const auto mode = body.properties.find("Mode");
        const auto inputUnit = body.properties.find("InputUnit");
        if (mode == body.properties.end() || inputUnit == body.properties.end()) return;
        profile.mode = mode->second->value.scalar.text == "Steps" ? Format2MeterMode::Steps : Format2MeterMode::Linear;
        profile.inputUnit = inputUnit->second->value.scalar.text == "Decibels" ? Format2MeterInputUnit::Decibels : Format2MeterInputUnit::Normalized;
        this->ParseMeterProperties(body, profile);
        this->ParseMeterSteps(body, profile);
        this->ValidateMeterMode(body, profile);
        this->meterProfileMidiSafe_[profile.id] = this->IsMeterProfileMidiSafe(profile);
        this->result_.surface.meterProfiles.push_back(std::move(profile));
    }

    void ParseMeterProperties(const Format2ParsedProfileBody& body, Format2MeterProfile& profile) {
        const auto inputRange = body.properties.find("InputRange");
        if (inputRange != body.properties.end() && inputRange->second->value.list && inputRange->second->value.items.size() == 2) {
            std::array<double, 2> range{};
            if (ParseFormat2FiniteScalar(inputRange->second->value.items[0], range[0]) && ParseFormat2FiniteScalar(inputRange->second->value.items[1], range[1])) profile.inputRange = range;
        }
        const auto outputRange = body.properties.find("OutputRange");
        if (outputRange != body.properties.end() && outputRange->second->value.list && outputRange->second->value.items.size() == 2) {
            std::array<int, 2> range{};
            if (ParseFormat2IntegerScalar(outputRange->second->value.items[0], range[0]) && ParseFormat2IntegerScalar(outputRange->second->value.items[1], range[1])) profile.outputRange = range;
        }
        const auto quantize = body.properties.find("Quantize");
        if (quantize != body.properties.end()) profile.quantize = quantize->second->value.scalar.text == "Round" ? Format2Quantize::Round : Format2Quantize::Floor;
        const auto defaultValue = body.properties.find("Default");
        if (defaultValue != body.properties.end()) {
            int value = 0;
            if (ParseFormat2IntegerScalar(defaultValue->second->value.scalar, value)) profile.defaultValue = value;
        }
    }

    void ParseMeterSteps(const Format2ParsedProfileBody& body, Format2MeterProfile& profile) {
        const auto lines = body.lines.find("Step");
        if (lines == body.lines.end()) return;
        for (const Format2SyntaxNode* line : lines->second) {
            const Format2PropertySyntax* minimum = FindFormat2NodeProperty(*line, "Minimum");
            const Format2PropertySyntax* output = FindFormat2NodeProperty(*line, "Output");
            if (!minimum || !output) continue;
            Format2MeterStep step;
            step.location = line->location;
            if (!ParseFormat2FiniteScalar(minimum->value.scalar, step.minimum) || !ParseFormat2IntegerScalar(output->value.scalar, step.output)) continue;
            if (!profile.steps.empty() && step.minimum <= profile.steps.back().minimum) this->AddDiagnostic("format2.surface.meter-profile.step.order", "MeterProfile Step Minimum values must increase strictly", minimum->value.location);
            profile.steps.push_back(step);
        }
    }

    void ValidateMeterMode(const Format2ParsedProfileBody& body, const Format2MeterProfile& profile) {
        if (profile.mode == Format2MeterMode::Linear) {
            if (!profile.inputRange) this->AddDiagnostic("format2.surface.meter-profile.input-range.required", "MeterProfile Mode=Linear requires InputRange", profile.location);
            else if ((*profile.inputRange)[0] >= (*profile.inputRange)[1]) this->AddDiagnostic("format2.surface.meter-profile.input-range.order", "MeterProfile InputRange must increase", body.properties.at("InputRange")->value.location);
            if (!profile.outputRange) this->AddDiagnostic("format2.surface.meter-profile.output-range.required", "MeterProfile Mode=Linear requires OutputRange", profile.location);
            else if ((*profile.outputRange)[0] == (*profile.outputRange)[1]) this->AddDiagnostic("format2.surface.meter-profile.output-range.distinct", "MeterProfile OutputRange endpoints must differ", body.properties.at("OutputRange")->value.location);
            if (!profile.quantize) this->AddDiagnostic("format2.surface.meter-profile.quantize.required", "MeterProfile Mode=Linear requires Quantize", profile.location);
            if (profile.defaultValue) this->AddDiagnostic("format2.surface.meter-profile.default.forbidden", "MeterProfile Mode=Linear does not accept Default", body.properties.at("Default")->nameLocation);
            if (!profile.steps.empty()) this->AddDiagnostic("format2.surface.meter-profile.step.forbidden", "MeterProfile Mode=Linear does not accept Step lines", profile.steps.front().location);
            return;
        }
        if (!profile.defaultValue) this->AddDiagnostic("format2.surface.meter-profile.default.required", "MeterProfile Mode=Steps requires Default", profile.location);
        if (profile.steps.empty()) this->AddDiagnostic("format2.surface.meter-profile.step.required", "MeterProfile Mode=Steps requires at least one Step", profile.location);
        if (profile.inputRange) this->AddDiagnostic("format2.surface.meter-profile.input-range.forbidden", "MeterProfile Mode=Steps does not accept InputRange", body.properties.at("InputRange")->nameLocation);
        if (profile.outputRange) this->AddDiagnostic("format2.surface.meter-profile.output-range.forbidden", "MeterProfile Mode=Steps does not accept OutputRange", body.properties.at("OutputRange")->nameLocation);
        if (profile.quantize) this->AddDiagnostic("format2.surface.meter-profile.quantize.forbidden", "MeterProfile Mode=Steps does not accept Quantize", body.properties.at("Quantize")->nameLocation);
    }

    bool IsMeterProfileMidiSafe(const Format2MeterProfile& profile) const {
        if (profile.outputRange && ((*profile.outputRange)[0] > 0x7F || (*profile.outputRange)[1] > 0x7F)) return false;
        if (profile.defaultValue && *profile.defaultValue > 0x7F) return false;
        for (const Format2MeterStep& step : profile.steps) {
            if (step.output > 0x7F) return false;
        }
        return true;
    }

    void ParseTextProfile(const Format2SyntaxNode& node) {
        Format2ParsedProfileBody body;
        if (!this->BeginTypedProfile(node, "TextProfile", body)) return;
        Format2TextProfile profile;
        profile.id = node.positionalTokens[1].text;
        profile.location = node.location;
        this->textProfileIds_[profile.id] = profile.location;
        const auto encoding = body.properties.find("Encoding");
        if (encoding == body.properties.end()) return;
        profile.encoding = encoding->second->value.scalar.text == "UTF8" ? Format2TextEncoding::Utf8 : Format2TextEncoding::Ascii7;
        this->ParseTextProperties(body, profile);
        this->ParseTextAlignments(body, profile);
        this->ValidateTextPresentation(body, profile);
        if (profile.encoding == Format2TextEncoding::Ascii7) {
            for (unsigned char character : profile.clearText) {
                if (character > 0x7F) {
                    this->AddDiagnostic("format2.surface.text-profile.clear-text.ascii", "TextProfile Encoding=ASCII7 requires ClearText to contain only ASCII characters", body.properties.at("ClearText")->value.location);
                    break;
                }
            }
        }
        this->textProfileEncodings_[profile.id] = profile.encoding;
        this->textProfileWidths_[profile.id] = profile.width;
        this->result_.surface.textProfiles.push_back(std::move(profile));
    }

    void ParseTextProperties(const Format2ParsedProfileBody& body, Format2TextProfile& profile) {
        const auto width = body.properties.find("Width");
        if (width != body.properties.end()) {
            int value = 0;
            if (ParseFormat2IntegerScalar(width->second->value.scalar, value) && value > 0) profile.width = value;
        }
        const auto padding = body.properties.find("Padding");
        if (padding != body.properties.end()) profile.padding = padding->second->value.scalar.text == "Space" ? Format2TextPadding::Space : Format2TextPadding::None;
        const auto clearText = body.properties.find("ClearText");
        if (clearText != body.properties.end()) profile.clearText = clearText->second->value.scalar.text;
        const auto silenceAsEmpty = body.properties.find("SilenceAsEmpty");
        if (silenceAsEmpty != body.properties.end()) profile.silenceAsEmpty = silenceAsEmpty->second->value.scalar.text == "true";
        const auto defaultAlignment = body.properties.find("DefaultAlignment");
        if (defaultAlignment != body.properties.end()) profile.defaultAlignment = this->ParseTextAlignment(defaultAlignment->second->value.scalar.text);
        const auto invertCode = body.properties.find("InvertCode");
        if (invertCode != body.properties.end()) {
            int value = 0;
            if (ParseFormat2IntegerScalar(invertCode->second->value.scalar, value)) profile.invertCode = value;
        }
        const auto presentationCombine = body.properties.find("PresentationCombine");
        if (presentationCombine != body.properties.end()) profile.presentationCombine = presentationCombine->second->value.scalar.text == "Add" ? Format2PresentationCombine::Add : Format2PresentationCombine::BitOr;
        if (profile.padding == Format2TextPadding::Space && !profile.width) this->AddDiagnostic("format2.surface.text-profile.padding.width", "TextProfile Padding=Space requires Width", padding->second->value.location);
    }

    void ParseTextAlignments(const Format2ParsedProfileBody& body, Format2TextProfile& profile) {
        std::set<Format2TextAlignment> alignments;
        std::set<int> codes;
        const auto lines = body.lines.find("Alignment");
        if (lines == body.lines.end()) return;
        for (const Format2SyntaxNode* line : lines->second) {
            const Format2PropertySyntax* code = FindFormat2NodeProperty(*line, "Code");
            if (line->positionalTokens.size() != 2 || !code) continue;
            Format2TextAlignmentEntry entry;
            entry.alignment = this->ParseTextAlignment(line->positionalTokens[1].text);
            entry.location = line->location;
            if (!ParseFormat2IntegerScalar(code->value.scalar, entry.code)) continue;
            if (!alignments.insert(entry.alignment).second) this->AddDiagnostic("format2.surface.text-profile.alignment.duplicate", "TextProfile Alignment is duplicated: " + line->positionalTokens[1].text, line->positionalTokens[1].location);
            if (!codes.insert(entry.code).second) this->AddDiagnostic("format2.surface.text-profile.code.duplicate", "TextProfile Alignment Code is duplicated", code->value.location);
            profile.alignments.push_back(entry);
        }
    }

    Format2TextAlignment ParseTextAlignment(const std::string& value) const {
        if (value == "Center") return Format2TextAlignment::Center;
        if (value == "Right") return Format2TextAlignment::Right;
        return Format2TextAlignment::Left;
    }

    void ValidateTextPresentation(const Format2ParsedProfileBody& body, const Format2TextProfile& profile) {
        if (!profile.alignments.empty() && !profile.defaultAlignment) this->AddDiagnostic("format2.surface.text-profile.default-alignment.required", "TextProfile with Alignment entries requires DefaultAlignment", profile.location);
        if (profile.alignments.empty() && profile.defaultAlignment) this->AddDiagnostic("format2.surface.text-profile.default-alignment.forbidden", "TextProfile DefaultAlignment requires Alignment entries", body.properties.at("DefaultAlignment")->nameLocation);
        bool defaultFound = !profile.defaultAlignment;
        for (const Format2TextAlignmentEntry& entry : profile.alignments) {
            if (profile.defaultAlignment && entry.alignment == *profile.defaultAlignment) defaultFound = true;
            if (entry.code > 0x7F) this->AddDiagnostic("format2.surface.text-profile.code.midi-range", "TextProfile presentation codes must fit one MIDI data byte from 0 through 0x7F", entry.location);
            if (profile.invertCode) {
                const long long combined = profile.presentationCombine == Format2PresentationCombine::Add ? static_cast<long long>(entry.code) + *profile.invertCode : entry.code | *profile.invertCode;
                if (combined > 0x7F) this->AddDiagnostic("format2.surface.text-profile.combined.midi-range", "TextProfile combined presentation code must fit one MIDI data byte from 0 through 0x7F", entry.location);
                if (profile.presentationCombine == Format2PresentationCombine::BitOr && (entry.code & *profile.invertCode) != 0) this->AddDiagnostic("format2.surface.text-profile.combine.overlap", "TextProfile BitOr requires disjoint Alignment and InvertCode bits", entry.location);
            }
        }
        if (!defaultFound) this->AddDiagnostic("format2.surface.text-profile.default-alignment.missing", "TextProfile DefaultAlignment must have a matching Alignment entry", body.properties.at("DefaultAlignment")->value.location);
        if (profile.invertCode && *profile.invertCode > 0x7F) this->AddDiagnostic("format2.surface.text-profile.invert-code.midi-range", "TextProfile InvertCode must fit one MIDI data byte from 0 through 0x7F", body.properties.at("InvertCode")->value.location);
    }

    void ParseColorCalibration(const Format2SyntaxNode& node) {
        if (node.positionalTokens.size() != 1 || !node.properties.empty()) {
            this->AddDiagnostic("format2.surface.color-calibration.header", "ColorCalibration does not accept an ID or header properties", node.location);
            return;
        }
        if (this->result_.surface.colorCalibration) {
            this->AddDiagnostic("format2.surface.color-calibration.duplicate", "ColorCalibration can occur only once", node.location);
            return;
        }
        Format2ParsedProfileBody body;
        if (!this->ParseSurfaceBlockBody(node, "ColorCalibration", body)) return;
        Format2ColorCalibration calibration;
        calibration.location = node.location;
        this->ParseCalibrationInteger(body, "InputMax", calibration.inputMax);
        this->ParseCalibrationOptionalInteger(body, "OutputMax", calibration.outputMax);
        this->ParseCalibrationInteger(body, "NeutralTolerancePercent", calibration.neutralTolerancePercent);
        this->ParseCalibrationFinite(body, "RedScale", calibration.redScale);
        this->ParseCalibrationFinite(body, "GreenScale", calibration.greenScale);
        this->ParseCalibrationFinite(body, "BlueScale", calibration.blueScale);
        this->ParseCalibrationFinite(body, "NeutralRedScale", calibration.neutralRedScale);
        this->ParseCalibrationFinite(body, "NeutralGreenScale", calibration.neutralGreenScale);
        this->ParseCalibrationFinite(body, "NeutralBlueScale", calibration.neutralBlueScale);
        this->ParseCalibrationFinite(body, "NeutralCurve", calibration.neutralCurve);
        this->result_.surface.colorCalibration = calibration;
    }

    void ParseCalibrationInteger(const Format2ParsedProfileBody& body, const std::string& name, int& destination) const {
        const auto property = body.properties.find(name);
        if (property != body.properties.end()) ParseFormat2IntegerScalar(property->second->value.scalar, destination);
    }

    void ParseCalibrationOptionalInteger(const Format2ParsedProfileBody& body, const std::string& name, std::optional<int>& destination) const {
        const auto property = body.properties.find(name);
        if (property == body.properties.end()) return;
        int value = 0;
        if (ParseFormat2IntegerScalar(property->second->value.scalar, value)) destination = value;
    }

    void ParseCalibrationFinite(const Format2ParsedProfileBody& body, const std::string& name, double& destination) const {
        const auto property = body.properties.find(name);
        if (property != body.properties.end()) ParseFormat2FiniteScalar(property->second->value.scalar, destination);
    }

    void ParseInitialization(const Format2SyntaxNode& node) {
        if (node.positionalTokens.size() != 1 || !node.properties.empty()) {
            this->AddDiagnostic("format2.surface.initialize.header", "Initialize does not accept an ID or header properties", node.location);
            return;
        }
        if (this->result_.surface.initialization) {
            this->AddDiagnostic("format2.surface.initialize.duplicate", "Initialize can occur only once", node.location);
            return;
        }
        if (this->result_.document.metadata.protocol != Format2SurfaceProtocol::Midi) {
            this->AddDiagnostic("format2.surface.initialize.protocol", "Initialize with MIDI messages requires Protocol=MIDI", node.location);
            return;
        }
        Format2ParsedProfileBody body;
        if (!this->ParseSurfaceBlockBody(node, "Initialize", body)) return;
        Format2SurfaceInitialization initialization;
        initialization.location = node.location;
        const auto messages = body.lines.find("MIDI");
        if (messages != body.lines.end()) {
            for (const Format2SyntaxNode* messageNode : messages->second) {
                const Format2PropertySyntax* bytesProperty = FindFormat2NodeProperty(*messageNode, "Bytes");
                if (!bytesProperty || !bytesProperty->value.list) continue;
                Format2MidiInitializationMessage message;
                message.location = messageNode->location;
                for (const Format2ScalarSyntax& item : bytesProperty->value.items) {
                    int byte = 0;
                    if (ParseFormat2IntegerScalar(item, byte)) message.bytes.push_back(byte);
                }
                if (!message.bytes.empty()) initialization.midiMessages.push_back(std::move(message));
            }
        }
        this->result_.surface.initialization = std::move(initialization);
    }

    void ParseFeedbackGroup(const Format2SyntaxNode& node) {
        std::vector<Format2SurfaceNamedBlock> declaration;
        this->ParseNamedBlock(node, declaration);
        if (declaration.empty()) return;
        Format2ParsedProfileBody body;
        if (!this->ParseSurfaceBlockBody(node, "FeedbackGroup", body)) return;
        Format2FeedbackGroup group;
        group.id = node.positionalTokens[1].text;
        group.location = node.location;
        const auto colorProfile = body.properties.find("ColorProfile");
        const auto payload = body.properties.find("Payload");
        if (colorProfile == body.properties.end() || payload == body.properties.end()) return;
        group.colorProfile = colorProfile->second->value.scalar.text;
        const auto emptyColor = body.properties.find("EmptyColor");
        if (emptyColor != body.properties.end()) ParseFormat2ColorScalar(emptyColor->second->value.scalar, group.emptyColor);
        const auto condition = body.properties.find("UseTrackColorWhen");
        if (condition != body.properties.end() && condition->second->value.scalar.text == "SourceTextPresent") group.useTrackColorWhen = Format2TrackColorCondition::SourceTextPresent;
        if (payload->second->value.list) {
            for (std::size_t itemIdx = 0; itemIdx + 1 < payload->second->value.items.size(); ++itemIdx) {
                int value = 0;
                if (ParseFormat2IntegerScalar(payload->second->value.items[itemIdx], value)) group.payloadPrefix.push_back(value);
            }
        }
        const auto slots = body.lines.find("Slot");
        if (slots != body.lines.end()) {
            for (const Format2SyntaxNode* slot : slots->second) this->ParseFeedbackGroupSlot(*slot, group);
        }
        this->result_.surface.feedbackGroups.push_back(std::move(group));
    }

    void ParseFeedbackGroupSlot(const Format2SyntaxNode& node, Format2FeedbackGroup& group) {
        const Format2PropertySyntax* source = FindFormat2NodeProperty(node, "Source");
        const Format2PropertySyntax* members = FindFormat2NodeProperty(node, "Members");
        if (!source || !members) return;
        Format2FeedbackGroupSlot slot;
        slot.location = node.location;
        slot.source = source->value.scalar.text;
        if (members->value.list) {
            for (const Format2ScalarSyntax& member : members->value.items) slot.members.push_back(member.text);
        }
        group.slots.push_back(std::move(slot));
    }

    void ParseOskLayout(const Format2SyntaxNode& node) {
        if (node.positionalTokens.size() != 1 || !node.properties.empty()) {
            this->AddDiagnostic("format2.surface.osk-layout.header", "OSKLayout does not accept an ID or header properties", node.location);
            return;
        }
        if (this->result_.surface.oskLayout) {
            this->AddDiagnostic("format2.surface.osk-layout.duplicate", "OSKLayout can occur only once", node.location);
            return;
        }
        if (!FindFormat2SurfaceBlockDefinition("OSKLayout")) {
            this->AddDiagnostic("format2.surface.schema.block", "The Surface I/O schema has no OSKLayout definition", node.location);
            return;
        }
        Format2OskLayout layout;
        layout.location = node.location;
        for (const Format2SyntaxNode& rowNode : node.children) {
            if (rowNode.kind != Format2SyntaxNodeKind::Block || rowNode.positionalTokens.size() != 1 || rowNode.positionalTokens[0].text != "Row" || !rowNode.properties.empty()) {
                this->AddDiagnostic("format2.surface.osk-layout.row", "OSKLayout accepts only Row blocks without properties", rowNode.location);
                continue;
            }
            this->ParseOskRow(rowNode, layout);
        }
        if (layout.rows.empty()) this->AddDiagnostic("format2.surface.osk-layout.row.required", "OSKLayout requires at least one Row", node.location);
        this->result_.surface.oskLayout = std::move(layout);
    }

    void ParseOskRow(const Format2SyntaxNode& node, Format2OskLayout& layout) {
        Format2OskRow row;
        row.location = node.location;
        for (const Format2SyntaxNode& cellNode : node.children) {
            if (cellNode.kind != Format2SyntaxNodeKind::Line || cellNode.positionalTokens.empty() || cellNode.positionalTokens[0].kind != Format2TokenKind::Bare) {
                this->AddDiagnostic("format2.surface.osk-layout.cell", "OSK Row accepts only Widget or Spacer lines", cellNode.location);
                continue;
            }
            const std::string& cellType = cellNode.positionalTokens[0].text;
            const Format2SurfaceLineDefinition* definition = FindFormat2SurfaceLineDefinition("OSKRow", cellType);
            if (!definition) {
                this->AddDiagnostic("format2.surface.osk-layout.cell.unknown", "Unknown OSK Row entry: " + cellType, cellNode.positionalTokens[0].location);
                continue;
            }
            const std::size_t expectedTokenCount = definition->argumentRule.empty() ? 1 : 2;
            if (cellNode.positionalTokens.size() != expectedTokenCount || (expectedTokenCount == 2 && cellNode.positionalTokens[1].kind != Format2TokenKind::Bare)) {
                this->AddDiagnostic("format2.surface.osk-layout.cell.argument", "OSK " + cellType + " has an invalid positional argument", cellNode.location);
                continue;
            }
            this->ValidateSurfaceLine("OSKRow", cellNode, *definition);
            row.cells.push_back(this->ParseOskCell(cellNode));
        }
        if (row.cells.empty()) this->AddDiagnostic("format2.surface.osk-layout.cell.required", "OSK Row requires at least one Widget or Spacer", node.location);
        else layout.rows.push_back(std::move(row));
    }

    Format2OskCell ParseOskCell(const Format2SyntaxNode& node) const {
        Format2OskCell cell;
        cell.location = node.location;
        cell.kind = node.positionalTokens[0].text == "Spacer" ? Format2OskCellKind::Spacer : Format2OskCellKind::Widget;
        if (cell.kind == Format2OskCellKind::Spacer) cell.width = 0.5;
        else cell.widget = node.positionalTokens[1].text;
        for (const Format2PropertySyntax& property : node.properties) {
            if (property.name == "Shape") cell.shape = property.value.scalar.text;
            else if (property.name == "Width") ParseFormat2FiniteScalar(property.value.scalar, cell.width);
            else if (property.name == "Height") ParseFormat2FiniteScalar(property.value.scalar, cell.height);
            else if (property.name == "Top") ParseFormat2FiniteScalar(property.value.scalar, cell.top);
            else if (property.name == "Group") cell.group = property.value.scalar.text;
            else if (property.name == "Label") cell.label = property.value.scalar.text;
            else if (property.name == "Color") {
                std::uint32_t color = 0;
                if (ParseFormat2ColorScalar(property.value.scalar, color)) cell.color = color;
            } else if (property.name == "Role") cell.role = property.value.scalar.text;
            else if (property.name == "PressTarget") cell.pressTarget = property.value.scalar.text;
            else if (property.name == "ScrollTarget") cell.scrollTarget = property.value.scalar.text;
            else if (property.name == "ValueTarget") cell.valueTarget = property.value.scalar.text;
            else if (property.name == "TouchTarget") cell.touchTarget = property.value.scalar.text;
            else if (property.name == "RotaryStyle") cell.rotaryStyle = property.value.scalar.text;
        }
        return cell;
    }

    void ValidateProfileReferences() {
        for (const Format2SurfaceWidget& widget : this->result_.surface.widgets) {
            for (const Format2SurfacePrimitive& primitive : widget.primitives) {
                if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder") {
                    const Format2PropertySyntax* profile = FindFormat2PrimitiveProperty(primitive, "Profile");
                    if (profile && !profile->value.list && !profile->value.scalar.quoted && this->encoderProfileIds_.find(profile->value.scalar.text) == this->encoderProfileIds_.end()) this->AddDiagnostic("format2.surface.encoder-profile.reference", "Input Encoder references unknown EncoderProfile: " + profile->value.scalar.text, profile->value.location);
                }
                const Format2PropertySyntax* valueProfile = FindFormat2PrimitiveProperty(primitive, "ValueProfile");
                if (valueProfile && !valueProfile->value.list && !valueProfile->value.scalar.quoted) this->ValidateValueProfileReference(primitive, *valueProfile);
                const Format2PropertySyntax* colorProfile = FindFormat2PrimitiveProperty(primitive, "ColorProfile");
                if (colorProfile && !colorProfile->value.list && !colorProfile->value.scalar.quoted) this->ValidateColorProfileReference(primitive, *colorProfile);
                const Format2PropertySyntax* ringProfile = FindFormat2PrimitiveProperty(primitive, "RingProfile");
                if (ringProfile && !ringProfile->value.list && !ringProfile->value.scalar.quoted) this->ValidateRingProfileReference(primitive, *ringProfile);
                const Format2PropertySyntax* barProfile = FindFormat2PrimitiveProperty(primitive, "BarProfile");
                if (barProfile && !barProfile->value.list && !barProfile->value.scalar.quoted) this->ValidateSimpleProfileReference(*barProfile, "BarProfile", this->barProfileIds_);
                const Format2PropertySyntax* meterProfile = FindFormat2PrimitiveProperty(primitive, "MeterProfile");
                if (meterProfile && !meterProfile->value.list && !meterProfile->value.scalar.quoted) this->ValidateMeterProfileReference(*meterProfile);
                const Format2PropertySyntax* textProfile = FindFormat2PrimitiveProperty(primitive, "TextProfile");
                if (textProfile && !textProfile->value.list && !textProfile->value.scalar.quoted) this->ValidateTextProfileReference(primitive, *textProfile);
            }
        }
    }

    void ValidateValueProfileReference(const Format2SurfacePrimitive& primitive, const Format2PropertySyntax& property) {
        if (this->valueProfileIds_.find(property.value.scalar.text) == this->valueProfileIds_.end()) {
            this->AddDiagnostic("format2.surface.value-profile.reference", "Primitive references unknown ValueProfile: " + property.value.scalar.text, property.value.location);
            return;
        }
        const auto profile = this->valueProfileDirections_.find(property.value.scalar.text);
        if (profile == this->valueProfileDirections_.end()) return;
        const bool supportsDirection = primitive.direction == Format2PrimitiveDirection::Input ? profile->second == Format2ValueProfileDirection::Decode || profile->second == Format2ValueProfileDirection::Both : profile->second == Format2ValueProfileDirection::Encode || profile->second == Format2ValueProfileDirection::Both;
        if (!supportsDirection) this->AddDiagnostic("format2.surface.value-profile.direction", "ValueProfile " + property.value.scalar.text + " does not support this primitive direction", property.value.location);
    }

    void ValidateColorProfileReference(const Format2SurfacePrimitive& primitive, const Format2PropertySyntax& property) {
        if (this->colorProfileIds_.find(property.value.scalar.text) == this->colorProfileIds_.end()) {
            this->AddDiagnostic("format2.surface.color-profile.reference", "Feedback references unknown ColorProfile: " + property.value.scalar.text, property.value.location);
            return;
        }
        const auto midiSafe = this->colorProfileMidiSafe_.find(property.value.scalar.text);
        if (primitive.encoding == Format2Encoding::MidiPalette && midiSafe != this->colorProfileMidiSafe_.end() && !midiSafe->second) this->AddDiagnostic("format2.surface.color-profile.midi-range", "MIDIPalette requires every ColorProfile Value to fit one MIDI data byte from 0 through 0x7F", property.value.location);
    }

    void ValidateSimpleProfileReference(const Format2PropertySyntax& property, const std::string& profileName, const std::map<std::string, Format2SourceLocation>& profileIds) {
        if (profileIds.find(property.value.scalar.text) == profileIds.end()) this->AddDiagnostic("format2.surface.profile.reference", "Feedback references unknown " + profileName + ": " + property.value.scalar.text, property.value.location);
    }

    void ValidateRingProfileReference(const Format2SurfacePrimitive& primitive, const Format2PropertySyntax& property) {
        if (this->ringProfileIds_.find(property.value.scalar.text) == this->ringProfileIds_.end()) {
            this->AddDiagnostic("format2.surface.ring-profile.reference", "Feedback references unknown RingProfile: " + property.value.scalar.text, property.value.location);
            return;
        }
        const auto segments = this->ringProfileSegments_.find(property.value.scalar.text);
        if (HasFormat2PrimitiveNestedBlock(primitive, "Configure") && (segments == this->ringProfileSegments_.end() || !segments->second)) this->AddDiagnostic("format2.surface.ring-profile.configure.segments", "Feedback Ring Configure requires a RingProfile with Segments", property.value.location);
        const auto midiSafe = this->ringProfileMidiSafe_.find(property.value.scalar.text);
        if (this->result_.document.metadata.protocol == Format2SurfaceProtocol::Midi && midiSafe != this->ringProfileMidiSafe_.end() && !midiSafe->second) this->AddDiagnostic("format2.surface.ring-profile.midi-range", "MIDI RingProfile values and style codes must fit one MIDI data byte from 0 through 0x7F", property.value.location);
    }

    void ValidateMeterProfileReference(const Format2PropertySyntax& property) {
        if (this->meterProfileIds_.find(property.value.scalar.text) == this->meterProfileIds_.end()) {
            this->AddDiagnostic("format2.surface.meter-profile.reference", "Feedback references unknown MeterProfile: " + property.value.scalar.text, property.value.location);
            return;
        }
        const auto midiSafe = this->meterProfileMidiSafe_.find(property.value.scalar.text);
        if (this->result_.document.metadata.protocol == Format2SurfaceProtocol::Midi && midiSafe != this->meterProfileMidiSafe_.end() && !midiSafe->second) this->AddDiagnostic("format2.surface.meter-profile.midi-range", "MIDI MeterProfile outputs must fit one MIDI data byte from 0 through 0x7F", property.value.location);
    }

    void ValidateTextProfileReference(const Format2SurfacePrimitive& primitive, const Format2PropertySyntax& property) {
        if (this->textProfileIds_.find(property.value.scalar.text) == this->textProfileIds_.end()) {
            this->AddDiagnostic("format2.surface.text-profile.reference", "Feedback references unknown TextProfile: " + property.value.scalar.text, property.value.location);
            return;
        }
        const auto encoding = this->textProfileEncodings_.find(property.value.scalar.text);
        if (this->result_.document.metadata.protocol == Format2SurfaceProtocol::Midi && encoding != this->textProfileEncodings_.end() && encoding->second != Format2TextEncoding::Ascii7) this->AddDiagnostic("format2.surface.text-profile.midi-encoding", "MIDI Text feedback requires TextProfile Encoding=ASCII7", property.value.location);
        const auto width = this->textProfileWidths_.find(property.value.scalar.text);
        if (primitive.encoding == Format2Encoding::MidiCharacters && (width == this->textProfileWidths_.end() || !width->second)) this->AddDiagnostic("format2.surface.text-profile.characters.width", "MIDICharacters requires a TextProfile with Width", property.value.location);
    }

    Format2SurfaceWidget* FindWidget(const std::string& id) {
        for (Format2SurfaceWidget& widget : this->result_.surface.widgets) {
            if (widget.id == id) return &widget;
        }
        return nullptr;
    }

    const Format2SurfaceWidget* FindWidget(const std::string& id) const {
        for (const Format2SurfaceWidget& widget : this->result_.surface.widgets) {
            if (widget.id == id) return &widget;
        }
        return nullptr;
    }

    bool WidgetHasCapability(const Format2SurfaceWidget& widget, Format2Capability capability) const {
        for (Format2Capability existing : widget.capabilities) {
            if (existing == capability) return true;
        }
        return false;
    }

    void ValidateFeedbackGroups() {
        std::map<std::string, std::string> memberGroups;
        std::map<std::vector<int>, Format2SourceLocation> payloadOwners;
        for (Format2FeedbackGroup& group : this->result_.surface.feedbackGroups) {
            if (this->result_.document.metadata.protocol != Format2SurfaceProtocol::Midi) this->AddDiagnostic("format2.surface.feedback-group.protocol", "FeedbackGroup Encoding=MIDISysEx requires Surface Protocol=MIDI", group.location);
            const auto colorProfile = this->colorProfileIds_.find(group.colorProfile);
            if (colorProfile == this->colorProfileIds_.end()) this->AddDiagnostic("format2.surface.feedback-group.color-profile.reference", "FeedbackGroup references unknown ColorProfile: " + group.colorProfile, group.location);
            const auto midiSafe = this->colorProfileMidiSafe_.find(group.colorProfile);
            if (midiSafe != this->colorProfileMidiSafe_.end() && !midiSafe->second) this->AddDiagnostic("format2.surface.feedback-group.color-profile.midi-range", "FeedbackGroup ColorProfile values must fit one MIDI data byte from 0 through 0x7F", group.location);
            if (!payloadOwners.emplace(group.payloadPrefix, group.location).second) this->AddDiagnostic("format2.surface.feedback-group.output.duplicate", "FeedbackGroup MIDISysEx output prefix is already owned by another FeedbackGroup", group.location);
            std::set<int> slotChannels;
            for (Format2FeedbackGroupSlot& slot : group.slots) this->ValidateFeedbackGroupSlot(group, slot, slotChannels, memberGroups);
        }
    }

    void ValidateFeedbackGroupSlot(Format2FeedbackGroup& group, Format2FeedbackGroupSlot& slot, std::set<int>& slotChannels, std::map<std::string, std::string>& memberGroups) {
        Format2SurfaceWidget* source = this->FindWidget(slot.source);
        if (!source) {
            this->AddDiagnostic("format2.surface.feedback-group.source.reference", "FeedbackGroup Slot references unknown Source Widget: " + slot.source, slot.location);
        } else if (!source->channel) {
            this->AddDiagnostic("format2.surface.feedback-group.source.channel", "FeedbackGroup Slot Source requires an explicit positive Widget Channel: " + slot.source, slot.location);
        } else if (!slotChannels.insert(*source->channel).second) {
            this->AddDiagnostic("format2.surface.feedback-group.slot.channel.duplicate", "FeedbackGroup Slots must use different Widget Channels", slot.location);
        }
        if (group.useTrackColorWhen == Format2TrackColorCondition::SourceTextPresent && source && !this->WidgetHasCapability(*source, Format2Capability::Text)) this->AddDiagnostic("format2.surface.feedback-group.source.text", "UseTrackColorWhen=SourceTextPresent requires Text feedback on Source Widget " + slot.source, slot.location);
        bool sourceIsMember = false;
        std::set<std::string> slotMembers;
        for (const std::string& memberId : slot.members) {
            if (!slotMembers.insert(memberId).second) this->AddDiagnostic("format2.surface.feedback-group.member.duplicate", "FeedbackGroup Slot Member is duplicated: " + memberId, slot.location);
            if (memberId == slot.source) sourceIsMember = true;
            Format2SurfaceWidget* member = this->FindWidget(memberId);
            if (!member) {
                this->AddDiagnostic("format2.surface.feedback-group.member.reference", "FeedbackGroup Slot references unknown Member Widget: " + memberId, slot.location);
                continue;
            }
            if (!member->channel) this->AddDiagnostic("format2.surface.feedback-group.member.channel", "FeedbackGroup Slot Member requires an explicit positive Widget Channel: " + memberId, slot.location);
            else if (source && source->channel && *member->channel != *source->channel) this->AddDiagnostic("format2.surface.feedback-group.member.channel.mismatch", "FeedbackGroup Slot Members must use the Source Widget Channel", slot.location);
            const auto existingGroup = memberGroups.find(memberId);
            if (existingGroup != memberGroups.end()) this->AddDiagnostic("format2.surface.feedback-group.member.owner", "Widget " + memberId + " already belongs to TrackColor FeedbackGroup " + existingGroup->second, slot.location);
            else memberGroups[memberId] = group.id;
            AddFormat2Capability(member->capabilities, Format2Capability::TrackColor);
        }
        if (!sourceIsMember) this->AddDiagnostic("format2.surface.feedback-group.source.member", "FeedbackGroup Slot Source must occur in its Members list: " + slot.source, slot.location);
    }

    void ValidateColorCalibration() {
        if (!this->result_.surface.colorCalibration) return;
        for (const Format2SurfaceWidget& widget : this->result_.surface.widgets) {
            if (this->WidgetHasCapability(widget, Format2Capability::Color) || this->WidgetHasCapability(widget, Format2Capability::TrackColor)) return;
        }
        this->AddDiagnostic("format2.surface.color-calibration.capability", "ColorCalibration requires at least one Color or TrackColor Feedback capability", this->result_.surface.colorCalibration->location);
    }

    void ValidateOskLayout() {
        if (!this->result_.surface.oskLayout) return;
        std::set<std::string> visibleWidgets;
        for (const Format2OskRow& row : this->result_.surface.oskLayout->rows) {
            for (const Format2OskCell& cell : row.cells) {
                if (cell.kind == Format2OskCellKind::Spacer) continue;
                if (!visibleWidgets.insert(cell.widget).second) this->AddDiagnostic("format2.surface.osk-layout.widget.duplicate", "OSKLayout Widget can be visible only once: " + cell.widget, cell.location);
                if (!this->FindWidget(cell.widget)) this->AddDiagnostic("format2.surface.osk-layout.widget.reference", "OSKLayout references unknown Widget: " + cell.widget, cell.location);
                this->ValidateOskTarget(cell.pressTarget, "PressTarget", Format2Capability::Press, cell.location);
                this->ValidateOskTarget(cell.scrollTarget, "ScrollTarget", Format2Capability::Relative, cell.location);
                this->ValidateOskTarget(cell.valueTarget, "ValueTarget", Format2Capability::Absolute, cell.location);
                this->ValidateOskTarget(cell.touchTarget, "TouchTarget", Format2Capability::Touch, cell.location);
            }
        }
    }

    void ValidateOskTarget(const std::string& targetId, const std::string& propertyName, Format2Capability capability, const Format2SourceLocation& location) {
        if (targetId.empty()) return;
        const Format2SurfaceWidget* target = this->FindWidget(targetId);
        if (!target) {
            this->AddDiagnostic("format2.surface.osk-layout.target.reference", propertyName + " references unknown Widget: " + targetId, location);
            return;
        }
        if (!this->WidgetHasCapability(*target, capability)) this->AddDiagnostic("format2.surface.osk-layout.target.capability", propertyName + " Widget does not provide the required capability: " + targetId, location);
    }

    void ParseWidget(const Format2SyntaxNode& node) {
        if (node.positionalTokens.size() != 2 || node.positionalTokens[1].kind != Format2TokenKind::Bare || !node.properties.empty()) {
            this->AddDiagnostic("format2.surface.widget.header", "Widget requires exactly one identifier", node.location);
            return;
        }
        const Format2Token& idToken = node.positionalTokens[1];
        if (!IsValidFormat2Identifier(idToken.text)) {
            this->AddDiagnostic("format2.surface.widget.id", "Widget ID is not a valid identifier: " + idToken.text, idToken.location);
            return;
        }
        const auto existing = this->widgetIds_.find(idToken.text);
        if (existing != this->widgetIds_.end()) {
            this->AddDiagnostic("format2.surface.widget.duplicate", "Widget ID is duplicated: " + idToken.text, idToken.location);
            return;
        }
        this->widgetIds_[idToken.text] = idToken.location;

        Format2SurfaceWidget widget;
        widget.id = idToken.text;
        widget.location = node.location;
        bool aliasSeen = false;
        bool channelSeen = false;
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind == Format2SyntaxNodeKind::Line) {
                this->ParseWidgetProperties(child, widget, aliasSeen, channelSeen);
                continue;
            }
            this->ParsePrimitive(child, widget);
        }
        for (const Format2SurfacePrimitive& primitive : widget.primitives) {
            for (Format2Capability capability : primitive.capabilities) {
                AddFormat2Capability(widget.capabilities, capability);
                if (capability == Format2Capability::TrackColor && !widget.channel) this->AddDiagnostic("format2.surface.track-color.channel", "Feedback Color with TrackColor=true requires Widget Channel", primitive.location);
            }
        }
        if (widget.primitives.empty()) this->AddDiagnostic("format2.surface.widget.primitive.required", "Widget " + widget.id + " requires at least one Input or Feedback block", widget.location);
        this->result_.surface.widgets.push_back(std::move(widget));
    }

    void ParseWidgetProperties(const Format2SyntaxNode& node, Format2SurfaceWidget& widget, bool& aliasSeen, bool& channelSeen) {
        if (!node.positionalTokens.empty()) this->AddDiagnostic("format2.surface.widget.line", "Widget lines can contain only Alias and Channel properties", node.positionalTokens.front().location);
        for (const Format2PropertySyntax& property : node.properties) {
            if (property.name == "Alias") {
                if (aliasSeen) {
                    this->AddDiagnostic("format2.surface.widget.alias.duplicate", "Widget Alias is duplicated", property.nameLocation);
                } else if (property.value.list || !property.value.scalar.quoted || property.value.scalar.text.empty()) {
                    this->AddDiagnostic("format2.surface.widget.alias.value", "Widget Alias requires one non-empty quoted string", property.value.location);
                } else {
                    widget.alias = property.value.scalar.text;
                }
                aliasSeen = true;
            } else if (property.name == "Channel") {
                int channel = 0;
                if (channelSeen) {
                    this->AddDiagnostic("format2.surface.widget.channel.duplicate", "Widget Channel is duplicated", property.nameLocation);
                } else if (!ParsePositiveFormat2Integer(property.value, channel)) {
                    this->AddDiagnostic("format2.surface.widget.channel.value", "Widget Channel requires a positive integer", property.value.location);
                } else {
                    widget.channel = channel;
                }
                channelSeen = true;
            } else {
                this->AddDiagnostic("format2.surface.widget.property.unknown", "Unknown Widget property: " + property.name, property.nameLocation);
            }
        }
    }

    void ParsePrimitive(const Format2SyntaxNode& node, Format2SurfaceWidget& widget) {
        if (node.positionalTokens.size() != 2 || node.positionalTokens[0].kind != Format2TokenKind::Bare || node.positionalTokens[1].kind != Format2TokenKind::Bare || !node.properties.empty()) {
            this->AddDiagnostic("format2.surface.primitive.header", "Widget child block must use Input Type or Feedback Type", node.location);
            return;
        }
        const std::string& direction = node.positionalTokens[0].text;
        if (direction != "Input" && direction != "Feedback") {
            this->AddDiagnostic("format2.surface.primitive.direction", "Unknown Widget child block: " + direction, node.location);
            return;
        }
        if (!IsValidFormat2Identifier(node.positionalTokens[1].text)) {
            this->AddDiagnostic("format2.surface.primitive.type", "Primitive type is not a valid identifier: " + node.positionalTokens[1].text, node.positionalTokens[1].location);
            return;
        }

        Format2SurfacePrimitive primitive;
        primitive.direction = direction == "Input" ? Format2PrimitiveDirection::Input : Format2PrimitiveDirection::Feedback;
        primitive.type = node.positionalTokens[1].text;
        primitive.location = node.location;
        for (const Format2SyntaxNode& child : node.children) {
            if (child.kind == Format2SyntaxNodeKind::Block) {
                primitive.nestedBlocks.push_back(child);
                continue;
            }
            if (!child.positionalTokens.empty()) this->AddDiagnostic("format2.surface.primitive.line", direction + " " + primitive.type + " lines can contain only named properties", child.positionalTokens.front().location);
            primitive.properties.insert(primitive.properties.end(), child.properties.begin(), child.properties.end());
        }
        this->ValidatePrimitive(primitive, direction);
        widget.primitives.push_back(std::move(primitive));
    }

    void ValidatePrimitive(Format2SurfacePrimitive& primitive, const std::string& directionName) {
        const Format2PrimitiveDefinition* primitiveDefinition = FindFormat2PrimitiveDefinition(primitive.direction, primitive.type);
        if (!primitiveDefinition) {
            this->AddDiagnostic("format2.surface.primitive.unknown", "Unknown " + directionName + " primitive: " + primitive.type, primitive.location);
            return;
        }
        primitive.capabilities = primitiveDefinition->capabilities;

        std::map<std::string, const Format2PropertySyntax*> properties;
        for (const Format2PropertySyntax& property : primitive.properties) {
            if (properties.find(property.name) != properties.end()) this->AddDiagnostic("format2.surface.primitive.property.duplicate", directionName + " " + primitive.type + " property is duplicated: " + property.name, property.nameLocation);
            else properties[property.name] = &property;
        }

        if (!this->result_.document.metadata.protocol) return;
        const Format2PropertySyntax* encodingProperty = FindFormat2PrimitiveProperty(primitive, "Encoding");
        const Format2RepresentationDefinition* representation = nullptr;
        if (encodingProperty) {
            if (encodingProperty->value.list || encodingProperty->value.scalar.quoted || encodingProperty->value.scalar.text.empty()) {
                this->AddDiagnostic("format2.surface.encoding.value", "Encoding requires one unquoted encoding name", encodingProperty->value.location);
                return;
            }
            representation = FindFormat2RepresentationDefinition(primitive.direction, primitive.type, *this->result_.document.metadata.protocol, encodingProperty->value.scalar.text);
            if (!representation) {
                this->AddDiagnostic("format2.surface.encoding.unsupported", "Encoding " + encodingProperty->value.scalar.text + " is not valid for " + directionName + " " + primitive.type + " with this Surface protocol", encodingProperty->value.location);
                return;
            }
        } else {
            std::vector<const Format2RepresentationDefinition*> matches;
            for (const Format2RepresentationDefinition* candidate : FindFormat2RepresentationDefinitions(primitive.direction, primitive.type, *this->result_.document.metadata.protocol)) {
                if (Format2PrimitiveMatchesRepresentation(primitive, *candidate)) matches.push_back(candidate);
            }
            if (matches.size() != 1) {
                const std::string reason = matches.empty() ? "the declared properties do not match one representation" : "more than one representation matches the declared properties";
                this->AddDiagnostic("format2.surface.encoding.required", "Encoding is required for " + directionName + " " + primitive.type + " because " + reason, primitive.location);
                return;
            }
            representation = matches.front();
        }

        primitive.encoding = representation->encoding;
        for (const Format2PropertySyntax& property : primitive.properties) {
            if (!Format2RepresentationAllowsProperty(*representation, property.name)) {
                this->AddDiagnostic("format2.surface.primitive.property.unknown", "Property " + property.name + " is not valid for " + directionName + " " + primitive.type + " with Encoding=" + representation->encodingName, property.nameLocation);
                continue;
            }
            if (property.name == "Encoding") continue;
            const Format2PropertyRule* rule = FindFormat2PropertyRule(*representation, property.name);
            if (!rule) {
                this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for property " + property.name, property.nameLocation);
                continue;
            }
            const std::string expected = ValidateFormat2ValueRule(property, rule->valueRule);
            if (!expected.empty()) this->AddDiagnostic("format2.surface.primitive.property.value", "Property " + property.name + " requires " + expected, property.value.location);
        }
        for (const std::string& required : representation->requiredProperties) {
            if (!FindFormat2PrimitiveProperty(primitive, required)) this->AddDiagnostic("format2.surface.primitive.property.required", directionName + " " + primitive.type + " with Encoding=" + representation->encodingName + " requires property " + required, primitive.location);
        }
        for (const Format2ConstraintViolation& violation : ValidateFormat2PropertyConstraints(primitive.properties, representation->constraints, primitive.location)) {
            this->AddDiagnostic("format2.surface.primitive.property.constraint", violation.message, violation.location);
        }

        std::set<std::string> nestedBlocks;
        for (const Format2SyntaxNode& block : primitive.nestedBlocks) {
            if (block.positionalTokens.empty()) {
                this->AddDiagnostic("format2.surface.primitive.nested.header", directionName + " " + primitive.type + " contains a child block without a type", block.location);
                continue;
            }
            const std::string& blockName = block.positionalTokens[0].text;
            if (!Format2RepresentationAllowsNestedBlock(*representation, blockName)) this->AddDiagnostic("format2.surface.primitive.nested.unknown", blockName + " is not valid inside " + directionName + " " + primitive.type, block.location);
            if (!nestedBlocks.insert(blockName).second) this->AddDiagnostic("format2.surface.primitive.nested.duplicate", blockName + " can occur only once inside " + directionName + " " + primitive.type, block.location);
            if (Format2RepresentationAllowsNestedBlock(*representation, blockName)) this->ValidateNestedBlock(block, primitive);
        }

        for (const Format2CapabilityCondition* condition : FindFormat2CapabilityConditions(primitive.direction, primitive.type)) {
            if (Format2PrimitiveMatchesCapabilityCondition(primitive, *condition)) AddFormat2Capability(primitive.capabilities, condition->capability);
        }
    }

    void ValidateNestedBlock(const Format2SyntaxNode& block, const Format2SurfacePrimitive& primitive) {
        if (block.positionalTokens.size() != 1 || block.positionalTokens[0].kind != Format2TokenKind::Bare || !block.properties.empty()) {
            this->AddDiagnostic("format2.surface.primitive.nested.header", "A nested transport block requires only its block name", block.location);
            return;
        }
        if (!this->result_.document.metadata.protocol) return;
        const std::string& blockName = block.positionalTokens[0].text;
        const Format2NestedBlockDefinition* definition = FindFormat2NestedBlockDefinition(blockName, primitive.direction, primitive.type, *this->result_.document.metadata.protocol);
        if (!definition) {
            this->AddDiagnostic("format2.surface.schema.nested", "The Surface I/O schema has no definition for nested block " + blockName, block.location);
            return;
        }

        std::vector<const Format2PropertySyntax*> properties;
        std::map<std::string, const Format2PropertySyntax*> propertiesByName;
        for (const Format2SyntaxNode& child : block.children) {
            if (child.kind == Format2SyntaxNodeKind::Block) {
                this->AddDiagnostic("format2.surface.primitive.nested.depth", blockName + " cannot contain another block", child.location);
                continue;
            }
            if (!child.positionalTokens.empty()) this->AddDiagnostic("format2.surface.primitive.nested.line", blockName + " lines can contain only named properties", child.positionalTokens.front().location);
            for (const Format2PropertySyntax& property : child.properties) {
                properties.push_back(&property);
                if (propertiesByName.find(property.name) != propertiesByName.end()) this->AddDiagnostic("format2.surface.primitive.nested.property.duplicate", blockName + " property is duplicated: " + property.name, property.nameLocation);
                else propertiesByName[property.name] = &property;
            }
        }

        for (const std::string& required : definition->requiredProperties) {
            if (propertiesByName.find(required) == propertiesByName.end()) this->AddDiagnostic("format2.surface.primitive.nested.property.required", blockName + " requires property " + required, block.location);
        }
        for (const Format2PropertySyntax* property : properties) {
            bool allowed = false;
            for (const std::string& required : definition->requiredProperties) {
                if (required == property->name) allowed = true;
            }
            for (const std::string& optional : definition->optionalProperties) {
                if (optional == property->name) allowed = true;
            }
            if (!allowed) {
                this->AddDiagnostic("format2.surface.primitive.nested.property.unknown", "Property " + property->name + " is not valid in " + blockName, property->nameLocation);
                continue;
            }
            const Format2PropertyRule* rule = FindFormat2PropertyRule(*definition, property->name);
            if (!rule) {
                this->AddDiagnostic("format2.surface.schema.property-rule", "The Surface I/O schema has no value rule for property " + property->name, property->nameLocation);
                continue;
            }
            const std::string expected = ValidateFormat2ValueRule(*property, rule->valueRule);
            if (!expected.empty()) this->AddDiagnostic("format2.surface.primitive.nested.property.value", "Property " + property->name + " requires " + expected, property->value.location);
        }
    }
};

Format2SurfaceParseResult ParseFormat2SurfaceSource(const std::string& source, const std::string& sourcePath) {
    Format2SurfaceDocumentParser parser(source, sourcePath);
    return parser.Parse();
}
