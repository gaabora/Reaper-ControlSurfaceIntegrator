#pragma once
// fb_novation.h — Novation Launchpad feedback processors: NovationLaunchpadMiniRGB7Bit.

class NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor() {}
    NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        rgba_color color;
        ForceColorValue(color);
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (color != lastColor_)
            ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        lastColor_ = color;

        struct {
            MIDI_event_ex_t evt;
            char data[64];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x20;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x29;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x0d;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x03;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x03;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.b / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;

        SendMidiSysExMessage(&midiSysExData.evt);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
    }
};
