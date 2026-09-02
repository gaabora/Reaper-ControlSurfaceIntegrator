#include "../integrator.h"
#include "../format2_surface_document.h"
#include "../format2_value_validation.h"
#include "format2_midi_runtime.h"
#include "midi_surface.h"
#include "midi_widgets.h"
#include "widget_factory.h"

const Format2PropertySyntax* Format2MidiRuntimeLoader::FindProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == name) return &property;
    return nullptr;
}

bool Format2MidiRuntimeLoader::ReadByte(const Format2ScalarSyntax& scalar, int& value) {
    char* end = nullptr;
    const long parsed = strtol(scalar.text.c_str(), &end, 0);
    if (!end || *end != '\0' || parsed < 0 || parsed > 255) return false;
    value = (int) parsed;
    return true;
}

bool Format2MidiRuntimeLoader::ReadBytes(const Format2PropertySyntax* property, vector<int>& values) {
    if (!property || !property->value.list) return false;
    values.clear();
    for (const Format2ScalarSyntax& item : property->value.items) {
        int value = 0;
        if (!ReadByte(item, value)) return false;
        values.push_back(value);
    }
    return true;
}

bool Format2MidiRuntimeLoader::ReadStatePayload(const Format2PropertySyntax* property, vector<Format2MidiSysExStatePayloadItem>& payload) {
    if (!property || !property->value.list || property->value.items.empty()) return false;
    payload.clear();
    for (const Format2ScalarSyntax& item : property->value.items) {
        int value = 0;
        if (ReadByte(item, value) && value <= 0x7F) payload.push_back({ Format2MidiSysExStatePayloadField::Byte, value });
        else if (item.text == "State7") payload.push_back({ Format2MidiSysExStatePayloadField::State, 0 });
        else if (item.text == "Red7") payload.push_back({ Format2MidiSysExStatePayloadField::Red, 0 });
        else if (item.text == "Green7") payload.push_back({ Format2MidiSysExStatePayloadField::Green, 0 });
        else if (item.text == "Blue7") payload.push_back({ Format2MidiSysExStatePayloadField::Blue, 0 });
        else return false;
    }
    return true;
}

bool Format2MidiRuntimeLoader::ReadTextPayload(const Format2PropertySyntax* property, vector<Format2MidiSysExTextPayloadItem>& payload) {
    if (!property || !property->value.list || property->value.items.empty()) return false;
    payload.clear();
    for (const Format2ScalarSyntax& item : property->value.items) {
        int value = 0;
        if (ReadByte(item, value) && value <= 0x7F) payload.push_back({ Format2MidiSysExTextPayloadField::Byte, value });
        else if (item.text == "TopMargin7") payload.push_back({ Format2MidiSysExTextPayloadField::TopMargin, 0 });
        else if (item.text == "BottomMargin7") payload.push_back({ Format2MidiSysExTextPayloadField::BottomMargin, 0 });
        else if (item.text == "Font7") payload.push_back({ Format2MidiSysExTextPayloadField::Font, 0 });
        else if (item.text == "TextPresentationCode") payload.push_back({ Format2MidiSysExTextPayloadField::TextPresentationCode, 0 });
        else if (item.text == "BackgroundRed7") payload.push_back({ Format2MidiSysExTextPayloadField::BackgroundRed, 0 });
        else if (item.text == "BackgroundGreen7") payload.push_back({ Format2MidiSysExTextPayloadField::BackgroundGreen, 0 });
        else if (item.text == "BackgroundBlue7") payload.push_back({ Format2MidiSysExTextPayloadField::BackgroundBlue, 0 });
        else if (item.text == "TextRed7") payload.push_back({ Format2MidiSysExTextPayloadField::TextRed, 0 });
        else if (item.text == "TextGreen7") payload.push_back({ Format2MidiSysExTextPayloadField::TextGreen, 0 });
        else if (item.text == "TextBlue7") payload.push_back({ Format2MidiSysExTextPayloadField::TextBlue, 0 });
        else if (item.text == "Text") payload.push_back({ Format2MidiSysExTextPayloadField::Text, 0 });
        else return false;
    }
    return !payload.empty() && payload.back().field == Format2MidiSysExTextPayloadField::Text;
}

bool Format2MidiRuntimeLoader::ReadRingConfigurePayload(const Format2PropertySyntax* property, vector<Format2MidiSysExRingConfigureItem>& payload) {
    if (!property || !property->value.list || property->value.items.empty()) return false;
    payload.clear();
    for (const Format2ScalarSyntax& item : property->value.items) {
        int value = 0;
        if (ReadByte(item, value) && value <= 0x7F) payload.push_back({ Format2MidiSysExRingConfigureField::Byte, value });
        else if (item.text == "SegmentMasks") payload.push_back({ Format2MidiSysExRingConfigureField::SegmentMasks, 0 });
        else if (item.text == "SegmentRed7") payload.push_back({ Format2MidiSysExRingConfigureField::SegmentRed, 0 });
        else if (item.text == "SegmentGreen7") payload.push_back({ Format2MidiSysExRingConfigureField::SegmentGreen, 0 });
        else if (item.text == "SegmentBlue7") payload.push_back({ Format2MidiSysExRingConfigureField::SegmentBlue, 0 });
        else return false;
    }
    return true;
}

vector<string> Format2MidiRuntimeLoader::MakeTokens(const string& type, const vector<int>& firstMessage, const vector<int>& secondMessage) {
    vector<string> tokens = { type };
    for (int value : firstMessage) tokens.push_back(to_string(value));
    for (int value : secondMessage) tokens.push_back(to_string(value));
    return tokens;
}

bool Format2MidiRuntimeLoader::IsSupported(const Format2SurfacePrimitive& primitive) {
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press" && primitive.encoding == Format2Encoding::MidiExact) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press" && primitive.encoding == Format2Encoding::MidiPrefix) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Touch" && primitive.encoding == Format2Encoding::MidiExact) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14 && !FindProperty(primitive, "ValueProfile")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi7 && !FindProperty(primitive, "ValueProfile")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder" && primitive.encoding == Format2Encoding::Midi7) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && primitive.encoding == Format2Encoding::MidiExact) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && primitive.encoding == Format2Encoding::MidiSysEx) {
        vector<Format2MidiSysExStatePayloadItem> payload;
        return ReadStatePayload(FindProperty(primitive, "Payload"), payload);
    }
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14 && !FindProperty(primitive, "ValueProfile") && !FindProperty(primitive, "InitialValue")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi7 && !FindProperty(primitive, "ValueProfile") && !FindProperty(primitive, "InitialValue")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Ring" && primitive.encoding == Format2Encoding::Midi7) {
        if (primitive.nestedBlocks.empty()) return true;
        if (primitive.nestedBlocks.size() != 1 || primitive.nestedBlocks[0].positionalTokens.empty() || primitive.nestedBlocks[0].positionalTokens[0].text != "Configure") return false;
        const Format2PropertySyntax* payloadProperty = nullptr;
        for (const Format2PropertySyntax& property : primitive.nestedBlocks[0].properties) if (property.name == "Payload") payloadProperty = &property;
        vector<Format2MidiSysExRingConfigureItem> payload;
        return ReadRingConfigurePayload(payloadProperty, payload);
    }
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Bar" && primitive.encoding == Format2Encoding::Midi7) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Meter" && primitive.encoding == Format2Encoding::Midi7) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::MidiPalette) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text" && primitive.encoding == Format2Encoding::MidiSysEx) {
        vector<Format2MidiSysExTextPayloadItem> payload;
        return ReadTextPayload(FindProperty(primitive, "Payload"), payload);
    }
    return primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::MidiRgb;
}

Format2MidiRuntimeLoadResult Format2MidiRuntimeLoader::Load(const string& filePath, Midi_ControlSurface* surface) {
    ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return Format2MidiRuntimeLoadResult::NotFormat2;
    ostringstream sourceBuffer;
    sourceBuffer << file.rdbuf();
    const string source = sourceBuffer.str();
    const size_t contentStart = source.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    const size_t firstText = source.find_first_not_of(" \t\r\n", contentStart);
    if (firstText == string::npos || source.compare(firstText, 5, "@Meta") != 0) return Format2MidiRuntimeLoadResult::NotFormat2;

    Format2SurfaceParseResult parsed = ParseFormat2SurfaceSource(source, filePath);
    if (!parsed.IsValid()) {
        for (const Format2Diagnostic& diagnostic : parsed.document.lexical.diagnostics) LogToConsole("[ERROR] Format 2 Surface parse failed in %s, line %d: %s\n", filePath.c_str(), diagnostic.location.line, diagnostic.message.c_str());
        return Format2MidiRuntimeLoadResult::Rejected;
    }
    if (parsed.document.metadata.protocol != Format2SurfaceProtocol::Midi) {
        LogToConsole("[ERROR] MIDI surface %s requires Protocol=MIDI\n", filePath.c_str());
        return Format2MidiRuntimeLoadResult::Rejected;
    }

    bool hasUnsupportedPrimitives = false;
    for (const Format2SurfaceWidget& definition : parsed.surface.widgets) {
        for (const Format2SurfacePrimitive& primitive : definition.primitives) {
            if (IsSupported(primitive)) continue;
            hasUnsupportedPrimitives = true;
            LogToConsole("[ERROR] Unsupported format 2 MIDI primitive in %s, line %d: %s %s\n", filePath.c_str(), primitive.location.line, primitive.direction == Format2PrimitiveDirection::Input ? "Input" : "Feedback", primitive.type.c_str());
        }
    }
    if (hasUnsupportedPrimitives) return Format2MidiRuntimeLoadResult::Rejected;

    surface->stepSize_.clear();
    surface->accelerationValuesForDecrement_.clear();
    surface->accelerationValuesForIncrement_.clear();
    surface->accelerationValues_.clear();
    for (const Format2EncoderProfile& profile : parsed.surface.encoderProfiles) {
        if (profile.delta) surface->stepSize_[profile.id] = *profile.delta;
        for (size_t valueIdx = 0; valueIdx < profile.increase.size(); ++valueIdx) surface->accelerationValuesForIncrement_[profile.id][profile.increase[valueIdx]] = (int) valueIdx;
        for (size_t valueIdx = 0; valueIdx < profile.decrease.size(); ++valueIdx) surface->accelerationValuesForDecrement_[profile.id][profile.decrease[valueIdx]] = (int) valueIdx;
        surface->accelerationValues_[profile.id] = profile.accelerationDeltas;
    }
    if (parsed.surface.colorCalibration) surface->ApplyFormat2ColorCalibration(*parsed.surface.colorCalibration);

    for (const Format2SurfaceWidget& definition : parsed.surface.widgets) {
        surface->AddWidget(surface, definition.id.c_str());
        Widget* widget = surface->GetWidgetByName(definition.id);
        if (!widget) continue;

        for (const Format2SurfacePrimitive& primitive : definition.primitives) {
            vector<string> tokens;
            string widgetClass;
            if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press" && primitive.encoding == Format2Encoding::MidiExact) {
                vector<int> on;
                vector<int> off;
                if (!ReadBytes(FindProperty(primitive, "On"), on)) continue;
                ReadBytes(FindProperty(primitive, "Off"), off);
                tokens = MakeTokens("Press", on, off);
                widget->MarkOskPressInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press" && primitive.encoding == Format2Encoding::MidiPrefix) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.size() != 2) continue;
                const string key = to_string(message[0] * 0x10000 + message[1] * 0x100);
                surface->AddMessageGenerator(key, make_unique<AnyPress_Midi_MessageGenerator>(surface->csi_, widget));
                widget->MarkOskPressInput();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Touch" && primitive.encoding == Format2Encoding::MidiExact) {
                vector<int> on;
                vector<int> off;
                if (!ReadBytes(FindProperty(primitive, "On"), on) || !ReadBytes(FindProperty(primitive, "Off"), off)) continue;
                tokens = MakeTokens("Touch", on, off);
                widget->MarkOskTouchInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14) {
                const Format2PropertySyntax* status = FindProperty(primitive, "Status");
                int statusByte = 0;
                if (!status || status->value.list || !ReadByte(status->value.scalar, statusByte)) continue;
                tokens = MakeTokens("Fader14Bit", { statusByte, 0, 0 });
                widget->MarkOskAbsoluteInput();
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.size() != 2) continue;
                const string key = to_string(message[0] * 0x10000 + message[1] * 0x100);
                surface->AddMessageGenerator(key, make_unique<Format2Midi7ValueMessageGenerator>(surface->csi_, widget));
                widget->MarkOskAbsoluteInput();
                widget->MarkOskValueFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.size() != 2) continue;
                const Format2PropertySyntax* profile = FindProperty(primitive, "Profile");
                const string key = to_string(message[0] * 0x10000 + message[1] * 0x100);
                if (profile && !profile->value.list) {
                    widgetClass = profile->value.scalar.text;
                    surface->AddMessageGenerator(key, make_unique<AcceleratedPreconfiguredEncoder_Midi_MessageGenerator>(surface->csi_, widget, widgetClass));
                } else {
                    const Format2PropertySyntax* mode = FindProperty(primitive, "Mode");
                    Format2MidiEncoderMode encoderMode = Format2MidiEncoderMode::SignedBit;
                    if (mode && mode->value.scalar.text == "SignedBitFixed") encoderMode = Format2MidiEncoderMode::SignedBitFixed;
                    else if (mode && mode->value.scalar.text == "Relative7Bit") encoderMode = Format2MidiEncoderMode::Relative7Bit;
                    surface->AddMessageGenerator(key, make_unique<Format2Midi7EncoderMessageGenerator>(surface->csi_, widget, encoderMode));
                }
                widget->SetOskWidgetClass(widgetClass);
                widget->MarkOskRelativeInput();
                widget->MarkOskValueFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && primitive.encoding == Format2Encoding::MidiExact) {
                vector<int> on;
                vector<int> off;
                if (!ReadBytes(FindProperty(primitive, "On"), on) || !ReadBytes(FindProperty(primitive, "Off"), off)) continue;
                tokens = MakeTokens("FB_TwoState", on, off);
                widget->MarkOskToggleFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && primitive.encoding == Format2Encoding::MidiSysEx) {
                vector<Format2MidiSysExStatePayloadItem> payload;
                if (!ReadStatePayload(FindProperty(primitive, "Payload"), payload)) continue;
                widget->GetFeedbackProcessors().push_back(make_unique<Format2MidiSysExStateFeedbackProcessor>(surface->csi_, surface, widget, payload));
                widget->MarkOskToggleFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14) {
                const Format2PropertySyntax* status = FindProperty(primitive, "Status");
                int statusByte = 0;
                if (!status || status->value.list || !ReadByte(status->value.scalar, statusByte)) continue;
                tokens = MakeTokens("FB_Fader14Bit", { statusByte, 0, 0 });
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.empty() || message.size() > 2) continue;
                const Format2PropertySyntax* valueBase = FindProperty(primitive, "ValueBase");
                const Format2PropertySyntax* combine = FindProperty(primitive, "Combine");
                const Format2PropertySyntax* echoGuard = FindProperty(primitive, "EchoGuardMs");
                const Format2PropertySyntax* suppressWhileTouched = FindProperty(primitive, "SuppressWhileTouched");
                int valueBaseByte = 0;
                if (valueBase) ReadByte(valueBase->value.scalar, valueBaseByte);
                Format2MidiValueCombine combineMode = Format2MidiValueCombine::Replace;
                if (combine && combine->value.scalar.text == "Add") combineMode = Format2MidiValueCombine::Add;
                else if (combine && combine->value.scalar.text == "BitOr") combineMode = Format2MidiValueCombine::BitOr;
                const int echoGuardMs = echoGuard ? atoi(echoGuard->value.scalar.text.c_str()) : 0;
                const bool suppress = suppressWhileTouched && suppressWhileTouched->value.scalar.text == "true";
                widget->GetFeedbackProcessors().push_back(make_unique<Format2Midi7ValueFeedbackProcessor>(surface->csi_, surface, widget, message, valueBaseByte, combineMode, echoGuardMs, suppress));
                widget->MarkOskValueFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Ring" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.empty() || message.size() > 2) continue;
                const Format2PropertySyntax* profileProperty = FindProperty(primitive, "RingProfile");
                if (!profileProperty || profileProperty->value.list) continue;
                const Format2RingProfile* profile = nullptr;
                for (const Format2RingProfile& candidate : parsed.surface.ringProfiles) if (candidate.id == profileProperty->value.scalar.text) { profile = &candidate; break; }
                if (!profile) continue;
                int valueBase = 0;
                const Format2PropertySyntax* valueBaseProperty = FindProperty(primitive, "ValueBase");
                if (valueBaseProperty) ReadByte(valueBaseProperty->value.scalar, valueBase);
                Format2MidiValueCombine valueCombine = Format2MidiValueCombine::Replace;
                const Format2PropertySyntax* combineProperty = FindProperty(primitive, "Combine");
                if (combineProperty && combineProperty->value.scalar.text == "Add") valueCombine = Format2MidiValueCombine::Add;
                else if (combineProperty && combineProperty->value.scalar.text == "BitOr") valueCombine = Format2MidiValueCombine::BitOr;
                Format2MidiRingStyleTarget styleTarget = Format2MidiRingStyleTarget::Value;
                const Format2PropertySyntax* styleTargetProperty = FindProperty(primitive, "StyleTarget");
                if (styleTargetProperty && styleTargetProperty->value.scalar.text == "Status") styleTarget = Format2MidiRingStyleTarget::Status;
                else if (styleTargetProperty && styleTargetProperty->value.scalar.text == "Data1") styleTarget = Format2MidiRingStyleTarget::Data1;
                const Format2PropertySyntax* styleShiftProperty = FindProperty(primitive, "StyleShift");
                const int styleShift = styleShiftProperty ? atoi(styleShiftProperty->value.scalar.text.c_str()) : 0;
                Format2MidiRingStyleCombine styleCombine = Format2MidiRingStyleCombine::BitOr;
                const Format2PropertySyntax* styleCombineProperty = FindProperty(primitive, "StyleCombine");
                if (styleCombineProperty && styleCombineProperty->value.scalar.text == "Add") styleCombine = Format2MidiRingStyleCombine::Add;
                vector<Format2MidiSysExRingConfigureItem> configurePayload;
                if (!primitive.nestedBlocks.empty()) {
                    const Format2PropertySyntax* payloadProperty = nullptr;
                    for (const Format2PropertySyntax& property : primitive.nestedBlocks[0].properties) if (property.name == "Payload") payloadProperty = &property;
                    if (!ReadRingConfigurePayload(payloadProperty, configurePayload)) continue;
                }
                widget->GetFeedbackProcessors().push_back(make_unique<Format2Midi7RingFeedbackProcessor>(surface->csi_, surface, widget, message, *profile, valueBase, valueCombine, styleTarget, styleShift, styleCombine, configurePayload));
                widget->MarkOskValueFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::MidiRgb) {
                vector<int> enable;
                vector<int> red;
                vector<int> green;
                vector<int> blue;
                const Format2PropertySyntax* enableProperty = FindProperty(primitive, "Enable");
                if (enableProperty) ReadBytes(enableProperty, enable);
                if (!ReadBytes(FindProperty(primitive, "Red"), red) || red.size() != 2 || !ReadBytes(FindProperty(primitive, "Green"), green) || green.size() != 2 || !ReadBytes(FindProperty(primitive, "Blue"), blue) || blue.size() != 2) continue;
                const Format2PropertySyntax* inactiveBrightness = FindProperty(primitive, "InactiveBrightness");
                const Format2PropertySyntax* activeBrightness = FindProperty(primitive, "ActiveBrightness");
                const bool hasStateBrightness = inactiveBrightness && activeBrightness;
                const float inactiveBrightnessValue = hasStateBrightness ? (float) atof(inactiveBrightness->value.scalar.text.c_str()) : 1.0f;
                const float activeBrightnessValue = hasStateBrightness ? (float) atof(activeBrightness->value.scalar.text.c_str()) : 1.0f;
                widget->GetFeedbackProcessors().push_back(make_unique<Format2MidiRgbFeedbackProcessor>(surface->csi_, surface, widget, std::array<int, 2>{ red[0], red[1] }, std::array<int, 2>{ green[0], green[1] }, std::array<int, 2>{ blue[0], blue[1] }, enable, hasStateBrightness, inactiveBrightnessValue, activeBrightnessValue));
                widget->MarkOskColorFeedback();
                if (hasStateBrightness) widget->MarkOskToggleFeedback();
                const Format2PropertySyntax* trackColor = FindProperty(primitive, "TrackColor");
                if (trackColor && trackColor->value.scalar.text == "true") surface->AddTrackColorFeedbackProcessor(widget->GetFeedbackProcessors().back().get());
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::MidiPalette) {
                vector<int> message;
                vector<int> companion;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.size() != 2) continue;
                const Format2PropertySyntax* profileProperty = FindProperty(primitive, "ColorProfile");
                if (!profileProperty || profileProperty->value.list) continue;
                const Format2ColorProfile* profile = nullptr;
                for (const Format2ColorProfile& candidate : parsed.surface.colorProfiles) if (candidate.id == profileProperty->value.scalar.text) { profile = &candidate; break; }
                if (!profile) continue;
                const Format2PropertySyntax* companionProperty = FindProperty(primitive, "Companion");
                if (companionProperty) ReadBytes(companionProperty, companion);
                const Format2PropertySyntax* companionOrder = FindProperty(primitive, "CompanionOrder");
                const bool companionBefore = companionOrder && companionOrder->value.scalar.text == "Before";
                widget->GetFeedbackProcessors().push_back(make_unique<Format2MidiPaletteFeedbackProcessor>(surface->csi_, surface, widget, std::array<int, 2>{ message[0], message[1] }, *profile, companion, companionBefore));
                widget->MarkOskColorFeedback();
                const Format2PropertySyntax* trackColor = FindProperty(primitive, "TrackColor");
                if (trackColor && trackColor->value.scalar.text == "true") surface->AddTrackColorFeedbackProcessor(widget->GetFeedbackProcessors().back().get());
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Bar" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                vector<int> styleMessage;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.empty() || message.size() > 2 || !ReadBytes(FindProperty(primitive, "StyleMessage"), styleMessage) || styleMessage.size() != 2) continue;
                const Format2PropertySyntax* profileProperty = FindProperty(primitive, "BarProfile");
                if (!profileProperty || profileProperty->value.list) continue;
                const Format2BarProfile* profile = nullptr;
                for (const Format2BarProfile& candidate : parsed.surface.barProfiles) if (candidate.id == profileProperty->value.scalar.text) { profile = &candidate; break; }
                if (!profile) continue;
                int valueBase = 0;
                const Format2PropertySyntax* valueBaseProperty = FindProperty(primitive, "ValueBase");
                if (valueBaseProperty) ReadByte(valueBaseProperty->value.scalar, valueBase);
                Format2MidiValueCombine combine = Format2MidiValueCombine::Replace;
                const Format2PropertySyntax* combineProperty = FindProperty(primitive, "Combine");
                if (combineProperty && combineProperty->value.scalar.text == "Add") combine = Format2MidiValueCombine::Add;
                else if (combineProperty && combineProperty->value.scalar.text == "BitOr") combine = Format2MidiValueCombine::BitOr;
                widget->GetFeedbackProcessors().push_back(make_unique<Format2Midi7BarFeedbackProcessor>(surface->csi_, surface, widget, message, std::array<int, 2>{ styleMessage[0], styleMessage[1] }, *profile, valueBase, combine));
                widget->MarkOskValueFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text" && primitive.encoding == Format2Encoding::MidiSysEx) {
                vector<Format2MidiSysExTextPayloadItem> payload;
                if (!ReadTextPayload(FindProperty(primitive, "Payload"), payload)) continue;
                const Format2PropertySyntax* profileProperty = FindProperty(primitive, "TextProfile");
                if (!profileProperty || profileProperty->value.list) continue;
                const Format2TextProfile* profile = nullptr;
                for (const Format2TextProfile& candidate : parsed.surface.textProfiles) if (candidate.id == profileProperty->value.scalar.text) { profile = &candidate; break; }
                if (!profile) continue;
                int topMargin = 0;
                int bottomMargin = 0;
                int font = 0;
                std::uint32_t backgroundColor = 0;
                std::uint32_t textColor = 0;
                const Format2PropertySyntax* topMarginProperty = FindProperty(primitive, "TopMargin");
                const Format2PropertySyntax* bottomMarginProperty = FindProperty(primitive, "BottomMargin");
                const Format2PropertySyntax* fontProperty = FindProperty(primitive, "Font");
                const Format2PropertySyntax* backgroundColorProperty = FindProperty(primitive, "BackgroundColor");
                const Format2PropertySyntax* textColorProperty = FindProperty(primitive, "TextColor");
                if (topMarginProperty) ReadByte(topMarginProperty->value.scalar, topMargin);
                if (bottomMarginProperty) ReadByte(bottomMarginProperty->value.scalar, bottomMargin);
                if (fontProperty) ReadByte(fontProperty->value.scalar, font);
                if (backgroundColorProperty) ParseFormat2ColorScalar(backgroundColorProperty->value.scalar, backgroundColor);
                if (textColorProperty) ParseFormat2ColorScalar(textColorProperty->value.scalar, textColor);
                widget->GetFeedbackProcessors().push_back(make_unique<Format2MidiSysExTextFeedbackProcessor>(surface->csi_, surface, widget, payload, *profile, topMargin, bottomMargin, font, backgroundColor, textColor));
                widget->MarkOskTextFeedback();
                if (std::find(primitive.capabilities.begin(), primitive.capabilities.end(), Format2Capability::Color) != primitive.capabilities.end()) widget->MarkOskColorFeedback();
                continue;
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Meter" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.empty() || message.size() > 2) continue;
                const Format2PropertySyntax* profileProperty = FindProperty(primitive, "MeterProfile");
                if (!profileProperty || profileProperty->value.list) continue;
                const Format2MeterProfile* profile = nullptr;
                for (const Format2MeterProfile& candidate : parsed.surface.meterProfiles) if (candidate.id == profileProperty->value.scalar.text) { profile = &candidate; break; }
                if (!profile) continue;
                int valueBase = 0;
                const Format2PropertySyntax* valueBaseProperty = FindProperty(primitive, "ValueBase");
                if (valueBaseProperty) ReadByte(valueBaseProperty->value.scalar, valueBase);
                Format2MidiValueCombine combine = Format2MidiValueCombine::Replace;
                const Format2PropertySyntax* combineProperty = FindProperty(primitive, "Combine");
                if (combineProperty && combineProperty->value.scalar.text == "Add") combine = Format2MidiValueCombine::Add;
                else if (combineProperty && combineProperty->value.scalar.text == "BitOr") combine = Format2MidiValueCombine::BitOr;
                const Format2PropertySyntax* refreshProperty = FindProperty(primitive, "Refresh");
                const bool continuous = refreshProperty && refreshProperty->value.scalar.text == "Continuous";
                const Format2PropertySyntax* refreshIntervalProperty = FindProperty(primitive, "RefreshIntervalMs");
                const int refreshIntervalMs = refreshIntervalProperty ? atoi(refreshIntervalProperty->value.scalar.text.c_str()) : 0;
                widget->GetFeedbackProcessors().push_back(make_unique<Format2Midi7MeterFeedbackProcessor>(surface->csi_, surface, widget, message, *profile, valueBase, combine, continuous, refreshIntervalMs));
                widget->MarkOskMeterFeedback();
                widget->MarkOskValueFeedback();
                continue;
            } else continue;

            MidiWidgetContext context;
            context.csi = surface->csi_;
            context.surface = surface;
            context.widget = widget;
            context.widgetClass = widgetClass;
            context.tokens = tokens;
            context.size = (int) tokens.size();
            const Format2PropertySyntax* suppressWhileTouched = FindProperty(primitive, "SuppressWhileTouched");
            context.suppressWhileTouched = suppressWhileTouched && !suppressWhileTouched->value.list && suppressWhileTouched->value.scalar.text == "true";
            const Format2PropertySyntax* echoGuard = FindProperty(primitive, "EchoGuardMs");
            context.echoGuardMs = echoGuard && !echoGuard->value.list ? atoi(echoGuard->value.scalar.text.c_str()) : 0;
            if (context.size > 3) {
                context.message1.midi_message[0] = atoi(tokens[1].c_str());
                context.message1.midi_message[1] = atoi(tokens[2].c_str());
                context.message1.midi_message[2] = atoi(tokens[3].c_str());
                context.oneByteKey = to_string(context.message1.midi_message[0] * 0x10000);
                context.twoByteKey = to_string(context.message1.midi_message[0] * 0x10000 + context.message1.midi_message[1] * 0x100);
                context.threeByteKey = to_string(context.message1.midi_message[0] * 0x10000 + context.message1.midi_message[1] * 0x100 + context.message1.midi_message[2]);
            }
            if (context.size > 6) {
                context.message2.midi_message[0] = atoi(tokens[4].c_str());
                context.message2.midi_message[1] = atoi(tokens[5].c_str());
                context.message2.midi_message[2] = atoi(tokens[6].c_str());
                context.threeByteKeyMsg2 = to_string(context.message2.midi_message[0] * 0x10000 + context.message2.midi_message[1] * 0x100 + context.message2.midi_message[2]);
            }
            if (!MidiWidgetRegistry::Dispatch(tokens[0], context)) LogToConsole("[ERROR] Unsupported format 2 MIDI runtime mapping in %s, line %d: %s\n", filePath.c_str(), primitive.location.line, tokens[0].c_str());
        }
    }

    if (parsed.surface.initialization) {
        vector<vector<int>> messages;
        for (const Format2MidiInitializationMessage& message : parsed.surface.initialization->midiMessages) messages.push_back(message.bytes);
        surface->SetFormat2InitializationMessages(messages);
    }

    if (parsed.surface.oskLayout) surface->ApplyFormat2OSKLayout(filePath, *parsed.surface.oskLayout);
    return Format2MidiRuntimeLoadResult::Loaded;
}
