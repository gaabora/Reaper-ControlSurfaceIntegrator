#pragma once

#include "../format2_color_profile.h"
#include "../format2_meter_profile.h"
#include "../format2_surface_document.h"
#include "../format2_text_profile.h"
#include "../format2_value_profile.h"

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

class Format2Midi14ValueFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int status_;
    bool suppressWhileTouched_;
    int echoGuardMs_;
    int lastEncodedValue_ = -1;
    std::optional<Format2ValueProfile> profile_;

    int Encode(double value) const {
        const double normalized = this->profile_ ? EncodeFormat2ValueProfile(*this->profile_, value) : value;
        return (int) (std::clamp(normalized, 0.0, 1.0) * 16383.0);
    }

public:
    Format2Midi14ValueFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int status, bool suppressWhileTouched, int echoGuardMs, const Format2ValueProfile* profile) : Midi_FeedbackProcessor(csi, surface, widget), status_(status), suppressWhileTouched_(suppressWhileTouched), echoGuardMs_(echoGuardMs), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}
    virtual ~Format2Midi14ValueFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2Midi14ValueFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        this->ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (this->suppressWhileTouched_ && this->surface_->GetIsChannelTouched(this->widget_->GetChannelNumber())) return;
        if (this->echoGuardMs_ > 0 && GetTickCount() - this->widget_->GetLastIncomingMessageTime() < (DWORD) this->echoGuardMs_) return;
        const int encoded = this->Encode(value);
        if (encoded == this->lastEncodedValue_) return;
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = encoded;
        this->SendMidiMessage(this->status_, encoded & 0x7F, (encoded >> 7) & 0x7F);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = this->Encode(value);
        this->ForceMidiMessage(this->status_, this->lastEncodedValue_ & 0x7F, (this->lastEncodedValue_ >> 7) & 0x7F);
    }
};

class Format2MidiSplitValueFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    std::array<int, 2> msbMessage_;
    std::array<int, 2> lsbMessage_;
    Format2MidiSplitPart commitPart_;
    int maximumValue_;
    int echoGuardMs_ = 0;
    bool suppressWhileTouched_ = false;
    int lastEncodedValue_ = -1;
    std::optional<Format2ValueProfile> profile_;

    int Encode(double value) const {
        const double normalized = this->profile_ ? EncodeFormat2ValueProfile(*this->profile_, value) : value;
        return (int) (std::clamp(normalized, 0.0, 1.0) * this->maximumValue_);
    }

    void SendPart(const std::array<int, 2>& message, int value) {
        this->SendMidiMessage(message[0], message[1], value);
    }

    void SendEncodedValue(int value) {
        const int msb = (value >> 7) & 0x7F;
        const int lsb = value & 0x7F;
        if (this->commitPart_ == Format2MidiSplitPart::Lsb) {
            this->SendPart(this->msbMessage_, msb);
            this->SendPart(this->lsbMessage_, lsb);
        } else {
            this->SendPart(this->lsbMessage_, lsb);
            this->SendPart(this->msbMessage_, msb);
        }
    }

public:
    Format2MidiSplitValueFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const std::array<int, 2>& msbMessage, const std::array<int, 2>& lsbMessage, int bits, Format2MidiSplitPart commitPart, int echoGuardMs, bool suppressWhileTouched, const Format2ValueProfile* profile)
        : Midi_FeedbackProcessor(csi, surface, widget), msbMessage_(msbMessage), lsbMessage_(lsbMessage), commitPart_(commitPart), maximumValue_((1 << bits) - 1), echoGuardMs_(echoGuardMs), suppressWhileTouched_(suppressWhileTouched), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}
    virtual ~Format2MidiSplitValueFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSplitValueFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        this->ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (this->suppressWhileTouched_ && this->surface_->GetIsChannelTouched(this->widget_->GetChannelNumber())) return;
        if (this->echoGuardMs_ > 0 && GetTickCount() - this->widget_->GetLastIncomingMessageTime() < (DWORD) this->echoGuardMs_) return;
        const int encoded = this->Encode(value);
        if (encoded == this->lastEncodedValue_) return;
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = encoded;
        this->SendEncodedValue(encoded);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = this->Encode(value);
        this->SendEncodedValue(this->lastEncodedValue_);
    }
};

class Format2Midi7ValueFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<int> message_;
    int valueBase_ = 0;
    Format2MidiValueCombine combine_ = Format2MidiValueCombine::Replace;
    int echoGuardMs_ = 0;
    bool suppressWhileTouched_ = false;
    std::optional<Format2ValueProfile> profile_;

    int Encode(double value) const {
        const double profileValue = this->profile_ ? EncodeFormat2ValueProfile(*this->profile_, value) : value;
        const int normalized = (int) (std::clamp(profileValue, 0.0, 1.0) * 127.0);
        if (this->combine_ == Format2MidiValueCombine::Add) return std::clamp(this->valueBase_ + normalized, 0, 127);
        if (this->combine_ == Format2MidiValueCombine::BitOr) return this->valueBase_ | normalized;
        return normalized;
    }

public:
    Format2Midi7ValueFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& message, int valueBase, Format2MidiValueCombine combine, int echoGuardMs, bool suppressWhileTouched, const Format2ValueProfile* profile)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), valueBase_(valueBase), combine_(combine), echoGuardMs_(echoGuardMs), suppressWhileTouched_(suppressWhileTouched), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}
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

class Format2MidiExactStateFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<int> on_;
    vector<int> off_;
    vector<int> clear_;
    vector<double> activeValues_;
    bool lastState_ = false;
    bool hasLastState_ = false;

    bool IsActive(double value) const {
        if (this->activeValues_.empty()) return value != ActionContext::BUTTON_RELEASE_MESSAGE_VALUE;
        return std::find(this->activeValues_.begin(), this->activeValues_.end(), value) != this->activeValues_.end();
    }

    void SendMessage(const vector<int>& message, bool force) {
        if (force) this->ForceMidiMessage(message[0], message[1], message[2]);
        else this->SendMidiMessage(message[0], message[1], message[2]);
    }

public:
    Format2MidiExactStateFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& on, const vector<int>& off, const vector<int>& clear, const vector<double>& activeValues)
        : Midi_FeedbackProcessor(csi, surface, widget), on_(on), off_(off), clear_(clear), activeValues_(activeValues) {}
    virtual ~Format2MidiExactStateFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiExactStateFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->hasLastState_ = false;
        this->SendMessage(this->clear_.empty() ? this->off_ : this->clear_, true);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const bool state = this->IsActive(value);
        if (this->hasLastState_ && state == this->lastState_) return;
        this->lastDoubleValue_ = value;
        this->lastState_ = state;
        this->hasLastState_ = true;
        this->SendMessage(state ? this->on_ : this->off_, false);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        const bool state = this->IsActive(value);
        this->lastDoubleValue_ = value;
        this->lastState_ = state;
        this->hasLastState_ = true;
        this->SendMessage(state ? this->on_ : this->off_, true);
    }
};

class Format2MidiCharactersTextFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int status_;
    int startData_;
    bool ascending_;
    Format2TextProfile profile_;
    string lastEncoded_;

    string EncodeText(const char* inputText) const {
        char restrictedText[MEDBUF];
        const char* source = this->surface_->GetRestrictedLengthText(inputText, restrictedText, sizeof(restrictedText));
        return EncodeFormat2TextProfile(this->profile_, source);
    }

    void Update(const char* inputText, bool force) {
        const string encoded = this->EncodeText(inputText);
        for (size_t characterIdx = 0; characterIdx < encoded.size(); ++characterIdx) {
            if (!force && characterIdx < this->lastEncoded_.size() && encoded[characterIdx] == this->lastEncoded_[characterIdx]) continue;
            const int data = this->ascending_ ? this->startData_ + (int) characterIdx : this->startData_ - (int) characterIdx;
            if (force) this->ForceMidiMessage(this->status_, data, (unsigned char) encoded[characterIdx]);
            else this->SendMidiMessage(this->status_, data, (unsigned char) encoded[characterIdx]);
        }
        this->lastEncoded_ = encoded;
    }

public:
    Format2MidiCharactersTextFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int status, int startData, bool ascending, const Format2TextProfile& profile)
        : Midi_FeedbackProcessor(csi, surface, widget), status_(status), startData_(startData), ascending_(ascending), profile_(profile) {}
    virtual ~Format2MidiCharactersTextFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiCharactersTextFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->lastStringValue_ = this->profile_.clearText;
        this->Update(this->profile_.clearText.c_str(), true);
    }

    virtual void SetValue(const PropertyList& properties, const char* const& inputText) override {
        this->lastStringValue_ = inputText;
        this->Update(inputText, false);
    }

    virtual void ForceValue(const PropertyList& properties, const char* const& inputText) override {
        this->lastStringValue_ = inputText;
        this->Update(inputText, true);
    }
};

class Format2MidiSysExStateFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExStatePayloadItem> payload_;
    vector<int> lastPayload_;

    static vector<rgba_color> ReadStateColors(const PropertyList& properties) {
        vector<rgba_color> colors;
        const char* propertyValue = properties.get_prop(PropertyType_StateColors);
        if (!propertyValue) return colors;
        string source = propertyValue;
        ReplaceAllWith(source, "[", "");
        ReplaceAllWith(source, "]", "");
        ReplaceAllWith(source, "\"", "");
        size_t start = 0;
        while (start <= source.size()) {
            const size_t separator = source.find(',', start);
            string token = source.substr(start, separator == string::npos ? string::npos : separator - start);
            token.erase(0, token.find_first_not_of(" \t"));
            const size_t lastText = token.find_last_not_of(" \t");
            if (lastText != string::npos) token.erase(lastText + 1);
            if (!token.empty()) {
                rgba_color color;
                GetColorValue(token.c_str(), color);
                colors.push_back(color);
            }
            if (separator == string::npos) break;
            start = separator + 1;
        }
        return colors;
    }

    vector<int> ResolvePayload(const PropertyList& properties, int state) const {
        const vector<rgba_color> colors = ReadStateColors(properties);
        const rgba_color sourceColor = colors.empty() ? rgba_color() : colors[(std::min)((size_t) state, colors.size() - 1)];
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(sourceColor, 127);
        vector<int> result;
        for (const Format2MidiSysExStatePayloadItem& item : this->payload_) {
            if (item.field == Format2MidiSysExStatePayloadField::Byte) result.push_back(item.byte);
            else if (item.field == Format2MidiSysExStatePayloadField::State) result.push_back(state ? 0x7F : 0x00);
            else if (item.field == Format2MidiSysExStatePayloadField::Red) result.push_back(deviceColor.r);
            else if (item.field == Format2MidiSysExStatePayloadField::Green) result.push_back(deviceColor.g);
            else if (item.field == Format2MidiSysExStatePayloadField::Blue) result.push_back(deviceColor.b);
        }
        return result;
    }

    void SendPayload(const vector<int>& payload) {
        SysExBuilder builder;
        builder.begin();
        for (int byte : payload) builder.add((unsigned char) byte);
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

public:
    Format2MidiSysExStateFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExStatePayloadItem>& payload)
        : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload) {}
    virtual ~Format2MidiSysExStateFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExStateFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        this->ForceValue(properties, ActionContext::BUTTON_RELEASE_MESSAGE_VALUE);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const int state = value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE ? 0 : 1;
        const vector<int> payload = this->ResolvePayload(properties, state);
        if (payload == this->lastPayload_) return;
        this->lastDoubleValue_ = value;
        this->lastPayload_ = payload;
        this->SendPayload(payload);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        const int state = value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE ? 0 : 1;
        this->lastDoubleValue_ = value;
        this->lastPayload_ = this->ResolvePayload(properties, state);
        this->SendPayload(this->lastPayload_);
    }
};

class Format2MidiSysExValueFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExValuePayloadItem> payload_;
    int lastValue_ = -1;

    void Send(int value) {
        SysExBuilder builder;
        builder.begin();
        for (const Format2MidiSysExValuePayloadItem& item : this->payload_) builder.add((unsigned char) (item.field == Format2MidiSysExValuePayloadField::Byte ? item.byte : value));
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

public:
    Format2MidiSysExValueFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExValuePayloadItem>& payload) : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload) {}
    virtual ~Format2MidiSysExValueFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExValueFeedbackProcessor"; }

    virtual void ForceClear() override { this->ForceValue(PropertyList(), 0.0); }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const int encoded = std::clamp((int) value, 0, 127);
        if (encoded == this->lastValue_) return;
        this->ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastValue_ = std::clamp((int) value, 0, 127);
        this->Send(this->lastValue_);
    }
};

class Format2MidiSysExColorFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExProfilePayloadItem> payload_;
    bool hasStateBrightness_ = false;
    bool active_ = false;
    float inactiveBrightness_ = 1.0f;
    float activeBrightness_ = 1.0f;
    rgba_color sourceColor_;

    void SendResolvedColor() {
        const float brightness = this->hasStateBrightness_ ? (this->active_ ? this->activeBrightness_ : this->inactiveBrightness_) : 1.0f;
        const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(this->sourceColor_, 127, brightness);
        SysExBuilder builder;
        builder.begin();
        for (const Format2MidiSysExProfilePayloadItem& item : this->payload_) {
            if (item.field == Format2MidiSysExProfilePayloadField::Byte) builder.add((unsigned char) item.byte);
            else if (item.field == Format2MidiSysExProfilePayloadField::Red) builder.add((unsigned char) deviceColor.r);
            else if (item.field == Format2MidiSysExProfilePayloadField::Green) builder.add((unsigned char) deviceColor.g);
            else if (item.field == Format2MidiSysExProfilePayloadField::Blue) builder.add((unsigned char) deviceColor.b);
        }
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

public:
    Format2MidiSysExColorFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExProfilePayloadItem>& payload, bool hasStateBrightness, float inactiveBrightness, float activeBrightness)
        : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload), hasStateBrightness_(hasStateBrightness), inactiveBrightness_(inactiveBrightness), activeBrightness_(activeBrightness) {}
    virtual ~Format2MidiSysExColorFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExColorFeedbackProcessor"; }

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
        this->ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        this->sourceColor_ = color;
        this->lastColor_ = color;
        this->SendResolvedColor();
    }

    virtual void ForceUpdateTrackColors() override { this->ForceColorValue(this->surface_->GetTrackColorForChannel(this->widget_->GetChannelNumber())); }
};

class Format2MidiSysExRingFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExProfilePayloadItem> payload_;
    Format2RingProfile profile_;
    vector<int> lastPayload_;

    const Format2RingStyleEntry& ResolveStyleEntry(const PropertyList& properties) const {
        Format2RingStyle style = Format2RingStyle::Dot;
        const char* value = properties.get_prop(PropertyType_RingStyle);
        if (value && IsSameString(value, "Fill")) style = Format2RingStyle::Fill;
        else if (value && IsSameString(value, "BoostCut")) style = Format2RingStyle::BoostCut;
        else if (value && IsSameString(value, "Spread")) style = Format2RingStyle::Spread;
        for (const Format2RingStyleEntry& entry : this->profile_.styles) if (entry.style == style) return entry;
        for (const Format2RingStyleEntry& entry : this->profile_.styles) if (entry.style == Format2RingStyle::Dot) return entry;
        return this->profile_.styles.front();
    }

    vector<int> ResolvePayload(const PropertyList& properties, double value) const {
        const Format2RingStyleEntry& entry = this->ResolveStyleEntry(properties);
        const double scaled = std::clamp(value, 0.0, 1.0) * (entry.steps - 1);
        const int position = this->profile_.quantize == Format2Quantize::Round ? (int) std::round(scaled) : (int) std::floor(scaled);
        const int ringValue = this->profile_.valueOffset + position;
        vector<int> result;
        for (const Format2MidiSysExProfilePayloadItem& item : this->payload_) {
            if (item.field == Format2MidiSysExProfilePayloadField::Byte) result.push_back(item.byte);
            else if (item.field == Format2MidiSysExProfilePayloadField::RingValue) result.push_back(ringValue);
            else if (item.field == Format2MidiSysExProfilePayloadField::RingStyleCode) result.push_back(entry.code);
        }
        return result;
    }

    void Send(const vector<int>& payload) {
        SysExBuilder builder;
        builder.begin();
        for (int byte : payload) builder.add((unsigned char) byte);
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

public:
    Format2MidiSysExRingFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExProfilePayloadItem>& payload, const Format2RingProfile& profile) : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload), profile_(profile) {}
    virtual ~Format2MidiSysExRingFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExRingFeedbackProcessor"; }

    virtual void ForceClear() override { this->ForceValue(PropertyList(), 0.0); }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const vector<int> payload = this->ResolvePayload(properties, value);
        if (payload == this->lastPayload_) return;
        this->lastDoubleValue_ = value;
        this->lastPayload_ = payload;
        this->Send(payload);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastPayload_ = this->ResolvePayload(properties, value);
        this->Send(this->lastPayload_);
    }
};

class Format2MidiSysExBarFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExProfilePayloadItem> payload_;
    Format2BarProfile profile_;
    vector<int> lastPayload_;

    int ResolveStyleCode(const PropertyList& properties, bool clear) const {
        Format2BarStyle style = clear ? Format2BarStyle::Off : this->profile_.defaultStyle;
        const char* value = properties.get_prop(PropertyType_BarStyle);
        if (!clear && value && IsSameString(value, "Normal")) style = Format2BarStyle::Normal;
        else if (!clear && value && IsSameString(value, "Bipolar")) style = Format2BarStyle::Bipolar;
        else if (!clear && value && IsSameString(value, "Fill")) style = Format2BarStyle::Fill;
        else if (!clear && value && IsSameString(value, "Spread")) style = Format2BarStyle::Spread;
        else if (!clear && value && IsSameString(value, "Off")) style = Format2BarStyle::Off;
        for (const Format2BarStyleEntry& entry : this->profile_.styles) if (entry.style == style) return entry.code;
        return 0;
    }

    vector<int> ResolvePayload(const PropertyList& properties, double value, bool clear) const {
        const int barValue = clear ? 0 : (int) (std::clamp(value, 0.0, 1.0) * 127.0);
        const int styleCode = this->ResolveStyleCode(properties, clear);
        vector<int> result;
        for (const Format2MidiSysExProfilePayloadItem& item : this->payload_) {
            if (item.field == Format2MidiSysExProfilePayloadField::Byte) result.push_back(item.byte);
            else if (item.field == Format2MidiSysExProfilePayloadField::BarValue) result.push_back(barValue);
            else if (item.field == Format2MidiSysExProfilePayloadField::BarStyleCode) result.push_back(styleCode);
        }
        return result;
    }

    void Send(const vector<int>& payload) {
        SysExBuilder builder;
        builder.begin();
        for (int byte : payload) builder.add((unsigned char) byte);
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

public:
    Format2MidiSysExBarFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExProfilePayloadItem>& payload, const Format2BarProfile& profile) : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload), profile_(profile) {}
    virtual ~Format2MidiSysExBarFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExBarFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->lastPayload_ = this->ResolvePayload(PropertyList(), 0.0, true);
        this->Send(this->lastPayload_);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const vector<int> payload = this->ResolvePayload(properties, value, false);
        if (payload == this->lastPayload_) return;
        this->lastDoubleValue_ = value;
        this->lastPayload_ = payload;
        this->Send(payload);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastPayload_ = this->ResolvePayload(properties, value, false);
        this->Send(this->lastPayload_);
    }
};

class Format2MidiSysExMeterFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExProfilePayloadItem> payload_;
    Format2MeterProfile profile_;
    bool continuous_ = false;
    DWORD refreshIntervalMs_ = 0;
    DWORD lastSendTime_ = 0;
    int lastEncodedValue_ = 0;
    bool hasLastValue_ = false;

    void Send(int value) {
        SysExBuilder builder;
        builder.begin();
        for (const Format2MidiSysExProfilePayloadItem& item : this->payload_) builder.add((unsigned char) (item.field == Format2MidiSysExProfilePayloadField::Byte ? item.byte : value));
        builder.end();
        this->SendMidiSysExMessage(builder.message());
        this->lastSendTime_ = GetTickCount();
    }

public:
    Format2MidiSysExMeterFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExProfilePayloadItem>& payload, const Format2MeterProfile& profile, bool continuous, int refreshIntervalMs) : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload), profile_(profile), continuous_(continuous), refreshIntervalMs_((DWORD) refreshIntervalMs) {}
    virtual ~Format2MidiSysExMeterFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExMeterFeedbackProcessor"; }

    virtual void ForceClear() override {
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
    vector<Format2MidiSysExRingConfigureItem> configurePayload_;
    std::array<int, 3> lastMessage_{};
    bool hasLastMessage_ = false;

    static rgba_color RgbColor(std::uint32_t value) {
        rgba_color color;
        color.r = (value >> 16) & 0xFF;
        color.g = (value >> 8) & 0xFF;
        color.b = value & 0xFF;
        return color;
    }

    vector<rgba_color> ResolveColors(const PropertyList& properties) const {
        const int segmentCount = this->profile_.segments.value_or(0);
        vector<rgba_color> colors;
        const char* propertyValue = properties.get_prop(PropertyType_RingColors);
        if (propertyValue) {
            string source = propertyValue;
            ReplaceAllWith(source, "[", "");
            ReplaceAllWith(source, "]", "");
            ReplaceAllWith(source, "\"", "");
            size_t start = 0;
            while (start <= source.size()) {
                const size_t separator = source.find(',', start);
                string token = source.substr(start, separator == string::npos ? string::npos : separator - start);
                token.erase(0, token.find_first_not_of(" \t"));
                const size_t lastText = token.find_last_not_of(" \t");
                if (lastText != string::npos) token.erase(lastText + 1);
                if (!token.empty()) {
                    rgba_color color;
                    GetColorValue(token.c_str(), color);
                    colors.push_back(color);
                }
                if (separator == string::npos) break;
                start = separator + 1;
            }
        }
        if (colors.size() == 1 && segmentCount > 1) colors.resize(segmentCount, colors.front());
        if ((int) colors.size() != segmentCount) colors.assign(segmentCount, RgbColor(this->profile_.defaultColor));
        return colors;
    }

    void SendConfiguration(const vector<rgba_color>& colors) {
        if (this->configurePayload_.empty() || colors.empty()) return;
        vector<rgba_color> groups;
        vector<vector<int>> masks;
        const int maskCount = ((int) colors.size() + 6) / 7;
        for (size_t colorIdx = 0; colorIdx < colors.size(); ++colorIdx) {
            size_t groupIdx = 0;
            while (groupIdx < groups.size() && groups[groupIdx] != colors[colorIdx]) ++groupIdx;
            if (groupIdx == groups.size()) {
                groups.push_back(colors[colorIdx]);
                masks.push_back(vector<int>(maskCount, 0));
            }
            masks[groupIdx][colorIdx / 7] |= 1 << (colorIdx % 7);
        }
        for (size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx) {
            const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(groups[groupIdx], 127);
            SysExBuilder builder;
            builder.begin();
            for (const Format2MidiSysExRingConfigureItem& item : this->configurePayload_) {
                if (item.field == Format2MidiSysExRingConfigureField::Byte) builder.add((unsigned char) item.byte);
                else if (item.field == Format2MidiSysExRingConfigureField::SegmentMasks) for (int mask : masks[groupIdx]) builder.add((unsigned char) mask);
                else if (item.field == Format2MidiSysExRingConfigureField::SegmentRed) builder.add((unsigned char) deviceColor.r);
                else if (item.field == Format2MidiSysExRingConfigureField::SegmentGreen) builder.add((unsigned char) deviceColor.g);
                else if (item.field == Format2MidiSysExRingConfigureField::SegmentBlue) builder.add((unsigned char) deviceColor.b);
            }
            builder.end();
            this->SendMidiSysExMessage(builder.message());
        }
    }

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
    Format2Midi7RingFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& message, const Format2RingProfile& profile, int valueBase, Format2MidiValueCombine valueCombine, Format2MidiRingStyleTarget styleTarget, int styleShift, Format2MidiRingStyleCombine styleCombine, const vector<Format2MidiSysExRingConfigureItem>& configurePayload)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), profile_(profile), valueBase_(valueBase), valueCombine_(valueCombine), styleTarget_(styleTarget), styleShift_(styleShift), styleCombine_(styleCombine), configurePayload_(configurePayload) {}
    virtual ~Format2Midi7RingFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2Midi7RingFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        const vector<rgba_color> colors = this->ResolveColors(properties);
        this->SendConfiguration(colors);
        this->ForceValue(properties, 0.0);
    }

    virtual void Configure(const vector<unique_ptr<ActionContext>>& contexts) override {
        const PropertyList emptyProperties;
        const PropertyList& properties = contexts.empty() ? emptyProperties : contexts[0]->GetWidgetProperties();
        const vector<rgba_color> colors = this->ResolveColors(properties);
        this->SendConfiguration(colors);
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

    virtual void ForceUpdateTrackColors() override { this->ForceColorValue(this->surface_->GetTrackColorForChannel(this->widget_->GetChannelNumber())); }
};

class Format2MidiPaletteFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    std::array<int, 2> message_{};
    Format2ColorProfile profile_;
    bool hasCompanion_ = false;
    std::array<int, 3> companion_{};
    bool companionBefore_ = false;

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
        this->Send(ResolveFormat2ColorProfileValue(this->profile_, this->surface_->GetDeviceFeedbackColor(this->lastColor_, 255)));
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (color == this->lastColor_) return;
        this->ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        this->lastColor_ = color;
        this->Send(ResolveFormat2ColorProfileValue(this->profile_, this->surface_->GetDeviceFeedbackColor(color, 255)));
    }

    virtual void ForceUpdateTrackColors() override { this->ForceColorValue(this->surface_->GetTrackColorForChannel(this->widget_->GetChannelNumber())); }
};

class Format2MidiTrackColorFeedbackGroupProcessor : public Midi_FeedbackProcessor
{
private:
    Format2FeedbackGroup group_;
    std::optional<Format2ColorProfile> profile_;
    map<string, bool> sourceTextPresent_;
    vector<int> lastPayload_;

    rgba_color EmptyColor() const { return UnpackFormat2Color(this->group_.emptyColor); }

    vector<int> ResolvePayload() const {
        vector<int> payload = this->group_.payloadPrefix;
        for (const Format2FeedbackGroupSlot& slot : this->group_.slots) {
            Widget* source = this->surface_->GetWidgetByName(slot.source);
            const bool sourcePresent = this->sourceTextPresent_.find(slot.source) != this->sourceTextPresent_.end() && this->sourceTextPresent_.at(slot.source);
            const bool useTrackColor = this->group_.useTrackColorWhen == Format2TrackColorCondition::Always || sourcePresent;
            const rgba_color color = useTrackColor && source ? this->surface_->GetTrackColorForChannel(source->GetChannelNumber() - 1) : this->EmptyColor();
            if (this->group_.colorEncoding == Format2TrackColorEncoding::Palette && this->profile_) {
                payload.push_back(ResolveFormat2ColorProfileValue(*this->profile_, this->surface_->GetDeviceFeedbackColor(color, 255)));
                continue;
            }
            const rgba_color deviceColor = this->surface_->GetDeviceFeedbackColor(color, 127);
            const double greenRatio = (double) deviceColor.g / 127.0;
            const double blueScale = this->group_.blueScaleAtGreenMinimum + greenRatio * (this->group_.blueScaleAtGreenMaximum - this->group_.blueScaleAtGreenMinimum);
            payload.push_back(deviceColor.r);
            payload.push_back(deviceColor.g);
            payload.push_back(std::clamp((int) (deviceColor.b * blueScale), 0, 127));
        }
        return payload;
    }

    void Send(bool force) {
        const vector<int> payload = this->ResolvePayload();
        if (!force && payload == this->lastPayload_) return;
        this->lastPayload_ = payload;
        SysExBuilder builder;
        builder.begin();
        for (int byte : payload) builder.add((unsigned char) byte);
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

public:
    Format2MidiTrackColorFeedbackGroupProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const Format2FeedbackGroup& group, const Format2ColorProfile* profile)
        : Midi_FeedbackProcessor(csi, surface, widget), group_(group) {
        if (profile) this->profile_ = *profile;
        for (const Format2FeedbackGroupSlot& slot : this->group_.slots) this->sourceTextPresent_[slot.source] = false;
    }
    virtual ~Format2MidiTrackColorFeedbackGroupProcessor() {}
    virtual const char* GetName() override { return "Format2MidiTrackColorFeedbackGroupProcessor"; }

    virtual void ForceClear() override {
        for (auto& source : this->sourceTextPresent_) source.second = false;
        this->Send(true);
    }

    virtual void ForceUpdateTrackColors() override { this->Send(false); }

    virtual void SetTrackColorSourceText(Widget* source, const char* text) override {
        const auto current = this->sourceTextPresent_.find(source->GetName());
        if (current == this->sourceTextPresent_.end()) return;
        const bool present = text && text[0] != '\0';
        if (current->second == present) return;
        current->second = present;
        this->Send(false);
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

class Format2MidiSysExTextFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<Format2MidiSysExTextPayloadItem> payload_;
    Format2TextProfile profile_;
    int topMargin_ = 0;
    int bottomMargin_ = 0;
    int font_ = 0;
    rgba_color backgroundColor_;
    rgba_color textColor_;
    vector<int> lastPayload_;

    static rgba_color RgbColor(std::uint32_t value) {
        rgba_color color;
        color.r = (value >> 16) & 0xFF;
        color.g = (value >> 8) & 0xFF;
        color.b = value & 0xFF;
        return color;
    }

    int ResolveInteger(const PropertyList& properties, PropertyType property, int defaultValue) const {
        const char* value = properties.get_prop(property);
        return value ? std::clamp(atoi(value), 0, 0x7F) : defaultValue;
    }

    rgba_color ResolveColor(const PropertyList& properties, const rgba_color& defaultColor, PropertyType generalProperty, PropertyType offProperty, PropertyType onProperty, int state) const {
        rgba_color color = defaultColor;
        const char* value = properties.get_prop(generalProperty);
        if (value) GetColorValue(value, color);
        if (state >= 0) {
            value = properties.get_prop(state ? onProperty : offProperty);
            if (value) GetColorValue(value, color);
        }
        return this->surface_->GetDeviceFeedbackColor(color, 127);
    }

    int ResolvePresentationCode(const PropertyList& properties) const {
        Format2TextAlignment alignment = this->profile_.defaultAlignment.value_or(Format2TextAlignment::Left);
        const char* alignmentValue = properties.get_prop(PropertyType_TextAlign);
        if (alignmentValue && IsSameString(alignmentValue, "Center")) alignment = Format2TextAlignment::Center;
        else if (alignmentValue && IsSameString(alignmentValue, "Right")) alignment = Format2TextAlignment::Right;
        else if (alignmentValue && IsSameString(alignmentValue, "Left")) alignment = Format2TextAlignment::Left;
        int code = 0;
        for (const Format2TextAlignmentEntry& entry : this->profile_.alignments) if (entry.alignment == alignment) { code = entry.code; break; }
        const char* invertValue = properties.get_prop(PropertyType_TextInvert);
        const bool inverted = invertValue && (IsSameString(invertValue, "Yes") || IsSameString(invertValue, "true"));
        const int invertCode = inverted ? this->profile_.invertCode.value_or(0) : 0;
        return this->profile_.presentationCombine == Format2PresentationCombine::Add ? code + invertCode : code | invertCode;
    }

    string EncodeText(const char* inputText) const {
        char restrictedText[MEDBUF];
        const char* source = this->surface_->GetRestrictedLengthText(inputText, restrictedText, sizeof(restrictedText));
        const size_t fixedFieldCount = this->payload_.empty() ? 0 : this->payload_.size() - 1;
        const size_t maximumPayloadText = fixedFieldCount < 253 ? 253 - fixedFieldCount : 0;
        return EncodeFormat2TextProfile(this->profile_, source, maximumPayloadText);
    }

    vector<int> ResolvePayload(const PropertyList& properties, const char* inputText, int state) const {
        const string encoded = this->EncodeText(inputText);
        const int topMargin = this->ResolveInteger(properties, PropertyType_TopMargin, this->topMargin_);
        const int bottomMargin = this->ResolveInteger(properties, PropertyType_BottomMargin, this->bottomMargin_);
        const int font = this->ResolveInteger(properties, PropertyType_Font, this->font_);
        const rgba_color backgroundColor = this->ResolveColor(properties, this->backgroundColor_, PropertyType_BackgroundColor, PropertyType_BackgroundColorOff, PropertyType_BackgroundColorOn, state);
        const rgba_color textColor = this->ResolveColor(properties, this->textColor_, PropertyType_TextColor, PropertyType_TextColorOff, PropertyType_TextColorOn, state);
        const int presentationCode = this->ResolvePresentationCode(properties);
        vector<int> result;
        for (const Format2MidiSysExTextPayloadItem& item : this->payload_) {
            if (item.field == Format2MidiSysExTextPayloadField::Byte) result.push_back(item.byte);
            else if (item.field == Format2MidiSysExTextPayloadField::TopMargin) result.push_back(topMargin);
            else if (item.field == Format2MidiSysExTextPayloadField::BottomMargin) result.push_back(bottomMargin);
            else if (item.field == Format2MidiSysExTextPayloadField::Font) result.push_back(font);
            else if (item.field == Format2MidiSysExTextPayloadField::TextPresentationCode) result.push_back(presentationCode);
            else if (item.field == Format2MidiSysExTextPayloadField::BackgroundRed) result.push_back(backgroundColor.r);
            else if (item.field == Format2MidiSysExTextPayloadField::BackgroundGreen) result.push_back(backgroundColor.g);
            else if (item.field == Format2MidiSysExTextPayloadField::BackgroundBlue) result.push_back(backgroundColor.b);
            else if (item.field == Format2MidiSysExTextPayloadField::TextRed) result.push_back(textColor.r);
            else if (item.field == Format2MidiSysExTextPayloadField::TextGreen) result.push_back(textColor.g);
            else if (item.field == Format2MidiSysExTextPayloadField::TextBlue) result.push_back(textColor.b);
            else for (unsigned char character : encoded) result.push_back(character);
        }
        return result;
    }

    void SendPayload(const vector<int>& payload) {
        SysExBuilder builder;
        builder.begin();
        for (int byte : payload) builder.add((unsigned char) byte);
        builder.end();
        this->SendMidiSysExMessage(builder.message());
    }

    void Update(const PropertyList& properties, const char* inputText, int state, bool force) {
        const vector<int> payload = this->ResolvePayload(properties, inputText, state);
        if (!force && payload == this->lastPayload_) return;
        this->lastPayload_ = payload;
        this->SendPayload(payload);
    }

public:
    Format2MidiSysExTextFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<Format2MidiSysExTextPayloadItem>& payload, const Format2TextProfile& profile, int topMargin, int bottomMargin, int font, std::uint32_t backgroundColor, std::uint32_t textColor)
        : Midi_FeedbackProcessor(csi, surface, widget), payload_(payload), profile_(profile), topMargin_(topMargin), bottomMargin_(bottomMargin), font_(font), backgroundColor_(RgbColor(backgroundColor)), textColor_(RgbColor(textColor)) {}
    virtual ~Format2MidiSysExTextFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2MidiSysExTextFeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        this->lastStringValue_ = this->profile_.clearText;
        this->Update(properties, this->profile_.clearText.c_str(), -1, true);
    }

    virtual void SetValue(const PropertyList& properties, const char* const& inputText) override {
        this->lastStringValue_ = inputText;
        this->Update(properties, inputText, -1, false);
    }

    virtual void ForceValue(const PropertyList& properties, const char* const& inputText) override {
        this->lastStringValue_ = inputText;
        this->Update(properties, inputText, -1, true);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const char* displayText = properties.get_prop(PropertyType_DisplayText);
        this->Update(properties, displayText ? displayText : "", value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE ? 0 : 1, false);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        const char* displayText = properties.get_prop(PropertyType_DisplayText);
        this->Update(properties, displayText ? displayText : "", value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE ? 0 : 1, true);
    }
};

class Format2Midi7MeterFeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    vector<int> message_;
    Format2MeterProfile profile_;
    int valueBase_ = 0;
    Format2MidiValueCombine combine_ = Format2MidiValueCombine::Replace;
    bool continuous_ = false;
    DWORD refreshIntervalMs_ = 0;
    DWORD lastSendTime_ = 0;
    int lastEncodedValue_ = 0;
    bool hasLastValue_ = false;

    int Encode(double value) const {
        const int profileValue = EncodeFormat2MeterProfile(this->profile_, value);
        if (this->combine_ == Format2MidiValueCombine::Add) return this->valueBase_ + profileValue;
        if (this->combine_ == Format2MidiValueCombine::BitOr) return this->valueBase_ | profileValue;
        return profileValue;
    }

    void Send(int value, bool force) {
        if (this->message_.size() == 1) {
            if (force) this->ForceMidiMessage(this->message_[0], value, 0);
            else this->SendMidiMessage(this->message_[0], value, 0);
        } else if (force) this->ForceMidiMessage(this->message_[0], this->message_[1], value);
        else this->SendMidiMessage(this->message_[0], this->message_[1], value);
        this->lastSendTime_ = GetTickCount();
    }

    int ClearValue() const {
        return ClearFormat2MeterProfileValue(this->profile_);
    }

public:
    Format2Midi7MeterFeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, const vector<int>& message, const Format2MeterProfile& profile, int valueBase, Format2MidiValueCombine combine, bool continuous, int refreshIntervalMs)
        : Midi_FeedbackProcessor(csi, surface, widget), message_(message), profile_(profile), valueBase_(valueBase), combine_(combine), continuous_(continuous), refreshIntervalMs_((DWORD) refreshIntervalMs) {}
    virtual ~Format2Midi7MeterFeedbackProcessor() {}
    virtual const char* GetName() override { return "Format2Midi7MeterFeedbackProcessor"; }

    virtual void ForceClear() override {
        this->lastDoubleValue_ = 0.0;
        this->lastEncodedValue_ = this->combine_ == Format2MidiValueCombine::Add ? this->valueBase_ + this->ClearValue() : this->combine_ == Format2MidiValueCombine::BitOr ? this->valueBase_ | this->ClearValue() : this->ClearValue();
        this->hasLastValue_ = true;
        this->Send(this->lastEncodedValue_, true);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        const int encoded = this->Encode(value);
        const DWORD now = GetTickCount();
        if (this->continuous_) {
            if (this->hasLastValue_ && now - this->lastSendTime_ < this->refreshIntervalMs_) return;
            this->lastDoubleValue_ = value;
            this->lastEncodedValue_ = encoded;
            this->hasLastValue_ = true;
            this->Send(encoded, true);
            return;
        }
        if (this->hasLastValue_ && encoded == this->lastEncodedValue_) return;
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = encoded;
        this->hasLastValue_ = true;
        this->Send(encoded, false);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        this->lastDoubleValue_ = value;
        this->lastEncodedValue_ = this->Encode(value);
        this->hasLastValue_ = true;
        this->Send(this->lastEncodedValue_, true);
    }
};
