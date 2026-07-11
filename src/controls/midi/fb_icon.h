#pragma once
//
//  fb_icon.h — Icon console feedback processors:
//    IconDisplay (variant of MCUDisplay with configurable SysEx manufacturer bytes).

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class IconDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int sysExByte1_;
    int sysExByte2_;
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    string lastStringSent_;

public:
    virtual ~IconDisplay_Midi_FeedbackProcessor() {}
    IconDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayUpperLower, int displayType, int displayRow, int channel, int sysExByte1, int sysExByte2) : Midi_FeedbackProcessor(csi, surface, widget), offset_(displayUpperLower  *56), displayType_(displayType), displayRow_(displayRow), channel_(channel), sysExByte1_(sysExByte1), sysExByte2_(sysExByte2)
    {
    }
    
    virtual const char *GetName() override { return "IconDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }

    virtual void SetValue(const PropertyList &properties, const char * const &inputText) override
    {
        if (!IsSameString(inputText, lastStringSent_.c_str()))
            ForceValue(properties, inputText);
    }
    
    virtual void ForceValue(const PropertyList &properties, const char * const &inputText) override
    {
        lastStringSent_ = inputText;
        
        char tmp[MEDBUF];
        const char *text = GetWidget()->GetSurface()->GetRestrictedLengthText(inputText, tmp, sizeof(tmp));

        if (IsSameString(text, "-150.00")) text = "";

        struct
        {
            MIDI_event_ex_t evt;
            char data[256];
        } midiSysExData;
        midiSysExData.evt.frame_offset = 0;
        midiSysExData.evt.size = 0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = sysExByte1_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = sysExByte2_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  *7 + offset_;
        
        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};
