#pragma once

#include <string>
#include <vector>

struct IntegratorConfigIssue {
    int lineNumber = 0;
    std::string message;
};

struct MidiIoConfig {
    int lineNumber = 0;
    std::string name;
    int channelCount = 0;
    int inputPort = 0;
    int outputPort = 0;
    int refreshRate = 0;
    int maxMessagesPerRun = 0;
};

struct OscIoConfig {
    int lineNumber = 0;
    std::string type;
    std::string name;
    int channelCount = 0;
    std::string receiveOnPort;
    std::string transmitToPort;
    std::string transmitToIpAddress;
    int maxPacketsPerRun = 0;
};

struct SurfaceAssignmentConfig {
    int lineNumber = 0;
    std::string surfaceName;
    std::string surfaceId;
    std::string mainZoneProfileId;
    std::string fxZoneProfileId;
    int startChannel = 0;
};

struct ListenerConfig {
    int lineNumber = 0;
    std::string broadcasterName;
    std::string listenerName;
    bool goHome = false;
    bool modifiers = false;
    bool fxMenu = false;
    bool selectedTrackFx = false;
    bool selectedTrackSends = false;
    bool selectedTrackReceives = false;
};

struct PageConfig {
    int lineNumber = 0;
    std::string name;
    bool followsMcp = true;
    bool synchPages = true;
    bool scrollLink = false;
    bool scrollSynch = false;
    std::vector<SurfaceAssignmentConfig> surfaces;
    std::vector<ListenerConfig> listeners;
};

struct IntegratorConfig {
    std::vector<MidiIoConfig> midiIo;
    std::vector<OscIoConfig> oscIo;
    std::vector<PageConfig> pages;
    std::vector<IntegratorConfigIssue> issues;
    int skippedSurfaceCount = 0;
    std::string fatalError;
};

IntegratorConfig ParseIntegratorConfig(const std::string& configPath);
