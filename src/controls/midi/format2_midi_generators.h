#pragma once

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
    int msb_ = 0;
    int lsb_ = 0;

public:
    Format2MidiSplitValueState(Widget* widget, int bits, Format2MidiSplitPart commitPart) : widget_(widget), commitPart_(commitPart), maximumValue_((1 << bits) - 1) {}

    void Process(Format2MidiSplitPart part, int value) {
        if (part == Format2MidiSplitPart::Msb) this->msb_ = value;
        else this->lsb_ = value;
        if (part != this->commitPart_) return;
        const int combined = (std::min)((this->msb_ << 7) | this->lsb_, this->maximumValue_);
        this->widget_->GetZoneManager()->DoAction(this->widget_, combined / (double) this->maximumValue_);
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
public:
    Format2Midi7ValueMessageGenerator(CSurfIntegrator* const csi, Widget* widget) : Midi_MessageGenerator(csi, widget) {}
    virtual ~Format2Midi7ValueMessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) override {
        this->widget_->GetZoneManager()->DoAction(this->widget_, midiMessage->midi_message[2] / 127.0);
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
