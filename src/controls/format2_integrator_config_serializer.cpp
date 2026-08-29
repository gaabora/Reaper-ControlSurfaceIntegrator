#include "integrator_config_parser.h"

#include <sstream>

static bool IsValidSerializedConfigId(const std::string& value) {
    if (value.empty() || !((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= 'a' && value[0] <= 'z'))) return false;
    for (char character : value) if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_')) return false;
    return true;
}

static bool IsValidSerializedBareValue(const std::string& value) {
    if (value.empty() || value.find("//") != std::string::npos) return false;
    for (char character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte <= 0x20 || byte == 0x7F || character == '"' || character == '{' || character == '}' || character == '[' || character == ']' || character == '(' || character == ')' || character == '=' || character == '+' || character == ',') return false;
    }
    return true;
}

static bool AppendSerializedProperty(std::ostringstream& stream, const std::string& indent, const std::string& name, const std::string& value, std::string& errorMessage) {
    if (!IsValidSerializedBareValue(value)) {
        errorMessage = "Property " + name + " requires a non-empty unquoted value without whitespace or format delimiters";
        return false;
    }
    stream << indent << name << '=' << value << '\n';
    return true;
}

static bool AppendSerializedIdProperty(std::ostringstream& stream, const std::string& indent, const std::string& name, const std::string& value, std::string& errorMessage) {
    if (!IsValidSerializedConfigId(value)) {
        errorMessage = "Property " + name + " must start with a letter and contain only ASCII letters, digits, or _";
        return false;
    }
    stream << indent << name << '=' << value << '\n';
    return true;
}

static bool AppendSerializedSettings(std::ostringstream& stream, const std::string& indent, const SettingOverrides& overrides, std::string& errorMessage) {
    if (overrides.values.empty()) return true;
    stream << indent << "Settings {\n";
    for (const auto& setting : overrides.values) {
        if (!AppendSerializedProperty(stream, indent + "  ", setting.first, setting.second, errorMessage)) return false;
    }
    stream << indent << "}\n";
    return true;
}

static bool AppendSerializedBlockId(std::ostringstream& stream, const std::string& blockName, const std::string& id, std::string& errorMessage) {
    if (IsValidSerializedConfigId(id)) {
        stream << blockName << ' ' << id << " {\n";
        return true;
    }
    errorMessage = blockName + " ID must start with a letter and contain only ASCII letters, digits, or _";
    return false;
}

static bool AppendSerializedMidiDevice(std::ostringstream& stream, const MidiIoConfig& device, std::string& errorMessage) {
    if (!AppendSerializedBlockId(stream, "Device", device.name, errorMessage)) return false;
    stream << "  Type=MIDI\n";
    stream << "  Channels=" << device.channelCount << '\n';
    stream << "  Input=" << device.inputPort << '\n';
    stream << "  Output=" << device.outputPort << '\n';
    stream << "  RefreshRate=" << device.refreshRate << '\n';
    stream << "  MaxMessagesPerRun=" << device.maxMessagesPerRun << '\n';
    if (!device.settingOverrides.values.empty()) stream << '\n';
    if (!AppendSerializedSettings(stream, "  ", device.settingOverrides, errorMessage)) return false;
    stream << "}\n\n";
    return true;
}

static bool AppendSerializedOscDevice(std::ostringstream& stream, const OscIoConfig& device, std::string& errorMessage) {
    if (!AppendSerializedBlockId(stream, "Device", device.name, errorMessage)) return false;
    if (device.type != "OSC" && device.type != "OSCX32") {
        errorMessage = "OSC Device " + device.name + " has an unsupported protocol type";
        return false;
    }
    stream << "  Type=OSC\n";
    stream << "  Protocol=" << (device.type == "OSCX32" ? "X32" : "Generic") << '\n';
    stream << "  Channels=" << device.channelCount << '\n';
    if (!AppendSerializedProperty(stream, "  ", "ReceivePort", device.receiveOnPort, errorMessage)) return false;
    if (!AppendSerializedProperty(stream, "  ", "TransmitPort", device.transmitToPort, errorMessage)) return false;
    if (!AppendSerializedProperty(stream, "  ", "Address", device.transmitToIpAddress, errorMessage)) return false;
    stream << "  MaxPacketsPerRun=" << device.maxPacketsPerRun << '\n';
    if (!device.settingOverrides.values.empty()) stream << '\n';
    if (!AppendSerializedSettings(stream, "  ", device.settingOverrides, errorMessage)) return false;
    stream << "}\n\n";
    return true;
}

static std::string SerializedLinkCategories(const ListenerConfig& link) {
    std::string categories;
    const auto append = [&categories](const std::string& category) {
        if (!categories.empty()) categories += ", ";
        categories += category;
    };
    if (link.goHome) append("Home");
    if (link.modifiers) append("Modifiers");
    if (link.fxMenu) append("FXMenu");
    if (link.selectedTrackFx) append("SelectedTrackFX");
    if (link.selectedTrackSends) append("SelectedTrackSends");
    if (link.selectedTrackReceives) append("SelectedTrackReceives");
    return categories;
}

static bool AppendSerializedSurface(std::ostringstream& stream, const SurfaceAssignmentConfig& surface, std::string& errorMessage) {
    if (!IsValidSerializedConfigId(surface.surfaceName)) {
        errorMessage = "Surface ID must start with a letter and contain only ASCII letters, digits, or _";
        return false;
    }
    stream << "  Surface " << surface.surfaceName << " {\n";
    if (!AppendSerializedIdProperty(stream, "    ", "Device", surface.deviceId, errorMessage)) return false;
    if (!AppendSerializedProperty(stream, "    ", "Template", surface.surfaceId, errorMessage)) return false;
    if (!AppendSerializedProperty(stream, "    ", "MainProfile", surface.mainZoneProfileId, errorMessage)) return false;
    if (!AppendSerializedProperty(stream, "    ", "FXProfile", surface.fxZoneProfileId, errorMessage)) return false;
    stream << "    StartChannel=" << surface.startChannel << '\n';
    stream << "  }\n";
    return true;
}

static bool AppendSerializedLink(std::ostringstream& stream, const ListenerConfig& link, std::string& errorMessage) {
    const std::string categories = SerializedLinkCategories(link);
    if (categories.empty()) {
        errorMessage = "Link Share requires at least one category";
        return false;
    }
    stream << "  Link {\n";
    if (!AppendSerializedIdProperty(stream, "    ", "From", link.broadcasterName, errorMessage)) return false;
    if (!AppendSerializedIdProperty(stream, "    ", "To", link.listenerName, errorMessage)) return false;
    stream << "    Share=[" << categories << "]\n";
    stream << "  }\n";
    return true;
}

static bool AppendSerializedPage(std::ostringstream& stream, const PageConfig& page, std::string& errorMessage) {
    if (!AppendSerializedBlockId(stream, "Page", page.name, errorMessage)) return false;
    stream << "  FollowMCP=" << (page.followsMcp ? "true" : "false") << '\n';
    stream << "  SyncPages=" << (page.synchPages ? "true" : "false") << '\n';
    stream << "  ScrollLink=" << (page.scrollLink ? "true" : "false") << '\n';
    stream << "  ScrollSync=" << (page.scrollSynch ? "true" : "false") << '\n';
    for (const SurfaceAssignmentConfig& surface : page.surfaces) {
        stream << '\n';
        if (!AppendSerializedSurface(stream, surface, errorMessage)) return false;
    }
    for (const ListenerConfig& link : page.listeners) {
        stream << '\n';
        if (!AppendSerializedLink(stream, link, errorMessage)) return false;
    }
    stream << "}\n\n";
    return true;
}

bool SerializeFormat2IntegratorConfig(const IntegratorConfig& config, std::string& source, std::string& errorMessage) {
    source.clear();
    errorMessage.clear();
    std::ostringstream stream;
    if (!config.productSettingOverrides.values.empty()) {
        if (!AppendSerializedSettings(stream, "", config.productSettingOverrides, errorMessage)) return false;
        stream << '\n';
    }
    for (const MidiIoConfig& device : config.midiIo) if (!AppendSerializedMidiDevice(stream, device, errorMessage)) return false;
    for (const OscIoConfig& device : config.oscIo) if (!AppendSerializedOscDevice(stream, device, errorMessage)) return false;
    for (const PageConfig& page : config.pages) if (!AppendSerializedPage(stream, page, errorMessage)) return false;
    source = stream.str();
    while (!source.empty() && source.back() == '\n') source.pop_back();
    source += '\n';
    return true;
}
