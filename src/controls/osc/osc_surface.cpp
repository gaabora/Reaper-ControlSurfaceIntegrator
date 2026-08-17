#include "../integrator.h"
#include "../surface_parser.h"
#include "osc_widgets.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC I/O Manager
////////////////////////////////////////////////////////////////////////////////////////////////////////
struct OSCSurfaceSocket {
    string surfaceName;
    oscpkt::UdpSocket* socket;
    int refcnt;

    OSCSurfaceSocket(const string& name, oscpkt::UdpSocket* s) {
        surfaceName = name;
        socket = s;
        refcnt = 1;
    }
    ~OSCSurfaceSocket() { delete socket; }
};

static WDL_PtrList<OSCSurfaceSocket> s_inputSockets;
static WDL_PtrList<OSCSurfaceSocket> s_outputSockets;

static oscpkt::UdpSocket* GetInputSocketForPort(string surfaceName, int inputPort) {
    for (int i = 0; i < s_inputSockets.GetSize(); ++i)
        if (s_inputSockets.Get(i)->surfaceName == surfaceName) {
            s_inputSockets.Get(i)->refcnt++;
            return s_inputSockets.Get(i)->socket; // return existing
        }

    // otherwise make new
    oscpkt::UdpSocket* newInputSocket = new oscpkt::UdpSocket();
    if (newInputSocket) {
        newInputSocket->bindTo(inputPort);
        if (!newInputSocket->isOk()) {
            //cerr << "Error opening port " << PORT_NUM << ": " << inSocket_.errorMessage() << "\n";
            delete newInputSocket;
            return NULL;
        }
        s_inputSockets.Add(new OSCSurfaceSocket(surfaceName, newInputSocket));
        return newInputSocket;
    }
    return NULL;
}

static oscpkt::UdpSocket* GetOutputSocketForAddressAndPort(const string& surfaceName, const string& address, int outputPort) {
    for (int i = 0; i < s_outputSockets.GetSize(); ++i)
        if (s_outputSockets.Get(i)->surfaceName == surfaceName) {
            s_outputSockets.Get(i)->refcnt++;
            return s_outputSockets.Get(i)->socket; // return existing
        }

    // otherwise make new
    oscpkt::UdpSocket* newOutputSocket = new oscpkt::UdpSocket();
    if (newOutputSocket) {
        if (!newOutputSocket->connectTo(address, outputPort)) {
            //cerr << "Error connecting " << remoteDeviceIP_ << ": " << outSocket_.errorMessage() << "\n";
            delete newOutputSocket;
            return NULL;
        }
        if (!newOutputSocket->isOk()) {
            //cerr << "Error opening port " << outPort_ << ": " << outSocket_.errorMessage() << "\n";
            delete newOutputSocket;
            return NULL;
        }
        s_outputSockets.Add(new OSCSurfaceSocket(surfaceName, newOutputSocket));
        return newOutputSocket;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurfaceIO
////////////////////////////////////////////////////////////////////////////////////////////////////////

OSC_X32ControlSurfaceIO::OSC_X32ControlSurfaceIO(CSurfIntegrator* const csi, const char* surfaceName, int channelCount, const char* receiveOnPort, const char* transmitToPort, const char* transmitToIpAddress, int maxPacketsPerRun)
    : OSC_ControlSurfaceIO(csi, surfaceName, channelCount, receiveOnPort, transmitToPort, transmitToIpAddress, maxPacketsPerRun) {
}

OSC_ControlSurfaceIO::OSC_ControlSurfaceIO(CSurfIntegrator* const csi, const char* surfaceName, int channelCount, const char* receiveOnPort, const char* transmitToPort, const char* transmitToIpAddress, int maxPacketsPerRun)
    : csi_(csi), name_(surfaceName), channelCount_(channelCount) {
    // private:
    maxPacketsPerRun_ = maxPacketsPerRun < 0 ? 0 : maxPacketsPerRun;

    if (!IsSameString(receiveOnPort, transmitToPort)) {
        inSocket_ = GetInputSocketForPort(surfaceName, atoi(receiveOnPort));
        outSocket_ = GetOutputSocketForAddressAndPort(surfaceName, transmitToIpAddress, atoi(transmitToPort));
    } else {
        // WHEN INPUT AND OUTPUT SOCKETS ARE THE SAME -- DO MAGIC :)
        oscpkt::UdpSocket* inSocket = GetInputSocketForPort(surfaceName, atoi(receiveOnPort));

        struct addrinfo hints;
        struct addrinfo* addressInfo;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET; // IPV4
        hints.ai_socktype = SOCK_DGRAM; // UDP
        hints.ai_flags = 0x00000001; // socket address is intended for bind
        getaddrinfo(transmitToIpAddress, transmitToPort, &hints, &addressInfo);
        memcpy(&(inSocket->remote_addr), (void*) (addressInfo->ai_addr), addressInfo->ai_addrlen);

        inSocket_ = inSocket;
        outSocket_ = inSocket;
    }
}

OSC_ControlSurfaceIO::~OSC_ControlSurfaceIO() {
    Sleep(OSC_DRAIN_SLEEP_MS);
    int count = 0;
    while (packetQueue_.GetSize() >= sizeof(int) && ++count < 100) {
        BeginRun();
        if (count) Sleep(OSC_DRAIN_SLEEP_MS);
    }
    if (inSocket_) {
        for (int x = 0; x < s_inputSockets.GetSize(); ++x) {
            if (s_inputSockets.Get(x)->socket == inSocket_) {
                if (!--s_inputSockets.Get(x)->refcnt) s_inputSockets.Delete(x, true);
                break;
            }
        }
    }
    if (outSocket_ && outSocket_ != inSocket_) {
        for (int x = 0; x < s_outputSockets.GetSize(); ++x) {
            if (s_outputSockets.Get(x)->socket == outSocket_) {
                if (!--s_outputSockets.Get(x)->refcnt) s_outputSockets.Delete(x, true);
                break;
            }
        }
    }
}

void OSC_ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface* surface) {
    if (inSocket_ != NULL && inSocket_->isOk()) {
        while (inSocket_->receiveNextPacket(0)) {
            packetReader_.init(inSocket_->packetData(), inSocket_->packetSize());
            oscpkt::Message* message;

            while (packetReader_.isOk() && (message = packetReader_.popMessage()) != 0) {
                if (message->arg().isFloat()) {
                    float value = 0;
                    message->arg().popFloat(value);
                    surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
                } else if (message->arg().isInt32()) {
                    int value;
                    message->arg().popInt32(value);
                    surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
                }
            }
        }
    }
}

void OSC_X32ControlSurfaceIO::HandleExternalInput(OSC_ControlSurface* surface) {
    if (inSocket_ != NULL && inSocket_->isOk()) {
        while (inSocket_->receiveNextPacket(0)) {
            packetReader_.init(inSocket_->packetData(), inSocket_->packetSize());
            oscpkt::Message* message;

            while (packetReader_.isOk() && (message = packetReader_.popMessage()) != 0) {
                if (message->arg().isFloat()) {
                    float value = 0;
                    message->arg().popFloat(value);
                    surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
                } else if (message->arg().isInt32()) {
                    int value;
                    message->arg().popInt32(value);

                    if (message->addressPattern() == "/-stat/selidx") {
                        string x32Select = message->addressPattern() + "/";

                        if (value < 10) x32Select += "0";

                        char buf[64];
                        snprintf(buf, sizeof(buf), "%d", value);
                        x32Select += buf;

                        surface->ProcessOSCMessage(x32Select.c_str(), 1.0);
                    } else
                        surface->ProcessOSCMessage(message->addressPattern().c_str(), value);
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// OSC_ControlSurface
////////////////////////////////////////////////////////////////////////////////////////////////////////
void OSC_ControlSurface::ProcessOSCWidgetFile(const string& filePath) { SurfaceTemplateParser::ParseOSCTemplate(filePath, this); }
void OSC_ControlSurface::ProcessOSCWidget(int& lineNumber, ifstream& surfaceTemplateFile, const vector<string>& in_tokens) { SurfaceTemplateParser::ParseOSCWidget(lineNumber, surfaceTemplateFile, in_tokens, this); }

OSC_ControlSurface::OSC_ControlSurface(CSurfIntegrator* const csi, IPageContext* page, const char* name, int channelOffset, const char* templateFilename, const char* zoneFolder, const char* vendorFxZoneFolder, const char* userFxZoneFolder, OSC_ControlSurfaceIO* surfaceIO, const SettingsValues& settings, const SettingOverrides& settingOverrides)
    : ControlSurface(csi, page, name, surfaceIO->GetChannelCount(), channelOffset, settings, settingOverrides), surfaceIO_(surfaceIO)
{
    ProcessOSCWidgetFile(templateFilename);
    InitHardwiredWidgets(this);
    InitZoneManager(csi_, this, zoneFolder, vendorFxZoneFolder, userFxZoneFolder);
}

void OSC_ControlSurface::ProcessOSCMessage(const char* message, double value) {
    if (MessageGeneratorsByMessage_.find(message) != MessageGeneratorsByMessage_.end())
        MessageGeneratorsByMessage_[message]->ProcessMessage(value);
    if (g_surfaceInDisplay) LogToConsole("IN <- %s %s %f\n", name_.c_str(), message, value);
}

void OSC_ControlSurface::SendOSCMessage(const char* zoneName) {
    string oscAddress(zoneName);
    ReplaceAllWith(oscAddress, s_BadFileChars, "_");
    oscAddress = "/" + oscAddress;

    surfaceIO_->SendOSCMessage(oscAddress.c_str());
    if (g_surfaceOutDisplay) LogToConsole("->LoadingZone---->%s\n", name_.c_str());
}

void OSC_ControlSurface::SendOSCMessage(const char* oscAddress, int value) {
    surfaceIO_->SendOSCMessage(oscAddress, value);
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %d # Surface::SendOSCMessage 1\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(const char* oscAddress, double value) {
    surfaceIO_->SendOSCMessage(oscAddress, value);
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %f # Surface::SendOSCMessage 2\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(const char* oscAddress, const char* value) {
    surfaceIO_->SendOSCMessage(oscAddress, value);
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %s # Surface::SendOSCMessage 3\n", name_.c_str(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor* feedbackProcessor, const char* oscAddress, double value) {
    surfaceIO_->SendOSCMessage(oscAddress, value);
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %f # Surface::SendOSCMessage 4\n", feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor* feedbackProcessor, const char* oscAddress, int value) {
    surfaceIO_->SendOSCMessage(oscAddress, value);
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %s %d # Surface::SendOSCMessage 5\n", name_.c_str(), feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}

void OSC_ControlSurface::SendOSCMessage(OSC_FeedbackProcessor* feedbackProcessor, const char* oscAddress, const char* value) {
    surfaceIO_->SendOSCMessage(oscAddress, value);
    if (g_surfaceOutDisplay) LogToConsole("OUT->%s %s %s %s # Surface::SendOSCMessage 6\n", name_.c_str(), feedbackProcessor->GetWidget()->GetName(), oscAddress, value);
}
