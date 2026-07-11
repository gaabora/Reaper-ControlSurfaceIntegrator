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

        SysExBuilder builder;
        builder.begin()
            .add(0x00).add(0x20).add(0x29).add(0x02).add(0x0d)
            .add(0x03).add(0x03)
            .add(midiFeedbackMessage1_.midi_message[1])
            .add(color.r / 2).add(color.g / 2).add(color.b / 2)
            .end();
        SendMidiSysExMessage(builder.message());
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG) LogToConsole("[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
    }
};
