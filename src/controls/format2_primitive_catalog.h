#pragma once

#include <string>
#include <vector>

#include "format2_document.h"

enum class Format2PrimitiveDirection {
    Input,
    Feedback,
};

enum class Format2Capability {
    Press,
    Release,
    Absolute,
    Relative,
    Touch,
    Toggle,
    Value,
    Color,
    TrackColor,
    Ring,
    Bar,
    Meter,
    Text,
};

enum class Format2Encoding {
    MidiExact,
    MidiPrefix,
    Midi7,
    Midi14,
    MidiSplit,
    MidiRgb,
    MidiPalette,
    MidiSysEx,
    MidiCharacters,
    OscFloat,
    OscInt,
    OscString,
};

struct Format2PrimitiveDefinition {
    Format2PrimitiveDirection direction = Format2PrimitiveDirection::Input;
    std::string name;
    std::vector<Format2Capability> capabilities;
};

struct Format2PropertyRule {
    std::string property;
    std::string valueRule;
};

struct Format2RepresentationDefinition {
    Format2PrimitiveDirection direction = Format2PrimitiveDirection::Input;
    std::string primitive;
    Format2SurfaceProtocol protocol = Format2SurfaceProtocol::Midi;
    Format2Encoding encoding = Format2Encoding::MidiExact;
    std::string encodingName;
    std::vector<std::string> requiredProperties;
    std::vector<std::string> optionalProperties;
    std::vector<std::string> nestedBlocks;
    std::vector<Format2PropertyRule> propertyRules;
    std::vector<std::string> constraints;
};

struct Format2CapabilityCondition {
    Format2PrimitiveDirection direction = Format2PrimitiveDirection::Input;
    std::string primitive;
    Format2Capability capability = Format2Capability::Press;
    std::vector<std::string> allProperties;
    std::string equalProperty;
    std::string equalValue;
    std::string nestedBlock;
    std::string listProperty;
    std::vector<std::string> anyListItems;
};

struct Format2NestedBlockDefinition {
    std::string name;
    Format2PrimitiveDirection parentDirection = Format2PrimitiveDirection::Input;
    std::string parentPrimitive;
    Format2SurfaceProtocol protocol = Format2SurfaceProtocol::Midi;
    std::vector<std::string> requiredProperties;
    std::vector<std::string> optionalProperties;
    std::vector<Format2PropertyRule> propertyRules;
};

struct Format2ProfileDefinition {
    std::string name;
    std::vector<std::string> requiredProperties;
    std::vector<std::string> optionalProperties;
    std::vector<Format2PropertyRule> propertyRules;
};

struct Format2ProfileLineDefinition {
    std::string name;
    std::string parentProfile;
    std::string argumentRule;
    std::vector<std::string> requiredProperties;
    std::vector<std::string> optionalProperties;
    std::vector<Format2PropertyRule> propertyRules;
    int minimumCount = 0;
};

struct Format2SurfaceBlockDefinition {
    std::string name;
    std::vector<std::string> requiredProperties;
    std::vector<std::string> optionalProperties;
    std::vector<Format2PropertyRule> propertyRules;
};

struct Format2SurfaceLineDefinition {
    std::string name;
    std::string parentBlock;
    std::string argumentRule;
    std::vector<std::string> requiredProperties;
    std::vector<std::string> optionalProperties;
    std::vector<Format2PropertyRule> propertyRules;
    int minimumCount = 0;
};

const Format2PrimitiveDefinition* FindFormat2PrimitiveDefinition(Format2PrimitiveDirection direction, const std::string& name);
const Format2RepresentationDefinition* FindFormat2RepresentationDefinition(Format2PrimitiveDirection direction, const std::string& primitive, Format2SurfaceProtocol protocol, const std::string& encoding);
std::vector<const Format2RepresentationDefinition*> FindFormat2RepresentationDefinitions(Format2PrimitiveDirection direction, const std::string& primitive, Format2SurfaceProtocol protocol);
std::vector<const Format2CapabilityCondition*> FindFormat2CapabilityConditions(Format2PrimitiveDirection direction, const std::string& primitive);
const Format2NestedBlockDefinition* FindFormat2NestedBlockDefinition(const std::string& name, Format2PrimitiveDirection parentDirection, const std::string& parentPrimitive, Format2SurfaceProtocol protocol);
const Format2ProfileDefinition* FindFormat2ProfileDefinition(const std::string& name);
const Format2ProfileLineDefinition* FindFormat2ProfileLineDefinition(const std::string& parentProfile, const std::string& name);
std::vector<const Format2ProfileLineDefinition*> FindFormat2ProfileLineDefinitions(const std::string& parentProfile);
const Format2SurfaceBlockDefinition* FindFormat2SurfaceBlockDefinition(const std::string& name);
const Format2SurfaceLineDefinition* FindFormat2SurfaceLineDefinition(const std::string& parentBlock, const std::string& name);
std::vector<const Format2SurfaceLineDefinition*> FindFormat2SurfaceLineDefinitions(const std::string& parentBlock);
bool Format2RepresentationAllowsProperty(const Format2RepresentationDefinition& definition, const std::string& property);
bool Format2RepresentationAllowsNestedBlock(const Format2RepresentationDefinition& definition, const std::string& block);
const Format2PropertyRule* FindFormat2PropertyRule(const Format2RepresentationDefinition& definition, const std::string& property);
const Format2PropertyRule* FindFormat2PropertyRule(const Format2NestedBlockDefinition& definition, const std::string& property);
const Format2PropertyRule* FindFormat2PropertyRule(const Format2ProfileDefinition& definition, const std::string& property);
const Format2PropertyRule* FindFormat2PropertyRule(const Format2ProfileLineDefinition& definition, const std::string& property);
const Format2PropertyRule* FindFormat2PropertyRule(const Format2SurfaceBlockDefinition& definition, const std::string& property);
const Format2PropertyRule* FindFormat2PropertyRule(const Format2SurfaceLineDefinition& definition, const std::string& property);
