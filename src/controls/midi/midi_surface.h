#pragma once
// midi_surface.h — Midi_ControlSurfaceIO and Midi_ControlSurface

#include "../preamble.h"
#include "../control_surface.h"
#include "../feedback.h"
class Midi_ControlSurfaceIO
{
private:
    // -----------------------------------------------------------------------
    // Background MIDI reader thread (Phase B thread-safety)
    //
    // Polls midiInput_ at ~1 kHz in a dedicated thread, placing raw event
    // bytes into incomingMidiQueue_.  HandleExternalInput() on the Run()
    // thread swaps the queue out under incomingMidiMutex_ and processes
    // the messages — completely decoupling hardware I/O from the 30 Hz loop.
    // -----------------------------------------------------------------------
    std::queue<std::vector<unsigned char>> incomingMidiQueue_;
    WDL_Mutex incomingMidiMutex_;
    std::atomic<bool> midiReaderActive_{ false };
    std::thread midiReaderThread_;

    void MidiReaderLoop() {
        while (midiReaderActive_.load(std::memory_order_relaxed)) {
            if (midiInput_) {
                midiInput_->SwapBufsPrecise(GetTickCount(), GetTickCount());
                MIDI_eventlist* list = midiInput_->GetReadBuf();
                int bpos = 0;
                MIDI_event_t* evt;

                // Collect outside the lock to minimise contention.
                std::vector<std::vector<unsigned char>> batch;
                while ((evt = list->EnumItems(&bpos))) {
                    const int sz = evt->size;
                    if (sz > 0 && sz <= 512)
                        batch.emplace_back(evt->midi_message, evt->midi_message + sz);
                }
                if (!batch.empty()) {
                    WDL_MutexLock lock(&incomingMidiMutex_);
                    for (auto& msg : batch)
                        incomingMidiQueue_.push(std::move(msg));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

protected:
    CSurfIntegrator* const csi_;
    string const name_;
    int const channelCount_;
    midi_Input* const midiInput_;
    midi_Output* const midiOutput_;
    WDL_Queue messageQueue_;
    const int maxMesssagesPerRun_;

    void SendMidiSysexMessage(MIDI_event_ex_t* midiMessage) {
        if (midiOutput_) midiOutput_->SendMsg(midiMessage, -1);
    }

public:
    Midi_ControlSurfaceIO(CSurfIntegrator* csi, const char* name, int channelCount, midi_Input* midiInput, midi_Output* midiOutput, int surfaceRefreshRate, int maxMesssagesPerRun)
        : csi_(csi), name_(name), channelCount_(channelCount), midiInput_(midiInput), midiOutput_(midiOutput), surfaceRefreshRate_(surfaceRefreshRate), maxMesssagesPerRun_(maxMesssagesPerRun)
    {
        if (midiInput_) {
            midiReaderActive_ = true;
            midiReaderThread_ = std::thread(&Midi_ControlSurfaceIO::MidiReaderLoop, this);
        }
    }

    ~Midi_ControlSurfaceIO() {
        // Stop the reader thread before releasing the MIDI handle it accesses.
        if (midiReaderThread_.joinable()) {
            midiReaderActive_ = false;
            midiReaderThread_.join();
        }
        if (midiInput_) ReleaseMidiInput(midiInput_);
        if (midiOutput_) ReleaseMidiOutput(midiOutput_);
    }

    int surfaceRefreshRate_;

    const char* GetName() { return name_.c_str(); }

    const int GetChannelCount() { return channelCount_; }

    void HandleExternalInput(Midi_ControlSurface* surface);

    void QueueMidiSysExMessage(MIDI_event_ex_t* midiMessage) {
        if (WDL_NOT_NORMALLY(midiMessage->size > 255)) return;

        unsigned char size = (unsigned char) midiMessage->size;
        messageQueue_.Add(&size, 1);
        messageQueue_.Add(midiMessage->midi_message, midiMessage->size);
    }

    void SendMidiMessage(int first, int second, int third) { if (midiOutput_) midiOutput_->Send(first, second, third, -1); }

    void Run() {
        int numSent = 0;

        while ((maxMesssagesPerRun_ == 0 || numSent < maxMesssagesPerRun_) && messageQueue_.Available() >= 1) {
            const unsigned char* msg = (const unsigned char*) messageQueue_.Get();
            const int msg_len = (int) *msg;
            if (WDL_NOT_NORMALLY(messageQueue_.Available() < 1 + msg_len)) // not enough data in queue, should not happen
                break;
            struct {
                MIDI_event_ex_t evt;
                char data[256];
            } midiSysExData;

            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = msg_len;
            memcpy(midiSysExData.evt.midi_message, msg + 1, msg_len);
            messageQueue_.Advance(1 + msg_len);
            SendMidiSysexMessage(&midiSysExData.evt);
            numSent++;
        }
        messageQueue_.Compact();
    }

    void Flush() {
        while (messageQueue_.Available() >= 1) {
            Sleep(2);

            const unsigned char* msg = (const unsigned char*) messageQueue_.Get();
            const int msg_len = (int) *msg;
            if (WDL_NOT_NORMALLY(messageQueue_.Available() < 1 + msg_len)) // not enough data in queue, should not happen
                break;
            struct {
                MIDI_event_ex_t evt;
                char data[256];
            } midiSysExData;

            midiSysExData.evt.frame_offset = 0;
            midiSysExData.evt.size = msg_len;
            memcpy(midiSysExData.evt.midi_message, msg + 1, msg_len);
            messageQueue_.Advance(1 + msg_len);
            SendMidiSysexMessage(&midiSysExData.evt);
        }
    }
};

class Midi_ControlSurface : public ControlSurface
{
private:
    Midi_ControlSurfaceIO* const surfaceIO_;

    DWORD lastRun_ = 0;

    void ProcessMidiWidget(int& lineNumber, ifstream& surfaceTemplateFile, const vector<string>& in_tokens);

    void ProcessMIDIWidgetFile(const string& filePath, Midi_ControlSurface* surface);

    // special processing for MCU meters
    bool hasMCUMeters_ = false;
    int displayType_ = 0x14;

    void InitializeMCU();
    void InitializeMCUXT();

    virtual void InitializeMeters() {
        if (hasMCUMeters_) {
            if (displayType_ == 0x14) InitializeMCU();
            else InitializeMCUXT();
        }
    }

    void SendSysexInitData(int line[], int numElem);

public:
    Midi_ControlSurface(CSurfIntegrator* const csi, IPageContext* page, const char* name, int channelOffset, const char* surfaceFile, const char* zoneFolder, const char* fxZoneFolder, Midi_ControlSurfaceIO* surfaceIO);

    virtual ~Midi_ControlSurface() {}

    void ProcessMidiMessage(const MIDI_event_ex_t* evt);
    virtual void SendMidiSysExMessage(MIDI_event_ex_t* midiMessage) override;
    virtual void SendMidiMessage(int first, int second, int third) override;

    virtual void SetHasMCUMeters(int displayType) {
        hasMCUMeters_ = true;
        displayType_ = displayType;
    }

    bool GetHasMCUMeters(void) { return hasMCUMeters_; }

    virtual void HandleExternalInput() override { surfaceIO_->HandleExternalInput(this); }

    virtual void FlushIO() override { surfaceIO_->Flush(); }

    virtual void RequestUpdate() override {
        const DWORD now = GetTickCount();
        const DWORD threshold = (DWORD) (1000 / max(surfaceIO_->surfaceRefreshRate_, 1));
        if ((now - lastRun_) < threshold) return;
        lastRun_ = now;
        surfaceIO_->Run();
        ControlSurface::RequestUpdate();
    }
};
