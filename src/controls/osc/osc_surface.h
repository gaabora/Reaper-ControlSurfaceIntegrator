#pragma once
//
//  osc_surface.h — OSC_ControlSurfaceIO and OSC_ControlSurface
//
#include "../preamble.h"
#include "../control_surface.h"
class OSC_ControlSurfaceIO
{
protected:
    CSurfIntegrator* const csi_;
    string const name_;
    int const channelCount_;
    oscpkt::UdpSocket* inSocket_ = NULL;
    oscpkt::UdpSocket* outSocket_ = NULL;
    oscpkt::PacketReader packetReader_;
    oscpkt::PacketWriter packetWriter_;
    oscpkt::Storage storageTmp_;
    int maxBundleSize_ = 0; // 0 = no bundles (would only be useful if the destination doesn't support bundles)
    int maxPacketsPerRun_; // 0 = no limit
    int sentPacketCount_ = 0; // count of packets sent this Run() slice, after maxPacketsPerRun_ packtees go into packetQueue_
    WDL_Queue packetQueue_;

public:
    OSC_ControlSurfaceIO(CSurfIntegrator* const csi, const char* name, int channelCount, const char* receiveOnPort, const char* transmitToPort, const char* transmitToIpAddress, int maxPacketsPerRun);
    virtual ~OSC_ControlSurfaceIO();

    const char* GetName() { return name_.c_str(); }

    const int GetChannelCount() { return channelCount_; }

    virtual void HandleExternalInput(OSC_ControlSurface* surface);

    void QueuePacket(const void* p, int sz) {
        if (WDL_NOT_NORMALLY(!outSocket_)) return;
        if (WDL_NOT_NORMALLY(!p || sz < 1)) return;
        if (WDL_NOT_NORMALLY(packetQueue_.GetSize() > 32 * 1024 * 1024)) return; // drop packets after 32MB queued
        if (maxPacketsPerRun_ != 0 && sentPacketCount_ >= maxPacketsPerRun_) {
            void* wr = packetQueue_.Add(NULL, sz + sizeof(int));
            if (WDL_NORMALLY(wr != NULL)) {
                memcpy(wr, &sz, sizeof(int));
                memcpy((char*) wr + sizeof(int), p, sz);
            }
        } else {
            outSocket_->sendPacket(p, sz);
            sentPacketCount_++;
        }
    }

    void QueueOSCMessage(oscpkt::Message* message) { 
        // NULL message flushes any latent bundles
        if (outSocket_ != NULL && outSocket_->isOk()) {
            if (maxBundleSize_ > 0 && packetWriter_.packetSize() > 0) {
                bool send_bundle;
                if (message) {
                    // oscpkt lacks the ability to calculate the size of a Message?
                    storageTmp_.clear();
                    message->packMessage(storageTmp_, true);
                    send_bundle = (packetWriter_.packetSize() + storageTmp_.size() > maxBundleSize_);
                } else {
                    send_bundle = true;
                }

                if (send_bundle) {
                    packetWriter_.endBundle();
                    QueuePacket(packetWriter_.packetData(), packetWriter_.packetSize());
                    packetWriter_.init();
                }
            }

            if (message) {
                if (maxBundleSize_ > 0 && packetWriter_.packetSize() == 0) {
                    packetWriter_.startBundle();
                }

                packetWriter_.addMessage(*message);

                if (maxBundleSize_ <= 0) {
                    QueuePacket(packetWriter_.packetData(), packetWriter_.packetSize());
                    packetWriter_.init();
                }
            }
        }
    }

    void SendOSCMessage(const char* oscAddress, double value) {
        if (outSocket_ != NULL && outSocket_->isOk()) {
            oscpkt::Message message;
            message.init(oscAddress).pushFloat((float) value);
            QueueOSCMessage(&message);
        }
    }

    void SendOSCMessage(const char* oscAddress, int value) {
        if (outSocket_ != NULL && outSocket_->isOk()) {
            oscpkt::Message message;
            message.init(oscAddress).pushInt32(value);
            QueueOSCMessage(&message);
        }
    }

    void SendOSCMessage(const char* oscAddress, const char* value) {
        if (outSocket_ != NULL && outSocket_->isOk()) {
            oscpkt::Message message;
            message.init(oscAddress).pushStr(value);
            QueueOSCMessage(&message);
        }
    }

    void SendOSCMessage(const char* value) {
        if (outSocket_ != NULL && outSocket_->isOk()) {
            oscpkt::Message message;
            message.init(value);
            QueueOSCMessage(&message);
        }
    }

    void BeginRun() {
        sentPacketCount_ = 0;
        // send any latent packets first
        while (packetQueue_.GetSize() >= sizeof(int)) {
            int sza;
            if (maxPacketsPerRun_ != 0 && sentPacketCount_ >= maxPacketsPerRun_)
                break;

            memcpy(&sza, packetQueue_.Get(), sizeof(int));
            packetQueue_.Advance(sizeof(int));
            if (WDL_NOT_NORMALLY(sza < 0 || packetQueue_.GetSize() < sza)) {
                packetQueue_.Clear();
            } else {
                if (WDL_NORMALLY(outSocket_ != NULL)) {
                    outSocket_->sendPacket(packetQueue_.Get(), sza);
                }
                packetQueue_.Advance(sza);
                sentPacketCount_++;
            }
        }
        packetQueue_.Compact();
    }

    virtual void Run() {
        QueueOSCMessage(NULL); // flush any latent bundles
    }
};

class OSC_X32ControlSurfaceIO : public OSC_ControlSurfaceIO
{
protected:
    DWORD X32HeartBeatRefreshInterval_ = 5000;
    DWORD X32HeartBeatLastRefreshTime_ = GetTickCount() - 30000;

public:
    OSC_X32ControlSurfaceIO(CSurfIntegrator* const csi, const char* name, int channelCount, const char* receiveOnPort, const char* transmitToPort, const char* transmitToIpAddress, int maxPacketsPerRun);
    virtual ~OSC_X32ControlSurfaceIO() {}

    virtual void HandleExternalInput(OSC_ControlSurface* surface) override;

    void Run() override {
        DWORD currentTime = GetTickCount();

        if ((currentTime - X32HeartBeatLastRefreshTime_) > X32HeartBeatRefreshInterval_) {
            X32HeartBeatLastRefreshTime_ = currentTime;
            SendOSCMessage("/xremote");
        }

        OSC_ControlSurfaceIO::Run();
    }
};

class OSC_ControlSurface : public ControlSurface
{
private:
    OSC_ControlSurfaceIO* const surfaceIO_;

public:
    OSC_ControlSurface(CSurfIntegrator* const csi, IPageContext* page, const char* name, int channelOffset, const char* templateFilename, const char* zoneFolder, const char* vendorFxZoneFolder, const char* userFxZoneFolder, OSC_ControlSurfaceIO* surfaceIO, const SettingsValues& settings, const SettingOverrides& settingOverrides);

    virtual ~OSC_ControlSurface() {}

    void ProcessOSCMessage(const char* message, double value);
    virtual void SendOSCMessage(const char* zoneName) override;
    virtual void SendOSCMessage(const char* zoneName, int value) override;
    virtual void SendOSCMessage(const char* zoneName, double value) override;
    virtual void SendOSCMessage(const char* zoneName, const char* value) override;

    virtual void RequestUpdate() override {
        surfaceIO_->BeginRun();
        ControlSurface::RequestUpdate();
        surfaceIO_->Run();
    }

    virtual void HandleExternalInput() override {
        surfaceIO_->HandleExternalInput(this);
    }
};
