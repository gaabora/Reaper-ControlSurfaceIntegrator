#pragma once
//
//  fb_xtouch.h — X-Touch family feedback processors:
//    XTouchDisplay (handles text + track colour SysEx for X-Touch/XT).

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class XTouchDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
private:
    int offset_;
    int displayType_;
    int displayRow_;
    int channel_;
    int preventUpdateTrackColors_;
    string lastStringSent_;
    vector<rgba_color> currentTrackColors_;

    enum XTouchColor {
        COLOR_INVALID = -1,
        COLOR_OFF = 0,
        COLOR_RED,
        COLOR_GREEN,
        COLOR_YELLOW,
        COLOR_BLUE,
        COLOR_MAGENTA,
        COLOR_CYAN,
        COLOR_WHITE
    };

    static XTouchColor colorFromString(const char *str)
    {
        if (IsSameString(str, "Black"))   return COLOR_OFF;
        if (IsSameString(str, "Red"))     return COLOR_RED;
        if (IsSameString(str, "Green"))   return COLOR_GREEN;
        if (IsSameString(str, "Yellow"))  return COLOR_YELLOW;
        if (IsSameString(str, "Blue"))    return COLOR_BLUE;
        if (IsSameString(str, "Magenta")) return COLOR_MAGENTA;
        if (IsSameString(str, "Cyan"))    return COLOR_CYAN;
        if (IsSameString(str, "White"))   return COLOR_WHITE;
        return COLOR_INVALID;
    }
        
public:
    virtual ~XTouchDisplay_Midi_FeedbackProcessor() {}
    XTouchDisplay_Midi_FeedbackProcessor(CSurfIntegrator *const csi, Midi_ControlSurface *surface, Widget *widget, int displayUpperLower, int displayType, int displayRow, int channel) : Midi_FeedbackProcessor(csi, surface, widget), offset_(displayUpperLower  *56), displayType_(displayType), displayRow_(displayRow), channel_(channel)
    {
        preventUpdateTrackColors_ = false;
        rgba_color color;
        for (int i = 0; i < surface_->GetNumChannels(); ++i)
            currentTrackColors_.push_back(color);
    }
        
    virtual const char *GetName() override { return "XTouchDisplay_Midi_FeedbackProcessor"; }

    virtual void ForceClear() override
    {
        const PropertyList properties;
        ForceValue(properties, "");
    }
    
    virtual void SetXTouchDisplayColors(const char *colors) override
    {
        preventUpdateTrackColors_ = true;
        
        vector<string> currentColors;
        GetTokens(currentColors, colors);
        
        struct
        {
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
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x72;
        
        for (int i = 0; i < surface_->GetNumChannels(); ++i)
        {
            XTouchColor msgColor = COLOR_WHITE;
            const char *curColorStr = currentColors.size() == 1 ? currentColors[0].c_str() : currentColors[i].c_str();
            XTouchColor curColor = colorFromString(curColorStr);
            if (curColor != COLOR_INVALID) msgColor = curColor;
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = (int)msgColor;
        }
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
    
    virtual void RestoreXTouchDisplayColors() override
    {
        preventUpdateTrackColors_ = false;
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
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x66;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayType_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayRow_;
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = channel_  * 7 + offset_;
        
        int cnt = 0;
        while (cnt++ < 7)
            midiSysExData.evt.midi_message[midiSysExData.evt.size++] = *text ? *text++ : ' ';
        
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
        
        ForceUpdateTrackColors();
    }
    
    virtual void ForceUpdateTrackColors() override
    {
        if (preventUpdateTrackColors_) return;
        
        struct
        {
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
        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x72;

        vector<rgba_color> trackColors;
        for (int i = 0; i < surface_->GetNumChannels(); ++i)
            trackColors.push_back(surface_->GetTrackColorForChannel(i));

        for (int i = 0; i < (int)trackColors.size(); ++i)
        {
            if (lastStringSent_ == "")
            {
                midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x07; // White
            }
            else
            {
                rgba_color color = trackColors[i];
                currentTrackColors_[i] = color;
                int surfaceColor = (int)rgbToColor(color.r, color.g, color.b);
                midiSysExData.evt.midi_message[midiSysExData.evt.size++] = surfaceColor;
            }
        }

        midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
        SendMidiSysExMessage(&midiSysExData.evt);
    }
};
