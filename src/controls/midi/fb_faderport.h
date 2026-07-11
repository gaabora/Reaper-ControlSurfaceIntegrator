#pragma once
// fb_faderport.h — Faderport family feedback processors: FPTwoStateRGB, FaderportRGB, FPVUMeter, FPValueBar, FPDisplay, FPScribbleStripMode.
// FaderportClassicFader14Bit_Midi_FeedbackProcessor lives in fb_generic.h because it shares the generic fader protocol.

class FPTwoStateRGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    bool active_;

public:
    virtual ~FPTwoStateRGB_Midi_FeedbackProcessor() {}
    FPTwoStateRGB_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {
        active_ = false;
    }

    virtual const char* GetName() override { return "FPTwoStateRGB_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        rgba_color color;
        ForceColorValue(color);
        active_ = false;
    }

    virtual void SetValue(const PropertyList& properties, double active) override { active_ = active != 0; }

    virtual void SetColorValue(const rgba_color& color) override {
        int RGBIndexDivider = 1 * 2;

        if (active_ == false) RGBIndexDivider = 9 * 2;

        rgba_color c;
        c.r = color.r / RGBIndexDivider;
        c.g = color.g / RGBIndexDivider;
        c.b = color.b / RGBIndexDivider;
        c.a = color.a;
        if (c != lastColor_)
            ForceColorValue(c);
    }

    virtual void ForceColorValue(const rgba_color& color) override {
        lastColor_ = color;
        SendMidiMessage(0x90, midiFeedbackMessage1_.midi_message[1], 0x7f);
        SendMidiMessage(0x91, midiFeedbackMessage1_.midi_message[1], color.r);
        SendMidiMessage(0x92, midiFeedbackMessage1_.midi_message[1], color.g);
        SendMidiMessage(0x93, midiFeedbackMessage1_.midi_message[1], color.b);

        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
    }
};

class FaderportRGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~FaderportRGB_Midi_FeedbackProcessor() {}
    FaderportRGB_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "FaderportRGB_Midi_FeedbackProcessor"; }

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

        SendMidiMessage(0x90, midiFeedbackMessage1_.midi_message[1], 0x7f);
        SendMidiMessage(0x91, midiFeedbackMessage1_.midi_message[1], color.r / 2);
        SendMidiMessage(0x92, midiFeedbackMessage1_.midi_message[1], color.g / 2);
        SendMidiMessage(0x93, midiFeedbackMessage1_.midi_message[1], color.b / 2);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
    }
};

class FPVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int channelNumber_;
    int lastMidiValue_;
    bool isClipOn_;

public:
    virtual ~FPVUMeter_Midi_FeedbackProcessor() {}
    FPVUMeter_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int channelNumber)
        : Midi_FeedbackProcessor(csi, surface, widget)
        , channelNumber_(channelNumber) {
        lastMidiValue_ = 0;
        isClipOn_ = false;
    }

    virtual const char* GetName() override { return "FPVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (lastMidiValue_ == value || GetMidiValue(value) < 7)
            return;

        if (channelNumber_ < 8)
            SendMidiMessage(0xd0 + channelNumber_, GetMidiValue(value), 0);
        else
            SendMidiMessage(0xc0 + channelNumber_ - 8, GetMidiValue(value), 0);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        lastMidiValue_ = (int) value;
        if (channelNumber_ < 8)
            ForceMidiMessage(0xd0 + channelNumber_, GetMidiValue(value), 0);
        else
            ForceMidiMessage(0xc0 + channelNumber_ - 8, GetMidiValue(value), 0);
    }

    int GetMidiValue(double value) {
        return int(value * 0xa0);
    }
};

class FPValueBar_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    double lastValue_;
    int channel_;

    int GetValueBarType(const PropertyList& properties) {
        // 0: Normal, 1: Bipolar, 2: Fill, 3: Spread, 4: Off
        const char* barstyle = properties.get_prop(PropertyType_BarStyle);
        if (barstyle) {
            if (IsSameString(barstyle, "Normal")) return 0;
            else if (IsSameString(barstyle, "BiPolar")) return 1;
            else if (IsSameString(barstyle, "Fill")) return 2;
            else if (IsSameString(barstyle, "Spread")) return 3;
        }
        return 4;
    }

public:
    virtual ~FPValueBar_Midi_FeedbackProcessor() {}
    FPValueBar_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int channel)
        : Midi_FeedbackProcessor(csi, surface, widget)
        , channel_(channel) {
        lastValue_ = 0;
    }

    virtual const char* GetName() override { return "FPValueBar_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (value == lastValue_) return;
        ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        lastValue_ = value;

        if (channel_ < 8) {
            SendMidiMessage(0xb0, channel_ + 0x30, int(lastValue_ * 127.0));
            SendMidiMessage(0xb0, channel_ + 0x38, GetValueBarType(properties));
        } else {
            SendMidiMessage(0xb0, channel_ - 8 + 0x40, int(lastValue_ * 127.0));
            SendMidiMessage(0xb0, channel_ - 8 + 0x48, GetValueBarType(properties));
        }
    }
};

class FPDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;

    int GetTextAlign(const PropertyList& properties) {
        // Center: 0, Left: 1, Right: 2
        const char* textalign = properties.get_prop(PropertyType_TextAlign);
        if (textalign) {
            if (IsSameString(textalign, "Left")) return 1;
            else if (IsSameString(textalign, "Right")) return 2;
        }
        return 0;
    }

    int GetTextInvert(const PropertyList& properties) {
        const char* textinvert = properties.get_prop(PropertyType_TextInvert);
        if (textinvert && IsSameString(textinvert, "Yes")) return 4;
        return 0;
    }

public:
    virtual ~FPDisplay_Midi_FeedbackProcessor() {}
    FPDisplay_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayType, int channel, int displayRow)
        : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channel_(channel), displayRow_(displayRow) {
        lastStringSent_ = " ";
    }

    virtual const char* GetName() override { return "FPDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, "");
    }

    virtual void SetValue(const PropertyList& properties, const char* const& inputText) override {
        if (!IsSameString(inputText, lastStringSent_.c_str()))
            ForceValue(properties, inputText);
    }

    virtual void ForceValue(const PropertyList& properties, const char* const& inputText) override {
        lastStringSent_ = inputText;

        char tmp[MEDBUF];
        const char* text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (text[0] == 0) text = "                            ";
        int invert = lastStringSent_ == "" ? 0 : GetTextInvert(properties);
        int align = 0x0000000 + invert + GetTextAlign(properties);

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x06;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x12;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = align;

        int length = (int) strlen(text);
        if (length > 30) length = 30;
        int count = 0;
        while (count < length) {
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text++;
            count++;
        }

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;

        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

class FPScribbleStripMode_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int displayType_;
    int channel_;
    int lastMode_;

    int GetMode(const PropertyList& properties) {
        int param = 2;
        const char* mode = properties.get_prop(PropertyType_Mode);
        if (mode) param = atoi(mode);
        if (param >= 0 && param < 9) return param;
        return 2;
    }

public:
    virtual ~FPScribbleStripMode_Midi_FeedbackProcessor() {}
    FPScribbleStripMode_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayType, int channel)
        : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channel_(channel) {
        lastMode_ = 0;
    }

    virtual const char* GetName() override { return "FPScribbleStripMode_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (lastMode_ == GetMode(properties)) return;
        ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        lastMode_ = GetMode(properties);

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;

        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x06;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x13;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00 + lastMode_;

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};
