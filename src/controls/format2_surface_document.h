#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "format2_document.h"
#include "format2_primitive_catalog.h"

struct Format2SurfacePrimitive {
    Format2PrimitiveDirection direction = Format2PrimitiveDirection::Input;
    std::string type;
    Format2SourceLocation location;
    std::optional<Format2Encoding> encoding;
    std::vector<Format2Capability> capabilities;
    std::vector<Format2PropertySyntax> properties;
    std::vector<Format2SyntaxNode> nestedBlocks;
};

struct Format2SurfaceWidget {
    std::string id;
    Format2SourceLocation location;
    std::optional<std::string> alias;
    std::optional<int> channel;
    std::vector<Format2Capability> capabilities;
    std::vector<Format2SurfacePrimitive> primitives;
};

struct Format2SurfaceNamedBlock {
    std::string type;
    std::string id;
    Format2SourceLocation location;
    Format2SyntaxNode syntax;
};

struct Format2EncoderProfile {
    std::string id;
    Format2SourceLocation location;
    std::vector<int> increase;
    std::vector<int> decrease;
    std::optional<double> delta;
    std::vector<double> accelerationDeltas;
};

enum class Format2ValueUnit {
    Normalized,
    Integer,
    Decibels,
};

enum class Format2ValueProfileDirection {
    Decode,
    Encode,
    Both,
};

enum class Format2Interpolation {
    Linear,
    Step,
};

struct Format2ValueProfilePoint {
    double input = 0.0;
    double output = 0.0;
    Format2SourceLocation location;
};

struct Format2ValueProfile {
    std::string id;
    Format2SourceLocation location;
    Format2ValueUnit inputUnit = Format2ValueUnit::Normalized;
    Format2ValueUnit outputUnit = Format2ValueUnit::Normalized;
    Format2ValueProfileDirection direction = Format2ValueProfileDirection::Both;
    Format2Interpolation interpolation = Format2Interpolation::Linear;
    std::vector<Format2ValueProfilePoint> points;
};

enum class Format2ColorMatch {
    Nearest,
    Exact,
    HueRanges,
};

struct Format2ColorProfileEntry {
    std::uint32_t color = 0;
    int value = 0;
    Format2SourceLocation location;
};

struct Format2HueRange {
    double minimum = 0.0;
    double maximum = 0.0;
    int value = 0;
    Format2SourceLocation location;
};

struct Format2ColorProfile {
    std::string id;
    Format2SourceLocation location;
    Format2ColorMatch match = Format2ColorMatch::Nearest;
    int defaultValue = 0;
    std::optional<double> minimumBrightness;
    std::optional<double> maximumNeutralSaturation;
    std::vector<Format2ColorProfileEntry> entries;
    std::vector<Format2HueRange> hueRanges;
};

enum class Format2Quantize {
    Floor,
    Round,
};

enum class Format2RingStyle {
    Dot,
    Fill,
    BoostCut,
    Spread,
};

struct Format2RingStyleEntry {
    Format2RingStyle style = Format2RingStyle::Dot;
    int code = 0;
    int steps = 1;
    Format2SourceLocation location;
};

struct Format2RingProfile {
    std::string id;
    Format2SourceLocation location;
    std::optional<int> segments;
    std::uint32_t defaultColor = 0;
    Format2Quantize quantize = Format2Quantize::Floor;
    int valueOffset = 0;
    std::vector<Format2RingStyleEntry> styles;
};

enum class Format2BarStyle {
    Normal,
    Bipolar,
    Fill,
    Spread,
    Off,
};

struct Format2BarStyleEntry {
    Format2BarStyle style = Format2BarStyle::Off;
    int code = 0;
    Format2SourceLocation location;
};

struct Format2BarProfile {
    std::string id;
    Format2SourceLocation location;
    Format2BarStyle defaultStyle = Format2BarStyle::Off;
    std::vector<Format2BarStyleEntry> styles;
};

enum class Format2MeterMode {
    Linear,
    Steps,
};

enum class Format2MeterInputUnit {
    Normalized,
    Decibels,
};

struct Format2MeterStep {
    double minimum = 0.0;
    int output = 0;
    Format2SourceLocation location;
};

struct Format2MeterProfile {
    std::string id;
    Format2SourceLocation location;
    Format2MeterMode mode = Format2MeterMode::Linear;
    Format2MeterInputUnit inputUnit = Format2MeterInputUnit::Normalized;
    std::optional<std::array<double, 2>> inputRange;
    std::optional<std::array<int, 2>> outputRange;
    std::optional<Format2Quantize> quantize;
    std::optional<int> defaultValue;
    std::vector<Format2MeterStep> steps;
};

enum class Format2TextEncoding {
    Ascii7,
    Utf8,
};

enum class Format2TextPadding {
    None,
    Space,
};

enum class Format2TextAlignment {
    Left,
    Center,
    Right,
};

enum class Format2PresentationCombine {
    Add,
    BitOr,
};

struct Format2TextAlignmentEntry {
    Format2TextAlignment alignment = Format2TextAlignment::Left;
    int code = 0;
    Format2SourceLocation location;
};

struct Format2TextProfile {
    std::string id;
    Format2SourceLocation location;
    Format2TextEncoding encoding = Format2TextEncoding::Ascii7;
    std::optional<int> width;
    Format2TextPadding padding = Format2TextPadding::None;
    std::string clearText;
    bool silenceAsEmpty = false;
    std::optional<Format2TextAlignment> defaultAlignment;
    std::vector<Format2TextAlignmentEntry> alignments;
    std::optional<int> invertCode;
    Format2PresentationCombine presentationCombine = Format2PresentationCombine::BitOr;
};

enum class Format2MidiSysExTextPayloadField {
    Byte,
    TopMargin,
    BottomMargin,
    Font,
    TextPresentationCode,
    BackgroundRed,
    BackgroundGreen,
    BackgroundBlue,
    TextRed,
    TextGreen,
    TextBlue,
    Text,
};

struct Format2MidiSysExTextPayloadItem {
    Format2MidiSysExTextPayloadField field = Format2MidiSysExTextPayloadField::Byte;
    int byte = 0;
};

enum class Format2MidiSysExStatePayloadField {
    Byte,
    State,
    Red,
    Green,
    Blue,
};

struct Format2MidiSysExStatePayloadItem {
    Format2MidiSysExStatePayloadField field = Format2MidiSysExStatePayloadField::Byte;
    int byte = 0;
};

enum class Format2MidiSysExValuePayloadField {
    Byte,
    Value,
};

struct Format2MidiSysExValuePayloadItem {
    Format2MidiSysExValuePayloadField field = Format2MidiSysExValuePayloadField::Byte;
    int byte = 0;
};

enum class Format2MidiSysExProfilePayloadField {
    Byte,
    Red,
    Green,
    Blue,
    RingValue,
    RingStyleCode,
    BarValue,
    BarStyleCode,
    MeterValue,
};

struct Format2MidiSysExProfilePayloadItem {
    Format2MidiSysExProfilePayloadField field = Format2MidiSysExProfilePayloadField::Byte;
    int byte = 0;
};

enum class Format2MidiSysExRingConfigureField {
    Byte,
    SegmentMasks,
    SegmentRed,
    SegmentGreen,
    SegmentBlue,
};

struct Format2MidiSysExRingConfigureItem {
    Format2MidiSysExRingConfigureField field = Format2MidiSysExRingConfigureField::Byte;
    int byte = 0;
};

enum class Format2TrackColorCondition {
    Always,
    SourceTextPresent,
};

enum class Format2TrackColorEncoding {
    Palette,
    Rgb7,
};

struct Format2FeedbackGroupSlot {
    std::string source;
    std::vector<std::string> members;
    Format2SourceLocation location;
};

struct Format2FeedbackGroup {
    std::string id;
    Format2SourceLocation location;
    Format2TrackColorEncoding colorEncoding = Format2TrackColorEncoding::Palette;
    std::string colorProfile;
    std::uint32_t emptyColor = 0;
    Format2TrackColorCondition useTrackColorWhen = Format2TrackColorCondition::Always;
    double blueScaleAtGreenMinimum = 1.0;
    double blueScaleAtGreenMaximum = 1.0;
    std::vector<int> payloadPrefix;
    std::vector<Format2FeedbackGroupSlot> slots;
};

struct Format2ColorCalibration {
    Format2SourceLocation location;
    int inputMax = 255;
    std::optional<int> outputMax;
    int neutralTolerancePercent = 0;
    double redScale = 1.0;
    double greenScale = 1.0;
    double blueScale = 1.0;
    double neutralRedScale = 1.0;
    double neutralGreenScale = 1.0;
    double neutralBlueScale = 1.0;
    double neutralCurve = 1.0;
};

enum class Format2OskCellKind {
    Widget,
    Spacer,
};

struct Format2OskCell {
    Format2OskCellKind kind = Format2OskCellKind::Widget;
    Format2SourceLocation location;
    std::string widget;
    std::string shape;
    double width = 1.0;
    double height = 1.0;
    double top = 0.0;
    std::string group;
    std::string label;
    std::optional<std::uint32_t> color;
    std::string role;
    std::string pressTarget;
    std::string scrollTarget;
    std::string valueTarget;
    std::string touchTarget;
    std::string rotaryStyle;
};

struct Format2OskRow {
    Format2SourceLocation location;
    std::vector<Format2OskCell> cells;
};

struct Format2OskLayout {
    Format2SourceLocation location;
    std::vector<Format2OskRow> rows;
};

struct Format2MidiInitializationMessage {
    Format2SourceLocation location;
    std::vector<int> bytes;
};

struct Format2SurfaceInitialization {
    Format2SourceLocation location;
    std::vector<Format2MidiInitializationMessage> midiMessages;
};

struct Format2SurfaceDocument {
    std::vector<Format2SurfaceNamedBlock> profiles;
    std::vector<Format2EncoderProfile> encoderProfiles;
    std::vector<Format2ValueProfile> valueProfiles;
    std::vector<Format2ColorProfile> colorProfiles;
    std::vector<Format2RingProfile> ringProfiles;
    std::vector<Format2BarProfile> barProfiles;
    std::vector<Format2MeterProfile> meterProfiles;
    std::vector<Format2TextProfile> textProfiles;
    std::vector<Format2FeedbackGroup> feedbackGroups;
    std::vector<Format2SurfaceWidget> widgets;
    std::optional<Format2ColorCalibration> colorCalibration;
    std::optional<Format2SurfaceInitialization> initialization;
    std::optional<Format2OskLayout> oskLayout;
};

struct Format2SurfaceParseResult {
    Format2DocumentParseResult document;
    Format2SurfaceDocument surface;

    bool IsValid() const { return this->document.IsValid(); }
};

Format2SurfaceParseResult ParseFormat2SurfaceSource(const std::string& source, const std::string& sourcePath);
