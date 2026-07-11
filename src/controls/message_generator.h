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

class AnyPress_MessageGenerator : public MessageGenerator
{
public:
    AnyPress_MessageGenerator(CSurfIntegrator* const csi, Widget* widget) : MessageGenerator(csi, widget) {}
    virtual ~AnyPress_MessageGenerator() {}

    virtual void ProcessMessage(double value) override {
        widget_->GetZoneManager()->DoAction(widget_, 1.0);
    }
};

class Touch_MessageGenerator : public MessageGenerator
{
public:
    Touch_MessageGenerator(CSurfIntegrator* const csi, Widget* widget) : MessageGenerator(csi, widget) {}
    virtual ~Touch_MessageGenerator() {}

    virtual void ProcessMessage(double value) override {
        widget_->GetZoneManager()->DoTouch(widget_, value);
    }
};

class Midi_MessageGenerator : public MessageGenerator
{
protected:
    Midi_MessageGenerator(CSurfIntegrator* const csi, Widget* widget) : MessageGenerator(csi, widget) {}

public:
    virtual ~Midi_MessageGenerator() {}
};
