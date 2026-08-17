#include "integrator_config_parser.h"

#include "preamble.h"

static bool IsYesProperty(const PropertyList& properties, PropertyType type) {
    const char* value = properties.get_prop(type);
    return value && IsSameString(value, "Yes");
}

static void AddConfigIssue(IntegratorConfig& config, int lineNumber, const string& message) {
    config.issues.push_back({ lineNumber, message });
}

static void AddSettingConfigIssue(IntegratorConfig& config, int lineNumber, const string& message) {
    config.settingsValid = false;
    config.issues.push_back({ lineNumber, message, true });
}

static bool SplitConfigPropertyToken(const string& token, string& key, string& value) {
    const size_t separator = token.find('=');
    if (separator == string::npos || separator == 0 || separator == token.size() - 1 || token.find('=', separator + 1) != string::npos) return false;
    key = token.substr(0, separator);
    value = token.substr(separator + 1);
    return true;
}

static bool IsSurfaceAssignmentProperty(const string& propertyName) {
    return propertyName == "Surface" || propertyName == "SurfaceFolder" || propertyName == "ZoneFolder" || propertyName == "FXZoneFolder" || propertyName == "StartChannel";
}

static void ParseSettingTokens(IntegratorConfig& config, SettingOverrides& overrides, int lineNumber, const vector<string>& tokens, size_t startIndex) {
    if (overrides.firstLineNumber == 0) overrides.firstLineNumber = lineNumber;
    for (size_t tokenIdx = startIndex; tokenIdx < tokens.size(); tokenIdx++) {
        string settingName;
        string settingValue;
        if (!SplitConfigPropertyToken(tokens[tokenIdx], settingName, settingValue)) {
            overrides.valid = false;
            AddSettingConfigIssue(config, lineNumber, "Invalid setting token: " + tokens[tokenIdx]);
            continue;
        }
        if (!FindSettingDefinition(settingName)) {
            overrides.valid = false;
            AddSettingConfigIssue(config, lineNumber, "Unknown setting: " + settingName);
            continue;
        }
        const auto existingLine = overrides.lineNumbers.find(settingName);
        if (existingLine != overrides.lineNumbers.end()) {
            overrides.valid = false;
            AddSettingConfigIssue(config, lineNumber, "Setting " + settingName + " is duplicated; first defined at line " + std::to_string(existingLine->second));
            continue;
        }
        overrides.values[settingName] = settingValue;
        overrides.lineNumbers[settingName] = lineNumber;
    }
}

static void AddSettingsValidationIssues(IntegratorConfig& config, const SettingOverrides& overrides, const vector<SettingValidationIssue>& issues) {
    for (const SettingValidationIssue& issue : issues) {
        const auto lineEntry = overrides.lineNumbers.find(issue.settingName);
        const int lineNumber = lineEntry == overrides.lineNumbers.end() ? overrides.firstLineNumber : lineEntry->second;
        AddSettingConfigIssue(config, lineNumber, issue.message);
    }
}

static void ParseSurfaceIoConfig(IntegratorConfig& config, int lineNumber, const vector<string>& tokens, const PropertyList& properties) {
    const char* type = properties.get_prop(PropertyType_SurfaceType);
    const char* name = properties.get_prop(PropertyType_SurfaceName);
    const char* channelCount = properties.get_prop(PropertyType_SurfaceChannelCount);
    if (!type || !name || !channelCount) {
        AddConfigIssue(config, lineNumber, "SurfaceType requires SurfaceName and SurfaceChannelCount");
        return;
    }
    if (tokens.size() != 7) {
        AddConfigIssue(config, lineNumber, "SurfaceType line must contain exactly seven properties");
        return;
    }

    if (IsSameString(type, s_MidiSurfaceToken)) {
        const char* inputPort = properties.get_prop(PropertyType_MidiInput);
        const char* outputPort = properties.get_prop(PropertyType_MidiOutput);
        const char* refreshRate = properties.get_prop(PropertyType_MIDISurfaceRefreshRate);
        const char* maxMessagesPerRun = properties.get_prop(PropertyType_MaxMIDIMesssagesPerRun);
        if (!inputPort || !outputPort || !refreshRate || !maxMessagesPerRun) {
            AddConfigIssue(config, lineNumber, "MIDI SurfaceType requires MidiInput, MidiOutput, MIDISurfaceRefreshRate, and MaxMIDIMesssagesPerRun");
            return;
        }
        config.midiIo.push_back({ lineNumber, name, atoi(channelCount), atoi(inputPort), atoi(outputPort), atoi(refreshRate), atoi(maxMessagesPerRun) });
        return;
    }

    if (IsSameString(type, s_OSCSurfaceToken) || IsSameString(type, s_OSCX32SurfaceToken)) {
        const char* receiveOnPort = properties.get_prop(PropertyType_ReceiveOnPort);
        const char* transmitToPort = properties.get_prop(PropertyType_TransmitToPort);
        const char* transmitToIpAddress = properties.get_prop(PropertyType_TransmitToIPAddress);
        const char* maxPacketsPerRun = properties.get_prop(PropertyType_MaxPacketsPerRun);
        if (!receiveOnPort || !transmitToPort || !transmitToIpAddress || !maxPacketsPerRun) {
            AddConfigIssue(config, lineNumber, "OSC SurfaceType requires ReceiveOnPort, TransmitToPort, TransmitToIPAddress, and MaxPacketsPerRun");
            return;
        }
        config.oscIo.push_back({ lineNumber, type, name, atoi(channelCount), receiveOnPort, transmitToPort, transmitToIpAddress, atoi(maxPacketsPerRun) });
        return;
    }

    AddConfigIssue(config, lineNumber, "Unsupported SurfaceType: " + string(type));
}

static void ParsePageConfig(IntegratorConfig& config, int lineNumber, const vector<string>& tokens, const PropertyList& properties, int& currentPageIndex, string& currentBroadcaster) {
    const char* pageName = properties.get_prop(PropertyType_PageName);
    currentPageIndex = -1;
    currentBroadcaster.clear();
    if (!pageName || tokens.size() <= 1) {
        AddConfigIssue(config, lineNumber, "PageName line is incomplete");
        return;
    }
    PageConfig page;
    page.lineNumber = lineNumber;
    page.name = pageName;
    page.followsMcp = !properties.get_prop(PropertyType_PageFollowsMCP) || !IsSameString(properties.get_prop(PropertyType_PageFollowsMCP), "No");
    page.synchPages = !properties.get_prop(PropertyType_SynchPages) || !IsSameString(properties.get_prop(PropertyType_SynchPages), "No");
    page.scrollLink = IsYesProperty(properties, PropertyType_ScrollLink);
    page.scrollSynch = IsYesProperty(properties, PropertyType_ScrollSynch);
    config.pages.push_back(page);
    currentPageIndex = static_cast<int>(config.pages.size()) - 1;
}

static void ParseSurfaceAssignment(IntegratorConfig& config, int lineNumber, const vector<string>& baseTokens, const vector<string>& settingTokens, const PropertyList& properties, int currentPageIndex) {
    if (currentPageIndex < 0) {
        AddConfigIssue(config, lineNumber, "Surface assignment appears before a valid PageName line");
        config.skippedSurfaceCount++;
        return;
    }
    const char* surfaceName = properties.get_prop(PropertyType_Surface);
    const char* surfaceId = properties.get_prop(PropertyType_SurfaceFolder);
    const char* startChannel = properties.get_prop(PropertyType_StartChannel);
    if (!surfaceName || !surfaceId || !startChannel || baseTokens.size() != 5) {
        AddConfigIssue(config, lineNumber, "Surface assignment must contain exactly five properties and requires Surface, SurfaceFolder, and StartChannel");
        config.skippedSurfaceCount++;
        return;
    }
    if (surfaceId[0] == '\0') {
        AddConfigIssue(config, lineNumber, "SurfaceFolder must contain a stable surface ID");
        config.skippedSurfaceCount++;
        return;
    }
    const char* mainZoneProfileId = properties.get_prop(PropertyType_ZoneFolder);
    const char* fxZoneProfileId = properties.get_prop(PropertyType_FXZoneFolder);
    SurfaceAssignmentConfig surfaceConfig;
    surfaceConfig.lineNumber = lineNumber;
    surfaceConfig.surfaceName = surfaceName;
    surfaceConfig.surfaceId = surfaceId;
    surfaceConfig.mainZoneProfileId = mainZoneProfileId && mainZoneProfileId[0] != '\0' ? mainZoneProfileId : surfaceId;
    surfaceConfig.fxZoneProfileId = fxZoneProfileId && fxZoneProfileId[0] != '\0' ? fxZoneProfileId : surfaceId;
    surfaceConfig.startChannel = atoi(startChannel);
    ParseSettingTokens(config, surfaceConfig.settingOverrides, lineNumber, settingTokens, 0);
    config.pages[currentPageIndex].surfaces.push_back(std::move(surfaceConfig));
}

static void ParseListenerConfig(IntegratorConfig& config, int lineNumber, const PropertyList& properties, int currentPageIndex, const string& currentBroadcaster) {
    const char* listenerName = properties.get_prop(PropertyType_Listener);
    if (currentPageIndex < 0) {
        AddConfigIssue(config, lineNumber, "Listener appears before a valid PageName line");
        return;
    }
    if (currentBroadcaster.empty()) {
        AddConfigIssue(config, lineNumber, "Listener appears before a Broadcaster line");
        return;
    }
    if (!listenerName || listenerName[0] == '\0') {
        AddConfigIssue(config, lineNumber, "Listener requires a surface name");
        return;
    }
    ListenerConfig listener;
    listener.lineNumber = lineNumber;
    listener.broadcasterName = currentBroadcaster;
    listener.listenerName = listenerName;
    listener.goHome = IsYesProperty(properties, PropertyType_GoHome);
    listener.modifiers = IsYesProperty(properties, PropertyType_Modifiers);
    listener.fxMenu = IsYesProperty(properties, PropertyType_FXMenu);
    listener.selectedTrackFx = IsYesProperty(properties, PropertyType_SelectedTrackFX);
    listener.selectedTrackSends = IsYesProperty(properties, PropertyType_SelectedTrackSends);
    listener.selectedTrackReceives = IsYesProperty(properties, PropertyType_SelectedTrackReceives);
    config.pages[currentPageIndex].listeners.push_back(listener);
}

static IntegratorConfig ParseIntegratorConfigStream(std::istream& configFile, const string& configPath) {
    IntegratorConfig config;
    int lineNumber = 0;
    int currentPageIndex = -1;
    string currentBroadcaster;
    for (string line; getline(configFile, line);) {
        lineNumber++;
        TrimLine(line);

        vector<string> tokens;
        GetTokens(tokens, line);
        if (lineNumber == 1) {
            PropertyList properties;
            if (!tokens.empty()) GetPropertiesFromTokens(0, static_cast<int>(tokens.size()), tokens, properties);
            const char* version = properties.get_prop(PropertyType_Version);
            if (!version) config.fatalError = string(ProductIdentity::ConfigFilename) + " has no Version property on line 1";
            else if (!IsSameString(version, s_MajorVersionToken)) config.fatalError = string(ProductIdentity::ConfigFilename) + " version is " + version + ", expected " + s_MajorVersionToken;
            if (!config.fatalError.empty()) return config;
            continue;
        }

        if (line.empty() || line[0] == '#' || IsCommentedOrEmpty(line)) continue;
        if (tokens.empty()) continue;

        if (tokens[0] == "Settings") {
            if (tokens.size() == 1) AddSettingConfigIssue(config, lineNumber, "Settings line requires at least one setting");
            else ParseSettingTokens(config, config.productSettingOverrides, lineNumber, tokens, 1);
            continue;
        }

        vector<string> baseTokens;
        vector<string> settingTokens;
        bool surfaceAssignmentLine = false;
        for (const string& token : tokens) {
            string propertyName;
            string propertyValue;
            if (SplitConfigPropertyToken(token, propertyName, propertyValue) && propertyName == "Surface") surfaceAssignmentLine = true;
        }
        for (const string& token : tokens) {
            string propertyName;
            string propertyValue;
            const bool propertyToken = SplitConfigPropertyToken(token, propertyName, propertyValue);
            if ((surfaceAssignmentLine && (!propertyToken || !IsSurfaceAssignmentProperty(propertyName))) || (propertyToken && FindSettingDefinition(propertyName))) settingTokens.push_back(token);
            else baseTokens.push_back(token);
        }
        PropertyList properties;
        if (!baseTokens.empty()) GetPropertiesFromTokens(0, static_cast<int>(baseTokens.size()), baseTokens, properties);

        if (properties.get_prop(PropertyType_SurfaceType)) ParseSurfaceIoConfig(config, lineNumber, tokens, properties);
        else if (properties.get_prop(PropertyType_PageName)) ParsePageConfig(config, lineNumber, tokens, properties, currentPageIndex, currentBroadcaster);
        else if (const char* broadcaster = properties.get_prop(PropertyType_Broadcaster)) {
            if (currentPageIndex < 0) AddConfigIssue(config, lineNumber, "Broadcaster appears before a valid PageName line");
            else if (broadcaster[0] == '\0') AddConfigIssue(config, lineNumber, "Broadcaster requires a surface name");
            else currentBroadcaster = broadcaster;
        } else if (properties.get_prop(PropertyType_Listener)) ParseListenerConfig(config, lineNumber, properties, currentPageIndex, currentBroadcaster);
        else if (properties.get_prop(PropertyType_Surface)) ParseSurfaceAssignment(config, lineNumber, baseTokens, settingTokens, properties, currentPageIndex);
        else AddConfigIssue(config, lineNumber, "Unknown configuration line");

        if (!settingTokens.empty() && !properties.get_prop(PropertyType_Surface)) AddSettingConfigIssue(config, lineNumber, "Setting overrides are allowed only on Settings and Surface lines");
    }

    if (lineNumber == 0) config.fatalError = string(ProductIdentity::ConfigFilename) + " is empty";

    SettingsValues compiledDefaults;
    vector<SettingValidationIssue> productSettingIssues;
    if (!compiledDefaults.TryApply(config.productSettingOverrides, "Product", config.productSettings, productSettingIssues)) {
        config.productSettingOverrides.valid = false;
        AddSettingsValidationIssues(config, config.productSettingOverrides, productSettingIssues);
    }
    for (PageConfig& page : config.pages) {
        for (SurfaceAssignmentConfig& surface : page.surfaces) {
            vector<SettingValidationIssue> surfaceSettingIssues;
            if (!config.productSettings.TryApply(surface.settingOverrides, "Surface", surface.effectiveSettings, surfaceSettingIssues)) {
                surface.settingOverrides.valid = false;
                surface.effectiveSettings = config.productSettings;
                AddSettingsValidationIssues(config, surface.settingOverrides, surfaceSettingIssues);
            }
        }
    }
    return config;
}

IntegratorConfig ParseIntegratorConfig(const string& configPath) {
    ifstream configFile(configPath);
    if (!configFile.is_open()) {
        IntegratorConfig config;
        config.fatalError = "Unable to open configuration file: " + configPath;
        return config;
    }
    return ParseIntegratorConfigStream(configFile, configPath);
}

IntegratorConfig ParseIntegratorConfigSource(const string& source, const string& configPath) {
    istringstream configStream(source);
    return ParseIntegratorConfigStream(configStream, configPath);
}
