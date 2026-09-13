#include "configured_device_channels.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "format2_surface_document.h"
static void AddConfiguredChannelIssue(IntegratorConfig& config, int lineNumber, const std::string& message) {
    config.issues.push_back({lineNumber, message, false});
}

static std::string FoldConfiguredDeviceId(const std::string& value) {
    std::string folded = value;
    for (char& character : folded) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    return folded;
}

static bool ReadConfiguredSurfaceSource(const std::filesystem::path& path, std::string& source) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) return false;
    source = buffer.str();
    return true;
}

void ResolveConfiguredDeviceChannels(IntegratorConfig& config, const ProductPaths& productPaths) {
    std::map<std::string, int*> devices;
    std::set<std::string> conflictingDevices;
    for (MidiIoConfig& device : config.midiIo) devices[FoldConfiguredDeviceId(device.name)] = &device.channelCount;
    for (OscIoConfig& device : config.oscIo) devices[FoldConfiguredDeviceId(device.name)] = &device.channelCount;
    for (const PageConfig& page : config.pages) {
        for (const SurfaceAssignmentConfig& surface : page.surfaces) {
            const auto device = devices.find(FoldConfiguredDeviceId(surface.deviceId));
            if (device == devices.end()) continue;
            if (conflictingDevices.count(device->first) > 0) continue;
            std::optional<std::filesystem::path> surfacePath;
            try {
                surfacePath = productPaths.FindSurfaceFile(surface.surfaceId);
            } catch (const std::exception& error) {
                AddConfiguredChannelIssue(config, surface.lineNumber, "Configuration " + productPaths.ConfigFile().string() + ":" + std::to_string(surface.lineNumber) + " cannot resolve Surface template '" + surface.surfaceId + "': " + error.what());
                continue;
            }
            if (!surfacePath) {
                AddConfiguredChannelIssue(config, surface.lineNumber, "Configuration " + productPaths.ConfigFile().string() + ":" + std::to_string(surface.lineNumber) + " uses missing Surface template '" + surface.surfaceId + "'. Install or select that template, then reload Devices.");
                continue;
            }
            std::string source;
            if (!ReadConfiguredSurfaceSource(*surfacePath, source)) {
                AddConfiguredChannelIssue(config, surface.lineNumber, "Cannot read Surface template " + surfacePath->string() + ". Check file permissions, then reload Devices.");
                continue;
            }
            const Format2SurfaceParseResult parsed = ParseFormat2SurfaceSource(source, surfacePath->string());
            if (!parsed.IsValid() || !parsed.document.metadata.channels) {
                const Format2Diagnostic* diagnostic = parsed.document.lexical.diagnostics.empty() ? nullptr : &parsed.document.lexical.diagnostics.front();
                std::string details = diagnostic ? diagnostic->message : "Surface metadata must contain a positive Channels value";
                if (diagnostic && diagnostic->code == "format2.surface.widget.header") details = "Widget declaration must have one name, for example: Widget Fader1 { ... }";
                const std::string location = surfacePath->string() + (diagnostic ? ":" + std::to_string(diagnostic->location.line) : "");
                AddConfiguredChannelIssue(config, surface.lineNumber, "Surface template " + location + " is invalid. " + details + ". Fix this file in Configuration Editor, then reload Devices.");
                continue;
            }
            const int channels = *parsed.document.metadata.channels;
            if (*device->second != 0 && *device->second != channels) {
                config.issues.push_back({surface.lineNumber, "Device " + surface.deviceId + " is assigned to Surface templates with different Channels values: " + std::to_string(*device->second) + " and " + std::to_string(channels), false});
                *device->second = 0;
                conflictingDevices.insert(device->first);
                continue;
            }
            *device->second = channels;
        }
    }
}
