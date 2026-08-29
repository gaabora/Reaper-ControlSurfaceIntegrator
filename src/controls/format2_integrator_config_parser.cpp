#include "integrator_config_parser.h"

#include "format2_document.h"

#include <charconv>
#include <map>
#include <set>
#include <utility>

static std::string FoldFormat2ConfigId(const std::string& value) {
    std::string folded = value;
    for (char& character : folded) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    return folded;
}

static bool IsValidFormat2ConfigId(const std::string& value) {
    if (value.empty() || !((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= 'a' && value[0] <= 'z'))) return false;
    for (char character : value) if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_')) return false;
    return true;
}

class Format2IntegratorConfigParser {
public:
    Format2IntegratorConfigParser(const std::string& source, const std::string& configPath) : lexical_(LexFormat2Source(source, configPath)) { ValidateFormat2Delimiters(this->lexical_); }

    IntegratorConfig Parse() {
        const std::vector<Format2SyntaxNode> root = ParseFormat2Syntax(this->lexical_, 0);
        this->ParseRoot(root);
        this->ValidateReferences();
        this->ApplySettings();
        for (const Format2Diagnostic& diagnostic : this->lexical_.diagnostics) this->AddIssue(diagnostic.location.line, diagnostic.message);
        if (root.empty() && this->lexical_.diagnostics.empty()) this->config_.fatalError = "Product configuration is empty";
        return std::move(this->config_);
    }

private:
    using PropertyMap = std::map<std::string, const Format2PropertySyntax*>;

    Format2LexResult lexical_;
    IntegratorConfig config_;
    std::map<std::string, int> deviceLines_;
    std::map<std::string, int> pageLines_;
    bool rootSettingsSeen_ = false;

    void AddIssue(int lineNumber, const std::string& message, bool settingIssue = false) {
        this->config_.issues.push_back({lineNumber, message, settingIssue});
        if (settingIssue) this->config_.settingsValid = false;
    }

    bool ParseBlockId(const Format2SyntaxNode& node, const std::string& blockName, std::string& id) {
        if (node.positionalTokens.size() != 2 || node.positionalTokens[0].kind != Format2TokenKind::Bare || node.positionalTokens[1].kind != Format2TokenKind::Bare) {
            this->AddIssue(node.location.line, blockName + " requires one unquoted identifier");
            return false;
        }
        id = node.positionalTokens[1].text;
        if (!IsValidFormat2ConfigId(id)) {
            this->AddIssue(node.positionalTokens[1].location.line, blockName + " ID must start with a letter and contain only ASCII letters, digits, or _");
            return false;
        }
        return true;
    }

    PropertyMap CollectProperties(const Format2SyntaxNode& block, const std::set<std::string>& allowed) {
        PropertyMap properties;
        for (const Format2SyntaxNode& child : block.children) {
            if (child.kind == Format2SyntaxNodeKind::Block) continue;
            if (!child.positionalTokens.empty()) this->AddIssue(child.location.line, "Configuration properties must use Name=Value");
            for (const Format2PropertySyntax& property : child.properties) {
                if (allowed.count(property.name) == 0) {
                    this->AddIssue(property.nameLocation.line, "Unknown " + block.positionalTokens.front().text + " property: " + property.name);
                    continue;
                }
                if (properties.count(property.name) > 0) {
                    this->AddIssue(property.nameLocation.line, "Property " + property.name + " is duplicated");
                    continue;
                }
                properties[property.name] = &property;
            }
        }
        return properties;
    }

    const Format2PropertySyntax* RequireProperty(const PropertyMap& properties, const std::string& name, int lineNumber) {
        const auto entry = properties.find(name);
        if (entry != properties.end()) return entry->second;
        this->AddIssue(lineNumber, "Required property is missing: " + name);
        return nullptr;
    }

    bool ReadScalar(const Format2PropertySyntax* property, std::string& value, bool allowQuoted = false) {
        if (!property) return false;
        if (property->value.list || property->value.scalar.text.empty() || (!allowQuoted && property->value.scalar.quoted)) {
            this->AddIssue(property->nameLocation.line, "Property " + property->name + " requires one unquoted value");
            return false;
        }
        value = property->value.scalar.text;
        return true;
    }

    bool ReadInteger(const Format2PropertySyntax* property, int& value, int minimum, int maximum) {
        std::string text;
        if (!this->ReadScalar(property, text)) return false;
        const char* begin = text.data();
        const char* end = begin + text.size();
        const std::from_chars_result parsed = std::from_chars(begin, end, value);
        if (parsed.ec != std::errc() || parsed.ptr != end || value < minimum || value > maximum) {
            this->AddIssue(property->nameLocation.line, "Property " + property->name + " must be an integer from " + std::to_string(minimum) + " through " + std::to_string(maximum));
            return false;
        }
        return true;
    }

    bool ReadBoolean(const Format2PropertySyntax* property, bool& value) {
        std::string text;
        if (!this->ReadScalar(property, text)) return false;
        if (text == "true") value = true;
        else if (text == "false") value = false;
        else {
            this->AddIssue(property->nameLocation.line, "Property " + property->name + " must be true or false");
            return false;
        }
        return true;
    }

    void ParseRoot(const std::vector<Format2SyntaxNode>& root) {
        for (const Format2SyntaxNode& node : root) {
            if (node.kind != Format2SyntaxNodeKind::Block || node.positionalTokens.empty() || node.positionalTokens[0].kind != Format2TokenKind::Bare) {
                this->AddIssue(node.location.line, "The product configuration root accepts only Settings, Device, and Page blocks");
                continue;
            }
            const std::string& name = node.positionalTokens[0].text;
            if (name == "Settings") this->ParseSettings(node, this->config_.productSettingOverrides, "Product");
            else if (name == "Device") this->ParseDevice(node);
            else if (name == "Page") this->ParsePage(node);
            else this->AddIssue(node.location.line, "Unknown product configuration block: " + name);
        }
        if (this->config_.midiIo.empty() && this->config_.oscIo.empty()) this->AddIssue(1, "Product configuration requires at least one Device block");
        if (this->config_.pages.empty()) this->AddIssue(1, "Product configuration requires at least one Page block");
    }

    void ParseSettings(const Format2SyntaxNode& block, SettingOverrides& overrides, const std::string& scope) {
        if (block.positionalTokens.size() != 1) this->AddIssue(block.location.line, "Settings does not accept a block ID", true);
        if (scope == "Product" && this->rootSettingsSeen_) this->AddIssue(block.location.line, "Root Settings block is duplicated", true);
        if (scope == "Product") this->rootSettingsSeen_ = true;
        overrides.firstLineNumber = block.location.line;
        for (const Format2SyntaxNode& child : block.children) {
            if (child.kind == Format2SyntaxNodeKind::Block || !child.positionalTokens.empty()) {
                overrides.valid = false;
                this->AddIssue(child.location.line, "Settings accepts only setting Name=Value lines", true);
                continue;
            }
            for (const Format2PropertySyntax& property : child.properties) {
                const Settings::Definition* definition = FindSettingDefinition(property.name);
                std::string value;
                if (!definition || !this->ReadScalar(&property, value)) {
                    overrides.valid = false;
                    this->AddIssue(property.nameLocation.line, definition ? "Invalid setting value: " + property.name : "Unknown setting: " + property.name, true);
                    continue;
                }
                if (definition->type == Settings::ValueType::Boolean) {
                    if (value == "true") value = "1";
                    else if (value == "false") value = "0";
                    else {
                        overrides.valid = false;
                        this->AddIssue(property.nameLocation.line, property.name + " must be true or false", true);
                        continue;
                    }
                }
                if (overrides.values.count(property.name) > 0) {
                    overrides.valid = false;
                    this->AddIssue(property.nameLocation.line, "Setting " + property.name + " is duplicated", true);
                    continue;
                }
                overrides.values[property.name] = value;
                overrides.lineNumbers[property.name] = property.nameLocation.line;
            }
        }
    }

    void ParseDevice(const Format2SyntaxNode& block) {
        std::string id;
        if (!this->ParseBlockId(block, "Device", id)) return;
        const std::string canonicalId = FoldFormat2ConfigId(id);
        if (this->deviceLines_.count(canonicalId) > 0) {
            this->AddIssue(block.location.line, "Device ID is duplicated case-insensitively: " + id);
            return;
        }
        this->deviceLines_[canonicalId] = block.location.line;
        const PropertyMap properties = this->CollectProperties(block, {"Type", "Channels", "Input", "Output", "RefreshRate", "MaxMessagesPerRun", "Protocol", "ReceivePort", "TransmitPort", "Address", "MaxPacketsPerRun"});
        std::string type;
        int channels = 0;
        const bool commonValid = this->ReadScalar(this->RequireProperty(properties, "Type", block.location.line), type) && this->ReadInteger(this->RequireProperty(properties, "Channels", block.location.line), channels, 1, 65535);
        SettingOverrides settings;
        int settingsCount = 0;
        for (const Format2SyntaxNode& child : block.children) if (child.kind == Format2SyntaxNodeKind::Block && !child.positionalTokens.empty() && child.positionalTokens[0].text == "Settings") {
            settingsCount++;
            this->ParseSettings(child, settings, "Device");
        } else if (child.kind == Format2SyntaxNodeKind::Block) this->AddIssue(child.location.line, "Device accepts only a nested Settings block");
        if (settingsCount > 1) this->AddIssue(block.location.line, "Device Settings block is duplicated", true);
        if (!commonValid) return;
        if (type == "MIDI") {
            MidiIoConfig device;
            device.lineNumber = block.location.line;
            device.name = id;
            device.channelCount = channels;
            device.refreshRate = 15;
            device.maxMessagesPerRun = 200;
            const bool valid = this->ReadInteger(this->RequireProperty(properties, "Input", block.location.line), device.inputPort, 0, 65535) && this->ReadInteger(this->RequireProperty(properties, "Output", block.location.line), device.outputPort, 0, 65535) && (!properties.count("RefreshRate") || this->ReadInteger(properties.at("RefreshRate"), device.refreshRate, 1, 65535)) && (!properties.count("MaxMessagesPerRun") || this->ReadInteger(properties.at("MaxMessagesPerRun"), device.maxMessagesPerRun, 1, 1000000));
            if (properties.count("Protocol") || properties.count("ReceivePort") || properties.count("TransmitPort") || properties.count("Address") || properties.count("MaxPacketsPerRun")) this->AddIssue(block.location.line, "MIDI Device contains an OSC-only property");
            device.settingOverrides = std::move(settings);
            if (valid) this->config_.midiIo.push_back(std::move(device));
        } else if (type == "OSC") {
            OscIoConfig device;
            device.lineNumber = block.location.line;
            device.name = id;
            device.channelCount = channels;
            device.type = "OSC";
            device.maxPacketsPerRun = 200;
            std::string protocol = "Generic";
            if (properties.count("Protocol")) this->ReadScalar(properties.at("Protocol"), protocol);
            if (protocol == "X32") device.type = "OSCX32";
            else if (protocol != "Generic") this->AddIssue(properties.at("Protocol")->nameLocation.line, "OSC Protocol must be Generic or X32");
            int receivePort = 0;
            int transmitPort = 0;
            const bool valid = this->ReadInteger(this->RequireProperty(properties, "ReceivePort", block.location.line), receivePort, 1, 65535) && this->ReadInteger(this->RequireProperty(properties, "TransmitPort", block.location.line), transmitPort, 1, 65535) && this->ReadScalar(this->RequireProperty(properties, "Address", block.location.line), device.transmitToIpAddress) && (!properties.count("MaxPacketsPerRun") || this->ReadInteger(properties.at("MaxPacketsPerRun"), device.maxPacketsPerRun, 1, 1000000));
            device.receiveOnPort = std::to_string(receivePort);
            device.transmitToPort = std::to_string(transmitPort);
            if (properties.count("Input") || properties.count("Output") || properties.count("RefreshRate") || properties.count("MaxMessagesPerRun")) this->AddIssue(block.location.line, "OSC Device contains a MIDI-only property");
            device.settingOverrides = std::move(settings);
            if (valid) this->config_.oscIo.push_back(std::move(device));
        } else {
            this->AddIssue(block.location.line, "Device Type must be MIDI or OSC");
        }
    }

    void ParsePage(const Format2SyntaxNode& block) {
        std::string id;
        if (!this->ParseBlockId(block, "Page", id)) return;
        const std::string canonicalId = FoldFormat2ConfigId(id);
        if (this->pageLines_.count(canonicalId) > 0) {
            this->AddIssue(block.location.line, "Page ID is duplicated case-insensitively: " + id);
            return;
        }
        this->pageLines_[canonicalId] = block.location.line;
        const PropertyMap properties = this->CollectProperties(block, {"FollowMCP", "SyncPages", "ScrollLink", "ScrollSync"});
        PageConfig page;
        page.lineNumber = block.location.line;
        page.name = id;
        if (properties.count("FollowMCP")) this->ReadBoolean(properties.at("FollowMCP"), page.followsMcp);
        if (properties.count("SyncPages")) this->ReadBoolean(properties.at("SyncPages"), page.synchPages);
        if (properties.count("ScrollLink")) this->ReadBoolean(properties.at("ScrollLink"), page.scrollLink);
        if (properties.count("ScrollSync")) this->ReadBoolean(properties.at("ScrollSync"), page.scrollSynch);
        std::set<std::string> surfaceIds;
        for (const Format2SyntaxNode& child : block.children) {
            if (child.kind != Format2SyntaxNodeKind::Block || child.positionalTokens.empty()) continue;
            if (child.positionalTokens[0].text == "Surface") this->ParseSurface(child, page, surfaceIds);
            else if (child.positionalTokens[0].text == "Link") this->ParseLink(child, page);
            else this->AddIssue(child.location.line, "Unknown Page child block: " + child.positionalTokens[0].text);
        }
        if (page.surfaces.empty()) this->AddIssue(block.location.line, "Page requires at least one valid Surface block");
        this->config_.pages.push_back(std::move(page));
    }

    void ParseSurface(const Format2SyntaxNode& block, PageConfig& page, std::set<std::string>& surfaceIds) {
        std::string id;
        if (!this->ParseBlockId(block, "Surface", id)) return;
        const std::string canonicalId = FoldFormat2ConfigId(id);
        if (!surfaceIds.insert(canonicalId).second) {
            this->AddIssue(block.location.line, "Surface ID is duplicated case-insensitively on Page " + page.name + ": " + id);
            return;
        }
        const PropertyMap properties = this->CollectProperties(block, {"Device", "Template", "MainProfile", "FXProfile", "StartChannel"});
        SurfaceAssignmentConfig surface;
        surface.lineNumber = block.location.line;
        surface.surfaceName = id;
        surface.startChannel = 0;
        if (!this->ReadScalar(this->RequireProperty(properties, "Device", block.location.line), surface.deviceId) || !this->ReadScalar(this->RequireProperty(properties, "Template", block.location.line), surface.surfaceId)) return;
        surface.mainZoneProfileId = surface.surfaceId;
        surface.fxZoneProfileId = surface.mainZoneProfileId;
        if (properties.count("MainProfile")) this->ReadScalar(properties.at("MainProfile"), surface.mainZoneProfileId);
        if (properties.count("FXProfile")) this->ReadScalar(properties.at("FXProfile"), surface.fxZoneProfileId);
        if (properties.count("StartChannel")) this->ReadInteger(properties.at("StartChannel"), surface.startChannel, 0, 1000000);
        page.surfaces.push_back(std::move(surface));
    }

    void ParseLink(const Format2SyntaxNode& block, PageConfig& page) {
        if (block.positionalTokens.size() != 1) {
            this->AddIssue(block.location.line, "Link does not accept a block ID");
            return;
        }
        const PropertyMap properties = this->CollectProperties(block, {"From", "To", "Share"});
        ListenerConfig link;
        link.lineNumber = block.location.line;
        if (!this->ReadScalar(this->RequireProperty(properties, "From", block.location.line), link.broadcasterName) || !this->ReadScalar(this->RequireProperty(properties, "To", block.location.line), link.listenerName)) return;
        const Format2PropertySyntax* share = this->RequireProperty(properties, "Share", block.location.line);
        if (!share || !share->value.list || share->value.items.empty()) {
            this->AddIssue(block.location.line, "Link Share requires a non-empty list");
            return;
        }
        std::set<std::string> categories;
        for (const Format2ScalarSyntax& category : share->value.items) {
            if (!categories.insert(category.text).second) this->AddIssue(category.location.line, "Link Share category is duplicated: " + category.text);
            else if (category.text == "Home") link.goHome = true;
            else if (category.text == "Modifiers") link.modifiers = true;
            else if (category.text == "FXMenu") link.fxMenu = true;
            else if (category.text == "SelectedTrackFX") link.selectedTrackFx = true;
            else if (category.text == "SelectedTrackSends") link.selectedTrackSends = true;
            else if (category.text == "SelectedTrackReceives") link.selectedTrackReceives = true;
            else this->AddIssue(category.location.line, "Unknown Link Share category: " + category.text);
        }
        page.listeners.push_back(std::move(link));
    }

    void ValidateReferences() {
        for (PageConfig& page : this->config_.pages) {
            std::set<std::string> surfaceIds;
            for (const SurfaceAssignmentConfig& surface : page.surfaces) {
                surfaceIds.insert(FoldFormat2ConfigId(surface.surfaceName));
                if (this->deviceLines_.count(FoldFormat2ConfigId(surface.deviceId)) == 0) this->AddIssue(surface.lineNumber, "Surface " + surface.surfaceName + " references unknown Device: " + surface.deviceId);
            }
            std::set<std::string> links;
            for (const ListenerConfig& link : page.listeners) {
                const std::string from = FoldFormat2ConfigId(link.broadcasterName);
                const std::string to = FoldFormat2ConfigId(link.listenerName);
                if (from == to || surfaceIds.count(from) == 0 || surfaceIds.count(to) == 0) this->AddIssue(link.lineNumber, "Link requires two distinct existing Surface IDs on Page " + page.name);
                if (!links.insert(from + "\n" + to).second) this->AddIssue(link.lineNumber, "Link is duplicated on Page " + page.name);
            }
        }
    }

    int FindSettingIssueLine(const SettingOverrides& overrides, const SettingValidationIssue& issue) const {
        const auto lineNumber = overrides.lineNumbers.find(issue.settingName);
        return lineNumber == overrides.lineNumbers.end() ? overrides.firstLineNumber : lineNumber->second;
    }

    void ApplyDeviceSettings(SettingOverrides& overrides, SettingsValues& effectiveSettings) {
        effectiveSettings = this->config_.productSettings;
        std::vector<SettingValidationIssue> issues;
        if (this->config_.productSettings.TryApply(overrides, "Device", effectiveSettings, issues)) return;
        overrides.valid = false;
        for (const SettingValidationIssue& issue : issues) this->AddIssue(this->FindSettingIssueLine(overrides, issue), issue.message, true);
    }

    void ApplyDeviceSettingsToSurfaces() {
        for (PageConfig& page : this->config_.pages) {
            for (SurfaceAssignmentConfig& surface : page.surfaces) {
                const std::string deviceId = FoldFormat2ConfigId(surface.deviceId);
                for (const MidiIoConfig& device : this->config_.midiIo) {
                    if (FoldFormat2ConfigId(device.name) != deviceId) continue;
                    surface.settingOverrides = device.settingOverrides;
                    surface.effectiveSettings = device.effectiveSettings;
                    break;
                }
                for (const OscIoConfig& device : this->config_.oscIo) {
                    if (FoldFormat2ConfigId(device.name) != deviceId) continue;
                    surface.settingOverrides = device.settingOverrides;
                    surface.effectiveSettings = device.effectiveSettings;
                    break;
                }
            }
        }
    }

    void ApplySettings() {
        SettingsValues defaults;
        std::vector<SettingValidationIssue> issues;
        if (!defaults.TryApply(this->config_.productSettingOverrides, "Product", this->config_.productSettings, issues)) {
            this->config_.productSettingOverrides.valid = false;
            for (const SettingValidationIssue& issue : issues) this->AddIssue(this->FindSettingIssueLine(this->config_.productSettingOverrides, issue), issue.message, true);
        }
        for (MidiIoConfig& device : this->config_.midiIo) this->ApplyDeviceSettings(device.settingOverrides, device.effectiveSettings);
        for (OscIoConfig& device : this->config_.oscIo) this->ApplyDeviceSettings(device.settingOverrides, device.effectiveSettings);
        this->ApplyDeviceSettingsToSurfaces();
    }
};

IntegratorConfig ParseFormat2IntegratorConfigSource(const std::string& source, const std::string& configPath) {
    Format2IntegratorConfigParser parser(source, configPath);
    return parser.Parse();
}
