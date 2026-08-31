#include "../integrator.h"
#include "../format2_surface_document.h"
#include "format2_osc_runtime.h"
#include "osc_surface.h"

class Format2OscValueMessageGenerator : public MessageGenerator
{
public:
    Format2OscValueMessageGenerator(CSurfIntegrator* const csi, Widget* widget) : MessageGenerator(csi, widget) {}

    virtual void ProcessMessage(double value) override {
        this->widget_->SetIncomingMessageTime(GetTickCount());
        this->widget_->GetZoneManager()->DoAction(this->widget_, value);
    }
};

class Format2OscFloatFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;
    int const echoGuardMs_;

public:
    Format2OscFloatFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, int echoGuardMs) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), echoGuardMs_(echoGuardMs) {}

    virtual void ForceValue(const PropertyList& properties, double value) override {
        if (this->echoGuardMs_ > 0 && (GetTickCount() - this->widget_->GetLastIncomingMessageTime()) < (DWORD) this->echoGuardMs_) return;
        this->lastDoubleValue_ = value;
        this->surface_->SendOSCMessage(this->address_.c_str(), value);
    }

    virtual void ForceClear() override {
        this->lastDoubleValue_ = 0.0;
        this->surface_->SendOSCMessage(this->address_.c_str(), 0.0);
    }
};

class Format2OscStringFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;

public:
    Format2OscStringFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address) : FeedbackProcessor(csi, widget), surface_(surface), address_(address) {}

    virtual void ForceValue(const PropertyList& properties, const char* const& value) override {
        this->lastStringValue_ = value;
        char restrictedText[MEDBUF];
        this->surface_->SendOSCMessage(this->address_.c_str(), this->widget_->GetSurface()->GetRestrictedLengthText(value, restrictedText, sizeof(restrictedText)));
    }

    virtual void ForceClear() override {
        this->lastStringValue_.clear();
        this->surface_->SendOSCMessage(this->address_.c_str(), "");
    }
};

class Format2OscHexRgbaFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;

public:
    Format2OscHexRgbaFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address) : FeedbackProcessor(csi, widget), surface_(surface), address_(address) {}

    virtual void SetColorValue(const rgba_color& color) override {
        if (this->lastColor_ == color) return;
        this->lastColor_ = color;
        char colorText[10];
        this->surface_->SendOSCMessage(this->address_.c_str(), color.rgba_to_string(colorText));
    }
};

const Format2PropertySyntax* Format2OscRuntimeLoader::FindProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == name) return &property;
    return nullptr;
}

string Format2OscRuntimeLoader::ReadAddress(const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* address = FindProperty(primitive, "Address");
    return !address || address->value.list ? string{} : address->value.scalar.text;
}

int Format2OscRuntimeLoader::ReadIntegerProperty(const Format2SurfacePrimitive& primitive, const string& name, int defaultValue) {
    const Format2PropertySyntax* property = FindProperty(primitive, name);
    if (!property || property->value.list) return defaultValue;
    char* end = nullptr;
    const long parsed = strtol(property->value.scalar.text.c_str(), &end, 10);
    return end && *end == '\0' ? (int) parsed : defaultValue;
}

bool Format2OscRuntimeLoader::IsSupported(const Format2SurfacePrimitive& primitive) {
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && primitive.encoding == Format2Encoding::OscFloat && !FindProperty(primitive, "ValueProfile")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && primitive.encoding == Format2Encoding::OscFloat
        && !FindProperty(primitive, "ValueProfile") && !FindProperty(primitive, "SuppressWhileTouched") && !FindProperty(primitive, "InitialValue")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text" && primitive.encoding == Format2Encoding::OscString && !FindProperty(primitive, "TextProfile")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::OscString) {
        const Format2PropertySyntax* format = FindProperty(primitive, "Format");
        return format && !format->value.list && format->value.scalar.text == "HexRGBA";
    }
    return false;
}

Format2OscRuntimeLoadResult Format2OscRuntimeLoader::Load(const string& filePath, OSC_ControlSurface* surface) {
    ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return Format2OscRuntimeLoadResult::NotFormat2;
    ostringstream sourceBuffer;
    sourceBuffer << file.rdbuf();
    const string source = sourceBuffer.str();
    const size_t contentStart = source.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    const size_t firstText = source.find_first_not_of(" \t\r\n", contentStart);
    if (firstText == string::npos || source.compare(firstText, 5, "@Meta") != 0) return Format2OscRuntimeLoadResult::NotFormat2;

    Format2SurfaceParseResult parsed = ParseFormat2SurfaceSource(source, filePath);
    if (!parsed.IsValid()) {
        for (const Format2Diagnostic& diagnostic : parsed.document.lexical.diagnostics) LogToConsole("[ERROR] Format 2 Surface parse failed in %s, line %d: %s\n", filePath.c_str(), diagnostic.location.line, diagnostic.message.c_str());
        return Format2OscRuntimeLoadResult::Rejected;
    }
    if (parsed.document.metadata.protocol != Format2SurfaceProtocol::Osc) {
        LogToConsole("[ERROR] OSC surface %s requires Protocol=OSC\n", filePath.c_str());
        return Format2OscRuntimeLoadResult::Rejected;
    }

    bool hasUnsupportedPrimitives = false;
    for (const Format2SurfaceWidget& definition : parsed.surface.widgets) {
        for (const Format2SurfacePrimitive& primitive : definition.primitives) {
            if (IsSupported(primitive)) continue;
            hasUnsupportedPrimitives = true;
            LogToConsole("[ERROR] Unsupported format 2 OSC primitive in %s, line %d: %s %s\n", filePath.c_str(), primitive.location.line, primitive.direction == Format2PrimitiveDirection::Input ? "Input" : "Feedback", primitive.type.c_str());
        }
    }
    if (hasUnsupportedPrimitives) return Format2OscRuntimeLoadResult::Rejected;

    for (const Format2SurfaceWidget& definition : parsed.surface.widgets) {
        surface->AddWidget(surface, definition.id.c_str());
        Widget* widget = surface->GetWidgetByName(definition.id);
        if (!widget) continue;
        for (const Format2SurfacePrimitive& primitive : definition.primitives) {
            const string address = ReadAddress(primitive);
            if (address.empty()) continue;
            if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value") {
                surface->MessageGeneratorsByMessage_.insert(make_pair(address, make_unique<Format2OscValueMessageGenerator>(surface->csi_, widget)));
                widget->MarkOskAbsoluteInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value") {
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscFloatFeedbackProcessor>(surface->csi_, surface, widget, address, ReadIntegerProperty(primitive, "EchoGuardMs", 0)));
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text") {
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscStringFeedbackProcessor>(surface->csi_, surface, widget, address));
                widget->MarkOskTextFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color") {
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscHexRgbaFeedbackProcessor>(surface->csi_, surface, widget, address));
                widget->MarkOskColorFeedback();
            }
        }
    }
    if (parsed.surface.oskLayout) surface->ApplyFormat2OSKLayout(filePath, *parsed.surface.oskLayout);
    return Format2OscRuntimeLoadResult::Loaded;
}
