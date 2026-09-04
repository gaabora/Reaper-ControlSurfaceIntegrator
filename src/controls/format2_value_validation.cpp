#include "format2_value_validation.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <set>
#include <system_error>

static bool ParseFormat2IntegerText(const std::string& text, int& parsedValue) {
    if (text.empty()) return false;
    int base = 10;
    const char* begin = text.data();
    const char* end = begin + text.size();
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        begin += 2;
    }
    const std::from_chars_result result = std::from_chars(begin, end, parsedValue, base);
    return result.ec == std::errc() && result.ptr == end;
}

static bool ParseFormat2IntegerValue(const Format2ValueSyntax& value, int& parsedValue) {
    return !value.list && ParseFormat2IntegerScalar(value.scalar, parsedValue);
}

static bool ParseFormat2FiniteText(const std::string& text, double& parsedValue) {
    if (text.empty()) return false;
    char* end = nullptr;
    parsedValue = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(parsedValue);
}

static bool ParseFormat2FiniteValue(const Format2ValueSyntax& value, double& parsedValue) {
    return !value.list && ParseFormat2FiniteScalar(value.scalar, parsedValue);
}

bool ParseFormat2IntegerScalar(const Format2ScalarSyntax& scalar, int& parsedValue) {
    return !scalar.quoted && ParseFormat2IntegerText(scalar.text, parsedValue);
}

bool ParseFormat2FiniteScalar(const Format2ScalarSyntax& scalar, double& parsedValue) {
    return !scalar.quoted && ParseFormat2FiniteText(scalar.text, parsedValue);
}

bool ParseFormat2ColorScalar(const Format2ScalarSyntax& scalar, std::uint32_t& parsedValue) {
    if (scalar.quoted || scalar.text.size() != 7 || scalar.text[0] != '#') return false;
    const char* begin = scalar.text.data() + 1;
    const char* end = scalar.text.data() + scalar.text.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsedValue, 16);
    return result.ec == std::errc() && result.ptr == end;
}

static bool ValidateFormat2MidiMessage(const Format2ValueSyntax& value, std::size_t minimumSize, std::size_t maximumSize) {
    if (!value.list || value.items.size() < minimumSize || value.items.size() > maximumSize) return false;
    for (std::size_t itemIdx = 0; itemIdx < value.items.size(); ++itemIdx) {
        int byte = 0;
        if (value.items[itemIdx].quoted || !ParseFormat2IntegerText(value.items[itemIdx].text, byte)) return false;
        if (itemIdx == 0) {
            if (byte < 0x80 || byte > 0xEF) return false;
        } else if (byte < 0 || byte > 0x7F) {
            return false;
        }
    }
    return true;
}

static bool ValidateFormat2MidiInitializationMessage(const Format2ValueSyntax& value) {
    if (!value.list || value.items.empty()) return false;
    std::vector<int> bytes;
    for (const Format2ScalarSyntax& item : value.items) {
        int byte = 0;
        if (!ParseFormat2IntegerScalar(item, byte) || byte < 0 || byte > 0xFF) return false;
        bytes.push_back(byte);
    }
    if (bytes[0] == 0xF0) {
        if (bytes.size() < 2 || bytes.back() != 0xF7) return false;
        for (std::size_t byteIdx = 1; byteIdx + 1 < bytes.size(); ++byteIdx) if (bytes[byteIdx] > 0x7F) return false;
        return true;
    }
    if (bytes[0] < 0x80) return false;
    std::size_t expectedSize = 0;
    if (bytes[0] <= 0xEF) expectedSize = (bytes[0] & 0xF0) == 0xC0 || (bytes[0] & 0xF0) == 0xD0 ? 2 : 3;
    else if (bytes[0] == 0xF1 || bytes[0] == 0xF3) expectedSize = 2;
    else if (bytes[0] == 0xF2) expectedSize = 3;
    else if (bytes[0] == 0xF6 || bytes[0] == 0xF8 || bytes[0] == 0xFA || bytes[0] == 0xFB || bytes[0] == 0xFC || bytes[0] == 0xFE || bytes[0] == 0xFF) expectedSize = 1;
    else return false;
    if (bytes.size() != expectedSize) return false;
    for (std::size_t byteIdx = 1; byteIdx < bytes.size(); ++byteIdx) if (bytes[byteIdx] > 0x7F) return false;
    return true;
}

static bool ValidateFormat2TextPayload(const Format2ValueSyntax& value) {
    if (!value.list || value.items.empty() || value.items.back().quoted || value.items.back().text != "Text") return false;
    const std::set<std::string> fields = { "TopMargin7", "BottomMargin7", "Font7", "TextPresentationCode", "BackgroundRed7", "BackgroundGreen7", "BackgroundBlue7", "TextRed7", "TextGreen7", "TextBlue7" };
    for (std::size_t itemIdx = 0; itemIdx + 1 < value.items.size(); ++itemIdx) {
        int byte = 0;
        if (ParseFormat2IntegerScalar(value.items[itemIdx], byte)) {
            if (byte < 0 || byte > 0x7F) return false;
        } else if (value.items[itemIdx].quoted || fields.find(value.items[itemIdx].text) == fields.end()) return false;
    }
    return true;
}

static bool ValidateFormat2StatePayload(const Format2ValueSyntax& value) {
    if (!value.list || value.items.size() < 2) return false;
    std::vector<std::string> fields;
    bool reachedField = false;
    size_t constantCount = 0;
    for (const Format2ScalarSyntax& item : value.items) {
        int byte = 0;
        if (ParseFormat2IntegerScalar(item, byte)) {
            if (reachedField || byte < 0 || byte > 0x7F) return false;
            constantCount++;
            continue;
        }
        reachedField = true;
        if (item.quoted) return false;
        fields.push_back(item.text);
    }
    return constantCount > 0 && (fields == std::vector<std::string>{ "State7" } || fields == std::vector<std::string>{ "Red7", "Green7", "Blue7" });
}

static bool ValidateFormat2ProfilePayload(const Format2ValueSyntax& value, const std::vector<std::string>& expectedFields) {
    if (!value.list || value.items.size() <= expectedFields.size()) return false;
    const std::size_t constantCount = value.items.size() - expectedFields.size();
    for (std::size_t itemIdx = 0; itemIdx < constantCount; ++itemIdx) {
        int byte = 0;
        if (!ParseFormat2IntegerScalar(value.items[itemIdx], byte) || byte < 0 || byte > 0x7F) return false;
    }
    for (std::size_t fieldIdx = 0; fieldIdx < expectedFields.size(); ++fieldIdx) {
        const Format2ScalarSyntax& item = value.items[constantCount + fieldIdx];
        if (item.quoted || item.text != expectedFields[fieldIdx]) return false;
    }
    return true;
}

static bool ValidateFormat2RingConfigurePayload(const Format2ValueSyntax& value) {
    if (!value.list || value.items.empty()) return false;
    const std::vector<std::string> fields = { "SegmentMasks", "SegmentRed7", "SegmentGreen7", "SegmentBlue7" };
    size_t fieldIdx = 0;
    for (const Format2ScalarSyntax& item : value.items) {
        int byte = 0;
        if (ParseFormat2IntegerScalar(item, byte)) {
            if (byte < 0 || byte > 0x7F) return false;
        } else if (item.quoted || fieldIdx >= fields.size() || item.text != fields[fieldIdx++]) return false;
    }
    return fieldIdx == fields.size();
}

static bool ValidateFormat2Enum(const Format2ValueSyntax& value, const std::string& values) {
    if (value.list || value.scalar.quoted) return false;
    std::size_t start = 0;
    while (start <= values.size()) {
        const std::size_t separator = values.find('|', start);
        if (value.scalar.text == values.substr(start, separator == std::string::npos ? std::string::npos : separator - start)) return true;
        if (separator == std::string::npos) break;
        start = separator + 1;
    }
    return false;
}

static bool ParseFormat2RuleArguments(const std::string& rule, const std::string& prefix, std::string& arguments) {
    if (rule.size() <= prefix.size() + 2 || rule.compare(0, prefix.size(), prefix) != 0 || rule[prefix.size()] != '(' || rule.back() != ')') return false;
    arguments = rule.substr(prefix.size() + 1, rule.size() - prefix.size() - 2);
    return true;
}

std::string ValidateFormat2ValueRule(const Format2PropertySyntax& property, const std::string& rule) {
    if (rule == "Identifier") {
        if (!property.value.list && !property.value.scalar.quoted && IsValidFormat2Identifier(property.value.scalar.text)) return {};
        return "one unquoted identifier";
    }
    if (rule == "Boolean") {
        if (!property.value.list && !property.value.scalar.quoted && (property.value.scalar.text == "true" || property.value.scalar.text == "false")) return {};
        return "true or false";
    }
    if (rule == "Finite" || rule == "NonZeroFinite") {
        double value = 0.0;
        if (ParseFormat2FiniteValue(property.value, value) && (rule != "NonZeroFinite" || value != 0.0)) return {};
        return rule == "NonZeroFinite" ? "one non-zero finite number" : "one finite number";
    }
    if (rule == "SignedInteger") {
        int value = 0;
        if (ParseFormat2IntegerValue(property.value, value)) return {};
        return "one signed integer";
    }
    if (rule == "PositiveInteger") {
        int value = 0;
        if (ParseFormat2IntegerValue(property.value, value) && value > 0) return {};
        return "one positive integer";
    }
    if (rule == "NonNegativeInteger") {
        int value = 0;
        if (ParseFormat2IntegerValue(property.value, value) && value >= 0) return {};
        return "one non-negative integer";
    }
    if (rule == "MIDIDataByte") {
        int value = 0;
        if (ParseFormat2IntegerValue(property.value, value) && value >= 0 && value <= 0x7F) return {};
        return "one MIDI data byte from 0 through 0x7F";
    }
    if (rule == "MIDIStatus") {
        int value = 0;
        if (ParseFormat2IntegerValue(property.value, value) && value >= 0x80 && value <= 0xEF) return {};
        return "one MIDI status byte from 0x80 through 0xEF";
    }
    if (rule == "MIDIPitchBendStatus") {
        int value = 0;
        if (ParseFormat2IntegerValue(property.value, value) && value >= 0xE0 && value <= 0xEF) return {};
        return "one pitch-bend status byte from 0xE0 through 0xEF";
    }
    if (rule == "MIDIMessage2") return ValidateFormat2MidiMessage(property.value, 2, 2) ? std::string{} : "a two-byte MIDI message prefix";
    if (rule == "MIDIMessage3") return ValidateFormat2MidiMessage(property.value, 3, 3) ? std::string{} : "a complete three-byte MIDI message";
    if (rule == "MIDIMessage1Or2") return ValidateFormat2MidiMessage(property.value, 1, 2) ? std::string{} : "a one- or two-byte MIDI message prefix";
    if (rule == "MIDIInitializationMessage") return ValidateFormat2MidiInitializationMessage(property.value) ? std::string{} : "one complete MIDI message";
    if (rule == "StatePayload") return ValidateFormat2StatePayload(property.value) ? std::string{} : "MIDI data bytes followed by State7 or Red7, Green7, and Blue7";
    if (rule == "TextPayload") return ValidateFormat2TextPayload(property.value) ? std::string{} : "MIDI data bytes and supported text fields followed by Text";
    if (rule == "RingConfigurePayload") return ValidateFormat2RingConfigurePayload(property.value) ? std::string{} : "MIDI data bytes plus one SegmentMasks and one segment RGB field set";
    if (rule == "ColorPayload") return ValidateFormat2ProfilePayload(property.value, { "Red7", "Green7", "Blue7" }) ? std::string{} : "MIDI data bytes followed by Red7, Green7, and Blue7";
    if (rule == "RingPayload") return ValidateFormat2ProfilePayload(property.value, { "RingValue7", "RingStyleCode7" }) ? std::string{} : "MIDI data bytes followed by RingValue7 and RingStyleCode7";
    if (rule == "BarPayload") return ValidateFormat2ProfilePayload(property.value, { "BarValue7", "BarStyleCode7" }) ? std::string{} : "MIDI data bytes followed by BarValue7 and BarStyleCode7";
    if (rule == "MeterPayload") return ValidateFormat2ProfilePayload(property.value, { "MeterValue7" }) ? std::string{} : "MIDI data bytes followed by MeterValue7";
    if (rule == "OSCAddress") {
        if (!property.value.list && !property.value.scalar.text.empty() && property.value.scalar.text[0] == '/') return {};
        return "one OSC address that starts with /";
    }
    if (rule == "RGBColor") {
        std::uint32_t value = 0;
        if (!property.value.list && ParseFormat2ColorScalar(property.value.scalar, value)) return {};
        return "one #RRGGBB color";
    }
    if (rule == "QuotedString") {
        if (!property.value.list && property.value.scalar.quoted) return {};
        return "one quoted string";
    }
    if (rule == "Hue") {
        double value = 0.0;
        if (ParseFormat2FiniteValue(property.value, value) && value >= 0.0 && value < 360.0) return {};
        return "one hue from 0 inclusive through 360 exclusive";
    }
    if (rule == "NonEmptyList") return property.value.list && !property.value.items.empty() ? std::string{} : "one non-empty list";
    if (rule == "IdentifierList") {
        if (!property.value.list || property.value.items.empty()) return "one non-empty list of identifiers";
        for (const Format2ScalarSyntax& item : property.value.items) {
            if (item.quoted || !IsValidFormat2Identifier(item.text)) return "one non-empty list of identifiers";
        }
        return {};
    }
    if (rule == "TrackColorPayload") {
        if (!property.value.list || property.value.items.size() < 2 || property.value.items.back().quoted || property.value.items.back().text != "SlotColors") return "constant bytes followed by SlotColors";
        for (std::size_t itemIdx = 0; itemIdx + 1 < property.value.items.size(); ++itemIdx) {
            int value = 0;
            if (!ParseFormat2IntegerScalar(property.value.items[itemIdx], value) || value < 0 || value > 0xFF) return "constant bytes followed by SlotColors";
        }
        return {};
    }
    if (rule == "ValuePayload") {
        if (!property.value.list || property.value.items.size() < 2 || property.value.items.back().quoted || property.value.items.back().text != "Value7") return "constant bytes followed by Value7";
        for (std::size_t itemIdx = 0; itemIdx + 1 < property.value.items.size(); ++itemIdx) {
            int value = 0;
            if (!ParseFormat2IntegerScalar(property.value.items[itemIdx], value) || value < 0 || value > 0xFF) return "constant bytes followed by Value7";
        }
        return {};
    }
    if (rule == "PositiveFinite") {
        double value = 0.0;
        if (ParseFormat2FiniteValue(property.value, value) && value > 0.0) return {};
        return "one positive finite number";
    }
    if (rule == "PositiveFiniteList") {
        if (!property.value.list || property.value.items.empty()) return "one non-empty list of positive finite numbers";
        for (const Format2ScalarSyntax& item : property.value.items) {
            double value = 0.0;
            if (!ParseFormat2FiniteScalar(item, value) || value <= 0.0) return "one non-empty list of positive finite numbers";
        }
        return {};
    }
    if (rule == "FiniteList") {
        if (!property.value.list || property.value.items.empty()) return "one non-empty list of finite numbers";
        for (const Format2ScalarSyntax& item : property.value.items) {
            double value = 0.0;
            if (!ParseFormat2FiniteScalar(item, value)) return "one non-empty list of finite numbers";
        }
        return {};
    }
    if (rule == "FinitePair") {
        if (!property.value.list || property.value.items.size() != 2) return "a list of two finite numbers";
        for (const Format2ScalarSyntax& item : property.value.items) {
            double value = 0.0;
            if (!ParseFormat2FiniteScalar(item, value)) return "a list of two finite numbers";
        }
        return {};
    }
    if (rule == "NonNegativeIntegerPair") {
        if (!property.value.list || property.value.items.size() != 2) return "a list of two non-negative integers";
        for (const Format2ScalarSyntax& item : property.value.items) {
            int value = 0;
            if (!ParseFormat2IntegerScalar(item, value) || value < 0) return "a list of two non-negative integers";
        }
        return {};
    }
    if (rule == "MIDIDataByteList") {
        if (!property.value.list || property.value.items.empty()) return "one non-empty list of unique MIDI data bytes";
        std::vector<int> values;
        for (const Format2ScalarSyntax& item : property.value.items) {
            int value = 0;
            if (!ParseFormat2IntegerScalar(item, value) || value < 0 || value > 0x7F) return "one non-empty list of unique MIDI data bytes";
            for (int existing : values) {
                if (existing == value) return "one non-empty list of unique MIDI data bytes";
            }
            values.push_back(value);
        }
        return {};
    }

    std::string arguments;
    if (ParseFormat2RuleArguments(rule, "Enum", arguments)) return ValidateFormat2Enum(property.value, arguments) ? std::string{} : "one of " + arguments;
    if (ParseFormat2RuleArguments(rule, "Integer", arguments)) {
        const std::size_t separator = arguments.find('|');
        int minimum = 0;
        int maximum = 0;
        int value = 0;
        if (separator != std::string::npos && ParseFormat2IntegerText(arguments.substr(0, separator), minimum) && ParseFormat2IntegerText(arguments.substr(separator + 1), maximum) && ParseFormat2IntegerValue(property.value, value) && value >= minimum && value <= maximum) return {};
        return "one integer from " + arguments;
    }
    if (ParseFormat2RuleArguments(rule, "Finite", arguments)) {
        const std::size_t separator = arguments.find('|');
        double minimum = 0.0;
        double maximum = 0.0;
        double value = 0.0;
        if (separator != std::string::npos && ParseFormat2FiniteText(arguments.substr(0, separator), minimum) && ParseFormat2FiniteText(arguments.substr(separator + 1), maximum) && ParseFormat2FiniteValue(property.value, value) && value >= minimum && value <= maximum) return {};
        return "one finite number from " + arguments;
    }
    return "a value supported by schema rule " + rule;
}
