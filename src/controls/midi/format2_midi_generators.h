#pragma once

#include "../format2_value_profile.h"

class Format2MidiExactPressMessageGenerator : public Midi_MessageGenerator
{
private:
    double value_;

public:
    Format2MidiExactPressMessageGenerator(CSurfIntegrator* const csi, Widget* widget, bool pressed) : Midi_MessageGenerator(csi, widget), value_(pressed ? 1.0 : 0.0) { widget->SetIsTwoState(); }
    virtual ~Format2MidiExactPressMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        this->widget_->GetZoneManager()->DoAction(this->widget_, this->value_);
    }
};

class Format2MidiExactTouchMessageGenerator : public Midi_MessageGenerator
{
private:
    double value_;

public:
    Format2MidiExactTouchMessageGenerator(CSurfIntegrator* const csi, Widget* widget, bool touched) : Midi_MessageGenerator(csi, widget), value_(touched ? 1.0 : 0.0) {}
    virtual ~Format2MidiExactTouchMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        this->widget_->GetZoneManager()->DoTouch(this->widget_, this->value_);
    }
};

class Format2MidiPrefixPressMessageGenerator : public Midi_MessageGenerator
{
public:
    Format2MidiPrefixPressMessageGenerator(CSurfIntegrator* const csi, Widget* widget) : Midi_MessageGenerator(csi, widget) { widget->SetIsTwoState(); }
    virtual ~Format2MidiPrefixPressMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        this->widget_->GetZoneManager()->DoAction(this->widget_, 1.0);
    }
};

class Format2Midi14ValueMessageGenerator : public Midi_MessageGenerator
{
private:
    std::optional<Format2ValueProfile> profile_;

public:
    Format2Midi14ValueMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const Format2ValueProfile* profile) : Midi_MessageGenerator(csi, widget), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}
    virtual ~Format2Midi14ValueMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        const double value = int14ToNormalized(midiMessage->midi_message[2], midiMessage->midi_message[1]);
        this->widget_->GetZoneManager()->DoAction(this->widget_, this->profile_ ? DecodeFormat2ValueProfile(*this->profile_, value) : value);
    }
};

class Format2MidiProfileEncoderMessageGenerator : public Midi_MessageGenerator
{
private:
    map<int, int> increase_;
    map<int, int> decrease_;

public:
    Format2MidiProfileEncoderMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const string& profile) : Midi_MessageGenerator(csi, widget) {
        this->increase_ = widget->GetSurface()->GetAccelerationValuesForIncrement(profile.c_str());
        this->decrease_ = widget->GetSurface()->GetAccelerationValuesForDecrement(profile.c_str());
        widget->SetStepSize(widget->GetSurface()->GetStepSize(profile.c_str()));
        widget->SetAccelerationValues(widget->GetSurface()->GetAccelerationValues(profile.c_str()));
    }
    virtual ~Format2MidiProfileEncoderMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        const int value = midiMessage->midi_message[2];
        if (this->increase_.count(value) > 0) this->widget_->GetZoneManager()->DoRelativeAction(this->widget_, this->increase_[value], 0.001);
        else if (this->decrease_.count(value) > 0) this->widget_->GetZoneManager()->DoRelativeAction(this->widget_, this->decrease_[value], -0.001);
    }
};

enum class Format2MidiEncoderMode {
    SignedBit,
    SignedBitFixed,
    Relative7Bit,
};

enum class Format2MidiSplitPart {
    Msb,
    Lsb,
};

class Format2MidiSplitValueState
{
private:
    Widget* widget_;
    Format2MidiSplitPart commitPart_;
    int maximumValue_;
    std::optional<Format2ValueProfile> profile_;
    int msb_ = 0;
    int lsb_ = 0;

public:
    Format2MidiSplitValueState(Widget* widget, int bits, Format2MidiSplitPart commitPart, const Format2ValueProfile* profile) : widget_(widget), commitPart_(commitPart), maximumValue_((1 << bits) - 1), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}

    void Process(Format2MidiSplitPart part, int value) {
        if (part == Format2MidiSplitPart::Msb) this->msb_ = value;
        else this->lsb_ = value;
        if (part != this->commitPart_) return;
        const int combined = (std::min)((this->msb_ << 7) | this->lsb_, this->maximumValue_);
        const double normalized = combined / (double) this->maximumValue_;
        this->widget_->GetZoneManager()->DoAction(this->widget_, this->profile_ ? DecodeFormat2ValueProfile(*this->profile_, normalized) : normalized);
    }
};

class Format2MidiSplitValueMessageGenerator : public Midi_MessageGenerator
{
private:
    shared_ptr<Format2MidiSplitValueState> state_;
    Format2MidiSplitPart part_;

public:
    Format2MidiSplitValueMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const shared_ptr<Format2MidiSplitValueState>& state, Format2MidiSplitPart part) : Midi_MessageGenerator(csi, widget), state_(state), part_(part) {}
    virtual ~Format2MidiSplitValueMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        this->state_->Process(this->part_, midiMessage->midi_message[2]);
    }
};

class Format2Midi7ValueMessageGenerator : public Midi_MessageGenerator
{
private:
    std::optional<Format2ValueProfile> profile_;

public:
    Format2Midi7ValueMessageGenerator(CSurfIntegrator* const csi, Widget* widget, const Format2ValueProfile* profile) : Midi_MessageGenerator(csi, widget), profile_(profile ? std::optional<Format2ValueProfile>(*profile) : std::nullopt) {}
    virtual ~Format2Midi7ValueMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        const double normalized = midiMessage->midi_message[2] / 127.0;
        this->widget_->GetZoneManager()->DoAction(this->widget_, this->profile_ ? DecodeFormat2ValueProfile(*this->profile_, normalized) : normalized);
    }
};

class Format2Midi7EncoderMessageGenerator : public Midi_MessageGenerator
{
private:
    Format2MidiEncoderMode mode_;
    int lastValue_ = -1;

public:
    Format2Midi7EncoderMessageGenerator(CSurfIntegrator* const csi, Widget* widget, Format2MidiEncoderMode mode) : Midi_MessageGenerator(csi, widget), mode_(mode) {}
    virtual ~Format2Midi7EncoderMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        const int value = midiMessage->midi_message[2];
        double delta = 0.0;
        if (this->mode_ == Format2MidiEncoderMode::SignedBit) {
            delta = (value & 0x3F) / 126.0;
            if (value & 0x40) delta = -delta;
        } else if (this->mode_ == Format2MidiEncoderMode::SignedBitFixed) {
            delta = value & 0x40 ? -1.0 / 64.0 : 1.0 / 64.0;
        } else {
            if (this->lastValue_ < 0) {
                this->lastValue_ = value;
                return;
            }
            delta = this->lastValue_ > value || (this->lastValue_ == 0 && value == 0) ? -1.0 / 64.0 : 1.0 / 64.0;
            this->lastValue_ = value;
        }
        this->widget_->GetZoneManager()->DoRelativeAction(this->widget_, delta);
    }
};
