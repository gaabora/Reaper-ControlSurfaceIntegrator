#include "format2_primitive_catalog.h"

#include <charconv>
#include <map>
#include <sstream>
#include <system_error>
#include <utility>

#include "surface_io_schema.h"

static std::vector<std::string> SplitFormat2SchemaList(const std::string& value) {
    if (value == "None") return {};
    std::vector<std::string> values;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        values.push_back(value.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return values;
}

static std::vector<Format2PropertyRule> ParseFormat2PropertyRules(const std::string& value) {
    std::vector<Format2PropertyRule> rules;
    for (const std::string& entry : SplitFormat2SchemaList(value)) {
        const std::size_t colon = entry.find(':');
        if (colon == std::string::npos) continue;
        rules.push_back({ entry.substr(0, colon), entry.substr(colon + 1) });
    }
    return rules;
}

static std::map<std::string, std::string> ParseFormat2SchemaProperties(const std::string& line) {
    std::map<std::string, std::string> properties;
    std::istringstream stream(line);
    std::string token;
    while (stream >> token) {
        const std::size_t equals = token.find('=');
        if (equals == std::string::npos) continue;
        properties[token.substr(0, equals)] = token.substr(equals + 1);
    }
    return properties;
}

static Format2PrimitiveDirection ParseFormat2SchemaDirection(const std::string& value) {
    return value == "Feedback" ? Format2PrimitiveDirection::Feedback : Format2PrimitiveDirection::Input;
}

static Format2SurfaceProtocol ParseFormat2SchemaProtocol(const std::string& value) {
    return value == "OSC" ? Format2SurfaceProtocol::Osc : Format2SurfaceProtocol::Midi;
}

static Format2Capability ParseFormat2SchemaCapability(const std::string& value) {
    static const std::map<std::string, Format2Capability> capabilities = {
        { "Press", Format2Capability::Press }, { "Release", Format2Capability::Release }, { "Absolute", Format2Capability::Absolute },
        { "Relative", Format2Capability::Relative }, { "Touch", Format2Capability::Touch }, { "Toggle", Format2Capability::Toggle },
        { "Value", Format2Capability::Value }, { "Color", Format2Capability::Color }, { "TrackColor", Format2Capability::TrackColor },
        { "Ring", Format2Capability::Ring }, { "Bar", Format2Capability::Bar }, { "Meter", Format2Capability::Meter }, { "Text", Format2Capability::Text },
    };
    return capabilities.at(value);
}

static Format2Encoding ParseFormat2SchemaEncoding(const std::string& value) {
    static const std::map<std::string, Format2Encoding> encodings = {
        { "MIDIExact", Format2Encoding::MidiExact }, { "MIDIPrefix", Format2Encoding::MidiPrefix }, { "MIDI7", Format2Encoding::Midi7 },
        { "MIDI14", Format2Encoding::Midi14 }, { "MIDISplit", Format2Encoding::MidiSplit }, { "MIDIRGB", Format2Encoding::MidiRgb },
        { "MIDIPalette", Format2Encoding::MidiPalette }, { "MIDISysEx", Format2Encoding::MidiSysEx }, { "MIDICharacters", Format2Encoding::MidiCharacters },
        { "OSCFloat", Format2Encoding::OscFloat }, { "OSCInt", Format2Encoding::OscInt }, { "OSCString", Format2Encoding::OscString },
    };
    return encodings.at(value);
}

struct Format2PrimitiveCatalogData {
    std::vector<Format2PrimitiveDefinition> primitives;
    std::vector<Format2CapabilityCondition> capabilityConditions;
    std::vector<Format2RepresentationDefinition> representations;
    std::vector<Format2NestedBlockDefinition> nestedBlocks;
    std::vector<Format2ProfileDefinition> profiles;
    std::vector<Format2ProfileLineDefinition> profileLines;
    std::vector<Format2SurfaceBlockDefinition> surfaceBlocks;
    std::vector<Format2SurfaceLineDefinition> surfaceLines;
};

static Format2PrimitiveCatalogData ParseFormat2PrimitiveCatalog() {
    Format2PrimitiveCatalogData catalog;
    std::istringstream source(SurfaceIoSchema::Source);
    std::string line;
    while (std::getline(source, line)) {
        const std::size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#' || line.compare(first, 8, "Version=") == 0) continue;
        const std::map<std::string, std::string> properties = ParseFormat2SchemaProperties(line.substr(first));
        const auto conditionalCapability = properties.find("CapabilityWhen");
        if (conditionalCapability != properties.end()) {
            Format2CapabilityCondition condition;
            condition.direction = ParseFormat2SchemaDirection(properties.at("Direction"));
            condition.primitive = properties.at("Primitive");
            condition.capability = ParseFormat2SchemaCapability(conditionalCapability->second);
            const auto allProperties = properties.find("AllProperties");
            if (allProperties != properties.end()) condition.allProperties = SplitFormat2SchemaList(allProperties->second);
            const auto propertyEquals = properties.find("PropertyEquals");
            if (propertyEquals != properties.end()) {
                const std::size_t colon = propertyEquals->second.find(':');
                condition.equalProperty = propertyEquals->second.substr(0, colon);
                condition.equalValue = propertyEquals->second.substr(colon + 1);
            }
            const auto nested = properties.find("Nested");
            if (nested != properties.end()) condition.nestedBlock = nested->second;
            const auto listProperty = properties.find("ListProperty");
            if (listProperty != properties.end()) condition.listProperty = listProperty->second;
            const auto anyListItems = properties.find("AnyListItems");
            if (anyListItems != properties.end()) condition.anyListItems = SplitFormat2SchemaList(anyListItems->second);
            catalog.capabilityConditions.push_back(std::move(condition));
            continue;
        }
        const auto primitive = properties.find("Primitive");
        if (primitive != properties.end()) {
            Format2PrimitiveDefinition definition;
            definition.direction = ParseFormat2SchemaDirection(properties.at("Direction"));
            definition.name = primitive->second;
            for (const std::string& capability : SplitFormat2SchemaList(properties.at("Capabilities"))) definition.capabilities.push_back(ParseFormat2SchemaCapability(capability));
            catalog.primitives.push_back(std::move(definition));
            continue;
        }
        const auto nestedBlock = properties.find("NestedBlock");
        if (nestedBlock != properties.end()) {
            Format2NestedBlockDefinition definition;
            definition.name = nestedBlock->second;
            definition.parentDirection = ParseFormat2SchemaDirection(properties.at("ParentDirection"));
            definition.parentPrimitive = properties.at("ParentPrimitive");
            definition.protocol = ParseFormat2SchemaProtocol(properties.at("Protocol"));
            definition.requiredProperties = SplitFormat2SchemaList(properties.at("Required"));
            definition.optionalProperties = SplitFormat2SchemaList(properties.at("Optional"));
            definition.propertyRules = ParseFormat2PropertyRules(properties.at("Rules"));
            catalog.nestedBlocks.push_back(std::move(definition));
            continue;
        }
        const auto profile = properties.find("Profile");
        if (profile != properties.end()) {
            Format2ProfileDefinition definition;
            definition.name = profile->second;
            definition.requiredProperties = SplitFormat2SchemaList(properties.at("Required"));
            definition.optionalProperties = SplitFormat2SchemaList(properties.at("Optional"));
            definition.propertyRules = ParseFormat2PropertyRules(properties.at("Rules"));
            catalog.profiles.push_back(std::move(definition));
            continue;
        }
        const auto profileLine = properties.find("ProfileLine");
        if (profileLine != properties.end()) {
            Format2ProfileLineDefinition definition;
            definition.name = profileLine->second;
            definition.parentProfile = properties.at("ParentProfile");
            const auto argument = properties.find("Argument");
            if (argument != properties.end() && argument->second != "None") definition.argumentRule = argument->second;
            definition.requiredProperties = SplitFormat2SchemaList(properties.at("Required"));
            definition.optionalProperties = SplitFormat2SchemaList(properties.at("Optional"));
            definition.propertyRules = ParseFormat2PropertyRules(properties.at("Rules"));
            const std::string& minimumCount = properties.at("MinimumCount");
            const std::from_chars_result parsed = std::from_chars(minimumCount.data(), minimumCount.data() + minimumCount.size(), definition.minimumCount);
            if (parsed.ec != std::errc() || parsed.ptr != minimumCount.data() + minimumCount.size()) definition.minimumCount = 0;
            catalog.profileLines.push_back(std::move(definition));
            continue;
        }
        const auto surfaceBlock = properties.find("SurfaceBlock");
        if (surfaceBlock != properties.end()) {
            Format2SurfaceBlockDefinition definition;
            definition.name = surfaceBlock->second;
            definition.requiredProperties = SplitFormat2SchemaList(properties.at("Required"));
            definition.optionalProperties = SplitFormat2SchemaList(properties.at("Optional"));
            definition.propertyRules = ParseFormat2PropertyRules(properties.at("Rules"));
            catalog.surfaceBlocks.push_back(std::move(definition));
            continue;
        }
        const auto surfaceLine = properties.find("SurfaceLine");
        if (surfaceLine != properties.end()) {
            Format2SurfaceLineDefinition definition;
            definition.name = surfaceLine->second;
            definition.parentBlock = properties.at("ParentBlock");
            const auto argument = properties.find("Argument");
            if (argument != properties.end() && argument->second != "None") definition.argumentRule = argument->second;
            definition.requiredProperties = SplitFormat2SchemaList(properties.at("Required"));
            definition.optionalProperties = SplitFormat2SchemaList(properties.at("Optional"));
            definition.propertyRules = ParseFormat2PropertyRules(properties.at("Rules"));
            const std::string& minimumCount = properties.at("MinimumCount");
            const std::from_chars_result parsed = std::from_chars(minimumCount.data(), minimumCount.data() + minimumCount.size(), definition.minimumCount);
            if (parsed.ec != std::errc() || parsed.ptr != minimumCount.data() + minimumCount.size()) definition.minimumCount = 0;
            catalog.surfaceLines.push_back(std::move(definition));
            continue;
        }
        const auto representation = properties.find("Representation");
        if (representation == properties.end()) continue;
        Format2RepresentationDefinition definition;
        definition.direction = ParseFormat2SchemaDirection(properties.at("Direction"));
        definition.primitive = representation->second;
        definition.protocol = ParseFormat2SchemaProtocol(properties.at("Protocol"));
        definition.encodingName = properties.at("Encoding");
        definition.encoding = ParseFormat2SchemaEncoding(definition.encodingName);
        definition.requiredProperties = SplitFormat2SchemaList(properties.at("Required"));
        definition.optionalProperties = SplitFormat2SchemaList(properties.at("Optional"));
        definition.nestedBlocks = SplitFormat2SchemaList(properties.at("Nested"));
        definition.propertyRules = ParseFormat2PropertyRules(properties.at("Rules"));
        const auto constraints = properties.find("Constraints");
        if (constraints != properties.end()) definition.constraints = SplitFormat2SchemaList(constraints->second);
        catalog.representations.push_back(std::move(definition));
    }
    return catalog;
}

static const Format2PrimitiveCatalogData& GetFormat2PrimitiveCatalog() {
    static const Format2PrimitiveCatalogData catalog = ParseFormat2PrimitiveCatalog();
    return catalog;
}

std::vector<const Format2CapabilityCondition*> FindFormat2CapabilityConditions(Format2PrimitiveDirection direction, const std::string& primitive) {
    std::vector<const Format2CapabilityCondition*> conditions;
    for (const Format2CapabilityCondition& condition : GetFormat2PrimitiveCatalog().capabilityConditions) {
        if (condition.direction == direction && condition.primitive == primitive) conditions.push_back(&condition);
    }
    return conditions;
}

const Format2NestedBlockDefinition* FindFormat2NestedBlockDefinition(const std::string& name, Format2PrimitiveDirection parentDirection, const std::string& parentPrimitive, Format2SurfaceProtocol protocol) {
    for (const Format2NestedBlockDefinition& definition : GetFormat2PrimitiveCatalog().nestedBlocks) {
        if (definition.name == name && definition.parentDirection == parentDirection && definition.parentPrimitive == parentPrimitive && definition.protocol == protocol) return &definition;
    }
    return nullptr;
}

const Format2ProfileDefinition* FindFormat2ProfileDefinition(const std::string& name) {
    for (const Format2ProfileDefinition& definition : GetFormat2PrimitiveCatalog().profiles) {
        if (definition.name == name) return &definition;
    }
    return nullptr;
}

const Format2ProfileLineDefinition* FindFormat2ProfileLineDefinition(const std::string& parentProfile, const std::string& name) {
    for (const Format2ProfileLineDefinition& definition : GetFormat2PrimitiveCatalog().profileLines) {
        if (definition.parentProfile == parentProfile && definition.name == name) return &definition;
    }
    return nullptr;
}

std::vector<const Format2ProfileLineDefinition*> FindFormat2ProfileLineDefinitions(const std::string& parentProfile) {
    std::vector<const Format2ProfileLineDefinition*> definitions;
    for (const Format2ProfileLineDefinition& definition : GetFormat2PrimitiveCatalog().profileLines) {
        if (definition.parentProfile == parentProfile) definitions.push_back(&definition);
    }
    return definitions;
}

const Format2SurfaceBlockDefinition* FindFormat2SurfaceBlockDefinition(const std::string& name) {
    for (const Format2SurfaceBlockDefinition& definition : GetFormat2PrimitiveCatalog().surfaceBlocks) {
        if (definition.name == name) return &definition;
    }
    return nullptr;
}

const Format2SurfaceLineDefinition* FindFormat2SurfaceLineDefinition(const std::string& parentBlock, const std::string& name) {
    for (const Format2SurfaceLineDefinition& definition : GetFormat2PrimitiveCatalog().surfaceLines) {
        if (definition.parentBlock == parentBlock && definition.name == name) return &definition;
    }
    return nullptr;
}

std::vector<const Format2SurfaceLineDefinition*> FindFormat2SurfaceLineDefinitions(const std::string& parentBlock) {
    std::vector<const Format2SurfaceLineDefinition*> definitions;
    for (const Format2SurfaceLineDefinition& definition : GetFormat2PrimitiveCatalog().surfaceLines) {
        if (definition.parentBlock == parentBlock) definitions.push_back(&definition);
    }
    return definitions;
}

const Format2PrimitiveDefinition* FindFormat2PrimitiveDefinition(Format2PrimitiveDirection direction, const std::string& name) {
    for (const Format2PrimitiveDefinition& definition : GetFormat2PrimitiveCatalog().primitives) {
        if (definition.direction == direction && definition.name == name) return &definition;
    }
    return nullptr;
}

const Format2RepresentationDefinition* FindFormat2RepresentationDefinition(Format2PrimitiveDirection direction, const std::string& primitive, Format2SurfaceProtocol protocol, const std::string& encoding) {
    for (const Format2RepresentationDefinition& definition : GetFormat2PrimitiveCatalog().representations) {
        if (definition.direction == direction && definition.primitive == primitive && definition.protocol == protocol && definition.encodingName == encoding) return &definition;
    }
    return nullptr;
}

std::vector<const Format2RepresentationDefinition*> FindFormat2RepresentationDefinitions(Format2PrimitiveDirection direction, const std::string& primitive, Format2SurfaceProtocol protocol) {
    std::vector<const Format2RepresentationDefinition*> definitions;
    for (const Format2RepresentationDefinition& definition : GetFormat2PrimitiveCatalog().representations) {
        if (definition.direction == direction && definition.primitive == primitive && definition.protocol == protocol) definitions.push_back(&definition);
    }
    return definitions;
}

bool Format2RepresentationAllowsProperty(const Format2RepresentationDefinition& definition, const std::string& property) {
    if (property == "Encoding") return true;
    for (const std::string& required : definition.requiredProperties) {
        if (required == property) return true;
    }
    for (const std::string& optional : definition.optionalProperties) {
        if (optional == property) return true;
    }
    return false;
}

bool Format2RepresentationAllowsNestedBlock(const Format2RepresentationDefinition& definition, const std::string& block) {
    for (const std::string& allowed : definition.nestedBlocks) {
        if (allowed == block) return true;
    }
    return false;
}

const Format2PropertyRule* FindFormat2PropertyRule(const Format2RepresentationDefinition& definition, const std::string& property) {
    for (const Format2PropertyRule& rule : definition.propertyRules) {
        if (rule.property == property) return &rule;
    }
    return nullptr;
}

const Format2PropertyRule* FindFormat2PropertyRule(const Format2NestedBlockDefinition& definition, const std::string& property) {
    for (const Format2PropertyRule& rule : definition.propertyRules) {
        if (rule.property == property) return &rule;
    }
    return nullptr;
}

const Format2PropertyRule* FindFormat2PropertyRule(const Format2ProfileDefinition& definition, const std::string& property) {
    for (const Format2PropertyRule& rule : definition.propertyRules) {
        if (rule.property == property) return &rule;
    }
    return nullptr;
}

const Format2PropertyRule* FindFormat2PropertyRule(const Format2ProfileLineDefinition& definition, const std::string& property) {
    for (const Format2PropertyRule& rule : definition.propertyRules) {
        if (rule.property == property) return &rule;
    }
    return nullptr;
}

const Format2PropertyRule* FindFormat2PropertyRule(const Format2SurfaceBlockDefinition& definition, const std::string& property) {
    for (const Format2PropertyRule& rule : definition.propertyRules) {
        if (rule.property == property) return &rule;
    }
    return nullptr;
}

const Format2PropertyRule* FindFormat2PropertyRule(const Format2SurfaceLineDefinition& definition, const std::string& property) {
    for (const Format2PropertyRule& rule : definition.propertyRules) {
        if (rule.property == property) return &rule;
    }
    return nullptr;
}
