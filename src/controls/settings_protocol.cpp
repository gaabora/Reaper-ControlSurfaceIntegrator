#include "integrator.h"

#include "integrator_config_parser.h"
#include "settings_config_editor.h"

#include <cctype>

struct SettingsCommandRequest {
    string requestId;
    string command;
    string scope;
    string pageName;
    string surfaceName;
    std::map<string, std::optional<string>> changes;
};

struct RuntimeSettingsApplication {
    ControlSurface* surface = nullptr;
    SettingsValues values;
    SettingOverrides overrides;
};

static string TrimSettingsProtocolValue(const string& value) {
    const size_t start = value.find_first_not_of(" \t\r");
    if (start == string::npos) return "";
    const size_t end = value.find_last_not_of(" \t\r");
    return value.substr(start, end - start + 1);
}

static bool IsValidSettingsRequestId(const string& requestId) {
    if (requestId.empty() || requestId.size() > 64) return false;
    for (char character : requestId) if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_' && character != '-') return false;
    return true;
}

static bool ParseSettingsCommandRequest(const string& payload, SettingsCommandRequest& request, string& errorMessage) {
    std::map<string, string> properties;
    istringstream stream(payload);
    for (string line; std::getline(stream, line);) {
        line = TrimSettingsProtocolValue(line);
        if (line.empty()) continue;
        const size_t separator = line.find('=');
        if (separator == string::npos || separator == 0) {
            errorMessage = "Invalid settings request line: " + line;
            return false;
        }
        const string key = line.substr(0, separator);
        const string value = line.substr(separator + 1);
        if (properties.count(key) > 0) {
            errorMessage = "Duplicate settings request property: " + key;
            return false;
        }
        properties[key] = value;
    }

    request.requestId = properties["RequestId"];
    request.command = properties["Command"];
    request.scope = properties["Scope"];
    request.pageName = properties["Page"];
    request.surfaceName = properties["Surface"];
    if (properties["Version"] != "1") {
        errorMessage = "Settings request Version must be 1";
        return false;
    }
    if (!IsValidSettingsRequestId(request.requestId)) {
        errorMessage = "Settings request has an invalid RequestId";
        return false;
    }
    if (request.command != "Query" && request.command != "Apply" && request.command != "Reload") {
        errorMessage = "Settings Command must be Query, Apply, or Reload";
        return false;
    }
    if (request.command != "Reload" && request.scope != "Product" && request.scope != "Surface") {
        errorMessage = "Settings Scope must be Product or Surface";
        return false;
    }
    if (request.scope == "Surface" && request.surfaceName.empty()) {
        errorMessage = "Surface settings require Surface";
        return false;
    }

    for (const auto& property : properties) {
        bool unset = false;
        string settingName;
        if (property.first.rfind("Set.", 0) == 0) settingName = property.first.substr(4);
        else if (property.first.rfind("Unset.", 0) == 0) {
            settingName = property.first.substr(6);
            unset = true;
        } else if (property.first == "Version" || property.first == "RequestId" || property.first == "Command" || property.first == "Scope" || property.first == "Page" || property.first == "Surface") continue;
        else {
            errorMessage = "Unknown settings request property: " + property.first;
            return false;
        }
        const Settings::Definition* definition = FindSettingDefinition(settingName);
        if (!definition) {
            errorMessage = "Unknown setting: " + settingName;
            return false;
        }
        if (request.command != "Apply") {
            errorMessage = "Set and Unset properties are allowed only for Apply";
            return false;
        }
        if (!SettingAllowsScope(*definition, request.scope)) {
            errorMessage = "Setting " + settingName + " is not allowed in " + request.scope + " scope";
            return false;
        }
        if (request.changes.count(settingName) > 0) {
            errorMessage = "Setting " + settingName + " has more than one change";
            return false;
        }
        if (unset) {
            if (property.second != "1") {
                errorMessage = "Unset." + settingName + " must equal 1";
                return false;
            }
            request.changes[settingName] = std::nullopt;
        } else {
            if (property.second.empty()) {
                errorMessage = "Set." + settingName + " requires a value";
                return false;
            }
            request.changes[settingName] = property.second;
        }
    }
    if (request.command == "Apply" && request.changes.empty()) {
        errorMessage = "Apply requires at least one Set or Unset property";
        return false;
    }
    return true;
}

static string SettingsProtocolErrorMessage(const string& message) {
    string sanitized = message;
    for (char& character : sanitized) if (character == '\r' || character == '\n') character = ' ';
    return sanitized;
}

static void PublishSettingsResponse(const string& requestId, bool success, const string& body) {
    const string responseKey = IsValidSettingsRequestId(requestId) ? "Response_" + requestId : "Response";
    string response = "Version=1\nStatus=" + string(success ? "OK" : "ERROR") + "\n";
    if (!body.empty()) response += success ? body : "Message=" + SettingsProtocolErrorMessage(body) + "\n";
    ::SetExtState(ProductIdentity::ExtStateSettings, responseKey.c_str(), response.c_str(), false);
}

static bool ReadSettingsConfigSource(const filesystem::path& configPath, string& source, string& errorMessage) {
    ifstream configFile(configPath, std::ios::binary);
    if (!configFile.is_open()) {
        errorMessage = "Cannot open settings configuration file: " + configPath.string();
        return false;
    }
    ostringstream buffer;
    buffer << configFile.rdbuf();
    if (!configFile.good() && !configFile.eof()) {
        errorMessage = "Cannot read settings configuration file: " + configPath.string();
        return false;
    }
    source = buffer.str();
    return true;
}

static string FormatSettingsConfigIssues(const IntegratorConfig& config) {
    string message;
    for (const IntegratorConfigIssue& issue : config.issues) {
        if (!issue.settingIssue) continue;
        if (!message.empty()) message += "; ";
        message += "line " + std::to_string(issue.lineNumber) + ": " + issue.message;
    }
    return message.empty() ? "Settings configuration is invalid" : message;
}

static bool BuildRuntimeSettingsApplications(vector<unique_ptr<Page>>& runtimePages, const IntegratorConfig& config, vector<RuntimeSettingsApplication>& applications, string& errorMessage) {
    for (auto& runtimePage : runtimePages) {
        const PageConfig* matchedPage = nullptr;
        for (const PageConfig& configuredPage : config.pages) {
            if (configuredPage.name != runtimePage->GetName()) continue;
            if (matchedPage) {
                errorMessage = "More than one PageName=" + string(runtimePage->GetName()) + " exists in the settings configuration";
                return false;
            }
            matchedPage = &configuredPage;
        }
        if (!matchedPage) {
            if (runtimePage->GetSurfaces().empty()) continue;
            errorMessage = "Cannot match runtime Page=" + string(runtimePage->GetName()) + " to the settings configuration";
            return false;
        }
        for (auto& runtimeSurface : runtimePage->GetSurfaces()) {
            const SurfaceAssignmentConfig* matchedSurface = nullptr;
            for (const SurfaceAssignmentConfig& configuredSurface : matchedPage->surfaces) {
                if (configuredSurface.surfaceName != runtimeSurface->GetName()) continue;
                if (matchedSurface) {
                    errorMessage = "More than one Surface=" + string(runtimeSurface->GetName()) + " exists on Page=" + runtimePage->GetName();
                    return false;
                }
                matchedSurface = &configuredSurface;
            }
            if (!matchedSurface) {
                errorMessage = "Cannot match runtime Surface=" + string(runtimeSurface->GetName()) + " on Page=" + runtimePage->GetName();
                return false;
            }
            applications.push_back({ runtimeSurface.get(), matchedSurface->effectiveSettings, matchedSurface->settingOverrides });
        }
    }
    return true;
}

static ControlSurface* FindRuntimeSettingsSurface(vector<unique_ptr<Page>>& pages, const string& pageName, const string& surfaceName, string& resolvedPageName, string& errorMessage) {
    ControlSurface* result = nullptr;
    for (auto& page : pages) {
        if (!pageName.empty() && page->GetName() != pageName) continue;
        for (auto& surface : page->GetSurfaces()) {
            if (surface->GetName() != surfaceName) continue;
            if (result) {
                errorMessage = pageName.empty() ? "Surface=" + surfaceName + " exists on more than one runtime Page; specify Page" : "More than one runtime Surface=" + surfaceName + " exists on Page=" + pageName;
                return nullptr;
            }
            result = surface.get();
            resolvedPageName = page->GetName();
        }
    }
    if (!result) errorMessage = pageName.empty() ? "Cannot find runtime Surface=" + surfaceName : "Cannot find runtime Surface=" + surfaceName + " on Page=" + pageName;
    return result;
}

static string BuildSettingsQueryBody(const SettingsCommandRequest& request, const SettingsValues& productSettings, const SettingOverrides& productOverrides, vector<unique_ptr<Page>>& pages, string& errorMessage) {
    const SettingsValues* effectiveSettings = &productSettings;
    const SettingOverrides* surfaceOverrides = nullptr;
    string resolvedPageName = request.pageName;
    if (request.scope == "Surface") {
        ControlSurface* surface = FindRuntimeSettingsSurface(pages, request.pageName, request.surfaceName, resolvedPageName, errorMessage);
        if (!surface) return "";
        effectiveSettings = &surface->GetSettings();
        surfaceOverrides = &surface->GetSettingOverrides();
    }

    string body = "Scope=" + request.scope + "\n";
    if (request.scope == "Surface") body += "Page=" + resolvedPageName + "\nSurface=" + request.surfaceName + "\n";
    for (const Settings::Definition& definition : Settings::Definitions) {
        if (!SettingAllowsScope(definition, request.scope)) continue;
        string source = "Compiled";
        if (surfaceOverrides && surfaceOverrides->valid && surfaceOverrides->values.count(definition.name) > 0) source = "Surface";
        else if (productOverrides.valid && productOverrides.values.count(definition.name) > 0) source = "Product";
        body += "Value." + string(definition.name) + "=" + effectiveSettings->GetString(definition.name) + "\n";
        body += "Source." + string(definition.name) + "=" + source + "\n";
        body += "Inherited." + string(definition.name) + "=" + (request.scope == "Surface" ? productSettings.GetString(definition.name) : string(definition.defaultValue)) + "\n";
    }
    return body;
}

void CSurfIntegrator::PollAndHandleSettingsCommands() {
    if (!::HasExtState(ProductIdentity::ExtStateSettingsCommand, "Request")) return;
    const string payload = ::GetExtState(ProductIdentity::ExtStateSettingsCommand, "Request");
    ::DeleteExtState(ProductIdentity::ExtStateSettingsCommand, "Request", false);

    SettingsCommandRequest request;
    string errorMessage;
    if (!ParseSettingsCommandRequest(payload, request, errorMessage)) {
        PublishSettingsResponse(request.requestId, false, errorMessage);
        return;
    }

    if (request.command == "Query") {
        const string body = BuildSettingsQueryBody(request, this->productSettings_, this->productSettingOverrides_, this->pages_, errorMessage);
        PublishSettingsResponse(request.requestId, errorMessage.empty(), errorMessage.empty() ? body : errorMessage);
        return;
    }

    const filesystem::path configPath = ProductPaths::FromReaperResourcePath().ConfigFile();
    string source;
    if (!ReadSettingsConfigSource(configPath, source, errorMessage)) {
        PublishSettingsResponse(request.requestId, false, errorMessage);
        return;
    }

    if (request.command == "Apply") {
        if (request.scope == "Surface" && request.pageName.empty()) {
            string resolvedPageName;
            if (!FindRuntimeSettingsSurface(this->pages_, "", request.surfaceName, resolvedPageName, errorMessage)) {
                PublishSettingsResponse(request.requestId, false, errorMessage);
                return;
            }
            request.pageName = resolvedPageName;
        }
        SettingsConfigEditRequest editRequest;
        editRequest.scope = request.scope;
        editRequest.pageName = request.pageName;
        editRequest.surfaceName = request.surfaceName;
        editRequest.changes = request.changes;
        string editedSource;
        if (!EditSettingsConfigSource(source, editRequest, editedSource, errorMessage)) {
            PublishSettingsResponse(request.requestId, false, errorMessage);
            return;
        }
        source = std::move(editedSource);
    }

    IntegratorConfig candidate = ParseIntegratorConfigSource(source, configPath.string());
    if (!candidate.fatalError.empty()) errorMessage = candidate.fatalError;
    else if (!candidate.settingsValid) errorMessage = FormatSettingsConfigIssues(candidate);

    vector<RuntimeSettingsApplication> applications;
    if (errorMessage.empty() && !BuildRuntimeSettingsApplications(this->pages_, candidate, applications, errorMessage)) applications.clear();
    if (!errorMessage.empty()) {
        PublishSettingsResponse(request.requestId, false, errorMessage);
        return;
    }

    if (request.command == "Apply" && !WriteSettingsConfigAtomically(configPath, source, errorMessage)) {
        PublishSettingsResponse(request.requestId, false, errorMessage);
        return;
    }

    this->productSettings_ = candidate.productSettings;
    this->productSettingOverrides_ = candidate.productSettingOverrides;
    for (RuntimeSettingsApplication& application : applications) application.surface->ApplySettings(application.values, application.overrides);
    PublishSettingsResponse(request.requestId, true, "Message=" + request.command + " completed\n");
}
