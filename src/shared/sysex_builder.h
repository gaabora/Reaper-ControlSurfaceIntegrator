//
//  sysex_builder.h
//  reaper_csurf_integrator
//
//  Reusable SysEx message construction helper.
//  Replaces the repeated raw struct manipulation pattern scattered across
//  ~20+ sites in midi_widgets.h.
//
//  Usage:
//      SysExBuilder builder;
//      builder.begin()
//             .add(0x00).add(0x00).add(0x66).add(deviceByte)
//             .add(displayRow)
//             .addText(text, 7)
//             .end();
//      SendMidiSysExMessage(builder.message());
//

#ifndef csi_sysex_builder_h
#define csi_sysex_builder_h

#include "types.h"

class SysExBuilder
{
    struct {
        MIDI_event_ex_t evt;
        char data[256];
    } midiSysExData_;

public:
    SysExBuilder()
    {
        midiSysExData_.evt.frame_offset = 0;
        midiSysExData_.evt.size = 0;
    }

    // Reset and start a new SysEx message (adds leading 0xF0)
    SysExBuilder& begin()
    {
        midiSysExData_.evt.frame_offset = 0;
        midiSysExData_.evt.size = 0;
        midiSysExData_.evt.midi_message[midiSysExData_.evt.size++] = 0xF0;
        return *this;
    }

    // Append a single byte
    SysExBuilder& add(unsigned char byte)
    {
        midiSysExData_.evt.midi_message[midiSysExData_.evt.size++] = byte;
        return *this;
    }

    // Append multiple bytes from a raw array
    SysExBuilder& addBytes(const unsigned char* bytes, int count)
    {
        for (int i = 0; i < count; ++i)
            midiSysExData_.evt.midi_message[midiSysExData_.evt.size++] = bytes[i];
        return *this;
    }

    // Append a null-terminated string, padding with spaces to maxLen.
    // If maxLen <= 0, copies until the null terminator (no padding).
    SysExBuilder& addText(const char* text, int maxLen = -1)
    {
        if (maxLen <= 0) {
            while (*text)
                midiSysExData_.evt.midi_message[midiSysExData_.evt.size++] = (unsigned char)*text++;
        } else {
            int cnt = 0;
            while (cnt++ < maxLen)
                midiSysExData_.evt.midi_message[midiSysExData_.evt.size++] =
                    *text ? (unsigned char)*text++ : ' ';
        }
        return *this;
    }

    // Append the trailing 0xF7 end-of-sysex marker
    SysExBuilder& end()
    {
        midiSysExData_.evt.midi_message[midiSysExData_.evt.size++] = 0xF7;
        return *this;
    }

    // Access the completed message for sending
    MIDI_event_ex_t* message() { return &midiSysExData_.evt; }

    // Number of bytes written so far (including 0xF0 and 0xF7 if added)
    int size() const { return midiSysExData_.evt.size; }
};

#endif /* csi_sysex_builder_h */
