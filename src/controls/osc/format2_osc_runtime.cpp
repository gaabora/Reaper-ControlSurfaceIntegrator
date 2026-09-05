#include "../integrator.h"
#include "../format2_color_profile.h"
#include "../format2_meter_profile.h"
#include "../format2_surface_document.h"
#include "../format2_text_profile.h"
#include "../format2_value_profile.h"
#include "../format2_value_validation.h"
#include "format2_osc_runtime.h"
#include "osc_surface.h"

#include <algorithm>
#include <cmath>

static const Format2PropertySyntax* FindFormat2OscProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    for (const Format2PropertySyntax& property : primitive.properties) if (property.name == name) return &property;
    return nullptr;
}

static const Format2PropertySyntax* FindFormat2OscNodeProperty(const Format2SyntaxNode& node, const string& name) {
    for (const Format2PropertySyntax& property : node.properties) if (property.name == name) return &property;
    return nullptr;
}

static std::optional<double> ReadFormat2OscFiniteProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    const Format2PropertySyntax* property = FindFormat2OscProperty(primitive, name);
    double value = 0.0;
    if (!property || property->value.list || !ParseFormat2FiniteScalar(property->value.scalar, value)) return std::nullopt;
    return value;
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
        const Format2ValueProfile* profile = FindFormat2ValueProfile(document, primitive);
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
    bool const suppressWhileTouched_;
    std::optional<Format2ValueProfile> profile_;

public:
    Format2OscValueFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, int echoGuardMs, bool integer, bool suppressWhileTouched, const Format2ValueProfile* profile) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), echoGuardMs_(echoGuardMs), integer_(integer), suppressWhileTouched_(suppressWhileTouched), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}

    virtual void ForceValue(const PropertyList& properties, double value) override {
        if (this->suppressWhileTouched_ && this->surface_->GetIsChannelTouched(this->widget_->GetChannelNumber())) return;
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
    std::optional<Format2TextProfile> profile_;

public:
    Format2OscStringFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, const Format2TextProfile* profile) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), profile_(profile ? std::optional<Format2TextProfile>(*profile) : std::nullopt) {}

    virtual void ForceValue(const PropertyList& properties, const char* const& value) override {
        this->lastStringValue_ = value;
        char restrictedText[MEDBUF];
        const char* restricted = this->widget_->GetSurface()->GetRestrictedLengthText(value, restrictedText, sizeof(restrictedText));
        const string encoded = this->profile_ ? EncodeFormat2TextProfile(*this->profile_, restricted) : restricted;
        this->surface_->SendOSCMessage(this->address_.c_str(), encoded.c_str());
    }

    virtual void ForceClear() override {
        const string clearText = this->profile_ ? EncodeFormat2TextProfile(*this->profile_, this->profile_->clearText.c_str()) : "";
        this->lastStringValue_ = clearText;
        this->surface_->SendOSCMessage(this->address_.c_str(), clearText.c_str());
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

class Format2OscIntColorFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;
    Format2ColorProfile profile_;

    void SendColor(const rgba_color& color) {
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(color, 255);
        this->surface_->SendOSCMessage(this->address_.c_str(), ResolveFormat2ColorProfileValue(this->profile_, deviceColor));
    }

public:
    Format2OscIntColorFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, const Format2ColorProfile& profile) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), profile_(profile) {}

    virtual void ForceClear() override {
        this->lastColor_ = rgba_color();
        this->SendColor(this->lastColor_);
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (this->lastColor_ == color) return;
        this->ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        this->lastColor_ = color;
        this->SendColor(color);
    }

    virtual void ForceUpdateTrackColors() override { this->ForceColorValue(this->surface_->GetTrackColorForChannel(this->widget_->GetChannelNumber())); }
};

class Format2OscRingFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const valueAddress_;
    string const styleAddress_;
    bool const integer_;
    Format2RingProfile profile_;
    int lastValue_ = 0;
    int lastStyle_ = 0;
    bool hasLastValue_ = false;
    bool hasLastStyle_ = false;

    const Format2RingStyleEntry& ResolveStyleEntry(const PropertyList& properties) const {
        Format2RingStyle style = Format2RingStyle::Dot;
        if (properties.GetFeedbackShape() == PropertyList::FeedbackShape::Level) style = Format2RingStyle::Fill;
        else if (properties.GetFeedbackShape() == PropertyList::FeedbackShape::Centered) style = Format2RingStyle::BoostCut;
        else if (properties.GetFeedbackShape() == PropertyList::FeedbackShape::Spread) style = Format2RingStyle::Spread;
        const char* value = properties.get_prop(PropertyType_RingStyle);
        if (value && IsSameString(value, "Fill")) style = Format2RingStyle::Fill;
        else if (value && IsSameString(value, "BoostCut")) style = Format2RingStyle::BoostCut;
        else if (value && IsSameString(value, "Spread")) style = Format2RingStyle::Spread;
        for (const Format2RingStyleEntry& entry : this->profile_.styles) if (entry.style == style) return entry;
        for (const Format2RingStyleEntry& entry : this->profile_.styles) if (entry.style == Format2RingStyle::Dot) return entry;
        return this->profile_.styles.front();
    }

    void Send(const string& address, int value) {
        if (this->integer_) this->surface_->SendOSCMessage(address.c_str(), value);
        else this->surface_->SendOSCMessage(address.c_str(), (double) value);
    }

    void Update(const PropertyList& properties, double value, bool force) {
        const Format2RingStyleEntry& entry = this->ResolveStyleEntry(properties);
        const double scaled = std::clamp(value, 0.0, 1.0) * (entry.steps - 1);
        const int position = this->profile_.quantize == Format2Quantize::Round ? (int) std::round(scaled) : (int) std::floor(scaled);
        const int encodedValue = this->profile_.valueOffset + position;
        if (force || !this->hasLastValue_ || encodedValue != this->lastValue_) this->Send(this->valueAddress_, encodedValue);
        if (!this->styleAddress_.empty() && (force || !this->hasLastStyle_ || entry.code != this->lastStyle_)) this->Send(this->styleAddress_, entry.code);
        this->lastDoubleValue_ = value;
        this->lastValue_ = encodedValue;
        this->lastStyle_ = entry.code;
        this->hasLastValue_ = true;
        this->hasLastStyle_ = true;
    }

public:
    Format2OscRingFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& valueAddress, const string& styleAddress, bool integer, const Format2RingProfile& profile) : FeedbackProcessor(csi, widget), surface_(surface), valueAddress_(valueAddress), styleAddress_(styleAddress), integer_(integer), profile_(profile) {}

    virtual void ForceClear() override { this->Update(PropertyList(), 0.0, true); }
    virtual void SetValue(const PropertyList& properties, double value) override { this->Update(properties, value, false); }
    virtual void ForceValue(const PropertyList& properties, double value) override { this->Update(properties, value, true); }
};

class Format2OscMeterFeedbackProcessor : public FeedbackProcessor
{
private:
    OSC_ControlSurface* const surface_;
    string const address_;
    bool const integer_;
    Format2MeterProfile profile_;
    bool continuous_ = false;
    DWORD refreshIntervalMs_ = 0;
    DWORD lastSendTime_ = 0;
    int lastEncodedValue_ = 0;
    bool hasLastValue_ = false;

    void Send(int value) {
        if (this->integer_) this->surface_->SendOSCMessage(this->address_.c_str(), value);
        else this->surface_->SendOSCMessage(this->address_.c_str(), (double) value);
        this->lastSendTime_ = GetTickCount();
    }

public:
    Format2OscMeterFeedbackProcessor(CSurfIntegrator* const csi, OSC_ControlSurface* surface, Widget* widget, const string& address, bool integer, const Format2MeterProfile& profile, bool continuous, int refreshIntervalMs) : FeedbackProcessor(csi, widget), surface_(surface), address_(address), integer_(integer), profile_(profile), continuous_(continuous), refreshIntervalMs_((DWORD) refreshIntervalMs) {}

    virtual void ForceClear() override {
        this->lastDoubleValue_ = 0.0;
        this->lastEncodedValue_ = ClearFormat2MeterProfileValue(this->profile_);
        this->hasLastValue_ = true;
        this->Send(this->lastEncodedValue_);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const int encoded = EncodeFormat2MeterProfile(this->profile_, value);
        const DWORD now = GetTickCount();
        if (this->continuous_ && this->hasLastValue_ && now - this->lastSendTime_ < this->refreshIntervalMs_) return;
        if (!this->continuous_ && this->hasLastValue_ && encoded == this->lastEncodedValue_) return;
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = encoded;
        this->hasLastValue_ = true;
        this->Send(encoded);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = EncodeFormat2MeterProfile(this->profile_, value);
        this->hasLastValue_ = true;
        this->Send(this->lastEncodedValue_);
    }
};

const Format2PropertySyntax* Format2OscRuntimeLoader::FindProperty(const Format2SurfacePrimitive& primitive, const string& name) {
    return FindFormat2OscProperty(primitive, name);
}

string Format2OscRuntimeLoader::ReadAddress(const Format2SurfacePrimitive& primitive) {
    const Format2PropertySyntax* address = FindProperty(primitive, primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Ring" ? "ValueAddress" : "Address");
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
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text" && primitive.encoding == Format2Encoding::OscString) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color" && primitive.encoding == Format2Encoding::OscInt) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Ring" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
    if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Meter" && (primitive.encoding == Format2Encoding::OscFloat || primitive.encoding == Format2Encoding::OscInt)) return true;
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
        if (definition.channel) widget->SetChannelNumber(*definition.channel);
        for (const Format2SurfacePrimitive& primitive : definition.primitives) {
            const string address = ReadAddress(primitive);
            if (address.empty()) continue;
            if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Press") {
                surface->MessageGeneratorsByMessage_.insert(std::make_pair(address, make_unique<Format2OscPressMessageGenerator>(surface->csi_, widget, primitive)));
                widget->MarkOskPressInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Touch") {
                surface->MessageGeneratorsByMessage_.insert(std::make_pair(address, make_unique<Format2OscTouchMessageGenerator>(surface->csi_, widget, primitive)));
                widget->MarkOskTouchInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Value") {
                surface->MessageGeneratorsByMessage_.insert(std::make_pair(address, make_unique<Format2OscValueMessageGenerator>(surface->csi_, widget, FindFormat2ValueProfile(parsed.surface, primitive))));
                widget->MarkOskAbsoluteInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Input && primitive.type == "Encoder") {
                surface->MessageGeneratorsByMessage_.insert(std::make_pair(address, make_unique<Format2OscEncoderMessageGenerator>(surface->csi_, surface, widget, parsed.surface, primitive)));
                widget->MarkOskRelativeInput();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Value") {
                const Format2PropertySyntax* suppressProperty = FindProperty(primitive, "SuppressWhileTouched");
                const bool suppressWhileTouched = suppressProperty && !suppressProperty->value.list && suppressProperty->value.scalar.text == "true";
                unique_ptr<Format2OscValueFeedbackProcessor> processor = make_unique<Format2OscValueFeedbackProcessor>(surface->csi_, surface, widget, address, ReadIntegerProperty(primitive, "EchoGuardMs", 0), primitive.encoding == Format2Encoding::OscInt, suppressWhileTouched, FindFormat2ValueProfile(parsed.surface, primitive));
                const std::optional<double> initialValue = ReadFormat2OscFiniteProperty(primitive, "InitialValue");
                if (initialValue) surface->AddInitialFeedbackValue(processor.get(), *initialValue);
                widget->GetFeedbackProcessors().push_back(std::move(processor));
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "State") {
                const std::optional<double> offValue = ReadFormat2OscFiniteProperty(primitive, "OffValue");
                const std::optional<double> onValue = ReadFormat2OscFiniteProperty(primitive, "OnValue");
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscStateFeedbackProcessor>(surface->csi_, surface, widget, address, primitive.encoding == Format2Encoding::OscInt, offValue.value_or(0.0), onValue.value_or(1.0)));
                widget->MarkOskToggleFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Text") {
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscStringFeedbackProcessor>(surface->csi_, surface, widget, address, FindFormat2TextProfile(parsed.surface, primitive)));
                widget->MarkOskTextFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Color") {
                if (primitive.encoding == Format2Encoding::OscInt) {
                    const Format2ColorProfile* profile = FindFormat2ColorProfile(parsed.surface, primitive);
                    if (!profile) continue;
                    widget->GetFeedbackProcessors().push_back(make_unique<Format2OscIntColorFeedbackProcessor>(surface->csi_, surface, widget, address, *profile));
                    const Format2PropertySyntax* trackColor = FindProperty(primitive, "TrackColor");
                    if (trackColor && !trackColor->value.list && trackColor->value.scalar.text == "true") surface->AddTrackColorFeedbackProcessor(widget->GetFeedbackProcessors().back().get());
                } else widget->GetFeedbackProcessors().push_back(make_unique<Format2OscHexRgbaFeedbackProcessor>(surface->csi_, surface, widget, address));
                widget->MarkOskColorFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Ring") {
                const Format2PropertySyntax* profileProperty = FindProperty(primitive, "RingProfile");
                if (!profileProperty || profileProperty->value.list) continue;
                const Format2RingProfile* profile = nullptr;
                for (const Format2RingProfile& candidate : parsed.surface.ringProfiles) if (candidate.id == profileProperty->value.scalar.text) { profile = &candidate; break; }
                if (!profile) continue;
                const Format2PropertySyntax* styleAddressProperty = FindProperty(primitive, "StyleAddress");
                const string styleAddress = styleAddressProperty && !styleAddressProperty->value.list ? styleAddressProperty->value.scalar.text : string{};
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscRingFeedbackProcessor>(surface->csi_, surface, widget, address, styleAddress, primitive.encoding == Format2Encoding::OscInt, *profile));
                widget->MarkOskValueFeedback();
            } else if (primitive.direction == Format2PrimitiveDirection::Feedback && primitive.type == "Meter") {
                const Format2MeterProfile* profile = FindFormat2MeterProfile(parsed.surface, primitive);
                if (!profile) continue;
                const Format2PropertySyntax* refresh = FindProperty(primitive, "Refresh");
                const bool continuous = refresh && !refresh->value.list && refresh->value.scalar.text == "Continuous";
                widget->GetFeedbackProcessors().push_back(make_unique<Format2OscMeterFeedbackProcessor>(surface->csi_, surface, widget, address, primitive.encoding == Format2Encoding::OscInt, *profile, continuous, ReadIntegerProperty(primitive, "RefreshIntervalMs", 0)));
                widget->MarkOskMeterFeedback();
                widget->MarkOskValueFeedback();
            }
        }
    }
    if (parsed.surface.oskLayout) surface->ApplyFormat2OSKLayout(filePath, *parsed.surface.oskLayout);
    return Format2OscRuntimeLoadResult::Loaded;
}
