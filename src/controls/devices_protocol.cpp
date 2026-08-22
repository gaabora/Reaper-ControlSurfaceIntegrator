#include "integrator.h"

#include "integrator_config_parser.h"
#include "settings_config_editor.h"
#include "../shared/product_paths.h"

#include <cctype>
#include <cstdint>
#include <utility>

extern HWND g_hwnd;

struct DevicesCommandRequest {
    string requestId;
    string command;
    string expectedRevision;
    string profileId;
    string source;
};

static string SanitizeDevicesValue(string value) {
    for (char& character : value) if (character == '\r' || character == '\n') character = ' ';
    return value;
}

static bool IsValidDevicesRequestId(const string& requestId) {
    if (requestId.empty() || requestId.size() > 64) return false;
    for (char character : requestId) if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '-') return false;
    return true;
}

static bool ParseDevicesCommandRequest(const string& payload, DevicesCommandRequest& request, string& errorMessage) {
    map<string, string> properties;
    istringstream stream(payload);
    for (string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const size_t separator = line.find('=');
        if (separator == string::npos || separator == 0) {
            errorMessage = "Invalid Devices request line: " + line;
            return false;
        }
        const string key = line.substr(0, separator);
        if (properties.count(key) > 0) {
            errorMessage = "Duplicate Devices request property: " + key;
            return false;
        }
        properties[key] = line.substr(separator + 1);
    }
    request.requestId = properties["RequestId"];
    request.command = properties["Command"];
    request.expectedRevision = properties["ExpectedRevision"];
    request.profileId = properties["Profile"];
    const int configLineCount = properties.count("ConfigLineCount") > 0 ? atoi(properties["ConfigLineCount"].c_str()) : 0;
    for (int lineIdx = 1; lineIdx <= configLineCount; ++lineIdx) {
        const string key = "ConfigLine." + to_string(lineIdx);
        if (properties.count(key) == 0) {
            errorMessage = "Devices request is missing " + key;
            return false;
        }
        request.source += properties[key] + "\n";
    }
    if (properties["Version"] != "1") errorMessage = "Devices request Version must be 1";
    else if (!IsValidDevicesRequestId(request.requestId)) errorMessage = "Devices request has an invalid RequestId";
    else if (request.command != "Query" && request.command != "Validate" && request.command != "Apply" && request.command != "CreateProfile" && request.command != "CopyProfile" && request.command != "OpenEditor") errorMessage = "Unsupported Devices Command";
    else if ((request.command == "CreateProfile" || request.command == "CopyProfile") && request.profileId.empty()) errorMessage = "Devices profile operation requires Profile";
    if ((request.command == "Validate" || request.command == "Apply") && request.source.empty()) errorMessage = "Devices configuration source is empty";
    if ((request.command == "Validate" || request.command == "Apply") && request.expectedRevision.empty()) errorMessage = "Devices configuration revision is missing";
    return errorMessage.empty();
}

static void AppendDevicesProperty(string& body, const string& key, const string& value) {
    body += key + "=" + SanitizeDevicesValue(value) + "\n";
}

static void AppendDevicesProperty(string& body, const string& key, int value) {
    AppendDevicesProperty(body, key, to_string(value));
}

static bool HasRuntimeMidiIo(const vector<unique_ptr<Midi_ControlSurfaceIO>>& runtimeIo, const string& name) {
    for (const auto& io : runtimeIo) if (io->GetName() == name) return true;
    return false;
}

static bool HasRuntimeOscIo(const vector<unique_ptr<OSC_ControlSurfaceIO>>& runtimeIo, const string& name) {
    for (const auto& io : runtimeIo) if (io->GetName() == name) return true;
    return false;
}

static Page* FindRuntimePage(const vector<unique_ptr<Page>>& pages, const string& name) {
    for (const auto& page : pages) if (page->GetName() == name) return page.get();
    return nullptr;
}

static bool HasRuntimeSurface(Page* page, const string& name) {
    if (!page) return false;
    for (const auto& surface : page->GetSurfaces()) if (surface->GetName() == name) return true;
    return false;
}

static string ConfiguredIoType(const IntegratorConfig& config, const string& name) {
    for (const MidiIoConfig& io : config.midiIo) if (io.name == name) return "MIDI";
    for (const OscIoConfig& io : config.oscIo) if (io.name == name) return "OSC";
    return "Missing";
}

static bool HasRuntimeIo(const vector<unique_ptr<Midi_ControlSurfaceIO>>& midiIo, const vector<unique_ptr<OSC_ControlSurfaceIO>>& oscIo, const string& name) {
    return HasRuntimeMidiIo(midiIo, name) || HasRuntimeOscIo(oscIo, name);
}

static string SurfaceSourceStatus(const ProductPaths& productPaths, const string& surfaceId) {
    try {
        const std::optional<filesystem::path> path = productPaths.FindSurfaceFile(surfaceId);
        if (!path) return "Missing";
        if (*path == productPaths.SurfaceFile(SurfaceSource::User, surfaceId)) return "User";
        return "Vendor";
    } catch (const std::exception&) {
        return "Invalid";
    }
}

static string MainProfileSourceStatus(const ProductPaths& productPaths, const string& profileId) {
    try {
        const std::optional<filesystem::path> path = productPaths.FindMainZones(profileId);
        if (!path) return "Missing";
        if (*path == productPaths.MainZones(ZoneSource::User, profileId)) return "User";
        return "Vendor";
    } catch (const std::exception&) {
        return "Invalid";
    }
}

static bool DirectoryExists(const filesystem::path& path) {
    std::error_code error;
    return filesystem::is_directory(path, error);
}

static bool OpenConfigurationEditor(const ProductPaths& productPaths, string& errorMessage) {
    const filesystem::path editorPath = productPaths.ConfigurationEditorExecutable();
    std::error_code fileError;
    if (!filesystem::is_regular_file(editorPath, fileError)) {
        errorMessage = "The standalone configuration editor is not installed";
        return false;
    }
#ifndef _WIN32
    filesystem::permissions(editorPath, filesystem::perms::owner_exec | filesystem::perms::group_exec, filesystem::perm_options::add, fileError);
    if (fileError) {
        errorMessage = "Cannot make the standalone configuration editor executable: " + fileError.message();
        return false;
    }
#endif
#ifdef _WIN32
    const HINSTANCE result = ::ShellExecute(g_hwnd, "open", editorPath.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<std::intptr_t>(result) > 32) return true;
#else
    if (::ShellExecute(g_hwnd, "open", editorPath.string().c_str(), "", "", SW_SHOWNORMAL) != FALSE) return true;
#endif
    errorMessage = "Cannot start the standalone configuration editor";
    return false;
}

static string FxProfileSourceStatus(const ProductPaths& productPaths, const string& profileId) {
    try {
        const bool vendorExists = DirectoryExists(productPaths.FxZones(ZoneSource::Vendor, profileId));
        const bool userExists = DirectoryExists(productPaths.FxZones(ZoneSource::User, profileId));
        if (vendorExists && userExists) return "Vendor + User";
        if (userExists) return "User";
        if (vendorExists) return "Vendor";
        return "Missing";
    } catch (const std::exception&) {
        return "Invalid";
    }
}

static string ListenerCategories(const ListenerConfig& listener) {
    vector<string> categories;
    if (listener.goHome) categories.push_back("GoHome");
    if (listener.modifiers) categories.push_back("Modifiers");
    if (listener.fxMenu) categories.push_back("FXMenu");
    if (listener.selectedTrackFx) categories.push_back("SelectedTrackFX");
    if (listener.selectedTrackSends) categories.push_back("SelectedTrackSends");
    if (listener.selectedTrackReceives) categories.push_back("SelectedTrackReceives");
    string result;
    for (size_t idx = 0; idx < categories.size(); ++idx) result += (idx == 0 ? "" : ", ") + categories[idx];
    return result;
}

static void AppendSettingOverrides(string& body, const string& prefix, const SettingOverrides& overrides) {
    AppendDevicesProperty(body, prefix + "SettingCount", static_cast<int>(overrides.values.size()));
    size_t settingIdx = 0;
    for (const auto& setting : overrides.values) {
        ++settingIdx;
        AppendDevicesProperty(body, prefix + "Setting." + to_string(settingIdx) + ".Name", setting.first);
        AppendDevicesProperty(body, prefix + "Setting." + to_string(settingIdx) + ".Value", setting.second);
    }
}

static void PublishDevicesResponse(const string& requestId, bool success, const string& body) {
    if (requestId.empty()) return;
    const string response = "Version=1\nStatus=" + string(success ? "OK" : "ERROR") + "\n" + (success ? body : "Message=" + SanitizeDevicesValue(body) + "\n");
    ::SetExtState(ProductIdentity::ExtStateDevices, ("Response_" + requestId).c_str(), response.c_str(), false);
}

static string ReadDevicesConfigSource(const filesystem::path& configPath, string& errorMessage) {
    ifstream inputFile(configPath, std::ios::binary);
    if (!inputFile.is_open()) {
        errorMessage = "Cannot open device configuration";
        return "";
    }
    ostringstream source;
    source << inputFile.rdbuf();
    if (!inputFile.good() && !inputFile.eof()) errorMessage = "Cannot read device configuration";
    return source.str();
}

static string DevicesConfigRevision(const string& source) {
    return to_string(source.size()) + "-" + to_string(std::hash<string>{}(source));
}

static vector<string> ListZoneProfileIds(const ProductPaths& productPaths) {
    map<string, string> profiles;
    const vector<filesystem::path> roots = { productPaths.VendorZonesRoot(), productPaths.UserZonesRoot() };
    for (const filesystem::path& root : roots) {
        std::error_code directoryError;
        if (!filesystem::is_directory(root, directoryError)) continue;
        for (const filesystem::directory_entry& entry : filesystem::directory_iterator(root, filesystem::directory_options::skip_permission_denied, directoryError)) {
            if (!entry.is_directory()) continue;
            const string profileId = entry.path().filename().string();
            try {
                productPaths.ZoneProfileDirectory(ZoneSource::User, profileId);
                profiles[profileId] = profileId;
            } catch (const std::exception&) {
            }
        }
    }
    vector<string> result;
    for (const auto& profile : profiles) result.push_back(profile.second);
    return result;
}

static bool CreateUserZoneProfile(const ProductPaths& productPaths, const string& profileId, string& errorMessage) {
    try {
        const filesystem::path profileRoot = productPaths.ZoneProfileDirectory(ZoneSource::User, profileId);
        const filesystem::path mainRoot = productPaths.MainZones(ZoneSource::User, profileId);
        const filesystem::path homeRoot = mainRoot / "HomeZones";
        const filesystem::path homePath = homeRoot / "Home.zon";
        if (filesystem::exists(profileRoot)) {
            errorMessage = "User Zone profile already exists";
            return false;
        }
        filesystem::create_directories(homeRoot);
        filesystem::create_directories(productPaths.FxZones(ZoneSource::User, profileId));
        const string source = "// @format zone 1\nZone \"Home\"\nZoneEnd\n";
        const filesystem::path temporaryPath = homeRoot / ".Home.zon.tmp";
        ofstream outputFile(temporaryPath, std::ios::binary | std::ios::trunc);
        outputFile << source;
        outputFile.close();
        if (!outputFile.good()) {
            errorMessage = "Cannot write the User Zone profile scaffold";
            return false;
        }
        std::error_code renameError;
        filesystem::rename(temporaryPath, homePath, renameError);
        if (renameError) {
            errorMessage = "Cannot install the User Zone profile scaffold: " + renameError.message();
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        errorMessage = string("Cannot create User Zone profile: ") + error.what();
        return false;
    }
}

static bool ValidateDevicesConfig(const IntegratorConfig& config, const ProductPaths& productPaths, string& errorMessage) {
    if (!config.fatalError.empty()) {
        errorMessage = config.fatalError;
        return false;
    }
    if (!config.issues.empty() || !config.settingsValid || config.skippedSurfaceCount > 0) {
        const IntegratorConfigIssue* issue = config.issues.empty() ? nullptr : &config.issues.front();
        errorMessage = issue ? "Line " + to_string(issue->lineNumber) + ": " + issue->message : "The device configuration contains invalid entries";
        return false;
    }
    map<string, string> ioTypes;
    for (const MidiIoConfig& io : config.midiIo) {
        if (io.name.empty() || io.channelCount < 1 || io.inputPort < 0 || io.outputPort < 0 || io.refreshRate < 1 || io.maxMessagesPerRun < 0) {
            errorMessage = "Invalid MIDI definition: " + io.name;
            return false;
        }
        if (ioTypes.count(io.name) > 0) {
            errorMessage = "Duplicate I/O name: " + io.name;
            return false;
        }
        ioTypes[io.name] = "MIDI";
    }
    for (const OscIoConfig& io : config.oscIo) {
        if (io.name.empty() || io.channelCount < 1 || io.receiveOnPort.empty() || io.transmitToPort.empty() || io.transmitToIpAddress.empty() || io.maxPacketsPerRun < 0) {
            errorMessage = "Invalid OSC definition: " + io.name;
            return false;
        }
        if (ioTypes.count(io.name) > 0) {
            errorMessage = "Duplicate I/O name: " + io.name;
            return false;
        }
        ioTypes[io.name] = "OSC";
    }
    map<string, bool> pageNames;
    for (const PageConfig& page : config.pages) {
        if (page.name.empty() || pageNames.count(page.name) > 0) {
            errorMessage = page.name.empty() ? "Page name must not be empty" : "Duplicate Page name: " + page.name;
            return false;
        }
        pageNames[page.name] = true;
        map<string, bool> surfaceNames;
        for (const SurfaceAssignmentConfig& surface : page.surfaces) {
            if (surface.surfaceName.empty() || surfaceNames.count(surface.surfaceName) > 0) {
                errorMessage = surface.surfaceName.empty() ? "Surface assignment name must not be empty" : "Duplicate Surface assignment on Page " + page.name + ": " + surface.surfaceName;
                return false;
            }
            surfaceNames[surface.surfaceName] = true;
            if (ioTypes.count(surface.surfaceName) == 0) {
                errorMessage = "Surface assignment " + page.name + " / " + surface.surfaceName + " has no matching I/O definition";
                return false;
            }
            if (SurfaceSourceStatus(productPaths, surface.surfaceId) == "Missing" || SurfaceSourceStatus(productPaths, surface.surfaceId) == "Invalid") {
                errorMessage = "Surface assignment " + page.name + " / " + surface.surfaceName + " has no valid Surface template";
                return false;
            }
            const string mainSource = MainProfileSourceStatus(productPaths, surface.mainZoneProfileId);
            if (mainSource == "Missing" || mainSource == "Invalid") {
                errorMessage = "Surface assignment " + page.name + " / " + surface.surfaceName + " has no valid Main Zone profile";
                return false;
            }
            if (surface.startChannel < 0) {
                errorMessage = "Surface assignment " + page.name + " / " + surface.surfaceName + " has an invalid start channel";
                return false;
            }
        }
        map<string, bool> listenerLinks;
        for (const ListenerConfig& listener : page.listeners) {
            const string link = listener.broadcasterName + "\n" + listener.listenerName;
            if (listener.broadcasterName == listener.listenerName || surfaceNames.count(listener.broadcasterName) == 0 || surfaceNames.count(listener.listenerName) == 0 || listenerLinks.count(link) > 0) {
                errorMessage = "Invalid Listener relationship on Page " + page.name + ": " + listener.broadcasterName + " -> " + listener.listenerName;
                return false;
            }
            if (listenerLinks.count(listener.listenerName + "\n" + listener.broadcasterName) > 0) {
                errorMessage = "Circular Listener relationship on Page " + page.name + ": " + listener.broadcasterName + " -> " + listener.listenerName;
                return false;
            }
            listenerLinks[link] = true;
        }
    }
    return true;
}

void CSurfIntegrator::PollAndHandleDevicesCommands() {
    if (!::HasExtState(ProductIdentity::ExtStateDevicesCommand, "Request")) return;
    const string payload = ::GetExtState(ProductIdentity::ExtStateDevicesCommand, "Request");
    ::DeleteExtState(ProductIdentity::ExtStateDevicesCommand, "Request", false);
    DevicesCommandRequest request;
    string errorMessage;
    if (!ParseDevicesCommandRequest(payload, request, errorMessage)) {
        PublishDevicesResponse(request.requestId, false, errorMessage);
        return;
    }

    const ProductPaths productPaths = ProductPaths::FromReaperResourcePath();
    if (request.command == "OpenEditor") {
        const bool success = OpenConfigurationEditor(productPaths, errorMessage);
        PublishDevicesResponse(request.requestId, success, success ? "Message=Standalone configuration editor opened\n" : errorMessage);
        return;
    }
    if (request.command == "CreateProfile") {
        const bool success = CreateUserZoneProfile(productPaths, request.profileId, errorMessage);
        PublishDevicesResponse(request.requestId, success, success ? "Message=User Zone profile created\n" : errorMessage);
        return;
    }
    if (request.command == "CopyProfile") {
        try {
            productPaths.CloneVendorMainZonesToUser(request.profileId);
            PublishDevicesResponse(request.requestId, true, "Message=Vendor Main copied to User\n");
            DAW::SendCommandMessage(REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES);
        } catch (const std::exception& error) {
            PublishDevicesResponse(request.requestId, false, error.what());
        }
        return;
    }
    string sourceError;
    const string configSource = ReadDevicesConfigSource(productPaths.ConfigFile(), sourceError);
    if (!sourceError.empty()) {
        PublishDevicesResponse(request.requestId, false, sourceError);
        return;
    }
    if (request.command == "Validate" || request.command == "Apply") {
        if (request.expectedRevision != DevicesConfigRevision(configSource)) {
            PublishDevicesResponse(request.requestId, false, "The device configuration changed outside this Control Panel. Revert to reload it before saving.");
            return;
        }
        const IntegratorConfig candidate = ParseIntegratorConfigSource(request.source, productPaths.ConfigFile().string());
        if (!ValidateDevicesConfig(candidate, productPaths, errorMessage)) {
            PublishDevicesResponse(request.requestId, false, errorMessage);
            return;
        }
        if (request.command == "Validate") {
            PublishDevicesResponse(request.requestId, true, "Message=Device configuration is valid\n");
            return;
        }
        try {
            for (const PageConfig& page : candidate.pages) for (const SurfaceAssignmentConfig& surface : page.surfaces) filesystem::create_directories(productPaths.FxZones(ZoneSource::User, surface.fxZoneProfileId));
        } catch (const std::exception& error) {
            PublishDevicesResponse(request.requestId, false, string("Cannot prepare User FX directories: ") + error.what());
            return;
        }
        if (!WriteSettingsConfigAtomically(productPaths.ConfigFile(), request.source, errorMessage)) {
            PublishDevicesResponse(request.requestId, false, errorMessage);
            return;
        }
        PublishDevicesResponse(request.requestId, true, "Message=Device configuration saved; REAPER is reconnecting CSI\n");
        DAW::SendCommandMessage(REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES);
        return;
    }
    const IntegratorConfig config = ParseIntegratorConfig(productPaths.ConfigFile().string());
    string body;
    AppendDevicesProperty(body, "ConfigVersion", s_MajorVersionToken);
    AppendDevicesProperty(body, "Revision", sourceError.empty() ? DevicesConfigRevision(configSource) : "");
    AppendDevicesProperty(body, "FatalError", config.fatalError);
    AppendSettingOverrides(body, "Product.", config.productSettingOverrides);
    const int currentPageIdx = this->currentPageIndex_.load();
    AppendDevicesProperty(body, "CurrentPage", currentPageIdx >= 0 && currentPageIdx < static_cast<int>(this->pages_.size()) ? this->pages_[currentPageIdx]->GetName() : "");
    AppendDevicesProperty(body, "EditorAvailable", filesystem::is_regular_file(productPaths.ConfigurationEditorExecutable()) ? 1 : 0);

    AppendDevicesProperty(body, "MidiCount", static_cast<int>(config.midiIo.size()));
    for (size_t midiIdx = 0; midiIdx < config.midiIo.size(); ++midiIdx) {
        const MidiIoConfig& io = config.midiIo[midiIdx];
        const string prefix = "Midi." + to_string(midiIdx + 1) + ".";
        char deviceName[512] = {};
        AppendDevicesProperty(body, prefix + "Line", io.lineNumber);
        AppendDevicesProperty(body, prefix + "Name", io.name);
        AppendDevicesProperty(body, prefix + "Channels", io.channelCount);
        AppendDevicesProperty(body, prefix + "InputPort", io.inputPort);
        AppendDevicesProperty(body, prefix + "InputName", io.inputPort >= 0 && GetMIDIInputName(io.inputPort, deviceName, sizeof(deviceName)) ? deviceName : "");
        deviceName[0] = '\0';
        AppendDevicesProperty(body, prefix + "OutputPort", io.outputPort);
        AppendDevicesProperty(body, prefix + "OutputName", io.outputPort >= 0 && GetMIDIOutputName(io.outputPort, deviceName, sizeof(deviceName)) ? deviceName : "");
        AppendDevicesProperty(body, prefix + "RefreshRate", io.refreshRate);
        AppendDevicesProperty(body, prefix + "MaxMessages", io.maxMessagesPerRun);
        AppendDevicesProperty(body, prefix + "Active", HasRuntimeMidiIo(this->midiSurfacesIO_, io.name) ? 1 : 0);
        char inputDeviceName[512] = {};
        const bool resolvedInput = io.inputPort >= 0 && GetMIDIInputName(io.inputPort, inputDeviceName, sizeof(inputDeviceName));
        const bool resolvedOutput = io.outputPort >= 0 && GetMIDIOutputName(io.outputPort, deviceName, sizeof(deviceName));
        AppendDevicesProperty(body, prefix + "RuntimeIssue", !resolvedInput ? "MIDI input port is unavailable" : (!resolvedOutput ? "MIDI output port is unavailable" : ""));
    }

    AppendDevicesProperty(body, "OscCount", static_cast<int>(config.oscIo.size()));
    for (size_t oscIdx = 0; oscIdx < config.oscIo.size(); ++oscIdx) {
        const OscIoConfig& io = config.oscIo[oscIdx];
        const string prefix = "Osc." + to_string(oscIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Line", io.lineNumber);
        AppendDevicesProperty(body, prefix + "Type", io.type);
        AppendDevicesProperty(body, prefix + "Name", io.name);
        AppendDevicesProperty(body, prefix + "Channels", io.channelCount);
        AppendDevicesProperty(body, prefix + "ReceivePort", io.receiveOnPort);
        AppendDevicesProperty(body, prefix + "TransmitPort", io.transmitToPort);
        AppendDevicesProperty(body, prefix + "Address", io.transmitToIpAddress);
        AppendDevicesProperty(body, prefix + "MaxPackets", io.maxPacketsPerRun);
        const bool active = HasRuntimeOscIo(this->oscSurfacesIO_, io.name);
        AppendDevicesProperty(body, prefix + "Active", active ? 1 : 0);
        AppendDevicesProperty(body, prefix + "RuntimeIssue", active ? "" : "OSC endpoint did not open");
    }

    AppendDevicesProperty(body, "PageCount", static_cast<int>(config.pages.size()));
    for (size_t pageIdx = 0; pageIdx < config.pages.size(); ++pageIdx) {
        const PageConfig& page = config.pages[pageIdx];
        Page* runtimePage = FindRuntimePage(this->pages_, page.name);
        const string prefix = "Page." + to_string(pageIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Line", page.lineNumber);
        AppendDevicesProperty(body, prefix + "Name", page.name);
        AppendDevicesProperty(body, prefix + "Active", runtimePage ? 1 : 0);
        AppendDevicesProperty(body, prefix + "Current", currentPageIdx >= 0 && currentPageIdx < static_cast<int>(this->pages_.size()) && this->pages_[currentPageIdx].get() == runtimePage ? 1 : 0);
        AppendDevicesProperty(body, prefix + "FollowsMcp", page.followsMcp ? 1 : 0);
        AppendDevicesProperty(body, prefix + "SynchPages", page.synchPages ? 1 : 0);
        AppendDevicesProperty(body, prefix + "ScrollLink", page.scrollLink ? 1 : 0);
        AppendDevicesProperty(body, prefix + "ScrollSynch", page.scrollSynch ? 1 : 0);
        AppendDevicesProperty(body, prefix + "SurfaceCount", static_cast<int>(page.surfaces.size()));
        for (size_t surfaceIdx = 0; surfaceIdx < page.surfaces.size(); ++surfaceIdx) {
            const SurfaceAssignmentConfig& surface = page.surfaces[surfaceIdx];
            const string surfacePrefix = prefix + "Surface." + to_string(surfaceIdx + 1) + ".";
            AppendDevicesProperty(body, surfacePrefix + "Line", surface.lineNumber);
            AppendDevicesProperty(body, surfacePrefix + "Name", surface.surfaceName);
            AppendDevicesProperty(body, surfacePrefix + "SurfaceId", surface.surfaceId);
            AppendDevicesProperty(body, surfacePrefix + "MainProfile", surface.mainZoneProfileId);
            AppendDevicesProperty(body, surfacePrefix + "FxProfile", surface.fxZoneProfileId);
            AppendDevicesProperty(body, surfacePrefix + "StartChannel", surface.startChannel);
            AppendDevicesProperty(body, surfacePrefix + "Active", HasRuntimeSurface(runtimePage, surface.surfaceName) ? 1 : 0);
            AppendDevicesProperty(body, surfacePrefix + "IoType", ConfiguredIoType(config, surface.surfaceName));
            AppendDevicesProperty(body, surfacePrefix + "IoActive", HasRuntimeIo(this->midiSurfacesIO_, this->oscSurfacesIO_, surface.surfaceName) ? 1 : 0);
            AppendDevicesProperty(body, surfacePrefix + "TemplateSource", SurfaceSourceStatus(productPaths, surface.surfaceId));
            AppendDevicesProperty(body, surfacePrefix + "MainSource", MainProfileSourceStatus(productPaths, surface.mainZoneProfileId));
            AppendDevicesProperty(body, surfacePrefix + "FxSource", FxProfileSourceStatus(productPaths, surface.fxZoneProfileId));
            AppendSettingOverrides(body, surfacePrefix, surface.settingOverrides);
        }
        AppendDevicesProperty(body, prefix + "ListenerCount", static_cast<int>(page.listeners.size()));
        for (size_t listenerIdx = 0; listenerIdx < page.listeners.size(); ++listenerIdx) {
            const ListenerConfig& listener = page.listeners[listenerIdx];
            const string listenerPrefix = prefix + "Listener." + to_string(listenerIdx + 1) + ".";
            AppendDevicesProperty(body, listenerPrefix + "Line", listener.lineNumber);
            AppendDevicesProperty(body, listenerPrefix + "Broadcaster", listener.broadcasterName);
            AppendDevicesProperty(body, listenerPrefix + "Listener", listener.listenerName);
            AppendDevicesProperty(body, listenerPrefix + "Categories", ListenerCategories(listener));
            AppendDevicesProperty(body, listenerPrefix + "Active", HasRuntimeSurface(runtimePage, listener.broadcasterName) && HasRuntimeSurface(runtimePage, listener.listenerName) ? 1 : 0);
        }
    }

    AppendDevicesProperty(body, "IssueCount", static_cast<int>(config.issues.size()));
    for (size_t issueIdx = 0; issueIdx < config.issues.size(); ++issueIdx) {
        const IntegratorConfigIssue& issue = config.issues[issueIdx];
        const string prefix = "Issue." + to_string(issueIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Line", issue.lineNumber);
        AppendDevicesProperty(body, prefix + "Kind", issue.settingIssue ? "Setting" : "Parser");
        AppendDevicesProperty(body, prefix + "Message", issue.message);
    }
    AppendDevicesProperty(body, "SkippedSurfaceCount", config.skippedSurfaceCount);
    vector<std::pair<int, string>> midiInputOptions;
    for (int inputIdx = 0; inputIdx < GetNumMIDIInputs(); ++inputIdx) {
        char deviceName[512] = {};
        if (GetMIDIInputName(inputIdx, deviceName, sizeof(deviceName))) midiInputOptions.emplace_back(inputIdx, deviceName);
    }
    AppendDevicesProperty(body, "MidiInputOptionCount", static_cast<int>(midiInputOptions.size()));
    for (size_t optionIdx = 0; optionIdx < midiInputOptions.size(); ++optionIdx) {
        const string prefix = "MidiInputOption." + to_string(optionIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Port", midiInputOptions[optionIdx].first);
        AppendDevicesProperty(body, prefix + "Name", midiInputOptions[optionIdx].second);
    }
    vector<std::pair<int, string>> midiOutputOptions;
    for (int outputIdx = 0; outputIdx < GetNumMIDIOutputs(); ++outputIdx) {
        char deviceName[512] = {};
        if (GetMIDIOutputName(outputIdx, deviceName, sizeof(deviceName))) midiOutputOptions.emplace_back(outputIdx, deviceName);
    }
    AppendDevicesProperty(body, "MidiOutputOptionCount", static_cast<int>(midiOutputOptions.size()));
    for (size_t optionIdx = 0; optionIdx < midiOutputOptions.size(); ++optionIdx) {
        const string prefix = "MidiOutputOption." + to_string(optionIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Port", midiOutputOptions[optionIdx].first);
        AppendDevicesProperty(body, prefix + "Name", midiOutputOptions[optionIdx].second);
    }
    const vector<string> surfaceIds = productPaths.ListSurfaceIds();
    AppendDevicesProperty(body, "SurfaceOptionCount", static_cast<int>(surfaceIds.size()));
    for (size_t surfaceIdx = 0; surfaceIdx < surfaceIds.size(); ++surfaceIdx) {
        const string prefix = "SurfaceOption." + to_string(surfaceIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Id", surfaceIds[surfaceIdx]);
        AppendDevicesProperty(body, prefix + "Source", SurfaceSourceStatus(productPaths, surfaceIds[surfaceIdx]));
    }
    const vector<string> profileIds = ListZoneProfileIds(productPaths);
    AppendDevicesProperty(body, "ProfileOptionCount", static_cast<int>(profileIds.size()));
    for (size_t profileIdx = 0; profileIdx < profileIds.size(); ++profileIdx) {
        const string prefix = "ProfileOption." + to_string(profileIdx + 1) + ".";
        AppendDevicesProperty(body, prefix + "Id", profileIds[profileIdx]);
        AppendDevicesProperty(body, prefix + "MainSource", MainProfileSourceStatus(productPaths, profileIds[profileIdx]));
        AppendDevicesProperty(body, prefix + "FxSource", FxProfileSourceStatus(productPaths, profileIds[profileIdx]));
        AppendDevicesProperty(body, prefix + "VendorMain", DirectoryExists(productPaths.MainZones(ZoneSource::Vendor, profileIds[profileIdx])) ? 1 : 0);
        AppendDevicesProperty(body, prefix + "UserMain", DirectoryExists(productPaths.MainZones(ZoneSource::User, profileIds[profileIdx])) ? 1 : 0);
    }
    PublishDevicesResponse(request.requestId, true, body);
}
