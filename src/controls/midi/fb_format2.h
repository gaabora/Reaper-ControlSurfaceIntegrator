#pragma once

#include "../format2_surface_document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

enum class Format2MidiValueCombine {
    Replace,
    Add,
    BitOr,
};

enum class Format2MidiRingStyleTarget {
    Status,
    Data1,
    Value,
};

enum class Format2MidiRingStyleCombine {
    Add,
    BitOr,
};

class Format2Midi7ValueFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<int> message_;
    int valueBase_ = 0;
    Format2MidiValueCombine combine_ = Format2MidiValueCombine::Replace;
    int echoGuardMs_ = 0;
    bool suppressWhileTouched_ = false;

    int Encode(double value) const {
        const int normalized = (int) (std::clamp(value, 0.0, 1.0) * 127.0);
        if (this->combine_ == Format2MidiValueCombine::Add) return std::clamp(this->valueBase_ + normalized, 0, 127);
        if (this->combine_ == Format2MidiValueCombine::BitOr) return this->valueBase_ | normalized;
        return normalized;
    }

public:
    Format2Midi7ValueFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& message, int valueBase, Format2MidiValueCombine combine, int echoGuardMs, bool suppressWhileTouched)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), valueBase_(valueBase), combine_(combine), echoGuardMs_(echoGuardMs), suppressWhileTouched_(suppressWhileTouched) {}
    virtual ~Format2Midi7ValueFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2Midi7ValueFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        this->ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (this->suppressWhileTouched_ && this->surface_->GetIsChannelTouched(this->widget_->GetChannelNumber())) return;
        if (this->echoGuardMs_ > 0 && GetTickCount() - this->widget_->GetLastIncomingMessageTime() < (DWORD) this->echoGuardMs_) return;
        if (value == this->lastDoubleValue_) return;
        this->ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        const int encoded = this->Encode(value);
        if (this->message_.size() == 1) this->SendMidiMessage(this->message_[0], encoded, 0);
        else this->SendMidiMessage(this->message_[0], this->message_[1], encoded);
    }
};

class Format2Midi7RingFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<int> message_;
    Format2RingProfile profile_;
    int valueBase_ = 0;
    Format2MidiValueCombine valueCombine_ = Format2MidiValueCombine::Replace;
    Format2MidiRingStyleTarget styleTarget_ = Format2MidiRingStyleTarget::Value;
    int styleShift_ = 0;
    Format2MidiRingStyleCombine styleCombine_ = Format2MidiRingStyleCombine::BitOr;
    std::array<int, 3> lastMessage_{};
    bool hasLastMessage_ = false;

    static Format2RingStyle ResolveStyle(const PropertyList& properties) {
        const char* value = properties.get_prop(PropertyType_RingStyle);
        if (value && IsSameString(value, "Fill")) return Format2RingStyle::Fill;
        if (value && IsSameString(value, "BoostCut")) return Format2RingStyle::BoostCut;
        if (value && IsSameString(value, "Spread")) return Format2RingStyle::Spread;
        return Format2RingStyle::Dot;
    }

    const Format2RingStyleEntry& ResolveStyleEntry(const PropertyList& properties) const {
        const Format2RingStyle style = ResolveStyle(properties);
        for (const Format2RingStyleEntry& entry : this->profile_.styles) if (entry.style == style) return entry;
        for (const Format2RingStyleEntry& entry : this->profile_.styles) if (entry.style == Format2RingStyle::Dot) return entry;
        return this->profile_.styles.front();
    }

    int CombineValue(int ringValue) const {
        if (this->valueCombine_ == Format2MidiValueCombine::Add) return this->valueBase_ + ringValue;
        if (this->valueCombine_ == Format2MidiValueCombine::BitOr) return this->valueBase_ | ringValue;
        return ringValue;
    }

    int CombineStyle(int targetValue, int styleValue) const {
        if (this->styleCombine_ == Format2MidiRingStyleCombine::Add) return targetValue + styleValue;
        return targetValue | styleValue;
    }

    std::array<int, 3> Encode(const PropertyList& properties, double value) const {
        const Format2RingStyleEntry& entry = this->ResolveStyleEntry(properties);
        const double scaled = std::clamp(value, 0.0, 1.0) * (entry.steps - 1);
        const int position = this->profile_.quantize == Format2Quantize::Round ? (int) std::round(scaled) : (int) std::floor(scaled);
        const int ringValue = this->CombineValue(this->profile_.valueOffset + position);
        const int styleValue = entry.code << this->styleShift_;
        std::array<int, 3> result = { this->message_[0], this->message_.size() == 2 ? this->message_[1] : ringValue, this->message_.size() == 2 ? ringValue : 0 };
        if (this->styleTarget_ == Format2MidiRingStyleTarget::Status) result[0] = this->CombineStyle(result[0], styleValue);
        else if (this->styleTarget_ == Format2MidiRingStyleTarget::Data1) result[1] = this->CombineStyle(result[1], styleValue);
        else if (this->message_.size() == 2) result[2] = this->CombineStyle(result[2], styleValue);
        else result[1] = this->CombineStyle(result[1], styleValue);
        return result;
    }

    void Send(const std::array<int, 3>& message) { this->SendMidiMessage(message[0], message[1], message[2]); }

public:
    Format2Midi7RingFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& message, const Format2RingProfile& profile, int valueBase, Format2MidiValueCombine valueCombine, Format2MidiRingStyleTarget styleTarget, int styleShift, Format2MidiRingStyleCombine styleCombine)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), profile_(profile), valueBase_(valueBase), valueCombine_(valueCombine), styleTarget_(styleTarget), styleShift_(styleShift), styleCombine_(styleCombine) {}
    virtual ~Format2Midi7RingFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2Midi7RingFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        this->ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const std::array<int, 3> message = this->Encode(properties, value);
        if (this->hasLastMessage_ && message == this->lastMessage_) return;
        this->lastDoubleValue_ = value;
        this->lastMessage_ = message;
        this->hasLastMessage_ = true;
        this->Send(message);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastMessage_ = this->Encode(properties, value);
        this->hasLastMessage_ = true;
        this->Send(this->lastMessage_);
    }
};

class Format2MidiRgbFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    bool hasEnable_ = false;
    std::array<int, 3> enable_{};
    std::array<int, 2> red_{};
    std::array<int, 2> green_{};
    std::array<int, 2> blue_{};
    bool hasStateBrightness_ = false;
    bool active_ = false;
    float inactiveBrightness_ = 1.0f;
    float activeBrightness_ = 1.0f;
    rgba_color sourceColor_;

    void SendResolvedColor() {
        const float brightness = this->hasStateBrightness_ ? (this->active_ ? this->activeBrightness_ : this->inactiveBrightness_) : 1.0f;
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(this->sourceColor_, 127, brightness);
        if (this->hasEnable_) this->SendMidiMessage(this->enable_[0], this->enable_[1], this->enable_[2]);
        this->SendMidiMessage(this->red_[0], this->red_[1], deviceColor.r);
        this->SendMidiMessage(this->green_[0], this->green_[1], deviceColor.g);
        this->SendMidiMessage(this->blue_[0], this->blue_[1], deviceColor.b);
    }

public:
    Format2MidiRgbFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const std::array<int, 2>& red, const std::array<int, 2>& green, const std::array<int, 2>& blue, const vector<int>& enable, bool hasStateBrightness, float inactiveBrightness, float activeBrightness)
        : Midi_FeedbackProcessor(csi, surface, widget), red_(red), green_(green), blue_(blue), hasStateBrightness_(hasStateBrightness), inactiveBrightness_(inactiveBrightness), activeBrightness_(activeBrightness) {
        if (enable.size() == 3) {
            this->hasEnable_ = true;
            this->enable_ = { enable[0], enable[1], enable[2] };
        }
    }

    virtual ~Format2MidiRgbFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiRgbFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->active_ = false;
        this->sourceColor_ = rgba_color();
        this->lastColor_ = this->sourceColor_;
        this->SendResolvedColor();
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (!this->hasStateBrightness_) return;
        const bool active = value != 0.0;
        if (active == this->active_) return;
        this->active_ = active;
        this->lastDoubleValue_ = value;
        this->SendResolvedColor();
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (color == this->lastColor_) return;
        this->sourceColor_ = color;
        this->lastColor_ = color;
        this->SendResolvedColor();
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        this->sourceColor_ = color;
        this->lastColor_ = color;
        this->SendResolvedColor();
    }
};

class Format2MidiPaletteFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    std::array<int, 2> message_{};
    Format2ColorProfile profile_;
    bool hasCompanion_ = false;
    std::array<int, 3> companion_{};
    bool companionBefore_ = false;

    static rgba_color UnpackColor(std::uint32_t color) {
        rgba_color result;
        result.r = (color >> 16) & 0xFF;
        result.g = (color >> 8) & 0xFF;
        result.b = color & 0xFF;
        return result;
    }

    static double Hue(const rgba_color& color) {
        const double red = color.r / 255.0;
        const double green = color.g / 255.0;
        const double blue = color.b / 255.0;
        const double maximum = (std::max)(red, (std::max)(green, blue));
        const double minimum = (std::min)(red, (std::min)(green, blue));
        const double difference = maximum - minimum;
        if (difference == 0.0) return 0.0;
        double hue = maximum == red ? 60.0 * std::fmod((green - blue) / difference, 6.0) : maximum == green ? 60.0 * ((blue - red) / difference + 2.0) : 60.0 * ((red - green) / difference + 4.0);
        if (hue < 0.0) hue += 360.0;
        return hue;
    }

    static bool ContainsHue(const Format2HueRange& range, double hue) {
        if (range.minimum < range.maximum) return hue >= range.minimum && hue < range.maximum;
        return hue >= range.minimum || hue < range.maximum;
    }

    int ResolveValue(const rgba_color& sourceColor) const {
        const rgba_color color = this->surface_->GetDeviceFeedbackColor(sourceColor, 255);
        if (this->profile_.match == Format2ColorMatch::HueRanges) {
            const double maximum = (std::max)(color.r, (std::max)(color.g, color.b)) / 255.0;
            const double minimum = (std::min)(color.r, (std::min)(color.g, color.b)) / 255.0;
            const double saturation = maximum == 0.0 ? 0.0 : (maximum - minimum) / maximum;
            if (maximum <= this->profile_.minimumBrightness.value_or(0.0) || saturation <= this->profile_.maximumNeutralSaturation.value_or(0.0)) return this->profile_.defaultValue;
            const double hue = Hue(color);
            for (const Format2HueRange& range : this->profile_.hueRanges) if (ContainsHue(range, hue)) return range.value;
            return this->profile_.defaultValue;
        }

        const std::uint32_t packed = ((std::uint32_t) color.r << 16) | ((std::uint32_t) color.g << 8) | (std::uint32_t) color.b;
        if (this->profile_.match == Format2ColorMatch::Exact) {
            for (const Format2ColorProfileEntry& entry : this->profile_.entries) if (entry.color == packed) return entry.value;
            return this->profile_.defaultValue;
        }

        int selectedValue = this->profile_.defaultValue;
        long long selectedDistance = (std::numeric_limits<long long>::max)();
        for (const Format2ColorProfileEntry& entry : this->profile_.entries) {
            const rgba_color entryColor = UnpackColor(entry.color);
            const long long redDifference = color.r - entryColor.r;
            const long long greenDifference = color.g - entryColor.g;
            const long long blueDifference = color.b - entryColor.b;
            const long long distance = redDifference * redDifference + greenDifference * greenDifference + blueDifference * blueDifference;
            if (distance < selectedDistance) {
                selectedDistance = distance;
                selectedValue = entry.value;
            }
        }
        return selectedValue;
    }

    void Send(int value) {
        if (this->hasCompanion_ && this->companionBefore_) this->SendMidiMessage(this->companion_[0], this->companion_[1], this->companion_[2]);
        this->SendMidiMessage(this->message_[0], this->message_[1], value);
        if (this->hasCompanion_ && !this->companionBefore_) this->SendMidiMessage(this->companion_[0], this->companion_[1], this->companion_[2]);
    }

public:
    Format2MidiPaletteFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const std::array<int, 2>& message, const Format2ColorProfile& profile, const vector<int>& companion, bool companionBefore)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), profile_(profile), companionBefore_(companionBefore) {
        if (companion.size() == 3) {
            this->hasCompanion_ = true;
            this->companion_ = { companion[0], companion[1], companion[2] };
        }
    }

    virtual ~Format2MidiPaletteFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiPaletteFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->lastColor_ = rgba_color();
        this->Send(this->ResolveValue(this->lastColor_));
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (color == this->lastColor_) return;
        this->ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        this->lastColor_ = color;
        this->Send(this->ResolveValue(color));
    }
};

class Format2Midi7BarFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<int> message_;
    std::array<int, 2> styleMessage_{};
    Format2BarProfile profile_;
    int valueBase_ = 0;
    Format2MidiValueCombine combine_ = Format2MidiValueCombine::Replace;
    int lastValue_ = 0;
    int lastStyle_ = 0;
    bool hasLastValue_ = false;
    bool hasLastStyle_ = false;

    int EncodeValue(double value) const {
        const int normalized = (int) (std::clamp(value, 0.0, 1.0) * 127.0);
        if (this->combine_ == Format2MidiValueCombine::Add) return std::clamp(this->valueBase_ + normalized, 0, 127);
        if (this->combine_ == Format2MidiValueCombine::BitOr) return this->valueBase_ | normalized;
        return normalized;
    }

    Format2BarStyle ResolveStyle(const PropertyList& properties) const {
        const char* value = properties.get_prop(PropertyType_BarStyle);
        if (value && IsSameString(value, "Normal")) return Format2BarStyle::Normal;
        if (value && IsSameString(value, "Bipolar")) return Format2BarStyle::Bipolar;
        if (value && IsSameString(value, "Fill")) return Format2BarStyle::Fill;
        if (value && IsSameString(value, "Spread")) return Format2BarStyle::Spread;
        if (value && IsSameString(value, "Off")) return Format2BarStyle::Off;
        return this->profile_.defaultStyle;
    }

    int ResolveStyleCode(const PropertyList& properties) const {
        const Format2BarStyle style = this->ResolveStyle(properties);
        return this->StyleCode(style);
    }

    int StyleCode(Format2BarStyle style) const {
        for (const Format2BarStyleEntry& entry : this->profile_.styles) if (entry.style == style) return entry.code;
        for (const Format2BarStyleEntry& entry : this->profile_.styles) if (entry.style == this->profile_.defaultStyle) return entry.code;
        return 0;
    }

    void SendValue(int value) {
        if (this->message_.size() == 1) this->SendMidiMessage(this->message_[0], value, 0);
        else this->SendMidiMessage(this->message_[0], this->message_[1], value);
    }

    void SendStyle(int style) { this->SendMidiMessage(this->styleMessage_[0], this->styleMessage_[1], style); }

public:
    Format2Midi7BarFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& message, const std::array<int, 2>& styleMessage, const Format2BarProfile& profile, int valueBase, Format2MidiValueCombine combine)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), styleMessage_(styleMessage), profile_(profile), valueBase_(valueBase), combine_(combine) {}
    virtual ~Format2Midi7BarFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2Midi7BarFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->lastDoubleValue_ = 0.0;
        this->lastValue_ = this->EncodeValue(0.0);
        this->lastStyle_ = this->StyleCode(Format2BarStyle::Off);
        this->hasLastValue_ = true;
        this->hasLastStyle_ = true;
        this->SendValue(this->lastValue_);
        this->SendStyle(this->lastStyle_);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        const int encodedValue = this->EncodeValue(value);
        const int encodedStyle = this->ResolveStyleCode(properties);
        if (!this->hasLastValue_ || encodedValue != this->lastValue_) {
            this->lastValue_ = encodedValue;
            this->hasLastValue_ = true;
            this->SendValue(encodedValue);
        }
        if (!this->hasLastStyle_ || encodedStyle != this->lastStyle_) {
            this->lastStyle_ = encodedStyle;
            this->hasLastStyle_ = true;
            this->SendStyle(encodedStyle);
        }
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastValue_ = this->EncodeValue(value);
        this->lastStyle_ = this->ResolveStyleCode(properties);
        this->hasLastValue_ = true;
        this->hasLastStyle_ = true;
        this->SendValue(this->lastValue_);
        this->SendStyle(this->lastStyle_);
    }
};
