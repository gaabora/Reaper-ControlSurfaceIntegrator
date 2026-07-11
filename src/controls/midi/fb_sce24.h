#pragma once
// fb_sce24.h — SCE24 family feedback processors: SCE24TwoStateLED, SCE24OLED, SCE24Text, SCE24Encoder.
//  Also contains the LEDRingRangeColor struct and GetColorValues() helper used by SCE24Encoder::Configure().

struct LEDRingRangeColor {
    rgba_color ringColor;
    int ringRangeHigh;
    int ringRangeMedium;
    int ringRangeLow;

    LEDRingRangeColor() {
        ringRangeHigh = 0;
        ringRangeMedium = 0;
        ringRangeLow = 0;
    }
};

static const vector<LEDRingRangeColor>& GetColorValues(const string& inputProperty) {
    static vector<LEDRingRangeColor> s_encoderRingColors;
    s_encoderRingColors.clear();

    string property = inputProperty;
    ReplaceAllWith(property, "\"", "");

    vector<string> colorDefs;
    GetTokens(colorDefs, property.c_str(), '+');

    for (int defi = 0; defi < (int) colorDefs.size(); ++defi) {
        vector<string> rangeDefs;
        GetTokens(rangeDefs, colorDefs[defi], '-');

        if (rangeDefs.size() > 2) {
            LEDRingRangeColor color;
            GetColorValue(rangeDefs[2].c_str(), color.ringColor);

            for (int i = atoi(rangeDefs[0].c_str()); i <= atoi(rangeDefs[1].c_str()); ++i) {
                if (i < 7)
                    color.ringRangeLow += 1 << i;
                else if (i > 6 && i < 14)
                    color.ringRangeMedium += 1 << (i - 7);
                else if (i > 13)
                    color.ringRangeHigh += 1 << (i - 14);
            }
            s_encoderRingColors.push_back(color);
        }
    }

    LEDRingRangeColor color;
    color.ringColor.r = 0;
    color.ringColor.g = 0;
    color.ringColor.b = 0;
    color.ringRangeLow = 7;
    color.ringRangeMedium = 0;
    color.ringRangeHigh = 0;
    s_encoderRingColors.push_back(color);

    return s_encoderRingColors;
}

class SCE24TwoStateLED_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    double lastValue_;

public:
    virtual ~SCE24TwoStateLED_Midi_FeedbackProcessor() {}
    SCE24TwoStateLED_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {
        lastValue_ = 0.0;
    }

    virtual const char* GetName() override { return "SCE24TwoStateLED_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (lastValue_ != value)
            ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        lastValue_ = value;

        rgba_color color;

        if (value == 0 && properties.get_prop(PropertyType_OffColor))
            GetColorValue(properties.get_prop(PropertyType_OffColor), color);
        else if (value == 1 && properties.get_prop(PropertyType_OnColor))
            GetColorValue(properties.get_prop(PropertyType_OnColor), color);

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = color.b / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

class SCE24OLED_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int topMargin_;
    int bottomMargin_;
    int font_;
    string lastStringSent_;
    rgba_color lastTextColorSent_;
    rgba_color lastBackgroundColorSent_;

public:
    virtual ~SCE24OLED_Midi_FeedbackProcessor() {}
    SCE24OLED_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1, int topMargin, int bottomMargin, int font)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1), topMargin_(topMargin), bottomMargin_(bottomMargin), font_(font) {
        lastStringSent_ = "";
    }

    virtual const char* GetName() override { return "SCE24OLED_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        char buf[SMLBUF];
        PropertyList properties;
        properties.set_prop(PropertyType_DisplayText, "");
        snprintf(buf, sizeof(buf), "%d", topMargin_);
        properties.set_prop(PropertyType_TopMargin, buf);
        snprintf(buf, sizeof(buf), "%d", bottomMargin_);
        properties.set_prop(PropertyType_BottomMargin, buf);
        snprintf(buf, sizeof(buf), "%d", font_);
        properties.set_prop(PropertyType_Font, buf);
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        ForceValue(properties, value);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        rgba_color backgroundColor;
        rgba_color textColor;

        const char* top = properties.get_prop(PropertyType_TopMargin);
        if (top) topMargin_ = atoi(top);

        const char* bottom = properties.get_prop(PropertyType_BottomMargin);
        if (bottom) bottomMargin_ = atoi(bottom);

        const char* font = properties.get_prop(PropertyType_Font);
        if (font) font_ = atoi(font);

        const char* col = properties.get_prop((value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) ? PropertyType_BackgroundColorOff : PropertyType_BackgroundColorOn);
        if (col)
            GetColorValue(col, backgroundColor);

        col = properties.get_prop((value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) ? PropertyType_TextColorOff : PropertyType_TextColorOn);
        if (col) GetColorValue(col, textColor);

        if (lastBackgroundColorSent_ == backgroundColor && lastTextColorSent_ == textColor) return;

        lastBackgroundColorSent_ = backgroundColor;
        lastTextColorSent_ = textColor;

        const char* displayText = properties.get_prop(PropertyType_DisplayText);
        if (!displayText) displayText = "";

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = topMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = bottomMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = font_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.b / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.b / 2;
        for (int i = 0; displayText[i]; ++i)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayText[i];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

class SCE24Text_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int topMargin_;
    int bottomMargin_;
    int font_;
    string lastStringSent_;

public:
    virtual ~SCE24Text_Midi_FeedbackProcessor() {}
    SCE24Text_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1, int topMargin, int bottomMargin, int font)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1), topMargin_(topMargin), bottomMargin_(bottomMargin), font_(font) {
        lastStringSent_ = "";
    }
    virtual const char* GetName() override { return "SCE24Text_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 63;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }

    virtual void SetValue(const PropertyList& properties, const char* const& inputText) override {
        if (!IsSameString(inputText, lastStringSent_.c_str())) 
            ForceValue(properties, inputText);
    }

    virtual void ForceValue(const PropertyList& properties, const char* const& inputText) override {
        lastStringSent_ = inputText;

        char tmp[MEDBUF];
        const char* displayText = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        rgba_color backgroundColor;
        rgba_color textColor;

        const char* top = properties.get_prop(PropertyType_TopMargin);
        if (top) topMargin_ = atoi(top);

        const char* bottom = properties.get_prop(PropertyType_BottomMargin);
        if (bottom) bottomMargin_ = atoi(bottom);

        const char* font = properties.get_prop(PropertyType_Font);
        if (font) font_ = atoi(font);

        const char* col = properties.get_prop(PropertyType_BackgroundColor);
        if (col) GetColorValue(col, backgroundColor);

        col = properties.get_prop(PropertyType_TextColor);
        if (col) GetColorValue(col, textColor);

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = topMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = bottomMargin_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = font_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = backgroundColor.b / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.r / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.g / 2;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = textColor.b / 2;
        for (int i = 0; displayText[i]; ++i)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayText[i];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};

class SCE24Encoder_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
public:
    virtual ~SCE24Encoder_Midi_FeedbackProcessor() {}
    SCE24Encoder_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {}

    virtual const char* GetName() override { return "SCE24Encoder_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 120;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 127;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 5;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);

        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(properties, value));
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0], midiFeedbackMessage1_.midi_message[1], GetMidiValue(properties, value));
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
            val = (1 + ((valueInt * 15) >> 8)) | (displayMode << 4);
        else
            val = (1 + ((valueInt * 15) >> 7)) | (displayMode << 4);

        return val + 64;
    }

    virtual void Configure(const vector<unique_ptr<ActionContext>>& contexts) override {
        if (contexts.size() == 0)
            return;

        const PropertyList& properties = contexts[0]->GetWidgetProperties();

        vector<LEDRingRangeColor> colors;

        const char* ledringcolor = properties.get_prop(PropertyType_LEDRingColor);
        const char* ledringcolors = properties.get_prop(PropertyType_LEDRingColors);
        const char* pushcolor = properties.get_prop(PropertyType_PushColor);

        if (properties.get_prop(PropertyType_Push) && pushcolor) {
            LEDRingRangeColor color;
            GetColorValue(pushcolor, color.ringColor);
            color.ringRangeLow = 7;
            color.ringRangeMedium = 0;
            color.ringRangeHigh = 0;
            colors.push_back(color);

            color.ringColor.r = 0; color.ringColor.g = 0; color.ringColor.b = 0;
            color.ringRangeLow = 120; 
            color.ringRangeMedium = 127;
            color.ringRangeHigh = 15;
            colors.push_back(color);
        } else if (ledringcolor) {
            LEDRingRangeColor color;
            GetColorValue(ledringcolor, color.ringColor);
            color.ringRangeLow = 120;
            color.ringRangeMedium = 127;
            color.ringRangeHigh = 15;
            colors.push_back(color);

            color.ringColor.r = 0; color.ringColor.g = 0; color.ringColor.b = 0;
            color.ringRangeLow = 7;
            color.ringRangeMedium = 0;
            color.ringRangeHigh = 0;
            colors.push_back(color);
        } else if (ledringcolors) {
            colors = GetColorValues(ledringcolors);
        }

        if (colors.size() == 0) {
            LEDRingRangeColor color;
            color.ringRangeLow = 127;
            color.ringRangeMedium = 127;
            color.ringRangeHigh = 15;
            colors.push_back(color);
        }

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;

        for (int i = 0; i < (int) colors.size(); ++i) {
            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = 0;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x02;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x38;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x01;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = midiFeedbackMessage1_.midi_message[1];
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringRangeLow;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringRangeMedium;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringRangeHigh;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringColor.r / 2;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringColor.g / 2;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = colors[i].ringColor.b / 2;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
            SendMidiSysExMessage(&midiSysExData.evt);
        }
    }
};
