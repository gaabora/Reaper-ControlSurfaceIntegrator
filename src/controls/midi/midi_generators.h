#pragma once
//
//  midi_generators.h — All Midi_CSIMessageGenerator subclasses
//
//  Requires preamble.h + message_generator.h already in scope (provided by
//  the including context: integrator.h → midi_surface.h → this file via
//  midi_widgets.h).

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class PressRelease_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MIDI_event_ex_t press_;
    MIDI_event_ex_t release_;
    
public:
    virtual ~PressRelease_Midi_CSIMessageGenerator() {}

    PressRelease_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t press) : Midi_CSIMessageGenerator(csi, widget), press_(press)
    {
        widget->SetIsTwoState();
    }
    
    PressRelease_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t press, MIDI_event_ex_t release) : Midi_CSIMessageGenerator(csi, widget), press_(press), release_(release)
    {
        widget->SetIsTwoState();
    }

    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoAction(widget_, midiMessage->IsEqualTo(&press_) ? 1 : 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Touch_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MIDI_event_ex_t press_;
    MIDI_event_ex_t release_;

public:
    virtual ~Touch_Midi_CSIMessageGenerator() {}
    
    Touch_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t press, MIDI_event_ex_t release) : Midi_CSIMessageGenerator(csi, widget), press_(press), release_(release) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoTouch(widget_, midiMessage->IsEqualTo(&press_) ? 1 : 0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AnyPress_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~AnyPress_Midi_CSIMessageGenerator() {}
    AnyPress_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget)
    {
        widget->SetIsTwoState();
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        // Doesn't matter what value was sent, just do it
        widget_->GetZoneManager()->DoAction(widget_, 1);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Fader14Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Fader14Bit_Midi_CSIMessageGenerator() {}
    Fader14Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoAction(widget_, int14ToNormalized(midiMessage->midi_message[2], midiMessage->midi_message[1]));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FaderportClassicFader14Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
protected:
    MIDI_event_ex_t message1_;
    MIDI_event_ex_t message2_;

public:
    virtual ~FaderportClassicFader14Bit_Midi_CSIMessageGenerator() {}
    FaderportClassicFader14Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, MIDI_event_ex_t message1, MIDI_event_ex_t message2) : Midi_CSIMessageGenerator(csi, widget), message1_(message1), message2_(message2) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        if (message1_.midi_message[1] == midiMessage->midi_message[1])
            message1_.midi_message[2] = midiMessage->midi_message[2];
        else if (message2_.midi_message[1] == midiMessage->midi_message[1])
            widget_->GetZoneManager()->DoAction(widget_, int14ToNormalized(message1_.midi_message[2], midiMessage->midi_message[2]));
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Fader7Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Fader7Bit_Midi_CSIMessageGenerator() {}
    Fader7Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        widget_->GetZoneManager()->DoAction(widget_, midiMessage->midi_message[2] / 127.0);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    map<int, int> accelerationValuesForIncrement_;
    map<int, int> accelerationValuesForDecrement_;
    
public:
    virtual ~AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator() {}
    AcceleratedPreconfiguredEncoder_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget)
    {
        const char * const widgetClass = "RotaryWidgetClass";
        accelerationValuesForIncrement_ = widget->GetSurface()->GetAccelerationValuesForIncrement(widgetClass);
        accelerationValuesForDecrement_ = widget->GetSurface()->GetAccelerationValuesForDecrement(widgetClass);
        widget->SetStepSize(widget->GetSurface()->GetStepSize(widgetClass));
        widget->SetAccelerationValues(widget->GetSurface()->GetAccelerationValues(widgetClass));
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        int val = midiMessage->midi_message[2];
        
        if (accelerationValuesForIncrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForIncrement_[val], 0.001);
        else if (accelerationValuesForDecrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForDecrement_[val], -0.001);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MFT_AcceleratedEncoder_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    map<int, int> accelerationValuesForIncrement_;
    map<int, int> accelerationValuesForDecrement_;

public:
    virtual ~MFT_AcceleratedEncoder_Midi_CSIMessageGenerator() {}
    MFT_AcceleratedEncoder_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget, vector<string> &params) : Midi_CSIMessageGenerator(csi, widget)
    {
        accelerationValuesForIncrement_[0x3f] = 0;
        accelerationValuesForIncrement_[0x3e] = 1;
        accelerationValuesForIncrement_[0x3d] = 2;
        accelerationValuesForIncrement_[0x3c] = 3;
        accelerationValuesForIncrement_[0x3b] = 4;
        accelerationValuesForIncrement_[0x3a] = 5;
        accelerationValuesForIncrement_[0x39] = 6;
        accelerationValuesForIncrement_[0x38] = 7;
        accelerationValuesForIncrement_[0x36] = 8;
        accelerationValuesForIncrement_[0x33] = 9;
        accelerationValuesForIncrement_[0x2f] = 10;

        accelerationValuesForDecrement_[0x41] = 0;
        accelerationValuesForDecrement_[0x42] = 1;
        accelerationValuesForDecrement_[0x43] = 2;
        accelerationValuesForDecrement_[0x44] = 3;
        accelerationValuesForDecrement_[0x45] = 4;
        accelerationValuesForDecrement_[0x46] = 5;
        accelerationValuesForDecrement_[0x47] = 6;
        accelerationValuesForDecrement_[0x48] = 7;
        accelerationValuesForDecrement_[0x4a] = 8;
        accelerationValuesForDecrement_[0x4d] = 9;
        accelerationValuesForDecrement_[0x51] = 10;
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        int val = midiMessage->midi_message[2];
        
        if (accelerationValuesForIncrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForIncrement_[val], 0.001);
        else if (accelerationValuesForDecrement_.count(val) > 0)
            widget_->GetZoneManager()->DoRelativeAction(widget_, accelerationValuesForDecrement_[val], -0.001);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Encoder_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~Encoder_Midi_CSIMessageGenerator() {}
    Encoder_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        double delta = (midiMessage->midi_message[2] & 0x3f) / 63.0;
        
        if (midiMessage->midi_message[2] & 0x40)
            delta = -delta;
        
        delta = delta / 2.0;

        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class EncoderPlain_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    virtual ~EncoderPlain_Midi_CSIMessageGenerator() {}
    EncoderPlain_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget) {}
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        double delta = 1.0 / 64.0;
        
        if (midiMessage->midi_message[2] & 0x40)
            delta = -delta;
        
        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Encoder7Bit_Midi_CSIMessageGenerator : public Midi_CSIMessageGenerator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int lastMessage;
    
public:
    virtual ~Encoder7Bit_Midi_CSIMessageGenerator() {}
    Encoder7Bit_Midi_CSIMessageGenerator(CSurfIntegrator *const csi, Widget *widget) : Midi_CSIMessageGenerator(csi, widget)
    {
        lastMessage = -1;
    }
    
    virtual void ProcessMidiMessage(const MIDI_event_ex_t *midiMessage) override
    {
        int currentMessage = midiMessage->midi_message[2];
        double delta = 1.0 / 64.0;
        
        if (lastMessage > currentMessage || (lastMessage == 0 && currentMessage == 0))
            delta = -delta;
            
        lastMessage = currentMessage;
        
        widget_->GetZoneManager()->DoRelativeAction(widget_, delta);
    }
};
