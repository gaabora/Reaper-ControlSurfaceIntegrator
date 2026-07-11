#pragma once
// fb_asparion.h — Asparion family feedback processors: AsparionRGB, AsparionEncoder, AsparionVUMeter, AsparionDisplay.

class AsparionRGB_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int preventUpdateTrackColors_;

public:
    virtual ~AsparionRGB_Midi_FeedbackProcessor() {}
    AsparionRGB_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {
        preventUpdateTrackColors_ = false;
    }

    virtual const char* GetName() override { return "AsparionRGB_Midi_FeedbackProcessor"; }

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
        SendMidiMessage(0x91, midiFeedbackMessage1_.midi_message[1], color.r / 2);
        SendMidiMessage(0x92, midiFeedbackMessage1_.midi_message[1], color.g / 2);
        SendMidiMessage(0x93, midiFeedbackMessage1_.midi_message[1], color.b / 2);
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] [%s] ForceColorValue %d %d %d\n", widget_->GetName(), color.r, color.g, color.b);
    }

    virtual void ForceUpdateTrackColors() override {
        if (preventUpdateTrackColors_) return;
        ForceColorValue(surface_->GetTrackColorForChannel(widget_->GetChannelNumber() - 1));
    }
};

class AsparionEncoder_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int displayMode_;

public:
    virtual ~AsparionEncoder_Midi_FeedbackProcessor() {}
    AsparionEncoder_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, MIDI_event_ex_t feedback1)
        : Midi_FeedbackProcessor(csi, surface, widget, feedback1) {
        displayMode_ = 0;
    }

    virtual const char* GetName() override { return "Encoder_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(midiFeedbackMessage1_.midi_message[0] + displayMode_, midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(midiFeedbackMessage1_.midi_message[0] + displayMode_, midiFeedbackMessage1_.midi_message[1] + 0x20, GetMidiValue(properties, value));
    }

    int GetMidiValue(const PropertyList& properties, double value) {
        displayMode_ = 2;
        const char* ringstyle = properties.get_prop(PropertyType_RingStyle);
        if (ringstyle) {
            if (IsSameString(ringstyle, "Fill")) displayMode_ = 1;
            else if (IsSameString(ringstyle, "Dot")) displayMode_ = 2;
        }
        return int(value * 127);
    }
};

class AsparionVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
protected:
    int displayType_;
    int channelNumber_;
    int lastMidiValue_;
    bool isClipOn_;
    bool isRight_;

public:
    virtual ~AsparionVUMeter_Midi_FeedbackProcessor() {}
    AsparionVUMeter_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayType, int channelNumber, bool isRight)
        : Midi_FeedbackProcessor(csi, surface, widget), displayType_(displayType), channelNumber_(channelNumber), isRight_(isRight) {
        lastMidiValue_ = 0;
        isClipOn_ = false;
    }

    virtual const char* GetName() override { return "AsparionVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        SendMidiMessage(isRight_ ? 0xd1 : 0xd0, (channelNumber_ << 4) | GetMidiValue(value), 0);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(isRight_ ? 0xd1 : 0xd0, (channelNumber_ << 4) | GetMidiValue(value), 0);
    }

    int GetMidiValue(double value) {
        int midiValue = int(value * 0x0f);
        if (midiValue > 0x0d) midiValue = 0x0d;
        return midiValue;
    }
};

class AsparionDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int displayRow_;
    int displayType_;
    int displayTextType_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~AsparionDisplay_Midi_FeedbackProcessor() {}
    AsparionDisplay_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayRow, int displayType, int displayTextType, int channel)
        : Midi_FeedbackProcessor(csi, surface, widget), displayRow_(displayRow), displayType_(displayType), displayTextType_(displayTextType), channel_(channel) {
    }

    virtual const char* GetName() override { return "AsparionDisplay_Midi_FeedbackProcessor"; }

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

        if (IsSameString(text, "-150.00")) text = ""; //FIXME: here and everywhere "-150.00" to const

        SysExBuilder builder;
        builder.begin().add(0x00).add(0x00).add(0x66).add(displayType_).add(displayTextType_);
        if (displayRow_ != 3) {
            builder.add(channel_ * 12).add(displayRow_);
        } else {
            builder.add(channel_ * 8);
        }
        const int linelen = displayRow_ == 3 ? 8 : 12;
        builder.addText(text, linelen).end();
        SendMidiSysExMessage(builder.message());
    }
};
