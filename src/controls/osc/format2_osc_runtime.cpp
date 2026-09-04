#include "../integrator.h"
#include "../format2_surface_document.h"
#include "../format2_value_validation.h"
#include "format2_osc_runtime.h"
#include "osc_surface.h"

static const Format2PropertySyntax* FindFormat2OscProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == name) return &property;
    return nullptr;
}

static const Format2PropertySyntax* FindFormat2OscNodeProperty(const Format2SyntaxNode& node, const string& name) {
    for (const Format2PropertySyntax& property : node.properties) if (property.name == name) return &property;
    return nullptr;
}

static const Format2ValueProfile* FindFormat2OscValueProfile(const Format2SurfaceDocument& document, const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* property = FindFormat2OscProperty(primitive, "ValueProfile");
    if (!property || property->value.list) return nullptr;
    for (const Format2ValueProfile& profile : document.valueProfiles) if (profile.id == property->value.scalar.text) return &profile;
    return nullptr;
}

static std::optional<double> ReadFormat2OscFiniteProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    const Format2PropertySyntax* property = FindFormat2OscProperty(primitive, name);
    double value = 0.0;
    if (!property || property->value.list || !ParseFormat2FiniteScalar(property->value.scalar, value)) return std::nullopt;
    return value;
}

static double InterpolateFormat2Value(double input, double firstInput, double firstOutput, double secondInput, double secondOutput) {
    if (secondInput == firstInput) return firstOutput;
    const double position = (input - firstInput) / (secondInput - firstInput);
    return firstOutput + position * (secondOutput - firstOutput);
}

static double DecodeFormat2ValueProfile(const Format2ValueProfile& profile, double input) {
    if (profile.points.empty()) return input;
    double output = profile.points.front().output;
    if (input >= profile.points.back().input) output = profile.points.back().output;
    else if (input > profile.points.front().input) {
        for (std::size_t pointIdx = 1; pointIdx < profile.points.size(); ++pointIdx) {
            if (input > profile.points[pointIdx].input) continue;
            output = profile.interpolation == Format2Interpolation::Step ? profile.points[pointIdx - 1].output : InterpolateFormat2Value(input, profile.points[pointIdx - 1].input, profile.points[pointIdx - 1].output, profile.points[pointIdx].input, profile.points[pointIdx].output);
            break;
        }
    }
    return profile.outputUnit == Format2ValueUnit::Decibels ? volToNormalized(DB2VAL(output)) : output;
}

static double EncodeFormat2ValueProfile(const Format2ValueProfile& profile, double input) {
    if (profile.points.empty()) return input;
    const double output = profile.outputUnit == Format2ValueUnit::Decibels ? VAL2DB(normalizedToVol(input)) : input;
    const bool increasing = profile.points.back().output > profile.points.front().output;
    if ((increasing && output <= profile.points.front().output) || (!increasing && output >= profile.points.front().output)) return profile.points.front().input;
    if ((increasing && output >= profile.points.back().output) || (!increasing && output <= profile.points.back().output)) return profile.points.back().input;
    for (std::size_t pointIdx = 1; pointIdx < profile.points.size(); ++pointIdx) {
        if ((increasing && output > profile.points[pointIdx].output) || (!increasing && output < profile.points[pointIdx].output)) continue;
        return InterpolateFormat2Value(output, profile.points[pointIdx - 1].output, profile.points[pointIdx - 1].input, profile.points[pointIdx].output, profile.points[pointIdx].input);
    }
    return profile.points.back().input;
}

class Format2OscValueMessageGenerator : public MessageGenerator
{
private:
    std::optional<Format2ValueProfile> profile_;

public:
    Format2OscValueMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const Format2ValueProfile* profile) : MessageGenerator(csi, widget), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}

    virtual void ProcessMessage(double value) override {
        this->widget_->SetIncomingMessageTime(GetTickCount());
        this->widget_->GetZoneManager()->DoAction(this->widget_, this->profile_ ? DecodeFormat2ValueProfile(*this->profile_, value) : value);
    }
};

class Format2OscPressMessageGenerator : public MessageGenerator
{
private:
    string match_ = "Any";
    std::optional<double> onValue_;
    std::optional<double> offValue_;

public:
    Format2OscPressMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const Format2SurfacePrimitive& primitive) : MessageGenerator(csi, widget) {
        const Format2PropertySyntax* match = FindFormat2OscProperty(primitive, "Match");
        if (match && !match->value.list) this->match_ = match->value.scalar.text;
        this->onValue_ = ReadFormat2OscFiniteProperty(primitive, "OnValue");
        this->offValue_ = ReadFormat2OscFiniteProperty(primitive, "OffValue");
    }

    virtual void ProcessMessage(double value) override {
        if (this->match_ == "Any") this->widget_->GetZoneManager()->DoAction(this->widget_, 1.0);
        else if (this->match_ == "NonZero") this->widget_->GetZoneManager()->DoAction(this->widget_, value != 0.0 ? 1.0 : 0.0);
        else if (this->onValue_ && value == *this->onValue_) this->widget_->GetZoneManager()->DoAction(this->widget_, 1.0);
        else if (this->offValue_ && value == *this->offValue_) this->widget_->GetZoneManager()->DoAction(this->widget_, 0.0);
    }
};

class Format2OscTouchMessageGenerator : public MessageGenerator
{
private:
    string match_ = "NonZero";
    std::optional<double> onValue_;
    std::optional<double> offValue_;

public:
    Format2OscTouchMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const Format2SurfacePrimitive& primitive) : MessageGenerator(csi, widget) {
        const Format2PropertySyntax* match = FindFormat2OscProperty(primitive, "Match");
        if (match && !match->value.list) this->match_ = match->value.scalar.text;
        this->onValue_ = ReadFormat2OscFiniteProperty(primitive, "OnValue");
        this->offValue_ = ReadFormat2OscFiniteProperty(primitive, "OffValue");
    }

    virtual void ProcessMessage(double value) override {
        if (this->match_ == "NonZero") this->widget_->GetZoneManager()->DoTouch(this->widget_, value != 0.0 ? 1.0 : 0.0);
        else if (this->onValue_ && value == *this->onValue_) this->widget_->GetZoneManager()->DoTouch(this->widget_, 1.0);
        else if (this->offValue_ && value == *this->offValue_) this->widget_->GetZoneManager()->DoTouch(this->widget_, 0.0);
    }
};

class Format2OscEncoderMessageGenerator : public MessageGenerator
{
private:
    OSC_ControlSurface* const surface_;
    std::optional<Format2ValueProfile> profile_;
    double scale_ = 1.0;
    string acknowledgeAddress_;
    int acknowledgeValue_ = 0;

public:
    Format2OscEncoderMessageGenerator(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const Format2SurfaceDocument& document, const Format2SurfacePrimitive& primitive) : MessageGenerator(csi, widget), surface_(surface) {
        const Format2ValueProfile* profile = FindFormat2OscValueProfile(document, primitive);
        if (profile) this->profile_ = *profile;
        const Format2PropertySyntax* scale = FindFormat2OscProperty(primitive, "Scale");
        if (scale && !scale->value.list) ParseFormat2FiniteScalar(scale->value.scalar, this->scale_);
        for (const Format2SyntaxNode& nested : primitive.nestedBlocks) {
            if (nested.positionalTokens.empty() || nested.positionalTokens[0].text != "Acknowledge") continue;
            const Format2PropertySyntax* address = FindFormat2OscNodeProperty(nested, "Address");
            const Format2PropertySyntax* value = FindFormat2OscNodeProperty(nested, "Value");
            if (address && !address->value.list) this->acknowledgeAddress_ = address->value.scalar.text;
            if (value && !value->value.list) ParseFormat2IntegerScalar(value->value.scalar, this->acknowledgeValue_);
        }
    }

    virtual void ProcessMessage(double value) override {
        const double delta = this->profile_ ? DecodeFormat2ValueProfile(*this->profile_, value) : value * this->scale_;
        this->widget_->SetLastIncomingDelta(delta);
        this->widget_->GetZoneManager()->DoRelativeAction(this->widget_, delta);
        if (!this->acknowledgeAddress_.empty()) this->surface_->SendOSCMessage(this->acknowledgeAddress_.c_str(), this->acknowledgeValue_);
    }
};

class Format2OscValueFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;
    int const echoGuardMs_;
    bool const integer_;
    std::optional<Format2ValueProfile> profile_;

public:
    Format2OscValueFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, int echoGuardMs, bool integer, const Format2ValueProfile* profile) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), echoGuardMs_(echoGuardMs), integer_(integer), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}

    virtual void ForceValue(const PropertyList& properties, double value) override {
        if (this->echoGuardMs_ > 0 && (GetTickCount() - this->widget_->GetLastIncomingMessageTime()) < (DWORD) this->echoGuardMs_) return;
        this->lastDoubleValue_ = value;
        const double output = this->profile_ ? EncodeFormat2ValueProfile(*this->profile_, value) : value;
        if (this->integer_) this->surface_->SendOSCMessage(this->address_.c_str(), (int) output);
        else this->surface_->SendOSCMessage(this->address_.c_str(), output);
    }

    virtual void ForceClear() override {
        this->lastDoubleValue_ = 0.0;
        const double output = this->profile_ ? EncodeFormat2ValueProfile(*this->profile_, 0.0) : 0.0;
        if (this->integer_) this->surface_->SendOSCMessage(this->address_.c_str(), (int) output);
        else this->surface_->SendOSCMessage(this->address_.c_str(), output);
    }
};

class Format2OscStateFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;
    bool const integer_;
    double const offValue_;
    double const onValue_;

    void SendValue(double value) {
        if (this->integer_) this->surface_->SendOSCMessage(this->address_.c_str(), (int) value);
        else this->surface_->SendOSCMessage(this->address_.c_str(), value);
    }

public:
    Format2OscStateFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, bool integer, double offValue, double onValue) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), integer_(integer), offValue_(offValue), onValue_(onValue) {}

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->SendValue(value == 0.0 ? this->offValue_ : this->onValue_);
    }

    virtual void ForceClear() override {
        this->lastDoubleValue_ = 0.0;
        this->SendValue(this->offValue_);
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
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(color);
        char colorText[10];
        this->surface_->SendOSCMessage(this->address_.c_str(), deviceColor.rgba_to_string(colorText));
    }
};

const Format2PropertySyntax* Format2OscRuntimeLoader::FindProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    return FindFormat2OscProperty(primitive, name);
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
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Touch" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)
        && !FindProperty(primitive, "SuppressWhileTouched") && !FindProperty(primitive, "InitialValue")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text" && primitive.encoding == Format2Encoding::OscString && !FindProperty(primitive, "TextProfile")) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::OscString) {
        const Format2PropertySyntax* format = FindProperty(primitive, "Format");
        return format && !format->value.list && format->value.scalar.text == "HexRGBA";
    }
    return false;
}

Format2OscRuntimeLoadResult Format2OscRuntimeLoader::Load(const string& filePath, OSC_ControlSurface* surface) {
    ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        LogToConsole("[ERROR] Cannot open format 2 OSC Surface file %s\n", filePath.c_str());
        return Format2OscRuntimeLoadResult::Rejected;
    }
    ostringstream sourceBuffer;
    sourceBuffer << file.rdbuf();
    const string source = sourceBuffer.str();
    const size_t contentStart = source.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    const size_t firstText = source.find_first_not_of(" \t\r\n", contentStart);
    if (firstText == string::npos || source.compare(firstText, 5, "@Meta") != 0) {
        LogToConsole("[ERROR] OSC Surface %s is not format 2. Import it with the configuration editor first.\n", filePath.c_str());
        return Format2OscRuntimeLoadResult::Rejected;
    }

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
    if (parsed.surface.colorCalibration) surface->ApplyFormat2ColorCalibration(*parsed.surface.colorCalibration);

    for (const Format2SurfaceWidget& definition : parsed.surface.widgets) {
        surface->AddWidget(surface, definition.id.c_str());
        Widget* widget = surface->GetWidgetByName(definition.id);
        if (!widget) continue;
        for (const Format2SurfacePrimitive& primitive : definition.primitives) {
            const string address = ReadAddress(primitive);
            if (address.empty()) continue;
            if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press") {
                surface->MessageGeneratorsByMessage_.insert(make_pair(address, make_unique<Format2OscPressMessageGenerator>(surface->csi_, widget, primitive)));
                widget->MarkOskPressInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Touch") {
                surface->MessageGeneratorsByMessage_.insert(make_pair(address, make_unique<Format2OscTouchMessageGenerator>(surface->csi_, widget, primitive)));
                widget->MarkOskTouchInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value") {
                surface->MessageGeneratorsByMessage_.insert(make_pair(address, make_unique<Format2OscValueMessageGenerator>(surface->csi_, widget, FindFormat2OscValueProfile(parsed.surface, primitive))));
                widget->MarkOskAbsoluteInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder") {
                surface->MessageGeneratorsByMessage_.insert(make_pair(address, make_unique<Format2OscEncoderMessageGenerator>(surface->csi_, surface, widget, parsed.surface, primitive)));
                widget->MarkOskRelativeInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value") {
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscValueFeedbackProcessor>(surface->csi_, surface, widget, address, ReadIntegerProperty(primitive, "EchoGuardMs", 0), primitive.encoding == Format2Encoding::OscInt, FindFormat2OscValueProfile(parsed.surface, primitive)));
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State") {
                const std::optional<double> offValue = ReadFormat2OscFiniteProperty(primitive, "OffValue");
                const std::optional<double> onValue = ReadFormat2OscFiniteProperty(primitive, "OnValue");
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscStateFeedbackProcessor>(surface->csi_, surface, widget, address, primitive.encoding == Format2Encoding::OscInt, offValue.value_or(0.0), onValue.value_or(1.0)));
                widget->MarkOskToggleFeedback();
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
