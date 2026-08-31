#pragma once

#include "../format2_surface_document.h"

#include <algorithm>
#include <array>
#include <cmath>

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
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(this->sourceColor_, 255, brightness);
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
