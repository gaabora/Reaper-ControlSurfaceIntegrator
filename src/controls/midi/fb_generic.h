#pragma once
// fb_generic.h — Generic/protocol-agnostic MIDI feedback processors: TwoState, Fader14Bit, FaderportClassicFader14Bit, Fader7Bit, Encoder, ConsoleOneVUMeter, ConsoleOneGainReductionMeter, MFT_RGB.
//  Requires the full preamble (preamble.h + feedback.h) to be in scope.

class TwoState_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~TwoState_Midi_FeedbackProcessor() {}
    TwoState_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1, MIDI_event_ex_t feedback2)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1, feedback2) {}

    virtual const char* GetName() override { return "TwoState_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) {
            if (midiFeedbackMessage2_.midi_message[0] != 0)
                ForceMidiMessage(midiFeedbackMessage2_.midi_message[0], midiFeedbackMessage2_.midi_message[1], midiFeedbackMessage2_.midi_message[2]);
            else
                ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], 0x00);
        } else
            ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], midiFeedbackMessage1_.midi_message[2]);
    }
};

class Fader14Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    double lastValue_ = 0.0;

public:
    virtual ~Fader14Bit_Midi_FeedbackProcessor() {}
    Fader14Bit_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "Fader14Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (value == lastValue_) return;
        lastValue_ = value;
        int volInt = int(value * 16383.0);
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], volInt & 0x7f, (volInt >> 7) & 0x7f);
    }
};

class FaderportClassicFader14Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    double lastValue_ = 0.0;

public:
    virtual ~FaderportClassicFader14Bit_Midi_FeedbackProcessor() {}
    FaderportClassicFader14Bit_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1, MIDI_event_ex_t feedback2)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1, feedback2) {}

    virtual const char* GetName() override { return "FaderportClassicFader14Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (value == lastValue_) return;
        lastValue_ = value;
        int volInt = int(value * 1024.0);
        if (midiFeedbackMessage1_.midi_message[2] != ((volInt >> 7) & 0x7f) || midiFeedbackMessage2_.midi_message[2] != (volInt & 0x7f)) {
            midiFeedbackMessage1_.midi_message[2] = (volInt >> 7) & 0x7f;
            midiFeedbackMessage2_.midi_message[2] = volInt & 0x7f;

            SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], midiFeedbackMessage1_.midi_message[2]);
            SendMidiMessage(midiFeedbackMessage2_.midi_message[0], midiFeedbackMessage2_.midi_message[1], midiFeedbackMessage2_.midi_message[2]);
        }
    }
};

class Fader7Bit_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~Fader7Bit_Midi_FeedbackProcessor() {}
    Fader7Bit_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "Fader7Bit_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(value * 127.0));
    }
};

class Encoder_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~Encoder_Midi_FeedbackProcessor() {}
    Encoder_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "Encoder_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }

    int GetMidiValue(const PropertyList& properties, double value) {
        int valueInt = int(value * 127);
        int displayMode = 0;
        const char* ringstyle = properties.get_prop(PropertyType_RingStyle);
        if (ringstyle) {
            if (IsSameString(ringstyle, "Dot")) displayMode = 0;
            else if (IsSameString(ringstyle, "BoostCut")) displayMode = 1;
            else if (IsSameString(ringstyle, "Fill")) displayMode = 2;
            else if (IsSameString(ringstyle, "Spread")) displayMode = 3;
        }

        int val = 0;
        if (displayMode == 3)
            val = (1 + ((valueInt * 11) >> 8)) | (displayMode << 4);
        else
            val = (1 + ((valueInt * 11) >> 7)) | (displayMode << 4);
        return val;
    }
};

class ConsoleOneVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~ConsoleOneVUMeter_Midi_FeedbackProcessor() {}
    ConsoleOneVUMeter_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "ConsoleOneVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(value));
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(value));
    }

    int GetMidiValue(double value) {
        double dB = VAL2DB(normalizedToVol(value)) + 2.5;
        double midiVal = 0;
        if (dB < 0)
            midiVal = pow(10.0, dB / 48) * 96;
        else
            midiVal = pow(10.0, dB / 60) * 96;

        return (int) midiVal;
    }
};

class ConsoleOneGainReductionMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    double minDB_;
    double maxDB_;

public:
    virtual ~ConsoleOneGainReductionMeter_Midi_FeedbackProcessor() {}
    ConsoleOneGainReductionMeter_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {
        minDB_ = 0.0;
        maxDB_ = 24.0;
    }

    virtual const char* GetName() override { return "ConsoleOneGainReductionMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(fabs(1.0 - value) * 127.0));
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], int(fabs(1.0 - value) * 127.0));
    }
};

// ---------------------------------------------------------------------------
// Color mapping table used by MFT_RGB (MIDI Fighter Twister)
// Stored in Blue-Green-Red format
// ---------------------------------------------------------------------------
static unsigned char s_colorMap7[128][3] = {
    { 0, 0, 0 }, // 0
    { 255, 0, 0 }, // 1 - Blue
    { 255, 21, 0 }, // 2 - Blue (Green Rising)
    { 255, 34, 0 },
    { 255, 46, 0 },
    { 255, 59, 0 },
    { 255, 68, 0 },
    { 255, 80, 0 },
    { 255, 93, 0 },
    { 255, 106, 0 },
    { 255, 119, 0 },
    { 255, 127, 0 },
    { 255, 140, 0 },
    { 255, 153, 0 },
    { 255, 165, 0 },
    { 255, 178, 0 },
    { 255, 191, 0 },
    { 255, 199, 0 },
    { 255, 212, 0 },
    { 255, 225, 0 },
    { 255, 238, 0 },
    { 255, 250, 0 }, // 21 - End of Blue's Reign

    { 250, 255, 0 }, // 22 - Green (Blue Fading)
    { 237, 255, 0 },
    { 225, 255, 0 },
    { 212, 255, 0 },
    { 199, 255, 0 },
    { 191, 255, 0 },
    { 178, 255, 0 },
    { 165, 255, 0 },
    { 153, 255, 0 },
    { 140, 255, 0 },
    { 127, 255, 0 },
    { 119, 255, 0 },
    { 106, 255, 0 },
    { 93, 255, 0 },
    { 80, 255, 0 },
    { 67, 255, 0 },
    { 59, 255, 0 },
    { 46, 255, 0 },
    { 33, 255, 0 },
    { 21, 255, 0 },
    { 8, 255, 0 },
    { 0, 255, 0 }, // 43 - Green

    { 0, 255, 12 }, // 44 - Green/ Red Rising
    { 0, 255, 25 },
    { 0, 255, 38 },
    { 0, 255, 51 },
    { 0, 255, 63 },
    { 0, 255, 72 },
    { 0, 255, 84 },
    { 0, 255, 97 },
    { 0, 255, 110 },
    { 0, 255, 123 },
    { 0, 255, 131 },
    { 0, 255, 144 },
    { 0, 255, 157 },
    { 0, 255, 170 },
    { 0, 255, 182 },
    { 0, 255, 191 },
    { 0, 255, 203 },
    { 0, 255, 216 },
    { 0, 255, 229 },
    { 0, 255, 242 },
    { 0, 255, 255 }, // 64 - Green + Red (Yellow)

    { 0, 246, 255 }, // 65 - Red, Green Fading
    { 0, 233, 255 },
    { 0, 220, 255 },
    { 0, 208, 255 },
    { 0, 195, 255 },
    { 0, 187, 255 },
    { 0, 174, 255 },
    { 0, 161, 255 },
    { 0, 148, 255 },
    { 0, 135, 255 },
    { 0, 127, 255 },
    { 0, 114, 255 },
    { 0, 102, 255 },
    { 0, 89, 255 },
    { 0, 76, 255 },
    { 0, 63, 255 },
    { 0, 55, 255 },
    { 0, 42, 255 },
    { 0, 29, 255 },
    { 0, 16, 255 },
    { 0, 4, 255 }, // 85 - End Red/Green Fading

    { 4, 0, 255 }, // 86 - Red/ Blue Rising
    { 16, 0, 255 },
    { 29, 0, 255 },
    { 42, 0, 255 },
    { 55, 0, 255 },
    { 63, 0, 255 },
    { 76, 0, 255 },
    { 89, 0, 255 },
    { 102, 0, 255 },
    { 114, 0, 255 },
    { 127, 0, 255 },
    { 135, 0, 255 },
    { 148, 0, 255 },
    { 161, 0, 255 },
    { 174, 0, 255 },
    { 186, 0, 255 },
    { 195, 0, 255 },
    { 208, 0, 255 },
    { 221, 0, 255 },
    { 233, 0, 255 },
    { 246, 0, 255 },
    { 255, 0, 255 }, // 107 - Blue + Red

    { 255, 0, 242 }, // 108 - Blue/ Red Fading
    { 255, 0, 229 },
    { 255, 0, 216 },
    { 255, 0, 204 },
    { 255, 0, 191 },
    { 255, 0, 182 },
    { 255, 0, 169 },
    { 255, 0, 157 },
    { 255, 0, 144 },
    { 255, 0, 131 },
    { 255, 0, 123 },
    { 255, 0, 110 },
    { 255, 0, 97 },
    { 255, 0, 85 },
    { 255, 0, 72 },
    { 255, 0, 63 },
    { 255, 0, 50 },
    { 255, 0, 38 },
    { 255, 0, 25 }, // 126 - Blue-ish
    { 225, 240, 240 } // 127 - White ?
};

inline int GetColorIntFromRGB(int r, int g, int b) {
    if (b == 0 && g == 0 && r == 0)
        return 0;
    else if (b > 224 && g > 239 && r > 239)
        return 127;
    else if (b == 255 && r == 0) {
        for (int i = 1; i < 22; ++i)
            if (g > s_colorMap7[i - 1][1] && g <= s_colorMap7[i][1])
                return i;
    } else if (g == 255 && r == 0) {
        for (int i = 22; i < 44; ++i)
            if (b < s_colorMap7[i - 1][0] && b >= s_colorMap7[i][0])
                return i;
    } else if (b == 0 && g == 255) {
        for (int i = 44; i < 65; ++i)
            if (r > s_colorMap7[i - 1][2] && r <= s_colorMap7[i][2])
                return i;
    } else if (b == 0 && r == 255) {
        for (int i = 65; i < 86; ++i)
            if (g < s_colorMap7[i - 1][1] && g >= s_colorMap7[i][1])
                return i;
    } else if (g == 0 && r == 255) {
        for (int i = 86; i < 108; ++i)
            if (b > s_colorMap7[i - 1][0] && b <= s_colorMap7[i][0])
                return i;
    } else if (b == 255 && g == 0) {
        for (int i = 108; i < 127; ++i)
            if (r < s_colorMap7[i - 1][2] && r >= s_colorMap7[i][2])
                return i;
    }

    return 0;
}

class MFT_RGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~MFT_RGB_Midi_FeedbackProcessor() {}
    MFT_RGB_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "MFT_RGB_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        if (g_debugLevel >= DEBUG_LEVEL_NOTICE)
            LogToConsole("[NOTICE] # ForceClear do not force LED off\n");
        // rgba_color color;
        // ForceColorValue(color);
    }

    virtual void SetColorValue(const rgba_color& color) override {
        if (color != lastColor_)
            ForceColorValue(color);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        lastColor_ = color;

        if ((color.r == 177 || color.r == 181) && color.g == 31) // sets different MFT modes
            SendMidiMessage(color.r, color.g, color.b);
        else {
            int colorInt = GetColorIntFromRGB(color.r, color.g, color.b);
            if (colorInt == 0) {
                if (g_debugLevel >= DEBUG_LEVEL_NOTICE)
                    LogToConsole("[NOTICE] ForceColorValue ignores color=0, do not force LED off\n");
            } else {
                SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], colorInt);
                SendMidiMessage(midiFeedbackMessage1_.midi_message[0] + 1, midiFeedbackMessage1_.midi_message[1], 47);
            }
        }
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
    }
};
