#pragma once
// fb_qcon.h — QCon family feedback processors: QConProXMasterVUMeter, QConLiteDisplay.

class QConProXMasterVUMeter_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    double minDB_;
    double maxDB_;
    int param_;

public:
    virtual ~QConProXMasterVUMeter_Midi_FeedbackProcessor() {}
    QConProXMasterVUMeter_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int param)
        : Midi_FeedbackProcessor(csi, surface, widget), param_(param) {
        minDB_ = 0.0;
        maxDB_ = 24.0;
    }

    virtual const char* GetName() override { return "QConProXMasterVUMeter_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override {
        const PropertyList properties;
        ForceValue(properties, 0.0);
    }

    int GetMidiMeterValue(double value) {
        int midiValue = 0;
        double dbValue = VAL2DB(normalizedToVol(value));
        if      (dbValue >= -60.1 && dbValue < -48.1)  midiValue = 0x01;
        else if (dbValue >= -48.1 && dbValue < -42.1)  midiValue = 0x02;
        else if (dbValue >= -42.1 && dbValue < -36.1)  midiValue = 0x03;
        else if (dbValue >= -36.1 && dbValue < -30.1)  midiValue = 0x04;
        else if (dbValue >= -30.1 && dbValue < -24.1)  midiValue = 0x05;
        else if (dbValue >= -24.1 && dbValue < -18.1)  midiValue = 0x06;
        else if (dbValue >= -18.1 && dbValue < -12.1)  midiValue = 0x07;
        else if (dbValue >= -12.1 && dbValue < -9.1 )  midiValue = 0x08;
        else if (dbValue >= -9.1  && dbValue < -6.1 )  midiValue = 0x09;
        else if (dbValue >= -6.1  && dbValue < -3.1 )  midiValue = 0x0A;
        else if (dbValue >= -3.1  && dbValue <  0.1 )  midiValue = 0x0B;
        else if (dbValue >=  0.1                    )  midiValue = 0x0E;
        return midiValue;
    }

    virtual void SetValue(const PropertyList& properties, double value) override {
        if (g_debugLevel >= DEBUG_LEVEL_DEBUG)
            LogToConsole("[DEBUG] QConProXMasterVUMeter_Midi_FeedbackProcessor: 0xd1, 0x%02x\n", (param_ << 4) | GetMidiMeterValue(value));
        SendMidiMessage(0xd1, (param_ << 4) | GetMidiMeterValue(value), 0);
    }

    virtual void ForceValue(const PropertyList& properties, double value) override {
        ForceMidiMessage(0xd1, (param_ << 4) | GetMidiMeterValue(value), 0);
    }
};

class QConLiteDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
{
private:
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~QConLiteDisplay_Midi_FeedbackProcessor() {}
    QConLiteDisplay_Midi_FeedbackProcessor(CSurfIntegrator* const csi, Midi_ControlSurface* surface, Widget* widget, int displayUpperLower, int displayType, int displayRow, int channel)
        : Midi_FeedbackProcessor(csi, surface, widget), offset_(displayUpperLower * 28), displayType_(displayType), displayRow_(displayRow), channel_(channel) {
    }

    virtual const char* GetName() override { return "QConLiteDisplay_Midi_FeedbackProcessor"; }

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

        struct {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_ * 7 + offset_;

        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};
