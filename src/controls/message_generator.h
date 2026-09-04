#pragma once
// message_generator.h — MessageGenerator base class and concrete subclasses

#include "preamble.h"
class MessageGenerator
{
protected:
    CSurfIntegrator* const csi_;
    Widget* const widget_;

public:
    MessageGenerator(CSurfIntegrator* const csi, Widget* widget) : csi_(csi), widget_(widget) {}
    virtual ~MessageGenerator() {}

    virtual void ProcessMidiMessage(const MIDI_event_ex_t* midiMessage) {}
    virtual void ProcessMessage(double value) {
        widget_->GetZoneManager()->DoAction(widget_, value);
    }
};

class Midi_MessageGenerator : public MessageGenerator
{
protected:
    Midi_MessageGenerator(CSurfIntegrator* const csi, Widget* widget) : MessageGenerator(csi, widget) {}

public:
    virtual ~Midi_MessageGenerator() {}
};
