#include "configured_device_channels.h"

#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "format2_surface_document.h"

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
                config.issues.push_back({surface.lineNumber, "Cannot resolve Surface template " + surface.surfaceId + ": " + error.what(), false});
                continue;
            }
            if (!surfacePath) {
                config.issues.push_back({surface.lineNumber, "Cannot derive Channels because Surface template is missing: " + surface.surfaceId, false});
                continue;
            }
            std::string source;
            if (!ReadConfiguredSurfaceSource(*surfacePath, source)) {
                config.issues.push_back({surface.lineNumber, "Cannot read Surface template to derive Channels: " + surfacePath->string(), false});
                continue;
            }
            const Format2SurfaceParseResult parsed = ParseFormat2SurfaceSource(source, surfacePath->string());
            if (!parsed.IsValid() || !parsed.document.metadata.channels) {
                const std::string details = parsed.document.lexical.diagnostics.empty() ? "Surface metadata has no valid Channels value" : parsed.document.lexical.diagnostics.front().message;
                config.issues.push_back({surface.lineNumber, "Cannot derive Channels from Surface template " + surface.surfaceId + ": " + details, false});
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
