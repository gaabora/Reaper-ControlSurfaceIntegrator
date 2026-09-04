#include "../integrator.h"
#include "midi_widgets.h"
#include "widget_factory.h"
#include "format2_midi_runtime.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi I/O Manager
////////////////////////////////////////////////////////////////////////////////////////////////////////
struct MidiPort {
    int port, refcnt;
    void* dev;

    MidiPort(int portidx, void* devptr) : port(portidx), refcnt(1), dev(devptr) {};
};

static WDL_TypedBuf<MidiPort> s_midiInputs;
static WDL_TypedBuf<MidiPort> s_midiOutputs;

void ReleaseMidiInput(midi_Input* input) {
    for (int i = 0; i < s_midiInputs.GetSize(); ++i)
        if (s_midiInputs.Get()[i].dev == (void*) input) {
            if (!--s_midiInputs.Get()[i].refcnt) {
                input->stop();
                delete input;
                s_midiInputs.Delete(i);
                break;
            }
        }
}

void ReleaseMidiOutput(midi_Output* output) {
    for (int i = 0; i < s_midiOutputs.GetSize(); ++i)
        if (s_midiOutputs.Get()[i].dev == (void*) output) {
            if (!--s_midiOutputs.Get()[i].refcnt) {
                delete output;
                s_midiOutputs.Delete(i);
                break;
            }
        }
}

midi_Input* GetMidiInputForPort(int inputPort) {
    for (int i = 0; i < s_midiInputs.GetSize(); ++i)
        if (s_midiInputs.Get()[i].port == inputPort) {
            s_midiInputs.Get()[i].refcnt++;
            return (midi_Input*) s_midiInputs.Get()[i].dev;
        }

    midi_Input* newInput = CreateMIDIInput(inputPort);

    if (newInput) {
        newInput->start();
        MidiPort midiInputPort(inputPort, newInput);
        s_midiInputs.Add(midiInputPort);
    }

    return newInput;
}

midi_Output* GetMidiOutputForPort(int outputPort) {
    for (int i = 0; i < s_midiOutputs.GetSize(); ++i)
        if (s_midiOutputs.Get()[i].port == outputPort) {
            s_midiOutputs.Get()[i].refcnt++;
            return (midi_Output*) s_midiOutputs.Get()[i].dev;
        }

    midi_Output* newOutput = CreateMIDIOutput(outputPort, false, NULL);

    if (newOutput) {
        MidiPort midiOutputPort(outputPort, newOutput);
        s_midiOutputs.Add(midiOutputPort);
    }

    return newOutput;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurfaceIO
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Midi_ControlSurfaceIO::HandleExternalInput(Midi_ControlSurface* surface) {
    if (midiInput_) {
        midiInput_->SwapBufsPrecise(GetTickCount(), GetTickCount());
        MIDI_eventlist* list = midiInput_->GetReadBuf();
        int bpos = 0;
        MIDI_event_t* evt;
        while ((evt = list->EnumItems(&bpos)))
            surface->ProcessMidiMessage((MIDI_event_ex_t*) evt);
    }
}

static bool IsMidiInputAvailable(int inputPort) {
    char deviceName[MEDBUF];
    return inputPort >= 0 && GetMIDIInputName(inputPort, deviceName, sizeof(deviceName));
}

static bool IsMidiOutputAvailable(int outputPort) {
    char deviceName[MEDBUF];
    return outputPort >= 0 && GetMIDIOutputName(outputPort, deviceName, sizeof(deviceName));
}

bool Midi_ControlSurfaceIO::PollForDeviceReconnect() {
    const DWORD now = GetTickCount();
    if ((now - this->lastDevicePoll_) < 1000) return false;
    this->lastDevicePoll_ = now;

    const bool inputAvailable = IsMidiInputAvailable(this->inputPort_);
    const bool outputAvailable = IsMidiOutputAvailable(this->outputPort_);

    if (!inputAvailable && this->midiInput_) {
        ReleaseMidiInput(this->midiInput_);
        this->midiInput_ = nullptr;
    }

    if (!outputAvailable && this->midiOutput_) {
        ReleaseMidiOutput(this->midiOutput_);
        this->midiOutput_ = nullptr;
    }

    bool reconnected = false;

    if (inputAvailable && !this->midiInput_) {
        this->midiInput_ = GetMidiInputForPort(this->inputPort_);
        reconnected = this->midiInput_ != nullptr;
    }

    if (outputAvailable && !this->midiOutput_) {
        this->midiOutput_ = GetMidiOutputForPort(this->outputPort_);
        reconnected = this->midiOutput_ != nullptr || reconnected;
    }

    if (reconnected && g_debugLevel >= DEBUG_LEVEL_NOTICE)
        LogToConsole("[NOTICE] MIDI device for surface %s is online; reinitializing the surface\n", this->name_.c_str());

    return reconnected;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
Midi_ControlSurface::Midi_ControlSurface(CSurfIntegrator* const csi, IPageContext* page, const char* name, int channelOffset, const char* surfaceFile, const char* zoneFolder, const char* vendorFxZoneFolder, const char* userFxZoneFolder, Midi_ControlSurfaceIO* surfaceIO, const SettingsValues& settings, const SettingOverrides& settingOverrides)
    : ControlSurface(csi, page, name, surfaceIO->GetChannelCount(), channelOffset, settings, settingOverrides), surfaceIO_(surfaceIO) {
    MidiWidgetRegistry::EnsureRegistered();
    Format2MidiRuntimeLoader::Load(surfaceFile, this);
    this->InitHardwiredWidgets(this);
    this->InitializeFormat2Messages();
    this->InitializeMeters();
    this->InitZoneManager(this->csi_, this, zoneFolder, vendorFxZoneFolder, userFxZoneFolder);
}

void Midi_ControlSurface::InitializeFormat2Messages() {
    for (const vector<int>& bytes : this->format2InitializationMessages_) {
        if (bytes.empty()) continue;
        if (bytes[0] == 0xF0) {
            SysExBuilder builder;
            builder.begin();
            for (size_t byteIdx = 1; byteIdx + 1 < bytes.size(); ++byteIdx) builder.add((unsigned char) bytes[byteIdx]);
            builder.end();
            this->SendMidiSysExMessage(builder.message());
        } else if (bytes.size() == 1) this->SendMidiMessage(bytes[0], 0, 0);
        else if (bytes.size() == 2) this->SendMidiMessage(bytes[0], bytes[1], 0);
        else if (bytes.size() == 3) this->SendMidiMessage(bytes[0], bytes[1], bytes[2]);
    }
}

void Midi_ControlSurface::ProcessMidiMessage(const MIDI_event_ex_t* evt) {
    if (g_surfaceRawInDisplay) LogToConsole("[DEBUG] IN <- %s %02x %02x %02x \n", name_.c_str(), evt->midi_message[0], evt->midi_message[1], evt->midi_message[2]);

    string threeByteKey = to_string(evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100 + evt->midi_message[2]);
    string twoByteKey = to_string(evt->midi_message[0] * 0x10000 + evt->midi_message[1] * 0x100);
    string oneByteKey = to_string(evt->midi_message[0] * 0x10000);

    // At this point we don't know how much of the message comprises the key, so try all three
    if (MessageGeneratorsByMessage_.find(threeByteKey) != MessageGeneratorsByMessage_.end())
        MessageGeneratorsByMessage_[threeByteKey]->ProcessMidiMessage(evt);
    else if (MessageGeneratorsByMessage_.find(twoByteKey) != MessageGeneratorsByMessage_.end())
        MessageGeneratorsByMessage_[twoByteKey]->ProcessMidiMessage(evt);
    else if (MessageGeneratorsByMessage_.find(oneByteKey) != MessageGeneratorsByMessage_.end())
        MessageGeneratorsByMessage_[oneByteKey]->ProcessMidiMessage(evt);
}

void Midi_ControlSurface::SendMidiSysExMessage(MIDI_event_ex_t* midiMessage) {
    surfaceIO_->QueueMidiSysExMessage(midiMessage);
    if (g_surfaceOutDisplay) {
        string output = "OUT->";
        output += name_ + " ";
        char buf[32];
        for (int i = 0; i < midiMessage->size; ++i) {
            snprintf(buf, sizeof(buf), "%02x ", midiMessage->midi_message[i]);
            output += buf;
        }
        output = "[DEBUG] " + output + " # Midi_ControlSurface::SendMidiSysExMessage\n";
        LogToConsole(output.c_str());
    }
}

void Midi_ControlSurface::SendMidiMessage(int first, int second, int third) {
    surfaceIO_->SendMidiMessage(first, second, third);
    if (g_surfaceOutDisplay) LogToConsole("[DEBUG] %s %02x %02x %02x # Midi_ControlSurface::SendMidiMessage\n", ("OUT->" + name_).c_str(), first, second, third);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Midi_ControlSurface — MCU Init
////////////////////////////////////////////////////////////////////////////////////////////////////////
static struct
{
    MIDI_event_ex_t evt;
    char data[MEDBUF];
} s_midiSysExData;

void Midi_ControlSurface::SendSysexInitData(int line[], int numElem) {
    memset(s_midiSysExData.data, 0, sizeof(s_midiSysExData.data));

    s_midiSysExData.evt.frame_offset = 0;
    s_midiSysExData.evt.size = 0;

    for (int i = 0; i < numElem; ++i)
        s_midiSysExData.evt.midi_message[s_midiSysExData.evt.size++] = line[i];

    SendMidiSysExMessage(&s_midiSysExData.evt);
}

void Midi_ControlSurface::InitializeMCU() {
    int line1[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    int line2[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x00, 0xF7 };
    int line3[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x21, 0x01, 0xF7 };
    int line4[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x00, 0x01, 0xF7 };
    int line5[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x01, 0x01, 0xF7 };
    int line6[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x02, 0x01, 0xF7 };
    int line7[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x03, 0x01, 0xF7 };
    int line8[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x04, 0x01, 0xF7 };
    int line9[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x05, 0x01, 0xF7 };
    int line10[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x06, 0x01, 0xF7 };
    int line11[] = { 0xF0, 0x00, 0x00, 0x66, 0x14, 0x20, 0x07, 0x01, 0xF7 };

    SendSysexInitData(line1, NUM_ELEM(line1));
    SendSysexInitData(line2, NUM_ELEM(line2));
    SendSysexInitData(line3, NUM_ELEM(line3));
    SendSysexInitData(line4, NUM_ELEM(line4));
    SendSysexInitData(line5, NUM_ELEM(line5));
    SendSysexInitData(line6, NUM_ELEM(line6));
    SendSysexInitData(line7, NUM_ELEM(line7));
    SendSysexInitData(line8, NUM_ELEM(line8));
    SendSysexInitData(line9, NUM_ELEM(line9));
    SendSysexInitData(line10, NUM_ELEM(line10));
    SendSysexInitData(line11, NUM_ELEM(line11));
}

void Midi_ControlSurface::InitializeMCUXT() {
    int line1[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    int line2[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x00, 0xF7 };
    int line3[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x21, 0x01, 0xF7 };
    int line4[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x00, 0x01, 0xF7 };
    int line5[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x01, 0x01, 0xF7 };
    int line6[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x02, 0x01, 0xF7 };
    int line7[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x03, 0x01, 0xF7 };
    int line8[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x04, 0x01, 0xF7 };
    int line9[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x05, 0x01, 0xF7 };
    int line10[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x06, 0x01, 0xF7 };
    int line11[] = { 0xF0, 0x00, 0x00, 0x66, 0x15, 0x20, 0x07, 0x01, 0xF7 };

    SendSysexInitData(line1, NUM_ELEM(line1));
    SendSysexInitData(line2, NUM_ELEM(line2));
    SendSysexInitData(line3, NUM_ELEM(line3));
    SendSysexInitData(line4, NUM_ELEM(line4));
    SendSysexInitData(line5, NUM_ELEM(line5));
    SendSysexInitData(line6, NUM_ELEM(line6));
    SendSysexInitData(line7, NUM_ELEM(line7));
    SendSysexInitData(line8, NUM_ELEM(line8));
    SendSysexInitData(line9, NUM_ELEM(line9));
    SendSysexInitData(line10, NUM_ELEM(line10));
    SendSysexInitData(line11, NUM_ELEM(line11));
}
