#include "settings_config_editor.h"

#include "integrator_config_parser.h"
#include "preamble.h"

#include <chrono>

static string FoldSettingsConfigId(const string& value) {
    string folded = value;
    for (char& character : folded) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    return folded;
}

static SettingOverrides* FindDeviceSettingOverrides(IntegratorConfig& config, const string& deviceId) {
    const string canonicalId = FoldSettingsConfigId(deviceId);
    for (MidiIoConfig& device : config.midiIo) if (FoldSettingsConfigId(device.name) == canonicalId) return &device.settingOverrides;
    for (OscIoConfig& device : config.oscIo) if (FoldSettingsConfigId(device.name) == canonicalId) return &device.settingOverrides;
    return nullptr;
}

static void ApplySettingChanges(SettingOverrides& overrides, const map<string, std::optional<string>>& changes) {
    for (const auto& change : changes) {
        if (change.second) overrides.values[change.first] = *change.second;
        else overrides.values.erase(change.first);
        overrides.lineNumbers.erase(change.first);
    }
    overrides.valid = true;
}

bool EditSettingsConfigSource(const string& source, const SettingsConfigEditRequest& request, string& result, string& errorMessage) {
    if (request.scope != "Product" && request.scope != "Device") {
        errorMessage = "Settings scope must be Product or Device";
        return false;
    }
    if (request.changes.empty()) {
        errorMessage = "Settings edit has no changes";
        return false;
    }

    IntegratorConfig config = ParseFormat2IntegratorConfigSource(source, "product configuration");
    if (!config.fatalError.empty()) {
        errorMessage = config.fatalError;
        return false;
    }
    if (!config.issues.empty()) {
        const IntegratorConfigIssue& issue = config.issues.front();
        errorMessage = "Cannot edit a product configuration with parser issues. Line " + std::to_string(issue.lineNumber) + ": " + issue.message;
        return false;
    }
    SettingOverrides* overrides = &config.productSettingOverrides;
    if (request.scope == "Device") {
        overrides = FindDeviceSettingOverrides(config, request.deviceId);
        if (!overrides) {
            errorMessage = "Cannot find Device=" + request.deviceId;
            return false;
        }
    }
    ApplySettingChanges(*overrides, request.changes);
    return SerializeFormat2IntegratorConfig(config, result, errorMessage);
}

bool WriteSettingsConfigAtomically(const filesystem::path& configPath, const string& source, string& errorMessage) {
    const long long timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    filesystem::path temporaryPath = configPath;
    temporaryPath += ".settings-" + std::to_string(timestamp) + ".tmp";
    {
        ofstream temporaryFile(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!temporaryFile.is_open()) {
            errorMessage = "Cannot create temporary settings file: " + temporaryPath.string();
            return false;
        }
        temporaryFile.write(source.data(), static_cast<std::streamsize>(source.size()));
        temporaryFile.flush();
        if (!temporaryFile.good()) {
            errorMessage = "Cannot write temporary settings file: " + temporaryPath.string();
            temporaryFile.close();
            std::error_code removeError;
            filesystem::remove(temporaryPath, removeError);
            return false;
        }
    }

#ifdef _WIN32
    if (!MoveFileExW(temporaryPath.wstring().c_str(), configPath.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        errorMessage = "Cannot replace settings configuration file; Windows error " + std::to_string(GetLastError());
        std::error_code removeError;
        filesystem::remove(temporaryPath, removeError);
        return false;
    }
#else
    std::error_code renameError;
    filesystem::rename(temporaryPath, configPath, renameError);
    if (renameError) {
        errorMessage = "Cannot replace settings configuration file: " + renameError.message();
        std::error_code removeError;
        filesystem::remove(temporaryPath, removeError);
        return false;
    }
#endif
    return true;
}
