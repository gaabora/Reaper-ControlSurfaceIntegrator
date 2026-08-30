#include "../integrator.h"
#include "../format2_surface_document.h"
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

vector<string> Format2MidiRuntimeLoader::MakeTokens(const string& type, const vector<int>& firstMessage, const vector<int>& secondMessage) {
    vector<string> tokens = { type };
    for (int value : firstMessage) tokens.push_back(to_string(value));
    for (int value : secondMessage) tokens.push_back(to_string(value));
    return tokens;
}

bool Format2MidiRuntimeLoader::IsSupported(const Format2SurfacePrimitive& primitive) {
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press" && primitive.encoding == Format2Encoding::MidiExact) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Touch" && primitive.encoding == Format2Encoding::MidiExact) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder" && primitive.encoding == Format2Encoding::Midi7) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && primitive.encoding == Format2Encoding::MidiExact) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14) return true;
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
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder" && primitive.encoding == Format2Encoding::Midi7) {
                vector<int> message;
                if (!ReadBytes(FindProperty(primitive, "Message"), message) || message.size() != 2) continue;
                const Format2PropertySyntax* profile = FindProperty(primitive, "Profile");
                if (profile && !profile->value.list) widgetClass = profile->value.scalar.text;
                tokens = MakeTokens("Encoder", { message[0], message[1], 0 });
                widget->SetOskWidgetClass(widgetClass);
                widget->MarkOskRelativeInput();
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && primitive.encoding == Format2Encoding::MidiExact) {
                vector<int> on;
                vector<int> off;
                if (!ReadBytes(FindProperty(primitive, "On"), on) || !ReadBytes(FindProperty(primitive, "Off"), off)) continue;
                tokens = MakeTokens("FB_TwoState", on, off);
                widget->MarkOskToggleFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::Midi14) {
                const Format2PropertySyntax* status = FindProperty(primitive, "Status");
                int statusByte = 0;
                if (!status || status->value.list || !ReadByte(status->value.scalar, statusByte)) continue;
                tokens = MakeTokens("FB_Fader14Bit", { statusByte, 0, 0 });
                widget->MarkOskValueFeedback();
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

    if (parsed.surface.oskLayout) surface->ApplyFormat2OSKLayout(filePath, *parsed.surface.oskLayout);
    return Format2MidiRuntimeLoadResult::Loaded;
}
