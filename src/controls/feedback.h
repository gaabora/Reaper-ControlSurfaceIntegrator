#pragma once
//
//  feedback.h — FeedbackProcessor base class and Midi_FeedbackProcessor
//
#include "preamble.h"

class FeedbackProcessor
{
protected:
    CSurfIntegrator* const csi_;
    Widget* const widget_;

    double lastDoubleValue_ = 0.0;
    string lastStringValue_;
    rgba_color lastColor_;

public:
    FeedbackProcessor(CSurfIntegrator* const csi, Widget* widget) : csi_(csi), widget_(widget) {}
    virtual ~FeedbackProcessor() {}
    virtual const char* GetName() { return "FeedbackProcessor"; }
    Widget* GetWidget() { return widget_; }
    virtual void Configure(const vector<unique_ptr<ActionContext>>& contexts) {}
    virtual void ForceValue(const PropertyList& properties, double value) {}
    virtual void ForceValue(const PropertyList& properties, const char* const& value) {}
    virtual void ForceColorValue(const rgba_color& color) {}
    virtual void ForceUpdateTrackColors() {}
    virtual void ForceClear() {}

    virtual void SetXTouchDisplayColors(const char* colors) {}
    virtual void RestoreXTouchDisplayColors() {}

    virtual void SetColorValue(const rgba_color& color) {}

    double GetLastDoubleValue() const { return lastDoubleValue_; }
    const rgba_color& GetLastColor() const { return lastColor_; }

    virtual void SetValue(const PropertyList& properties, double value) {
        if (lastDoubleValue_ != value) {
            lastDoubleValue_ = value;
            ForceValue(properties, value);
        }
    }

    virtual void SetValue(const PropertyList& properties, const char* const& value) {
        if (lastStringValue_ != value) {
            lastStringValue_ = value;
            ForceValue(properties, value);
        }
    }
};

class Midi_FeedbackProcessor : public FeedbackProcessor
{
protected:
    Midi_ControlSurface* const surface_;

    MIDI_event_ex_t lastMessageSent_;
    MIDI_event_ex_t midiFeedbackMessage1_;
    MIDI_event_ex_t midiFeedbackMessage2_;

    Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget)
        : FeedbackProcessor(csi, widget), surface_(surface) {}

    Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : FeedbackProcessor(csi, widget), surface_(surface), midiFeedbackMessage1_(feedback1) {}

    Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1, MIDI_event_ex_t feedback2)
        : FeedbackProcessor(csi, widget), surface_(surface), midiFeedbackMessage1_(feedback1), midiFeedbackMessage2_(feedback2) {}

    void SendMidiSysExMessage(MIDI_event_ex_t* midiMessage);
    void SendMidiMessage(int first, int second, int third);
    void ForceMidiMessage(int first, int second, int third);
    void LogMessage(char* value);

public:
    ~Midi_FeedbackProcessor() {}

    virtual const char* GetName() override { return "Midi_FeedbackProcessor"; }
};

void ReleaseMidiInput(midi_Input* input);
void ReleaseMidiOutput(midi_Output* output);
